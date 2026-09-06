#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <ftw.h>
#include <limits.h>
#include <sys/types.h>
#include <stdint.h>  

static uint64_t total_size = 0;

static int calculate_size(
    const char *path,
    const struct stat *st,
    int typeflag,
    struct FTW *ftwbuf
)
{
    (void)path;
    (void)typeflag;
    (void)ftwbuf;

    total_size += (uint64_t)st->st_size;

    return 0;
}

uint64_t get_directory_size(const char *path)
{
    total_size = 0;

    if (nftw(path, calculate_size, 16, FTW_PHYS) != 0)
        return 0;

    return total_size;
}

static int catpkg_copy_file(
    const char *source,
    const char *destination,
    mode_t mode
)
{
    int source_fd = open(
        source,
        O_RDONLY
    );

    if (source_fd < 0) {

        fprintf(
            stderr,
            "catpkg: open \"%s\": ",
            source
        );

        perror(NULL);

        return 1;
    }


    int destination_fd = open(
        destination,
        O_WRONLY |
        O_CREAT |
        O_TRUNC,
        mode & 07777
    );

    if (destination_fd < 0) {

        fprintf(
            stderr,
            "catpkg: open \"%s\": ",
            destination
        );

        perror(NULL);

        close(source_fd);

        return 1;
    }


    char buffer[65536];


    for (;;) {

        ssize_t bytes_read = read(
            source_fd,
            buffer,
            sizeof(buffer)
        );


        if (bytes_read == 0)
            break;


        if (bytes_read < 0) {

            fprintf(
                stderr,
                "catpkg: read \"%s\": ",
                source
            );

            perror(NULL);

            close(source_fd);
            close(destination_fd);
            unlink(destination);

            return 1;
        }


        ssize_t offset = 0;


        while (offset < bytes_read) {

            ssize_t bytes_written = write(
                destination_fd,
                buffer + offset,
                (size_t)(bytes_read - offset)
            );


            if (bytes_written < 0) {

                fprintf(
                    stderr,
                    "catpkg: write \"%s\": ",
                    destination
                );

                perror(NULL);

                close(source_fd);
                close(destination_fd);
                unlink(destination);

                return 1;
            }


            offset += bytes_written;
        }
    }


    if (fchmod(
            destination_fd,
            mode & 07777
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: fchmod \"%s\": ",
            destination
        );

        perror(NULL);

        close(source_fd);
        close(destination_fd);
        unlink(destination);

        return 1;
    }


    if (close(destination_fd) != 0) {

        fprintf(
            stderr,
            "catpkg: close \"%s\": ",
            destination
        );

        perror(NULL);

        close(source_fd);
        unlink(destination);

        return 1;
    }


    close(source_fd);

    return 0;
}


/*
 * Recursively copy a directory tree.
 *
 * The callback is executed by nftw().
 */
struct catpkg_copy_context {
    const char *source_root;
    const char *destination_root;
};


static struct catpkg_copy_context catpkg_copy_ctx;


static int catpkg_copy_directory_callback(
    const char *path,
    const struct stat *statbuf,
    int type,
    struct FTW *ftwbuf
)
{
    if (type != FTW_D &&
        type != FTW_F &&
        type != FTW_SL &&
        type != FTW_SLN) {

        fprintf(
            stderr,
            "catpkg: unsupported filesystem object \"%s\"\n",
            path
        );

        return 1;
    }


    const char *relative_path =
        path + strlen(
            catpkg_copy_ctx.source_root
        );


    while (*relative_path == '/')
        relative_path++;


    char destination[PATH_MAX];


    if (*relative_path == '\0') {

        if (snprintf(
                destination,
                sizeof(destination),
                "%s",
                catpkg_copy_ctx.destination_root
            ) < 0) {

            return 1;
        }
    }
    else {

        int written = snprintf(
            destination,
            sizeof(destination),
            "%s/%s",
            catpkg_copy_ctx.destination_root,
            relative_path
        );

        if (written < 0 ||
            (size_t)written >= sizeof(destination)) {

            fprintf(
                stderr,
                "catpkg: destination path is too long\n"
            );

            return 1;
        }
    }


    /*
     * Directory.
     */
    if (type == FTW_D) {

        if (mkdir(
                destination,
                statbuf->st_mode & 07777
            ) != 0) {

            if (errno == EEXIST)
                return 0;


            fprintf(
                stderr,
                "catpkg: mkdir \"%s\": ",
                destination
            );

            perror(NULL);

            return 1;
        }


        return 0;
    }


    /*
     * Symbolic link.
     */
    if (type == FTW_SL ||
        type == FTW_SLN) {

        char link_target[PATH_MAX];

        ssize_t length = readlink(
            path,
            link_target,
            sizeof(link_target) - 1
        );


        if (length < 0) {

            fprintf(
                stderr,
                "catpkg: readlink \"%s\": ",
                path
            );

            perror(NULL);

            return 1;
        }


        link_target[length] = '\0';


        if (symlink(
                link_target,
                destination
            ) != 0) {

            fprintf(
                stderr,
                "catpkg: symlink \"%s\": ",
                destination
            );

            perror(NULL);

            return 1;
        }


        return 0;
    }


    /*
     * Regular file.
     */
    if (type == FTW_F) {

        return catpkg_copy_file(
            path,
            destination,
            statbuf->st_mode
        );
    }


    return 0;
}


