#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <ftw.h>

#include "configuration.h"
#include "catpkg/pkginfo.h"
#include "utils/sha256.h"
#include "utils/fs.h"
#include "build.h"
#include "verify.h"


/*
 * Check whether PACKAGEINFO exists
 * and is a regular file.
 */
static int packageinfo_exists(
    const char *path
)
{
    struct stat st;

    if (stat(path, &st) != 0)
        return 0;

    return S_ISREG(st.st_mode);
}


/*
 * Create TAR archive from the current
 * working directory.
 *
 * Equivalent to:
 *
 * tar -cf <archive> .
 *
 * The archive itself is excluded.
 */
static int create_tar(
    const char *archive
)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("catpkg: fork");
        return 1;
    }

    if (pid == 0) {

        /*
         * Try /usr/bin/tar.
         *
         * Remove "./" from paths stored in the archive.
         */
        execl(
            "/usr/bin/tar",
            "tar",
            "-cf",
            archive,
            "--exclude",
            archive,
            "--transform=s|^\\./||",
            ".",
            (char *)NULL
        );

        /*
         * Fallback to /bin/tar.
         */
        execl(
            "/bin/tar",
            "tar",
            "-cf",
            archive,
            "--exclude",
            archive,
            "--transform=s|^\\./||",
            ".",
            (char *)NULL
        );

        perror("catpkg: tar");

        _exit(127);
    }


    int status;

    if (waitpid(
            pid,
            &status,
            0
        ) < 0) {

        perror("catpkg: waitpid");
        return 1;
    }


    if (!WIFEXITED(status)) {

        fprintf(
            stderr,
            "catpkg: tar terminated abnormally\n"
        );

        return 1;
    }


    int exit_code =
        WEXITSTATUS(status);


    if (exit_code != 0) {

        fprintf(
            stderr,
            "catpkg: tar exited with code %d\n",
            exit_code
        );

        return 1;
    }


    return 0;
}


/*
 * Remove PACKAGEINFO from an existing TAR
 * and insert the new PACKAGEINFO.
 *
 * packageinfo_tmp:
 *
 *     ./PACKAGEINFO.tmp
 *
 * archive:
 *
 *     package.tar
 *
 * Result:
 *
 *     package.tar
 *     └── ./PACKAGEINFO
 *
 * The temporary file itself is never stored
 * in the archive.
 */
