#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <inttypes.h>
#include <libgen.h>

#include "configuration.h"
#include "install.h"
#include "verify.h"

// Utils
#include "catpkg/path.h"
#include "catpkg/pkginfo.h"
#include "catpkg/builder.h"
#include "catpkg/cache.h"
#include "catpkg/command.h"
#include "utils/nftw.h"
#include "utils/fs.h"
#include "utils/allow.h"
#include "utils/sha256.h"

/*
 * The end result:
 *
 * CATPKG_BUILD_PATH        -> /
 * CATPKG_TMP_FILES_PATH    -> CATPKG_FILES_PATH
 * CATPKG_TMP_METADATA_PATH -> CATPKG_METADATA_PATH
 */


int catpkg_install(
    const char *path,
    bool assume_yes
)
{
    if (path == NULL) {

        fprintf(
            stderr,
            "catpkg: package path is NULL\n"
        );

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Validate package
     * --------------------------------------------------------
     */


    /*
     * Check package extension.
     */

    if (!is_catpackage(path)) {

        fprintf(
            stderr,
            "catpkg: \"%s\" is not a .catpackage file\n",
            path
        );

        return 1;
    }

    /*
     * --------------------------------------------------------
     * Parse PACKAGEINFO
     *
     * PackageInfo MUST remain alive until the whole builder
     * transaction has finished, because request values may
     * point directly into its fields.
     * --------------------------------------------------------
     */

    struct PackageInfo *info = CATPKG_Parse(path);

    if (info == NULL)
        return 1;

    const struct PackageField *name_field =
        PackageInfo_Find(info, "name");

    const struct PackageField *version_field =
        PackageInfo_Find(info, "version");

    const struct PackageField *sha256_field =
        PackageInfo_Find(info, "SHA");

    if (
        name_field == NULL ||
        version_field == NULL ||
        sha256_field == NULL
    ) {
        PackageInfo_Free(info);
        return 1;
    }

    /*
     * --------------------------------------------------------
     * Package fullname
     * --------------------------------------------------------
     */

    size_t package_fullname_size =
        strlen(name_field->value) +
        1 +
        strlen(version_field->value) +
        1;

    char *package_fullname =
        malloc(package_fullname_size);

    if (package_fullname == NULL) {
        PackageInfo_Free(info);
        return 1;
    }

    snprintf(
        package_fullname,
        package_fullname_size,
        "%s@%s",
        name_field->value,
        version_field->value
    );

    /*
     * --------------------------------------------------------
     * Builder
     * --------------------------------------------------------
     */

    struct CatpkgBuilder builder;

    if (catpkg_builder_init(&builder) != 0) {
        free(package_fullname);
        PackageInfo_Free(info);
        return 1;
    }

    /*
     * Every dynamically allocated request value below belongs
     * to this function, not to Builder.
     *
     * Therefore all of them must remain alive until:
     *
     *     apply()
     *     or
     *     revert()
     *
     * has finished.
     */

    char *metadata_path = NULL;
    char *files_path = NULL;
    char *fileinfo_path = NULL;
    char *package_path = NULL;
    char *package_info_path = NULL;
    char *package_commands_path = NULL;

    /*
     * --------------------------------------------------------
     * Calculate paths
     * --------------------------------------------------------
     */

    metadata_path =
        make_catpkg_path(
            CATPKG_METADATA_PATH,
            package_fullname
        );

    files_path =
        make_catpkg_path(
            CATPKG_FILES_PATH,
            package_fullname
        );

    fileinfo_path =
        make_catpkg_path(
            CATPKG_FILES_PATH "/" CATPKG_FILE,
            package_fullname
        );

    package_path =
        make_catpkg_path(
            CATPKG_PACKAGE_PATH,
            package_fullname
        );

    package_info_path =
        make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            package_fullname
        );

    package_commands_path =
        make_catpkg_path(
            CATPKG_PACKAGE_DATA
            "/.catpkg/commands",
            name_field -> value
        );

    if (
        metadata_path == NULL ||
        files_path == NULL ||
        package_path == NULL
    ) {
        free(metadata_path);
        free(files_path);
        free(package_path);
        free(package_fullname);
        free(fileinfo_path);
        free(package_info_path);
        free(package_commands_path);

        catpkg_builder_free(&builder);
        PackageInfo_Free(info);

        return 1;
    }

    catpkg_builder_request(
        &builder,
        BUILD_LOG_START,
        NULL
    );

    #include "install/BUILDER_INSTALL_PROTOCOLS.inc"

    catpkg_builder_request(
        &builder,
        CATPKG_MERGE_DATABASE,
        CATPKG_DATABASE_DIR_PATH "/" CATPKG_DATABASE
    );

    catpkg_builder_request(
        &builder,
        BUILD_LOG_END,
        NULL
    );

    /*
     * --------------------------------------------------------
     * At this point Builder has already calculated the total
     * change_size.
     *
     * Nothing has been applied yet.
     * --------------------------------------------------------
     */

    struct stat st;

    char package_size[64] = "-";
    char will_be_installed[64] = "-";
    char changing[64] = "-";

    if (stat(path, &st) != 0) {
        perror("stat");
        return 1;
    }

    catpkg_format_size(
        (size_t)st.st_size,
        package_size,
        sizeof(package_size)
    );

    size_t absolute =
        (size_t)(
            builder.change_size < 0
                ? -builder.change_size
                : builder.change_size
        );

    catpkg_format_size(
        absolute,
        will_be_installed,
        sizeof(will_be_installed)
    );

    int64_t changing_size =
        (int64_t)builder.change_size - (int64_t)st.st_size;

    char sign = changing_size < 0 ? '-' : '+';

    absolute =
        (size_t)(
            changing_size < 0
                ? -changing_size
                : changing_size
        );

    catpkg_format_size(
        absolute,
        changing,
        sizeof(changing)
    );

    printf(
        ALLOW_INSTALLATION,
        package_fullname,
        package_size,
        will_be_installed,
        sign,
        changing
    );
    /*
     * Confirmation should happen HERE, before apply().
     *
     * The exact condition can use builder.change_size.
     */

/*            name_field->value,
            version_field->value,
            builder.change_size*/

    if (!catpkg_confirm()) {
        goto cancel;
    }

    /*
     * --------------------------------------------------------
     * Apply
     * --------------------------------------------------------
     */

    printf("Starting the installation of the \"%s\" package...\n", package_fullname);
    if (catpkg_builder_apply(&builder) != 0) {

        /*
         * Builder only reverts requests which were successfully
         * applied, according to applied_count.
         */

        printf("ERROR Recovery: Reverting the changes...\n");
        catpkg_builder_revert(&builder);

        goto error;
    }

    /*
     * --------------------------------------------------------
     * Success
     * --------------------------------------------------------
     */

    printf("The package “%s” has been installed!\n", package_fullname);
    free(metadata_path);
    free(files_path);
    free(package_path);
    free(package_fullname);
    free(package_info_path);
    free(package_commands_path);
    free(fileinfo_path);

    catpkg_builder_free(&builder);
    PackageInfo_Free(info);

    printf("Continued.\n");
    return 0;


prepare_error:

    /*
     * Preparation itself did not modify the filesystem.
     * Just destroy the builder and owned temporary values.
     */

cancel:

    free(metadata_path);
    free(files_path);
    free(package_path);
    free(package_info_path);
    free(package_commands_path);
    free(package_fullname);

    catpkg_builder_free(&builder);
    PackageInfo_Free(info);

    return 1;


error:
    printf("An error occurred during the package installation.\n");
    free(metadata_path);
    free(files_path);
    free(package_path);
    free(package_info_path);
    free(package_commands_path);
    free(package_fullname);

    catpkg_builder_free(&builder);
    PackageInfo_Free(info);
    printf("Continued.\n");

    return 1;
}