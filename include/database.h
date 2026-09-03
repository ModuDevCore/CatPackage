#ifndef CATPKG_PACKAGEMANAGER_DATABASE_H
#define CATPKG_PACKAGEMANAGER_DATABASE_H

#include <stddef.h>

struct PackageMatches {
    char **items;
    size_t count;
};

struct PackageInfo *catpkg_find_package_info(
    const char *package_fullname
);

struct PackageMatches catpkg_find_package(
    const char *package_name
);
struct PackageMatches catpkg_find_package_fullname(
    const char *package_fullname
);

void PackageMatches_Free(
    struct PackageMatches *matches
);

struct PackageMatches catpkg_find_version(
    const char *package_name
);

struct PackageMatches catpkg_find_cached_package(
    const char *package_fullname
);

#endif