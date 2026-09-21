#include "catpkg/builder.h"
#include "catpkg/pkginfo.h"
#include "catpkg/command.h"
#include "utils/fs.h"
#include "catpkg/path.h"
#include "catpkg/ctpg.h"
#include "configuration.h"
#include "verify.h"
#include "database.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <libgen.h>
#include <dirent.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/statvfs.h>

#define CATPKG_DEBUG

#ifdef CATPKG_DEBUG

#define CATPKG_STRINGIFY_(x) #x
#define CATPKG_STRINGIFY(x) CATPKG_STRINGIFY_(x)

#define CATPKG_ERROR \
    "ERROR_" CATPKG_STRINGIFY(__LINE__)

#else

#define CATPKG_ERROR \
    "ERROR"

#endif

#define CATPKG_BUILD_LOG_TREE(revert) \
    do { \
        builder_clear_progress(); \
        catpkg_build_log( \
            "%s── ", \
            builder, \
            false, \
            ((revert \
                ? builder->applied_count - 1 <= 0 \
                : builder->applied_count >= builder->requests_size - 1) \
                    ? "└" \
                    : "├") \
        ); \
        build_log_line_started = true; \
    } while (0)

#define CATPKG_BUILD_LOG_ARGS(operation, path) \
    operation, path

#define CATPKG_BUILD_LOG_MOVE_ARGS(operation, source, destination) \
    operation, source, destination

#define CATPKG_BUILD_LOG_CTPG_ARGS(name, vesrion) \
    name, vesrion

#define CATPKG_BUILD_LOG(format, result, revert, args) \
    catpkg_build_log( \
        format "%s%s\n", \
        builder, \
        true, \
        args, \
        (result)[0] != '\0' ? \
            ((revert ? builder->applied_count - 1 <= 0 : \
                       builder->applied_count >= builder->requests_size - 1) ? \
                "\n \t└── " : "\n|\t└── ") : \
            "", \
        (result)[0] != '\0' ? result : "")

#define CATPKG_BUILD_LOG_MOVE_ERROR(operation, source, destination) \
    CATPKG_BUILD_LOG( \
        "%s: \"%s\" -> \"%s\"", \
        CATPKG_ERROR, \
        0, \
        CATPKG_BUILD_LOG_MOVE_ARGS(operation, source, destination))

#define CATPKG_BUILD_LOG_MOVE_OK(operation, source, destination) \
    CATPKG_BUILD_LOG( \
        "%s: \"%s\" -> \"%s\"", \
        "OK", \
        0, \
        CATPKG_BUILD_LOG_MOVE_ARGS(operation, source, destination))

#define CATPKG_BUILD_LOG_PATH_OK(operation, path) \
    CATPKG_BUILD_LOG( \
        "%s: \"%s\"", \
        "OK", \
        0, \
        CATPKG_BUILD_LOG_ARGS(operation, path))

#define CATPKG_BUILD_LOG_PATH_ERROR(operation, path) \
    CATPKG_BUILD_LOG( \
        "%s: \"%s\"", \
        CATPKG_ERROR, \
        0, \
        CATPKG_BUILD_LOG_ARGS(operation, path))


/*
 *	Service state
 */

static bool build_log_active = 0;
static bool build_log_line_started = false;
static struct CtpgContext *ctpg_context = NULL;
static struct OperationSecureContext *opsec_context = NULL;

static void builder_progress_bar(
    size_t current,
    size_t total
)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return;

    size_t width = ws.ws_col;

    if (width < 10)
        return;

    if (current > total)
        current = total;

    double progress = total > 0
        ? (double)current / (double)total
        : 1.0;

    size_t percent = (size_t)(progress * 100.0);

    size_t bar_width = width - 7;

    size_t filled =
        (size_t)(progress * (double)bar_width);

    printf("\r[");

    for (size_t i = 0; i < bar_width; i++) {
        if (i < filled)
            putchar('=');
        else if (i == filled && filled < bar_width)
            putchar('>');
        else
            putchar(' ');
    }

    printf("] %3zu%%", percent);

    fflush(stdout);
}

