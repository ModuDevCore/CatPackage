#ifndef CATPKG_UTILS_PKGINFO_H
#define CATPKG_UTILS_PKGINFO_H

#include <stddef.h>

struct PackageField {
    char *name;
    char *value;
};

struct PackageInfo {
    struct PackageField *fields;
    size_t fields_count;
};

struct PackageInfo *CATPKG_Parse(
    const char *filePath
);

const struct PackageField *PackageInfo_Find(
    const struct PackageInfo *info,
    const char *name
);

void PackageInfo_Free(
    struct PackageInfo *info
);

struct PackageFieldMatches {
    const struct PackageField **items;
    size_t count;
};

struct PackageFieldMatches PackageInfo_FindAll(
    const struct PackageInfo *info,
    const char *name
);

void PackageFieldMatches_Free(
    struct PackageFieldMatches *matches
);

int is_catpackage(
    const char *path
);

#endif