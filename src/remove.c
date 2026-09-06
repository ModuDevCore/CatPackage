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

int catpkg_calc_released(
    uint64_t *will_be_released,
    const char *package_fullname,
    bool excludeCache
) {
    struct stat st;
    
    /*
     * Calculate the cache size.
    */
    if(!excludeCache) {    
        char *catpkg_cache_path = make_catpkg_path(
            CATPKG_CACHE_PACKAGES "/" CATPKG_CACHE_CATPACKAGE,
            package_fullname
        );
        char *catpkg_cache_path_zstd = make_catpkg_path(
            CATPKG_CACHE_PACKAGES "/" CATPKG_CACHE_CATPACKAGE_ZSTD,
            package_fullname
        );
        if (stat(catpkg_cache_path, &st) == 0)
            *will_be_released += (uint64_t)st.st_size;
        if(stat(catpkg_cache_path_zstd, &st) == 0)
            *will_be_released += (uint64_t)st.st_size;

        free(catpkg_cache_path);
        free(catpkg_cache_path_zstd);
    }


    /*
     * Calculating the size of files stored in files.
    */
    char *catpkg_files_path = make_catpkg_path(
        CATPKG_FILES_PATH,
        package_fullname
    );

    if (catpkg_files_path == NULL) {
        perror(
            "catpkg: make_catpkg_path"
        );

        return 1;
    }

    for (size_t i = 0;; i++) {

        char file_path[PATH_MAX];

        int written = snprintf(
            file_path,
            sizeof(file_path),
            "%s/%zu",
            catpkg_files_path,
            i
        );


        if(stat(file_path, &st) == 0)
            *will_be_released += (uint64_t)st.st_size;
        else
            break;

        struct PackageInfo *file_info = CATPKG_Parse(file_path);

        if (file_info == NULL) {
            fprintf(
                stderr,
                "catpkg: failed to parse file metadata \"%s\"\n",
                file_path
            );

            free(catpkg_files_path);
            return 1;
        }

        const struct PackageField *path_field =
            PackageInfo_Find(file_info, "path");

        if (path_field == NULL ||
            path_field->value == NULL) {

            fprintf(
                stderr,
                "catpkg: file metadata \"%s\" has no path field\n",
                file_path
            );

            PackageInfo_Free(file_info);
            free(catpkg_files_path);
            return 1;
        }

        if (stat(path_field->value, &st) == 0)
            *will_be_released += (uint64_t)st.st_size;

        PackageInfo_Free(file_info);
    }
    free(catpkg_files_path);
    return 0;
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


    /*
     * Build full package name:
     *
     * packageName@version
     */
    char *package_fullname = make_catpkg_path(
        "%s@%s",
        package_name,
        selected_version
    );

    if (package_fullname == NULL) {

        perror(
            "catpkg: make_catpkg_path"
        );

        free(selected_version);

        PackageMatches_Free(&versions);
        PackageMatches_Free(&matches);

        return 1;
    }

    uint64_t will_be_released = 0;
    char will_be_released_str[64] = "-";

    catpkg_calc_released(&will_be_released, package_fullname, false);
    catpkg_format_size(
        will_be_released,
        will_be_released_str,
        sizeof(will_be_released_str)
    );
    printf(
        ALLOW_REMOVE,
        package_fullname,
        will_be_released_str
    );
    if(!assume_yes){
        printf("! Remove it? ");
        if (catpkg_confirm() == 0) {
            return 1;
        }
    }

    printf(
        "Start of package removal \"%s\"...\n",
        package_fullname
    );


    /*
     * Build the complete package directory:
     *
     * CATPKG_PACKAGE_PATH/<package>@<version>
     *
     * This directory contains all package-specific
     * data, including files and metadata.
     */
    char *package_path = make_catpkg_path(
        CATPKG_PACKAGE_PATH,
        package_fullname
    );

    if (package_path == NULL) {

        perror(
            "catpkg: make_catpkg_path"
        );

        free(package_fullname);
        free(selected_version);

        PackageMatches_Free(&versions);
        PackageMatches_Free(&matches);

        return 1;
    }


    /*
     * Build path containing file metadata.
     *
     * We only read this information here.
     * The directory itself is removed later together
     * with the complete package directory.
     */

    char *catpkg_files_path = make_catpkg_path(
        CATPKG_FILES_PATH,
        package_fullname
    );

    struct PackageInfo **files_info = NULL;
    size_t files_info_count = 0;


    /*
     * Read information about every installed file.
     *
     * Files are expected to be numbered:
     *
     * 0
     * 1
     * 2
     * ...
     */
    for (size_t i = 0;; i++) {

        char file_path[PATH_MAX];


        int written = snprintf(
            file_path,
            sizeof(file_path),
            "%s/%zu",
            catpkg_files_path,
            i
        );


        if (written < 0 ||
            (size_t)written >= sizeof(file_path)) {

            fprintf(
                stderr,
                "catpkg: file path is too long\n"
            );


            for (size_t j = 0;
                 j < files_info_count;
                 j++) {

                PackageInfo_Free(
                    files_info[j]
                );
            }


            free(files_info);
            free(catpkg_files_path);
            free(package_path);
            free(package_fullname);
            free(selected_version);

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }


        FILE *file = fopen(
            file_path,
            "r"
        );


        if (file == NULL) {

            if (errno == ENOENT)
                break;


            perror(
                "catpkg: fopen"
            );


            for (size_t j = 0;
                 j < files_info_count;
                 j++) {

                PackageInfo_Free(
                    files_info[j]
                );
            }


            free(files_info);
            free(catpkg_files_path);
            free(package_path);
            free(package_fullname);
            free(selected_version);

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }


        fclose(file);


        /*
         * Add PackageInfo pointer to the array.
         */
        struct PackageInfo **tmp = realloc(
            files_info,
            (files_info_count + 1) *
            sizeof(*files_info)
        );


        if (tmp == NULL) {

            perror(
                "catpkg: realloc"
            );


            for (size_t j = 0;
                 j < files_info_count;
                 j++) {

                PackageInfo_Free(
                    files_info[j]
                );
            }


            free(files_info);
            free(catpkg_files_path);
            free(package_path);
            free(package_fullname);
            free(selected_version);

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }


        files_info = tmp;


        files_info[files_info_count] =
            CATPKG_Parse(file_path);


        if (files_info[files_info_count] == NULL) {

            fprintf(
                stderr,
                "catpkg: failed to parse \"%s\"\n",
                file_path
            );


            for (size_t j = 0;
                 j < files_info_count;
                 j++) {

                PackageInfo_Free(
                    files_info[j]
                );
            }


            free(files_info);
            free(catpkg_files_path);
            free(package_path);
            free(package_fullname);
            free(selected_version);

            PackageMatches_Free(&versions);
            PackageMatches_Free(&matches);

            return 1;
        }


        files_info_count++;
    }


    /*
     * Remove package files from the actual filesystem.
     */
    size_t missing = 0;
    size_t failed = 0;


    for (size_t i = 0;
         i < files_info_count;
         i++) {

        const struct PackageField *path_field =
            PackageInfo_Find(
                files_info[i],
                "path"
            );


        if (path_field == NULL ||
            path_field->value == NULL) {

            fprintf(
                stderr,
                "catpkg: PACKAGEINFO has no "
                "\"path\" field\n"
            );


            PackageInfo_Free(
                files_info[i]
            );

            failed++;

            continue;
        }


        /*
         * Check whether the installed file exists.
         */
        if (access(
                path_field->value,
                F_OK
            ) != 0) {

            if (errno == ENOENT) {

                printf(
                    "Warning: \"%s\" file not found.\n",
                    path_field->value
                );

                missing++;
            }
            else {

                fprintf(
                    stderr,
                    "catpkg: cannot access \"%s\": ",
                    path_field->value
                );

                perror(NULL);

                failed++;
            }


            PackageInfo_Free(
                files_info[i]
            );

            continue;
        }


        /*
         * Remove installed file.
         */
        if (remove(
                path_field->value
            ) != 0) {

            fprintf(
                stderr,
                "catpkg: failed to remove \"%s\": ",
                path_field->value
            );

            perror(NULL);

            failed++;

            PackageInfo_Free(
                files_info[i]
            );

            continue;
        }


        printf(
            "- File: \"%s\" successfully deleted\n",
            path_field->value
        );


        PackageInfo_Free(
            files_info[i]
        );
    }


    /*
     * Clear package cache.
     *
     * Missing cache is not considered
     * an error during package removal.
     */
    printf(
        "Clearing the cache...\n"
    );


    if (catpkg_clear_package_cache(
            package_name,
            selected_version
        ) != 0) {

        printf(
            "Package cache was not found "
            "or was already cleared.\n"
        );
    }


    /*
     * Remove the complete package directory.
     *
     * This recursively removes everything belonging
     * to the package, including:
     *
     *     metadata/
     *     files/
     *     and any other package-specific data.
     *
     * No separate removal of metadata/files is needed.
     */
    printf(
        "Clearing package directory...\n"
    );


    if (catpkg_remove_recursive(
            package_path
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to remove package directory "
            "\"%s\"\n",
            package_path
        );

        failed++;
    }
    else {

        printf(
            "Package directory removed successfully.\n"
        );
    }


    /*
     * Print result before freeing package_fullname.
     */
    if (failed > 0) {

        printf(
            "Package \"%s\" was removed with %zu issue(s).\n",
            package_fullname,
            failed
        );
    }
    else {

        printf(
            "Package \"%s\" successfully removed.\n",
            package_fullname
        );
    }


    if (missing > 0) {

        printf(
            "Missing files: %zu\n",
            missing
        );
    }


    /*
     * Free resources.
     */
    free(files_info);
    free(catpkg_files_path);
    free(package_path);
    free(package_fullname);
    free(selected_version);

    PackageMatches_Free(&versions);
    PackageMatches_Free(&matches);


    if (failed > 0)
        return 1;


    return 0;
}