static size_t catpkg_directory_count(const char *path)
{
    DIR *dir = opendir(path);

    if (dir == NULL)
        return 0;

    size_t count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (
            strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0
        )
            continue;

        count++;
    }

    closedir(dir);

    return count;
}

/*
 * ============================================================
 * Cache
 * ============================================================
 */

static void builder_clear_progress(void)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return;

    printf("\r%*s\r", (int)ws.ws_col, "");
    fflush(stdout);
}

static void catpkg_build_log(
    const char *format,
    struct CatpkgBuilder *builder,
    bool progressbar_enabled,
    ...)
{
    if (
        progressbar_enabled &&
        !build_log_line_started
    ) {
        builder_clear_progress();
    }

    if (build_log_active) {
        va_list args;

        va_start(args, progressbar_enabled);
        vprintf(format, args);
        va_end(args);

        fflush(stdout);
    }

    if (progressbar_enabled) {
        build_log_line_started = false;

        builder_progress_bar(
            builder->applied_count + 1,
            builder->requests_size
        );
    }
}

static int catpkg_builder_cache_change(
    const char *path
)
{
    int pid = fork();

    if (pid < 0)
        return 1;

    if (pid == 0) {

        char *full_path =
            catpkg_absolute_path(path);

        if (full_path == NULL)
            _exit(1);

        if (access(
                CATPKG_CACHE "/.catpkg_builder_cache",
                F_OK
            ) == 0) {

            execl(
                "/usr/bin/tar",
                "tar",
                "-rf",
                CATPKG_CACHE "/.catpkg_builder_cache",
                "-C",
                "/",
                full_path + 1,
                (char *)NULL
            );

        } else {
            execl(
                "/usr/bin/tar",
                "tar",
                "-cf",
                CATPKG_CACHE "/.catpkg_builder_cache",
                "-C",
                "/",
                full_path + 1,
                (char *)NULL
            );
        }

        free(full_path);

        _exit(1);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0)
        return 1;

    if (!WIFEXITED(status))
        return 1;

    if (WEXITSTATUS(status) != 0)
        return 1;

    return 0;
}


static int catpkg_builder_cache_del(
    const char *path
)
{
    int pid = fork();

    if (pid < 0)
        return 1;

    if (pid == 0) {

        char *full_path =
            catpkg_absolute_path(path);

        if (full_path == NULL)
            _exit(1);

        execl(
            "/usr/bin/tar",
            "tar",
            "--delete",
            "-f",
            CATPKG_CACHE "/.catpkg_builder_cache",
            full_path + 1,
            (char *)NULL
        );

        free(full_path);

        _exit(1);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0)
        return 1;

    if (!WIFEXITED(status))
        return 1;

    if (WEXITSTATUS(status) != 0)
        return 1;

    return 0;
}


static int catpkg_builder_cache_extract(
    const char *path,
    const char *extract_to
)
{
    int pid = fork();

    if (pid < 0)
        return 1;

    if (pid == 0) {
        char *full_path =
            catpkg_absolute_path(path);

        if (full_path == NULL)
            _exit(1);

        execl(
            "/usr/bin/tar",
            "tar",
            "-xf",
            CATPKG_CACHE "/.catpkg_builder_cache",
            "-C",
            extract_to,
            full_path + 1,
            (char *)NULL
        );

        free(full_path);

        _exit(1);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0)
        return 1;

    if (!WIFEXITED(status))
        return 1;

    if (WEXITSTATUS(status) != 0)
        return 1;

    return 0;
}

/*
 * ============================================================
 * UID parsing
 * ============================================================
 */

