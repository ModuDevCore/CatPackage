#ifndef CATPKG_UTILS_BUILDER_H
#define CATPKG_UTILS_BUILDER_H
#include <stddef.h>
#include <sys/types.h>
#include <catpkg/pkginfo.h>
#include "./builder/protocols.h"
#include "catpkg/ctpg.h"

enum CatpkgErrorCode {
    CATPKG_OK = 0,
    CATPKG_DEPENDENCY_ERROR = 1,
    CATPKG_INVALID_CONTEXT = 2,
    CATPKG_NOT_FOUND = 3
};

struct CatpkgError {
    enum CatpkgErrorCode code;
    const char *path;
    const char *required_by;
};

struct OperationSecureContext {
    struct PackageInfo *package_info;
    struct PackageInfo *file_info;
};

struct MkdirSoftContext {
    char **created_paths;
    size_t created_paths_size;
};

struct CatpkgRequest {
    enum CatpkgProtocol protocol;
    void *value;
    void *result;
    void *context;

    uid_t previous_euid;
};

struct CatpkgBuilder {
    struct CatpkgRequest **requests;
    size_t requests_size;
    size_t applied_count;
    off_t change_size;
};

int catpkg_builder_init(
    struct CatpkgBuilder *builder
);

int catpkg_builder_request(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value
);

int catpkg_builder_request_value(
    struct CatpkgBuilder *builder,
    enum CatpkgProtocol protocol,
    void *value,
    void *result
);

int catpkg_builder_apply(
    struct CatpkgBuilder *builder
);

int catpkg_builder_revert(
    struct CatpkgBuilder *builder
);

int catpkg_builder_free(
    struct CatpkgBuilder *builder
);
#endif