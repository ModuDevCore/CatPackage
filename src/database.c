#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>

#include "configuration.h"
#include "catpkg/path.h"
#include "catpkg/pkginfo.h"
#include "database.h"

struct PackageInfo *catpkg_find_package_info(const char *package_fullname)
{
    char *package_path = make_catpkg_path(
        CATPKG_METADATA_PATH "/%s",
        package_fullname,
        CATPKG_PACKAGEINFO
    );

    if (package_path == NULL) {
        return NULL;
    }

    struct PackageInfo *info = CATPKG_Parse(package_path);
    free(package_path);
    return info;
}

struct PackageMatches catpkg_find_package(
    const char *package_name
)
{
    struct PackageMatches matches = {
        .items = NULL,
        .count = 0
    };

    DIR *dir = opendir(CATPKG_PACKAGE_ROOT);

    if (dir == NULL) {
        perror("catpkg: opendir");
        return matches;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (entry->d_type != DT_DIR)
            continue;

        char *package_info_path = make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            entry->d_name
        );

        if (package_info_path == NULL)
            continue;

        struct PackageInfo *info =
            CATPKG_Parse(package_info_path);

        free(package_info_path);

        if (info == NULL)
            continue;

        const struct PackageField *name_field =
            PackageInfo_Find(info, "name");

        if (name_field == NULL ||
            name_field->value == NULL) {
            PackageInfo_Free(info);
            continue;
        }

        /*
         * Check for an exact package name match first.
         */
        if (strcmp(name_field->value, package_name) == 0) {

            /*
             * Remove any previously collected partial matches.
             */
            for (size_t i = 0; i < matches.count; i++)
                free(matches.items[i]);

            free(matches.items);

            matches.items = malloc(sizeof(*matches.items));

            if (matches.items == NULL) {
                perror("catpkg: malloc");
                matches.count = 0;
                PackageInfo_Free(info);
                break;
            }

            matches.items[0] = strdup(name_field->value);

            if (matches.items[0] == NULL) {
                perror("catpkg: strdup");
                free(matches.items);
                matches.items = NULL;
                matches.count = 0;
                PackageInfo_Free(info);
                break;
            }

            matches.count = 1;

            PackageInfo_Free(info);

            /*
             * An exact match was found.
             * No further search is necessary.
             */
            break;
        }

        /*
         * No exact match was found yet.
         *
         * Check whether the package name starts
         * with the requested name.
         */
        size_t package_name_length = strlen(package_name);

        if (strncmp(
                name_field->value,
                package_name,
                package_name_length
            ) == 0) {

            char **tmp = realloc(
                matches.items,
                (matches.count + 1) *
                sizeof(*matches.items)
            );

            if (tmp == NULL) {
                perror("catpkg: realloc");
                PackageInfo_Free(info);
                break;
            }

            matches.items = tmp;

            matches.items[matches.count] =
                strdup(name_field->value);

            if (matches.items[matches.count] == NULL) {
                perror("catpkg: strdup");
                PackageInfo_Free(info);
                break;
            }

            matches.count++;
        }

        PackageInfo_Free(info);
    }

    closedir(dir);

    return matches;
}
struct PackageMatches catpkg_find_version(
    const char *package_name
)
{
    struct PackageMatches matches = {
        .items = NULL,
        .count = 0
    };

    DIR *dir = opendir(CATPKG_PACKAGE_ROOT);

    if (dir == NULL) {
        perror("catpkg: opendir");
        return matches;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (entry->d_type != DT_DIR)
            continue;


        char *package_info_path = make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            entry->d_name
        );

        if (package_info_path == NULL)
            continue;

        struct PackageInfo *info =
            CATPKG_Parse(package_info_path);

        free(package_info_path);

        if (info == NULL)
            continue;

        const struct PackageField *name_field =
            PackageInfo_Find(info, "name");

        const struct PackageField *version_field =
            PackageInfo_Find(info, "version");

        if (name_field == NULL ||
            name_field->value == NULL ||
            version_field == NULL ||
            version_field->value == NULL) {

            PackageInfo_Free(info);
            continue;
        }

        if (strcmp(name_field->value, package_name) != 0) {
            PackageInfo_Free(info);
            continue;
        }

        char **tmp = realloc(
            matches.items,
            (matches.count + 1) *
            sizeof(*matches.items)
        );

        if (tmp == NULL) {
            perror("catpkg: realloc");
            PackageInfo_Free(info);
            break;
        }

        matches.items = tmp;

        matches.items[matches.count] =
            strdup(version_field->value);

        if (matches.items[matches.count] == NULL) {
            perror("catpkg: strdup");
            PackageInfo_Free(info);
            break;
        }

        matches.count++;

        PackageInfo_Free(info);
    }

    closedir(dir);

    return matches;
}
void PackageMatches_Free(
    struct PackageMatches *matches
)
{
    if (matches == NULL)
        return;

    for (size_t i = 0; i < matches->count; i++)
        free(matches->items[i]);

    free(matches->items);

    matches->items = NULL;
    matches->count = 0;
}

