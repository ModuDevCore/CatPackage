#ifndef CATPKG_UTILS_NFTW_H
#define CATPKG_UTILS_NFTW_H
#include <ftw.h>

extern char **nftw_build_paths;
extern size_t nftw_build_paths_count;

extern char **nftw_build_dirs;
extern size_t nftw_build_dirs_count;

int catpkg_find_build_path(
    const char *path,
    const struct stat *sb,
    int typeflag,
    struct FTW *ftwbuf
);

#endif