static int catpkg_remove_directory_callback(
    const char *path,
    const struct stat *statbuf,
    int type,
    struct FTW *ftwbuf
)
{
    (void)statbuf;
    (void)ftwbuf;


    if (type == FTW_F ||
        type == FTW_SL ||
        type == FTW_SLN) {

        if (unlink(path) != 0) {

            fprintf(
                stderr,
                "catpkg: unlink \"%s\": ",
                path
            );

            perror(NULL);

            return 1;
        }

        return 0;
    }


    if (type == FTW_D) {

        if (rmdir(path) != 0) {

            fprintf(
                stderr,
                "catpkg: rmdir \"%s\": ",
                path
            );

            perror(NULL);

            return 1;
        }

        return 0;
    }


    return 0;
}


static int catpkg_remove_directory(
    const char *path
)
{
    return nftw(
        path,
        catpkg_remove_directory_callback,
        64,
        FTW_DEPTH | FTW_PHYS
    );
}


int catpkg_move_directory(
    const char *source,
    const char *destination
)
{
    if (source == NULL ||
        destination == NULL) {

        errno = EINVAL;

        return 1;
    }


    /*
     * First try the normal rename().
     *
     * This is fast and atomic when both
     * paths are on the same filesystem.
     */
    if (rename(
            source,
            destination
        ) == 0) {

        return 0;
    }


    /*
     * If the filesystems are different,
     * rename() cannot work.
     */
    if (errno != EXDEV) {

        fprintf(
            stderr,
            "catpkg: rename directory \"%s\" -> \"%s\": ",
            source,
            destination
        );

        perror(NULL);

        return 1;
    }


    /*
     * Make sure the destination does not
     * already exist.
     */
    struct stat destination_stat;

    if (lstat(
            destination,
            &destination_stat
        ) == 0) {

        fprintf(
            stderr,
            "catpkg: destination directory \"%s\" already exists\n",
            destination
        );

        return 1;
    }


    if (errno != ENOENT) {

        fprintf(
            stderr,
            "catpkg: lstat \"%s\": ",
            destination
        );

        perror(NULL);

        return 1;
    }


    /*
     * Read source directory information.
     */
    struct stat source_stat;

    if (stat(
            source,
            &source_stat
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: stat \"%s\": ",
            source
        );

        perror(NULL);

        return 1;
    }


    if (!S_ISDIR(source_stat.st_mode)) {

        fprintf(
            stderr,
            "catpkg: \"%s\" is not a directory\n",
            source
        );

        return 1;
    }


    /*
     * Create the destination root.
     */
    if (mkdir(
            destination,
            source_stat.st_mode & 07777
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: mkdir \"%s\": ",
            destination
        );

        perror(NULL);

        return 1;
    }


    /*
     * Configure recursive copy.
     */
    catpkg_copy_ctx.source_root =
        source;

    catpkg_copy_ctx.destination_root =
        destination;


    /*
     * Copy everything recursively.
     */
    int result = nftw(
        source,
        catpkg_copy_directory_callback,
        64,
        FTW_PHYS
    );


    if (result != 0) {

        fprintf(
            stderr,
            "catpkg: failed to copy directory \"%s\"\n",
            source
        );

        /*
         * Remove partially copied destination.
         */
        catpkg_remove_directory(
            destination
        );

        return 1;
    }


    /*
     * Copy succeeded.
     *
     * Now remove the original directory.
     */
    if (catpkg_remove_directory(
            source
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to remove source directory \"%s\"\n",
            source
        );

        /*
         * Do NOT remove destination here.
         *
         * The copy itself succeeded.
         */
        return 1;
    }


    return 0;
}

