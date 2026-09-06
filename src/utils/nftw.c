#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ftw.h>

#include "configuration.h"

#include "utils/nftw.h"

char **nftw_build_paths = NULL;
size_t nftw_build_paths_count = 0;

char **nftw_build_dirs = NULL;
size_t nftw_build_dirs_count = 0;

int catpkg_find_build_path(
    const char *path,
    const struct stat *sb,
    int typeflag,
    struct FTW *ftwbuf
) {
    if (typeflag == FTW_F || typeflag == FTW_SL) {
        char **tmp = realloc(
            nftw_build_paths,
            (nftw_build_paths_count + 1) * sizeof(char *)
        );

        if (tmp == NULL)
            return 1;

        nftw_build_paths = tmp;

		const char *relative = path + strlen(CATPKG_BUILD_PATH);

		size_t len = strlen(relative) + 1;

		nftw_build_paths[nftw_build_paths_count] = malloc(len);

		if (nftw_build_paths[nftw_build_paths_count] == NULL)
		    return 1;

		snprintf(
		    nftw_build_paths[nftw_build_paths_count],
		    len,
		    "%s",
		    relative
		);

        nftw_build_paths_count++;
    }

    if (typeflag == FTW_D && strcmp(path, CATPKG_BUILD_PATH) != 0) {
        char **tmp = realloc(
            nftw_build_dirs,
            (nftw_build_dirs_count + 1) * sizeof(char *)
        );

        if (tmp == NULL)
            return 1;

        nftw_build_dirs = tmp;

        const char *relative = path + strlen(CATPKG_BUILD_PATH);

        size_t len = strlen(relative) + 2;

        nftw_build_dirs[nftw_build_dirs_count] = malloc(len);

        if (nftw_build_dirs[nftw_build_dirs_count] == NULL)
            return 1;

        snprintf(
            nftw_build_dirs[nftw_build_dirs_count],
            len,
            "%s",
            relative
        );

        nftw_build_dirs_count++;
    }

    return 0;
}