static int catpkg_parse_uid(
    const char *value,
    uid_t *uid
)
{
    if (value == NULL || uid == NULL)
        return 1;

    errno = 0;

    char *end;

    unsigned long parsed =
        strtoul(value, &end, 10);

    /*
     * Empty string.
     */
    if (end == value)
        return 1;

    /*
     * Invalid trailing characters.
     *
     * "1000abc" is invalid.
     */
    if (*end != '\0')
        return 1;

    /*
     * strtoul() overflow.
     */
    if (errno == ERANGE)
        return 1;

    /*
     * Make sure the value fits into uid_t.
     */
    if (parsed > (unsigned long)((uid_t)-1))
        return 1;

    *uid = (uid_t)parsed;

    return 0;
}
static int calc_size(
	off_t *change_size,
	enum CatpkgProtocol protocol,
	struct CatpkgRequest *request,
	const bool *revert
) {
    struct stat statbuf;

    *change_size = 0;

    char *request_val =
        request->value;

	if(!revert)
		switch(protocol) {
            #include "builder/behaviors/protocols/calc_size.inc"
		}
	return 0;
}

static int check_opsec_context(
    struct CatpkgBuilder *builder,
    struct CatpkgRequest *request,
    enum CatpkgProtocol protocol,
    struct CatpkgError *error
) {
    struct PackageInfo *info = opsec_context -> package_info;
    struct PackageInfo *file_info = opsec_context -> file_info;
    
    char *request_val =
            request->value;
    switch(protocol) {
        #include "builder/behaviors/protocols/check_opsec_context.inc"
    }
    return CATPKG_OK;
}

/*
 * ============================================================
 * Builder initialization
 * ============================================================
 */

int catpkg_builder_init(
    struct CatpkgBuilder *builder
)
{
    if (builder == NULL)
        return 1;

    builder->requests = NULL;
    builder->requests_size = 0;
    builder->applied_count = 0;
    builder->change_size = 0;

    return 0;
}


/*
 * ============================================================
 * Request
 * ============================================================
 *
 *
 * result is not owned by Builder.
 *
 * ============================================================
 */

int catpkg_builder_request(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value
)
{
    if (builder == NULL)
        return 1;

    struct CatpkgRequest *request =
        malloc(sizeof(*request));

    if (request == NULL)
        return 1;

    request->protocol = protocol;
    request->result = NULL;
    request->previous_euid = 0;
    request->value_copy = 0;

    request->value = value;

    size_t new_size =
        builder->requests_size + 1;

    struct CatpkgRequest **tmp =
        realloc(
            builder->requests,
            new_size * sizeof(*builder->requests)
        );

    if (tmp == NULL) {

        free(request->value);
        free(request);

        return 1;
    }

    builder->requests = tmp;

    builder->requests[
        builder->requests_size
    ] = request;

    builder->requests_size = new_size;

    off_t change_size = 0;
    calc_size(
    	&change_size, 
    	protocol,
    	request,
    	0
    );
    builder -> change_size += change_size;
    return 0;
}

int catpkg_builder_request_copy(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value
)
{
    if (builder == NULL)
        return 1;

    struct CatpkgRequest *request =
        malloc(sizeof(*request));

    if (request == NULL)
        return 1;

    request->protocol = protocol;
    request->result = NULL;
    request->previous_euid = 0;
    request->value_copy = 1;


    /*
     * Make a private copy of value.
     *
     * request owns this memory from now on.
     */

    if (value != NULL) {
        request->value =
            strdup((const char *)value);

        if (request->value == NULL) {
            free(request);
            return 1;
        }
    }
    else {
        request->value = NULL;
    }


    /*
     * Add request to builder.
     */

    size_t new_size =
        builder->requests_size + 1;

    struct CatpkgRequest **tmp =
        realloc(
            builder->requests,
            new_size *
                sizeof(*builder->requests)
        );

    if (tmp == NULL) {
        free(request->value);
        free(request);

        return 1;
    }

    builder->requests = tmp;

    builder->requests[
        builder->requests_size
    ] = request;

    builder->requests_size =
        new_size;


    /*
     * Calculate size change.
     */

    off_t change_size = 0;

    calc_size(
        &change_size,
        protocol,
        request,
        0
    );

    builder->change_size +=
        change_size;

    return 0;
}