int catpkg_move_file(
    const char *source,
    const char *destination
)
{
    if (source == NULL ||
        destination == NULL) {

        errno = EINVAL;
        return 1;
    }

    /*
     * First try rename().
     */
    if (rename(source, destination) == 0)
        return 0;

    /*
     * Different filesystems.
     */
    if (errno != EXDEV) {

        fprintf(
            stderr,
            "catpkg: rename file \"%s\" -> \"%s\": ",
            source,
            destination
        );

        perror(NULL);

        return 1;
    }

    /*
     * Open source.
     */
    int source_fd = open(
        source,
        O_RDONLY
    );

    if (source_fd < 0) {

        fprintf(
            stderr,
            "catpkg: open \"%s\": ",
            source
        );

        perror(NULL);

        return 1;
    }

    /*
     * Get source permissions.
     */
    struct stat st;

    if (fstat(source_fd, &st) != 0) {

        fprintf(
            stderr,
            "catpkg: fstat \"%s\": ",
            source
        );

        perror(NULL);

        close(source_fd);

        return 1;
    }

    /*
     * Create destination.
     */
    int destination_fd = open(
        destination,
        O_WRONLY |
        O_CREAT |
        O_TRUNC,
        st.st_mode & 07777
    );

    if (destination_fd < 0) {

        fprintf(
            stderr,
            "catpkg: open \"%s\": ",
            destination
        );

        perror(NULL);

        close(source_fd);

        return 1;
    }

    char buffer[65536];

    for (;;) {

        ssize_t bytes_read = read(
            source_fd,
            buffer,
            sizeof(buffer)
        );

        if (bytes_read == 0)
            break;

        if (bytes_read < 0) {

            fprintf(
                stderr,
                "catpkg: read \"%s\": ",
                source
            );

            perror(NULL);

            close(source_fd);
            close(destination_fd);

            unlink(destination);

            return 1;
        }

        ssize_t offset = 0;

        while (offset < bytes_read) {

            ssize_t bytes_written = write(
                destination_fd,
                buffer + offset,
                (size_t)(bytes_read - offset)
            );

            if (bytes_written < 0) {

                fprintf(
                    stderr,
                    "catpkg: write \"%s\": ",
                    destination
                );

                perror(NULL);

                close(source_fd);
                close(destination_fd);

                unlink(destination);

                return 1;
            }

            offset += bytes_written;
        }
    }

    /*
     * Preserve permissions.
     */
    if (fchmod(
            destination_fd,
            st.st_mode & 07777
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: fchmod \"%s\": ",
            destination
        );

        perror(NULL);

        close(source_fd);
        close(destination_fd);

        unlink(destination);

        return 1;
    }

    if (close(destination_fd) != 0) {

        fprintf(
            stderr,
            "catpkg: close \"%s\": ",
            destination
        );

        perror(NULL);

        close(source_fd);
        unlink(destination);

        return 1;
    }

    close(source_fd);

    /*
     * Copy succeeded.
     * Remove original.
     */
    if (unlink(source) != 0) {

        fprintf(
            stderr,
            "catpkg: unlink \"%s\": ",
            source
        );

        perror(NULL);

        return 1;
    }

    return 0;
}

void catpkg_format_size(
    off_t size, 
    char *buffer, 
    size_t buffer_size
)
{
    const char *units[] = {
        "B", "KiB", "MiB", "GiB", "TiB"
    };

    double value = (double)size;
    int unit = 0;

    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }

    snprintf(
        buffer,
        buffer_size,
        "%.2f %s",
        value,
        units[unit]
    );
}

char *catpkg_normalize_tar_path(
    const char *path
)
{
    if (path == NULL)
        return NULL;

    while (path[0] == '/') {
        path++;
    }

    while (path[0] == '.' && path[1] == '/') {
        path += 2;

        while (path[0] == '/') {
            path++;
        }
    }

    return strdup(path);
}