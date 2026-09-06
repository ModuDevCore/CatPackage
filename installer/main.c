#include "update.h"
#include "install.h"
#include "remove.h"
#include "database.h"

#include "catpkg/pkginfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>

extern unsigned char catpkg_embedded_package[];
extern unsigned int catpkg_embedded_package_size;

#define INSTALLER_TMP_DIR \
    "/tmp/catpkg-installer"

#define INSTALLER_PACKAGE_PATH \
    INSTALLER_TMP_DIR "/catpkg-1.0.0-beta.catpackage"

static int mkdir_p(const char *path, mode_t mode)
{
    char buffer[4096];

    if (path == NULL)
        return -1;

    size_t length = strlen(path);

    if (length == 0 || length >= sizeof(buffer))
        return -1;

    strcpy(buffer, path);

    for (char *p = buffer + 1; *p != '\0'; p++) {

        if (*p != '/')
            continue;

        *p = '\0';

        if (mkdir(buffer, mode) != 0 && errno != EEXIST) {
            *p = '/';
            return -1;
        }

        *p = '/';
    }

    if (mkdir(buffer, mode) != 0 && errno != EEXIST)
        return -1;

    return 0;
}

static int create_tmp_directory(void)
{
    printf(
        "[INFO] Creating temporary directory: %s\n",
        INSTALLER_TMP_DIR
    );

    if (mkdir_p(INSTALLER_TMP_DIR, 0755) != 0) {

        if (errno == EEXIST) {
            printf(
                "[INFO] Temporary directory already exists.\n"
            );

            return 0;
        }

        fprintf(
            stderr,
            "[ERROR] Failed to create temporary directory '%s': %s\n",
            INSTALLER_TMP_DIR,
            strerror(errno)
        );

        return 1;
    }

    printf(
        "[INFO] Temporary directory created successfully.\n"
    );

    return 0;
}

static int extract_package(void)
{
    printf(
        "[INFO] Extracting embedded catpackage...\n"
    );

    printf(
        "[INFO] Destination: %s\n",
        INSTALLER_PACKAGE_PATH
    );

    printf(
        "[INFO] Package size: %u bytes\n",
        catpkg_embedded_package_size
    );

    FILE *file = fopen(
        INSTALLER_PACKAGE_PATH,
        "wb"
    );

    if (file == NULL) {
        fprintf(
            stderr,
            "[ERROR] Failed to open '%s': %s\n",
            INSTALLER_PACKAGE_PATH,
            strerror(errno)
        );

        return 1;
    }

    size_t written = fwrite(
        catpkg_embedded_package,
        1,
        catpkg_embedded_package_size,
        file
    );

    if (written != catpkg_embedded_package_size) {

        fprintf(
            stderr,
            "[ERROR] Failed to write catpackage: "
            "wrote %zu of %u bytes: %s\n",
            written,
            catpkg_embedded_package_size,
            strerror(errno)
        );

        fclose(file);
        remove(INSTALLER_PACKAGE_PATH);

        return 1;
    }

    if (fclose(file) != 0) {

        fprintf(
            stderr,
            "[ERROR] Failed to close catpackage file: %s\n",
            strerror(errno)
        );

        remove(INSTALLER_PACKAGE_PATH);

        return 1;
    }

    printf(
        "[INFO] Catpackage extracted successfully.\n"
    );

    return 0;
}

static int cleanup(void)
{
    printf(
        "[INFO] Removing temporary catpackage...\n"
    );

    if (remove(INSTALLER_PACKAGE_PATH) != 0) {

        if (errno == ENOENT) {

            printf(
                "[INFO] Temporary catpackage does not exist.\n"
            );

            return 0;
        }

        fprintf(
            stderr,
            "[ERROR] Failed to remove '%s': %s\n",
            INSTALLER_PACKAGE_PATH,
            strerror(errno)
        );

        return 1;
    }

    printf(
        "[INFO] Temporary catpackage removed successfully.\n"
    );

    return 0;
}

int main(void)
{
    printf(
        "========================================\n"
    );

    printf(
        "        CATPKG BOOTSTRAP INSTALLER\n"
    );

    printf(
        "========================================\n"
    );

    printf(
        "[INFO] Starting installation...\n"
    );

    /*
     * 1. Create temporary directory.
     */
    if (create_tmp_directory() != 0) {

        fprintf(
            stderr,
            "[FATAL] Failed to prepare installer.\n"
        );

        return 1;
    }

    /*
     * 2. Extract embedded catpackage.
     */
    if (extract_package() != 0) {

        fprintf(
            stderr,
            "[FATAL] Failed to extract catpackage.\n"
        );

        return 1;
    }

    struct PackageInfo *info = CATPKG_Parse(INSTALLER_PACKAGE_PATH);
    if (info == NULL) {
        fprintf(
            stderr,
            "catpkg: failed to parse PACKAGEINFO\n"
        );
        return 1;
    }
    const struct PackageField *package_name_field = PackageInfo_Find(info, "name");
    const struct PackageField *package_version_field = PackageInfo_Find(info, "version");
    struct PackageMatches packages_matches = catpkg_find_package(package_name_field -> value);

    if(packages_matches.count > 0) {
        printf("WARNING: An existing version of catpkg has been found.\nThe existing version of catpkg will be updated.\n");
        struct PackageMatches packages_v_matches = catpkg_find_version(package_name_field -> value);
        if(strcmp(packages_v_matches.items[0], package_version_field -> value) == 0) {
            printf("WARNING: The version of the capkg package you are about to install matches the current one: “%s@%s” (The current version will be reinstalled).\n", package_name_field -> value, package_version_field -> value);
        }
    }

    if(packages_matches.count > 0) {
        /*
         * Updating the existing package.  
        */      
        printf(
            "[INFO] Updating catpkg...\n"
        );

        int result = catpkg_update(
            INSTALLER_PACKAGE_PATH
        );

        if (result != 0) {

            fprintf(
                stderr,
                "[ERROR] catpkg updating failed "
                "with exit code %d.\n",
                result
            );

            cleanup();

            return result;
        }

        printf(
            "[INFO] catpkg updating successfully.\n"
        );

        /*
         * Remove temporary package.
         */
        if (cleanup() != 0) {

            fprintf(
                stderr,
                "[WARNING] Updating succeeded, "
                "but cleanup failed.\n"
            );

            return 1;
        }

        printf(
            "========================================\n"
        );

        printf(
            "       UPDATE COMPLETED\n"
        );

        printf(
            "========================================\n"
        );
        return 0;
    }

    printf(
        "[INFO] Installing catpkg...\n"
    );

    int result = catpkg_install(
        INSTALLER_PACKAGE_PATH,
        false
    );

    if (result != 0) {

        fprintf(
            stderr,
            "[ERROR] catpkg installation failed "
            "with exit code %d.\n",
            result
        );

        cleanup();

        return result;
    }

    printf(
        "[INFO] catpkg installed successfully.\n"
    );

    /*
     * Remove temporary package.
     */
    if (cleanup() != 0) {

        fprintf(
            stderr,
            "[WARNING] Installation succeeded, "
            "but cleanup failed.\n"
        );

        return 1;
    }

    printf(
        "========================================\n"
    );

    printf(
        "       INSTALLATION COMPLETED\n"
    );

    printf(
        "========================================\n"
    );

    return 0;
}