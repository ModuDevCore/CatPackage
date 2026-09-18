#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <ftw.h>
#include <stdint.h> 

#include "database.h"
#include "catpkg/builder.h"
#include "utils/fs.h"
#include "utils/allow.h"
#include "configuration.h"
#include "catpkg/path.h"
#include "catpkg/pkginfo.h"
#include "catpkg/cache.h"


/*
 * Recursively remove a directory and everything inside it.
 *
 * Every successfully removed object is printed.
 */
static int catpkg_remove_recursive_callback(
    const char *path,
    const struct stat *statbuf,
    int type,
    struct FTW *ftwbuf
)
{
    (void)statbuf;
    (void)ftwbuf;


    /*
     * Remove regular files and symbolic links.
     */
    if (type == FTW_F ||
        type == FTW_SL ||
        type == FTW_SLN) {

        if (remove(path) != 0) {

            fprintf(
                stderr,
                "catpkg: failed to remove \"%s\": ",
                path
            );

            perror(NULL);

            return 1;
        }

        printf(
            "- File: \"%s\" successfully deleted\n",
            path
        );

        return 0;
    }


    /*
     * Remove directories after their contents.
     *
     * FTW_DEPTH guarantees that directories are visited
     * after everything inside them has been processed.
     */
    if (type == FTW_D ||
    type == FTW_DP) {

        if (rmdir(path) != 0) {

            fprintf(
                stderr,
                "catpkg: failed to remove directory \"%s\": ",
                path
            );

            perror(NULL);

            return 1;
        }

        printf(
            "- Directory: \"%s\" successfully deleted\n",
            path
        );

        return 0;
    }


    return 0;
}

/*
 * Recursively remove a directory tree.
 */
static int catpkg_remove_recursive(
    const char *path
)
{
    if (path == NULL)
        return 1;


    /*
     * Nothing to remove.
     */
    if (access(path, F_OK) != 0) {

        if (errno == ENOENT)
            return 0;

        fprintf(
            stderr,
            "catpkg: cannot access \"%s\": ",
            path
        );

        perror(NULL);

        return 1;
    }


    return nftw(
        path,
        catpkg_remove_recursive_callback,
        64,
        FTW_DEPTH | FTW_PHYS
    );
}

int catpkg_remove(
    const char *package_name,
    bool assume_yes
)
{
    /*
     * Check whether the package exists.
     */
    struct PackageMatches matches =
        catpkg_find_package(package_name);

    if (matches.count == 0) {

        printf(
            "None of the packages match the name.\n"
        );

        PackageMatches_Free(&matches);

        return 1;
    }


    /*
     * Find all installed versions of the package.
     */
    struct PackageMatches versions =
        catpkg_find_version(package_name);

    if (versions.count == 0) {

        printf(
            "No installed versions of the package were found.\n"
        );

        PackageMatches_Free(&versions);
        PackageMatches_Free(&matches);

        return 1;
    }


    char *selected_version = NULL;

    /*
     * If only one version is installed,
     * select it automatically.
     */
    if (versions.count == 1) {

        selected_version = strdup(
            versions.items[0]
        );

        if (selected_version == NULL) {

            perror(
                "catpkg: strdup"
            );

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }
    }
    else {

        /*
         * Several versions are installed.
         * Ask the user which version should be removed.
         */
        printf(
            "Several versions of \"%s\" have been found.\n",
            package_name
        );

        printf(
            "Available versions:\n"
        );

        for (size_t i = 0;
             i < versions.count;
             i++) {

            printf(
                "  %s\n",
                versions.items[i]
            );
        }


        printf(
            "Enter the version to remove: "
        );

        fflush(stdout);


        char input[256];

        if (fgets(
                input,
                sizeof(input),
                stdin
            ) == NULL) {

            fprintf(
                stderr,
                "catpkg: failed to read version\n"
            );

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }


        /*
         * Remove newline.
         */
        input[strcspn(
            input,
            "\n"
        )] = '\0';


        /*
         * Remove carriage return.
         */
        input[strcspn(
            input,
            "\r"
        )] = '\0';


        if (input[0] == '\0') {

            fprintf(
                stderr,
                "catpkg: version cannot be empty\n"
            );

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }


        /*
         * Check whether the entered version exists.
         */
        for (size_t i = 0;
             i < versions.count;
             i++) {

            if (strcmp(
                    input,
                    versions.items[i]
                ) != 0)
                continue;


            selected_version = strdup(
                versions.items[i]
            );

            if (selected_version == NULL) {

                perror(
                    "catpkg: strdup"
                );

                PackageMatches_Free(&versions);
                PackageMatches_Free(&matches);

                return 1;
            }

            break;
        }


        if (selected_version == NULL) {

            fprintf(
                stderr,
                "catpkg: version \"%s\" was not found\n",
                input
            );

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }
    }

    char *package_fullname = make_catpkg_path(
        "%s@%s",
        package_name,
        selected_version
    );

    // Builder

    char *catpkg_packageinfo_path = make_catpkg_path(
        CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
        package_fullname
    );

    struct CatpkgBuilder builder;

    if (catpkg_builder_init(&builder) != 0) {
        free(package_fullname);
        return 1;
    }

    struct PackageInfo *info = CATPKG_Parse(catpkg_packageinfo_path);
    struct PackageInfo *database_fileinfo = CATPKG_Parse(CATPKG_DATABASE_DIR_PATH "/" CATPKG_DATABASE);

    struct OperationSecureContext opsc_context = {
        info,
        database_fileinfo
    };

    catpkg_builder_request(
        &builder,
        BUILD_LOG_START,
        NULL
    );

    #include "remove/BUILDER_REMOVE_PROTOCOLS.inc"

    catpkg_builder_request(
        &builder,
        BUILD_LOG_END,
        NULL
    );

    char will_be_installed[64] = "-";

    size_t absolute =
        (size_t)(builder.change_size < 0 ? -builder.change_size : builder.change_size);

    catpkg_format_size(
        absolute,
        will_be_installed,
        sizeof(will_be_installed)
    );


    printf(ALLOW_REMOVE, 
        package_fullname, 
        will_be_installed
    );

    if (!catpkg_confirm()) {
        goto cancel;
    }

    if (catpkg_builder_apply(&builder) != 0) {

        /*
         * Builder only reverts requests which were successfully
         * applied, according to applied_count.
         */

        printf("ERROR Recovery: Reverting the changes...\n");
        catpkg_builder_revert(&builder);
        
        goto error;
    }

    printf("The package “%s” has been removed!\n", package_fullname);

    PackageInfo_Free(database_fileinfo);
    PackageInfo_Free(info);
    free(package_fullname);

    catpkg_builder_free(&builder);

    printf("Continued.\n");
    return 0;

    prepare_error:

        /*
         * Preparation itself did not modify the filesystem.
         * Just destroy the builder and owned temporary values.
         */

    cancel:

        PackageInfo_Free(database_fileinfo);
        PackageInfo_Free(info);
        free(package_fullname);

        catpkg_builder_free(&builder);

        return 1;

    error:
        printf("Failed to delete the package, try deleting the package manually or resolve the error.\n We recommend: Try installing the package again and then deleting it.\n", package_fullname);

        PackageInfo_Free(database_fileinfo);
        PackageInfo_Free(info);
        free(package_fullname);

        catpkg_builder_free(&builder);

        printf("Continued.\n");
        return 1;
}