struct PackageMatches catpkg_find_package_fullname(
    const char *package_fullname
)
{
    struct PackageMatches matches = {
        .items = NULL,
        .count = 0
    };

    DIR *dir = opendir(CATPKG_PACKAGE_ROOT);

    if (dir == NULL) {
        perror("catpkg: opendir");
        return matches;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (entry->d_type != DT_DIR)
            continue;

        char *package_info_path = make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            entry->d_name
        );

        if (package_info_path == NULL)
            continue;

        struct PackageInfo *info =
            CATPKG_Parse(package_info_path);

        free(package_info_path);

        if (info == NULL)
            continue;

        const struct PackageField *name_field =
            PackageInfo_Find(info, "name");

        const struct PackageField *version_field =
            PackageInfo_Find(info, "version");

        if (name_field == NULL ||
            name_field->value == NULL ||
            version_field == NULL ||
            version_field->value == NULL) {

            PackageInfo_Free(info);
            continue;
        }

        char *current_fullname = make_catpkg_path(
            "%s@%s",
            name_field->value,
            version_field->value
        );

        if (current_fullname == NULL) {
            PackageInfo_Free(info);
            continue;
        }

        if (strcmp(current_fullname, package_fullname) == 0) {

            char **tmp = realloc(
                matches.items,
                (matches.count + 1) *
                sizeof(*matches.items)
            );

            if (tmp == NULL) {
                perror("catpkg: realloc");
                free(current_fullname);
                PackageInfo_Free(info);
                break;
            }

            matches.items = tmp;

            matches.items[matches.count] =
                strdup(current_fullname);

            if (matches.items[matches.count] == NULL) {
                perror("catpkg: strdup");
                free(current_fullname);
                PackageInfo_Free(info);
                break;
            }

            matches.count++;
        }

        free(current_fullname);
        PackageInfo_Free(info);
    }

    closedir(dir);

    return matches;
}
struct PackageMatches catpkg_find_cached_package(
    const char *package_fullname
)
{
    struct PackageMatches matches = {
        .items = NULL,
        .count = 0
    };

    if (package_fullname == NULL ||
        package_fullname[0] == '\0')
        return matches;

    DIR *dir = opendir(CATPKG_CACHE_PACKAGES);

    if (dir == NULL) {
        perror("catpkg: opendir");
        return matches;
    }

    size_t search_length = strlen(package_fullname);

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        /*
         * Check for .catpackage.zst
         */
        const char *zst_suffix = ".catpackage.zst";
        size_t zst_suffix_length = strlen(zst_suffix);

        /*
         * Check for .catpackage
         */
        const char *catpackage_suffix = ".catpackage";
        size_t catpackage_suffix_length =
            strlen(catpackage_suffix);

        const char *matched_suffix = NULL;
        size_t matched_suffix_length = 0;

        size_t entry_length = strlen(entry->d_name);

        if (entry_length > zst_suffix_length &&
            strcmp(
                entry->d_name +
                entry_length -
                zst_suffix_length,
                zst_suffix
            ) == 0) {

            matched_suffix = zst_suffix;
            matched_suffix_length = zst_suffix_length;

        } else if (
            entry_length > catpackage_suffix_length &&
            strcmp(
                entry->d_name +
                entry_length -
                catpackage_suffix_length,
                catpackage_suffix
            ) == 0
        ) {

            matched_suffix = catpackage_suffix;
            matched_suffix_length =
                catpackage_suffix_length;
        }

        /*
         * Not a CatPackage cache file.
         */
        if (matched_suffix == NULL)
            continue;

        /*
         * Calculate the package full name without
         * the cache file extension.
         */
        size_t fullname_length =
            entry_length - matched_suffix_length;

        /*
         * Check whether the package name matches
         * the requested name.
         */
        if (fullname_length < search_length)
            continue;

        if (strncmp(
                entry->d_name,
                package_fullname,
                search_length
            ) != 0)
            continue;

        /*
         * Make sure the requested name is a complete
         * package fullname prefix.
         *
         * For example:
         *
         * foo@1.0  -> foo@1.0.0
         *
         * is not considered a match when an exact
         * version was requested.
         *
         * But:
         *
         * foo@ -> foo@1.0.0
         *
         * is valid.
         */
        if (search_length > 0 &&
            package_fullname[search_length - 1] != '@') {

            if (fullname_length != search_length)
                continue;
        }

        /*
         * Check whether this package fullname was
         * already added.
         *
         * This prevents:
         *
         * foo@1.0.0.catpackage
         * foo@1.0.0.catpackage.zst
         *
         * from producing two matches.
         */
        int already_found = 0;

        for (size_t i = 0; i < matches.count; i++) {

            if (strcmp(
                    matches.items[i],
                    entry->d_name
                ) == 0) {

                already_found = 1;
                break;
            }

            /*
             * Compare against the fullname without
             * the extension.
             */
            if (strlen(matches.items[i]) == fullname_length &&
                strncmp(
                    matches.items[i],
                    entry->d_name,
                    fullname_length
                ) == 0) {

                already_found = 1;
                break;
            }
        }

        if (already_found)
            continue;

        /*
         * Allocate space for the package fullname.
         */
        char *fullname = malloc(
            fullname_length + 1
        );

        if (fullname == NULL) {
            perror("catpkg: malloc");
            break;
        }

        memcpy(
            fullname,
            entry->d_name,
            fullname_length
        );

        fullname[fullname_length] = '\0';

        /*
         * Add the package fullname to the results.
         */
        char **tmp = realloc(
            matches.items,
            (matches.count + 1) *
            sizeof(*matches.items)
        );

        if (tmp == NULL) {
            perror("catpkg: realloc");
            free(fullname);
            break;
        }

        matches.items = tmp;

        matches.items[matches.count] = fullname;
        matches.count++;
    }

    closedir(dir);

    return matches;
}