static int replace_packageinfo(
    const char *archive,
    const char *packageinfo_tmp
)
{
    char temp_dir[] = ".catpkg_packageinfo_XXXXXX";

    /*
     * Create temporary directory.
     */
    if (mkdtemp(temp_dir) == NULL) {
        perror(
            "catpkg: failed to create temporary directory"
        );

        return 1;
    }


    /*
     * Build path:
     *
     * .catpkg_packageinfo_XXXXXX/PACKAGEINFO
     */
    char packageinfo_path[PATH_MAX];

    int written = snprintf(
        packageinfo_path,
        sizeof(packageinfo_path),
        "%s/" CATPKG_PACKAGEINFO,
        temp_dir
    );

    if (written < 0 ||
        (size_t)written >= sizeof(packageinfo_path)) {

        fprintf(
            stderr,
            "catpkg: temporary PACKAGEINFO path is too long\n"
        );

        rmdir(temp_dir);

        return 1;
    }


    /*
     * Copy PACKAGEINFO.tmp into the temporary
     * directory under the final name PACKAGEINFO.
     */
    FILE *src = fopen(
        packageinfo_tmp,
        "rb"
    );

    if (src == NULL) {
        perror(
            "catpkg: failed to open temporary PACKAGEINFO"
        );

        rmdir(temp_dir);

        return 1;
    }


    FILE *dst = fopen(
        packageinfo_path,
        "wb"
    );

    if (dst == NULL) {
        perror(
            "catpkg: failed to create temporary PACKAGEINFO copy"
        );

        fclose(src);
        rmdir(temp_dir);

        return 1;
    }


    char buffer[65536];
    size_t bytes;

    int copy_failed = 0;

    while ((bytes = fread(
                buffer,
                1,
                sizeof(buffer),
                src
            )) > 0) {

        if (fwrite(
                buffer,
                1,
                bytes,
                dst
            ) != bytes) {

            perror(
                "catpkg: failed to write PACKAGEINFO copy"
            );

            copy_failed = 1;
            break;
        }
    }


    if (ferror(src)) {
        perror(
            "catpkg: failed to read temporary PACKAGEINFO"
        );

        copy_failed = 1;
    }


    if (fclose(src) != 0) {
        perror(
            "catpkg: failed to close temporary PACKAGEINFO"
        );

        copy_failed = 1;
    }


    if (fclose(dst) != 0) {
        perror(
            "catpkg: failed to close PACKAGEINFO copy"
        );

        copy_failed = 1;
    }


    if (copy_failed) {
        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    /*
     * First remove the old PACKAGEINFO.
     *
     * tar --delete -f archive ./PACKAGEINFO
     */
    pid_t pid = fork();

    if (pid < 0) {
        perror("catpkg: fork");
        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    if (pid == 0) {

        execl(
            "/usr/bin/tar",
            "tar",
            "--delete",
            "-f",
            archive,
            CATPKG_PACKAGEINFO,
            (char *)NULL
        );

        execl(
            "/bin/tar",
            "tar",
            "--delete",
            "-f",
            archive,
            CATPKG_PACKAGEINFO,
            (char *)NULL
        );

        perror("catpkg: tar");

        _exit(127);
    }


    int status;

    if (waitpid(
            pid,
            &status,
            0
        ) < 0) {

        perror("catpkg: waitpid");

        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to remove old PACKAGEINFO from archive\n"
        );

        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    /*
     * Now append the new PACKAGEINFO.
     *
     * We use:
     *
     * tar -rf archive -C temp_dir PACKAGEINFO
     *
     * This is important because the file is stored
     * as "PACKAGEINFO", not ".catpkg_.../PACKAGEINFO".
     */
    pid = fork();

    if (pid < 0) {
        perror("catpkg: fork");

        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    if (pid == 0) {

        execl(
            "/usr/bin/tar",
            "tar",
            "-rf",
            archive,
            "-C",
            temp_dir,
            "PACKAGEINFO",
            (char *)NULL
        );

        execl(
            "/bin/tar",
            "tar",
            "-rf",
            archive,
            "-C",
            temp_dir,
            "PACKAGEINFO",
            (char *)NULL
        );

        perror("catpkg: tar");

        _exit(127);
    }


    if (waitpid(
            pid,
            &status,
            0
        ) < 0) {

        perror("catpkg: waitpid");

        unlink(packageinfo_path);
        rmdir(temp_dir);

        return 1;
    }


    /*
     * Remove temporary files.
     */
    unlink(packageinfo_path);
    rmdir(temp_dir);


    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to insert new PACKAGEINFO into archive\n"
        );

        return 1;
    }


    return 0;
}


static off_t package_size;


static int catpkg_size_callback(
    const char *path,
    const struct stat *st,
    int typeflag,
    struct FTW *ftwbuf
)
{
    (void)ftwbuf;

    if (strcmp(path, "./CATPACKAGE") == 0)
        return 0;

    if (typeflag == FTW_F)
        package_size += st->st_size;

    return 0;
}


off_t catpkg_get_size(
    const char *path
)
{
    package_size = 0;

    if (nftw(
            path,
            catpkg_size_callback,
            20,
            FTW_PHYS
        ) != 0) {

        return -1;
    }

    return package_size;
}


/*
 * Build package.
 *
 * Reads:
 *
 *     PACKAGEINFO
 *
 * Creates:
 *
 *     <name>@<version>.catpackage
 *
 *
 * Build process:
 *
 *     1. Create TAR
 *     2. Calculate SHA-256 excluding PACKAGEINFO
 *     3. Generate PACKAGEINFO.tmp
 *     4. Replace PACKAGEINFO inside TAR
 *     5. Rename TAR -> CATPACKAGE
 */
int catpkg_build(const char *a10e)
{
    printf("[INFO] Starting package build...\n");
    struct utsname system_info;

    /*
     * Check PACKAGEINFO.
     */
    printf(
        "[INFO] Checking %s...\n",
        CATPKG_PACKAGEINFO
    );


    if (!packageinfo_exists(CATPKG_PACKAGEINFO)) {

        fprintf(
            stderr,
            "catpkg: %s not found\n",
            CATPKG_PACKAGEINFO
        );

        return 1;
    }


    printf(
        "[INFO] %s found.\n",
        CATPKG_PACKAGEINFO
    );


    /*
     * Parse PACKAGEINFO.
     *
     * Keep the original file intact.
     */
    printf(
        "[INFO] Parsing %s...\n",
        CATPKG_PACKAGEINFO
    );


    struct PackageInfo *info =
        CATPKG_Parse(CATPKG_PACKAGEINFO);


    if (info == NULL) {

        fprintf(
            stderr,
            "catpkg: failed to parse %s\n",
            CATPKG_PACKAGEINFO
        );

        return 1;
    }


    /*
     * Find package name.
     */
    const struct PackageField *name_field =
        PackageInfo_Find(
            info,
            "name"
        );


    if (name_field == NULL ||
        name_field->value == NULL ||
        name_field->value[0] == '\0') {

        fprintf(
            stderr,
            "catpkg: PACKAGEINFO does not contain "
            "a valid package name\n"
        );

        PackageInfo_Free(info);

        return 1;
    }


    /*
     * Find package version.
     */
    const struct PackageField *version_field =
        PackageInfo_Find(
            info,
            "version"
        );


    if (version_field == NULL ||
        version_field->value == NULL ||
        version_field->value[0] == '\0') {

        fprintf(
            stderr,
            "catpkg: PACKAGEINFO does not contain "
            "a valid package version\n"
        );

        PackageInfo_Free(info);

        return 1;
    }
    /*
     * Find package architecture.
     */
    const struct PackageField *architecture_field =
        PackageInfo_Find(
            info,
            "architecture"
        );

    /*
     * Build archive names.
     */
    char tar_path[PATH_MAX];
    char catpackage_path[PATH_MAX];


    int written = snprintf(
        tar_path,
        sizeof(tar_path),
        "%s@%s.tar",
        name_field->value,
        version_field->value
    );


    if (written < 0 ||
        (size_t)written >= sizeof(tar_path)) {

        fprintf(
            stderr,
            "catpkg: TAR path is too long\n"
        );

        PackageInfo_Free(info);

        return 1;
    }


    written = snprintf(
        catpackage_path,
        sizeof(catpackage_path),
        "%s@%s.catpackage",
        name_field->value,
        version_field->value
    );


    if (written < 0 ||
        (size_t)written >= sizeof(catpackage_path)) {

        fprintf(
            stderr,
            "catpkg: CATPACKAGE path is too long\n"
        );

        PackageInfo_Free(info);

        return 1;
    }


    /*
     * Find package description.
     */
    const struct PackageField *description_field =
        PackageInfo_Find(
            info,
            "description"
        );


    /*
     * Find package operations.
     */
    const struct PackageField *operations_field =
        PackageInfo_Find(
            info,
            "operations"
        );
    
    /*
     * Calculate package size.
     *
     * This is done before replacing PACKAGEINFO.
     */
    off_t size =
        catpkg_get_size("./");

    /*
     * Create TAR archive.
     */
    printf(
        "[INFO] Creating TAR archive...\n"
    );

    printf(
        "[INFO] Destination: %s\n",
        tar_path
    );


    if (create_tar(tar_path) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to create TAR archive\n"
        );

        PackageInfo_Free(info);

        return 1;
    }


    printf(
        "[INFO] TAR archive created successfully.\n"
    );


    if (size < 0) {

        fprintf(
            stderr,
            "catpkg: failed to calculate package size\n"
        );

        PackageInfo_Free(info);

        unlink(tar_path);

        return 1;
    }


    char size_buffer[64];


    catpkg_format_size(
        size,
        size_buffer,
        sizeof(size_buffer)
    );


    /*
     * Sign package.
     *
     * catpkg_sign_catpackage() ignores PACKAGEINFO.
     */
    printf(
        "[INFO] Calculating package SHA-256...\n"
    );


    struct sha256_buff hash;


    if (catpkg_sign_catpackage(
            tar_path,
            &hash
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to calculate package SHA-256\n"
        );

        PackageInfo_Free(info);
        unlink(tar_path);

        return 1;
    }


    char hex[65];


    sha256_read_hex(
        &hash,
        hex
    );


    hex[64] = '\0';


    printf(
        "Package size: %s\n",
        size_buffer
    );


    printf(
        "sha256: %s\n",
        hex
    );


    /*
     * Generate temporary PACKAGEINFO.
     */
    char packageinfo_tmp[PATH_MAX];


    written = snprintf(
        packageinfo_tmp,
        sizeof(packageinfo_tmp),
        "%s.tmp",
        CATPKG_PACKAGEINFO
    );


    if (written < 0 ||
        (size_t)written >= sizeof(packageinfo_tmp)) {

        fprintf(
            stderr,
            "catpkg: temporary PACKAGEINFO path is too long\n"
        );

        PackageInfo_Free(info);
        unlink(tar_path);

        return 1;
    }


    printf(
        "[INFO] Generating %s...\n",
        packageinfo_tmp
    );


    FILE *file = fopen(
        packageinfo_tmp,
        "w"
    );


    if (file == NULL) {

        perror(
            "catpkg: failed to create temporary PACKAGEINFO"
        );

        PackageInfo_Free(info);
        unlink(tar_path);

        return 1;
    }


    /*
     * CATPKG format.
     */
    fprintf(
        file,
        "CATPKG 1.0\n"
    );


    /*
     * Section 1: name.
     */
    fprintf(
        file,
        "%s\n"
        "-\n",
        name_field->value
    );


    /*
     * Section 2: version.
     */
    fprintf(
        file,
        "%s\n"
        "-\n",
        version_field->value
    );

    /*
     * Section 3: architecture.
     */

    if(architecture_field == NULL) {
        uname(&system_info);
        fprintf(
            file,
            "%s\n"
            "-\n",
            a10e
        );
    }
    else {
        fprintf(
            file,
            "%s\n"
            "-\n",
            architecture_field->value
        );
    }


    /*
     * Section 4: description.
     */
    if (description_field == NULL ||
        description_field->value == NULL ||
        description_field->value[0] == '\0') {

        fprintf(
            file,
            "-\n"
        );

    } else {

        fprintf(
            file,
            "%s\n",
            description_field->value
        );

        fprintf(
            file,
            "-\n"
        );
    }


    /*
     * Section 5: SHA-256.
     */
    fprintf(
        file,
        "%s\n"
        "-\n",
        hex
    );


    /*
     * Section 6: package size.
     */
    fprintf(
        file,
        "%jd\n"
        "-\n",
        (intmax_t)size
    );


    /*
     * Section 7: dependencies.
     */
    struct PackageFieldMatches dependencies =
        PackageInfo_FindAll(
            info,
            "dependency"
        );


    for (size_t i = 0;
         i < dependencies.count;
         i++) {

        if (dependencies.items[i] == NULL ||
            dependencies.items[i]->value == NULL)
            continue;


        fprintf(
            file,
            "%s\n",
            dependencies.items[i]->value
        );
    }


    /*
     * End dependencies section.
     */
    fprintf(
        file,
        "-\n"
    );


    PackageFieldMatches_Free(
        &dependencies
    );


    /*
     * Section 8: filesystem operations.
     */
    if (operations_field != NULL &&
        operations_field->value != NULL &&
        operations_field->value[0] != '\0') {

        fputs(
            operations_field->value,
            file
        );
    }


    /*
     * Make sure everything was written.
     */
    if (fflush(file) != 0) {

        perror(
            "catpkg: failed to write PACKAGEINFO"
        );

        fclose(file);
        unlink(packageinfo_tmp);
        PackageInfo_Free(info);
        unlink(tar_path);

        return 1;
    }


    if (fclose(file) != 0) {

        perror(
            "catpkg: failed to close PACKAGEINFO"
        );

        unlink(packageinfo_tmp);
        PackageInfo_Free(info);
        unlink(tar_path);

        return 1;
    }


    printf(
        "[INFO] %s generated successfully.\n",
        packageinfo_tmp
    );


    /*
     * Replace PACKAGEINFO inside TAR.
     *
     * IMPORTANT:
     *
     * The archive has already been signed.
     * PACKAGEINFO is excluded from the signature,
     * therefore replacing it does not invalidate
     * the calculated SHA-256.
     */
    printf(
        "[INFO] Replacing PACKAGEINFO inside TAR...\n"
    );


    if (replace_packageinfo(
            tar_path,
            packageinfo_tmp
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to replace PACKAGEINFO "
            "inside TAR archive\n"
        );

        unlink(packageinfo_tmp);
        PackageInfo_Free(info);
        unlink(tar_path);

        return 1;
    }


    /*
     * PACKAGEINFO.tmp is no longer needed.
     */
    if (unlink(packageinfo_tmp) != 0) {

        perror(
            "catpkg: failed to remove temporary PACKAGEINFO"
        );

        /*
         * This is not fatal for the package itself,
         * because the archive has already been built.
         */
    }


    /*
     * Remove old CATPACKAGE.
     */
    if (unlink(catpackage_path) != 0 &&
        errno != ENOENT) {

        fprintf(
            stderr,
            "catpkg: failed to remove old \"%s\": ",
            catpackage_path
        );

        perror(NULL);

        PackageInfo_Free(info);
        unlink(tar_path);

        return 1;
    }


    /*
     * Rename TAR -> CATPACKAGE.
     */
    printf(
        "[INFO] Converting TAR archive to CATPACKAGE...\n"
    );


    if (rename(
            tar_path,
            catpackage_path
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to rename \"%s\" "
            "to \"%s\": ",
            tar_path,
            catpackage_path
        );

        perror(NULL);

        unlink(tar_path);
        PackageInfo_Free(info);

        return 1;
    }


    printf(
        "[INFO] CATPACKAGE created successfully.\n"
    );


    printf(
        "[INFO] Package: %s\n",
        catpackage_path
    );


    PackageInfo_Free(info);


    printf(
        "[INFO] Package build completed successfully.\n"
    );


    return 0;
}