/*
 * ============================================================
 * Request with result
 * ============================================================
 */

int catpkg_builder_request_value(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value,
    void *result
)
{
    if (builder == NULL)
        return 1;

    struct CatpkgRequest *request =
        malloc(sizeof(*request));

    if (request == NULL)
        return 1;

    request->protocol = protocol;
    request->result = result;
    request->previous_euid = 0;

    request->value = value;

    size_t new_size =
        builder->requests_size + 1;

    struct CatpkgRequest **tmp =
        realloc(
            builder->requests,
            new_size * sizeof(*builder->requests)
        );

    if (tmp == NULL) {

        free(request->value);
        free(request);

        return 1;
    }

    builder->requests = tmp;

    builder->requests[
        builder->requests_size
    ] = request;

    builder->requests_size = new_size;

    off_t change_size = 0;
    calc_size(
    	&change_size, 
    	protocol,
    	request,
    	0
    );
    builder -> change_size += change_size;

    return 0;
}


/*
 * ============================================================
 * Apply
 * ============================================================
 */

int catpkg_builder_apply(
    struct CatpkgBuilder *builder
)
{
    if (builder == NULL)
        return 1;

    /*
     * Start from the first unapplied request.
     *
     * This also allows apply() to continue after
     * a previous partial failure.
     */
    for (
        size_t i = builder->applied_count;
        i < builder->requests_size;
        i++
    ) {
        
        struct CatpkgRequest *request =
            builder->requests[i];

        char *request_val =
            request->value;
        char *request_result =
            request->result;

        switch (request->protocol) {
            #include "builder/behaviors/protocols/apply.inc"
        }


        /*
         * Request was successfully applied.
         */
        builder->applied_count++;
    }
    return 0;
}


/*
 * ============================================================
 * Revert
 * ============================================================
 */

#pragma push_macro("CATPKG_BUILD_LOG")
#pragma push_macro("CATPKG_BUILD_LOG_TREE")

#undef CATPKG_BUILD_LOG
#undef CATPKG_BUILD_LOG_TREE

#define CATPKG_BUILD_LOG_TREE(revert) \
    catpkg_build_log("%s── ", \
        builder, \
        false, \
        ((revert ? builder->applied_count - 1 <= 0 : builder->applied_count >= builder->requests_size - 1)) ? "└" : "├")

#define CATPKG_BUILD_LOG(format, result, revert, args) \
    catpkg_build_log( \
        format "%s%s\n", \
        builder, \
        false, \
        args, \
        (result)[0] != '\0' ? \
            ((revert ? builder->applied_count - 1 <= 0 : \
                       builder->applied_count >= builder->requests_size - 1) ? \
                "\n \t└── " : "\n|\t└── ") : \
            "", \
        (result)[0] != '\0' ? result : "")

int catpkg_builder_revert(
    struct CatpkgBuilder *builder
)
{
    if (builder == NULL)
        return 1;

    while (builder->applied_count > 0) {

        size_t index =
            builder->applied_count - 1;

        struct CatpkgRequest *request =
            builder->requests[index];

        char *request_val =
            request->value;

        switch (request->protocol) {
            #include "builder/behaviors/protocols/revert.inc"
        }

        /*
         * Request was successfully reverted.
         */
        builder->applied_count--;
    }

    return 0;
}

#pragma pop_macro("CATPKG_BUILD_LOG_TREE")
#pragma pop_macro("CATPKG_BUILD_LOG")


/*
 * ============================================================
 * Free
 * ============================================================
 */

int catpkg_builder_free(
    struct CatpkgBuilder *builder
)
{
    if (builder == NULL)
        return 1;

    for (
        size_t i = 0;
        i < builder->requests_size;
        i++
    ) {
        if(builder->requests[i]->value_copy)
            free(builder->requests[i]->value);
        free(
            builder->requests[i]
        );
    }

    free(builder->requests);

    builder->requests = NULL;
    builder->requests_size = 0;
    builder->applied_count = 0;

    return 0;
}