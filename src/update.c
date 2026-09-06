#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "configuration.h"
#include "install.h"
#include "verify.h"
#include "remove.h"
#include "database.h"
#include "utils/allow.h"
#include "utils/sha256.h"
#include "utils/fs.h"
#include "catpkg/path.h"
#include "catpkg/pkginfo.h"

int catpkg_update(
    const char *path
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
     * Parse PACKAGEINFO.
     */

    struct PackageInfo *info =
        CATPKG_Parse(path);

    if (info == NULL) {

        fprintf(
            stderr,
            "catpkg: failed to parse PACKAGEINFO\n"
        );

        return 1;
    }

    /*
     * Get package name and version.
     */

    const struct PackageField *name =
        PackageInfo_Find(
            info,
            "name"
        );

    const struct PackageField *version =
        PackageInfo_Find(
            info,
            "version"
        );

    const struct PackageField *sha256_catpackage =
        PackageInfo_Find(
            info,
            "SHA"
        );

    if (name == NULL || name->value == NULL) {

        fprintf(
            stderr,
            "catpkg: package name not found in PACKAGEINFO\n"
        );

        PackageInfo_Free(info);

        return 1;
    }

    if (version == NULL || version->value == NULL) {

        fprintf(
            stderr,
            "catpkg: package version not found in PACKAGEINFO\n"
        );

        PackageInfo_Free(info);

        return 1;
    }

    /*
     * Build full package name:
     *
     * packageName@version
     */

    char *package_fullname =
        make_catpkg_path(
            "%s@%s",
            name->value,
            version->value
        );

    if (package_fullname == NULL) {

        fprintf(
            stderr,
            "catpkg: failed to allocate package name\n"
        );

        PackageInfo_Free(info);

        return 1;
    }

    struct stat st;

    if (stat(path, &st) != 0) {
        perror("stat");
        return 1;
    }

    uint64_t will_be_installed = 0;
    char will_be_installed_str[64] = "-";
    char package_size_str[64];
    char changing_str[64] = "-";
    char sign_str[3] = "";

    catpkg_format_size(
        st.st_size,
        package_size_str,
        sizeof(package_size_str)
    );

    if (
        sha256_catpackage == NULL ||
        sha256_catpackage->value == NULL ||
        catpkg_integrity_catpackage(path, sha256_catpackage->value) == 0
    ) {
        printf(
            "Warning: The package was modified after assembly; "
            "it is not possible to calculate the actual size. "
            "It is recommended to verify the package by reassembling it.\n"
        );
    }
    else {
        struct PackageInfo *info_installed_package =
            catpkg_find_package_info(package_fullname);

        if (info_installed_package == NULL) {
            printf(
                "Warning: Unable to find information about the installed package; "
                "it is not possible to calculate the size change.\n"
            );
        }
        else {
            /*
             * Size of the currently installed package.
             * This is needed because the old package occupies space already.
             */
            uint64_t installed_package_size = 0;

            /*
             * Size of the new package.
             */
            uint64_t new_package_size = 0;

            /*
             * Space that will be released by removing the old package.
             */
            uint64_t will_be_released = 0;

            catpkg_calc_installed(
                &installed_package_size,
                info_installed_package
            );

            catpkg_calc_installed(
                &new_package_size,
                info
            );

            catpkg_calc_released(
                &will_be_released,
                package_fullname,
                true
            );

            /*
             * The old package already occupies space.
             *
             * We only need to show how much additional space
             * the new package requires after the old package is removed.
             *
             * Example:
             *
             *     old package:  100 MiB
             *     new package:  120 MiB
             *     released:     100 MiB
             *
             *     additional:   20 MiB
             */
            int64_t difference =
                (int64_t)new_package_size -
                (int64_t)will_be_released;

            sign_str[0] = '~';
            sign_str[1] = difference >= 0 ? '+' : '-';
            sign_str[2] = '\0';

            uint64_t absolute_difference =
                difference >= 0
                    ? (uint64_t)difference
                    : (uint64_t)(-difference);

            catpkg_format_size(
                absolute_difference,
                changing_str,
                sizeof(changing_str)
            );

            /*
             * will_be_installed should represent the amount of
             * additional space required, not the total package size.
             */
            will_be_installed =
                difference > 0
                    ? (uint64_t)difference
                    : 0;

            catpkg_format_size(
                will_be_installed,
                will_be_installed_str,
                sizeof(will_be_installed_str)
            );

            PackageInfo_Free(info_installed_package);
        }
    }


    /*
     * Ask for confirmation.
     */

    printf(
        ALLOW_UPDATE,
        package_fullname,
        package_size_str,
        will_be_installed_str,
        sign_str,
        changing_str
    );

    printf("! Update it? [Y/n] ");

    if (catpkg_confirm() == 0) {

        free(package_fullname);
        PackageInfo_Free(info);

        return 1;
    }

    /*
     * Remove currently installed package.
     */

    if (catpkg_remove(name->value, true) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to remove old package\n"
        );

        free(package_fullname);
        PackageInfo_Free(info);

        return 1;
    }

    /*
     * Install new package.
     */

    if (catpkg_install(path, true) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to install updated package\n"
        );

        free(package_fullname);
        PackageInfo_Free(info);

        return 1;
    }

    /*
     * Cleanup.
     */

    free(package_fullname);

    PackageInfo_Free(info);

    return 0;
}