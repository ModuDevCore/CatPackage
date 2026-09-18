/*
 * ============================================================
 * Size calculation debug
 * ============================================================
 *
 * 1 - enabled
 * 0 - disabled
 */

#define CATPKG_CALC_SIZE_DEBUG 1

#if CATPKG_CALC_SIZE_DEBUG

    #define CALC_SIZE_DEBUG(protocol, value)                         \
        fprintf(                                                     \
            stderr,                                                  \
            "[CALC_SIZE] %-28s %10lld B | %s\n",                    \
            protocol,                                                \
            (long long)*change_size,                                 \
            (value) != NULL ? (value) : "-"                          \
        )

#else

    #define CALC_SIZE_DEBUG(protocol, value) \
        ((void)0)

#endif


case CTPG_OPEN:
    if (
        ctpg_context_init(
            &ctpg_context,
            request_val
        ) != 0
    ) {
        return 1;
    }

    *change_size = 0;

    CALC_SIZE_DEBUG(
        "CTPG_OPEN",
        request_val
    );

    break;


case CTPG_CLOSE:
    ctpg_context_close(
        &ctpg_context
    );

    *change_size = 0;

    CALC_SIZE_DEBUG(
        "CTPG_CLOSE",
        request_val
    );

    break;


case MOVE_FILE:
    /*
     * Moving within the same filesystem does not
     * change allocated disk space.
     */
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "MOVE_FILE",
        request_val
    );

    break;


case MOVE_DIR:
    /*
     * Moving within the same filesystem does not
     * change allocated disk space.
     */
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "MOVE_DIR",
        request_val
    );

    break;


case REMOVE_DIR:
case CTPG_REMOVE_DIR_OBSOLETE:
{
    const char *protocol_name =
        request->protocol == CTPG_REMOVE_DIR_OBSOLETE
            ? "CTPG_REMOVE_DIR_OBSOLETE"
            : "REMOVE_DIR";

    if (request_val == NULL)
        return 1;

    if (
        request->protocol ==
        CTPG_REMOVE_DIR_OBSOLETE
    ) {
        int is_obsolete =
            catpkg_builder_ctpg_path_obsolete(
                ctpg_context,
                request_val,
                1
            );

        if (is_obsolete < 0)
            return 1;

        if (is_obsolete == 0) {
            *change_size = 0;

            char debug_path[PATH_MAX];

            snprintf(
                debug_path,
                sizeof(debug_path),
                "%s (NOT OBSOLETE)",
                request_val
            );

            CALC_SIZE_DEBUG(
                protocol_name,
                debug_path
            );

            break;
        }
    }

    struct stat st;

    if (
        lstat(
            request_val,
            &st
        ) != 0
    ) {
        return 1;
    }

    if (!S_ISDIR(st.st_mode))
        return 1;

    /*
     * POSIX st_blocks is measured in
     * 512-byte units.
     */
    *change_size =
        -(
            (off_t)st.st_blocks *
            512
        );

    CALC_SIZE_DEBUG(
        protocol_name,
        request_val
    );

    break;
}


case REMOVE_FILE:
case CTPG_REMOVE_FILE_OBSOLETE:
{
    const char *protocol_name =
        request->protocol == CTPG_REMOVE_FILE_OBSOLETE
            ? "CTPG_REMOVE_FILE_OBSOLETE"
            : "REMOVE_FILE";

    if (request_val == NULL)
        return 1;

    if (
        request->protocol ==
        CTPG_REMOVE_FILE_OBSOLETE
    ) {
        int is_obsolete =
            catpkg_builder_ctpg_file_obsolete(
                ctpg_context,
                request_val
            );

        if (is_obsolete < 0)
            return 1;

        if (is_obsolete == 0) {
            *change_size = 0;

            char debug_path[PATH_MAX];

            snprintf(
                debug_path,
                sizeof(debug_path),
                "%s (NOT OBSOLETE)",
                request_val
            );

            CALC_SIZE_DEBUG(
                protocol_name,
                debug_path
            );

            break;
        }
    }

    struct stat st;

    if (
        lstat(
            request_val,
            &st
        ) != 0
    ) {
        return 1;
    }

    *change_size =
        -(
            (off_t)st.st_blocks *
            512
        );

    CALC_SIZE_DEBUG(
        protocol_name,
        request_val
    );

    break;
}


case MKDIR:
{
    if (request_val == NULL)
        return 1;

    char path[PATH_MAX];

    if (
        snprintf(
            path,
            sizeof(path),
            "%s",
            request_val
        ) >= (int)sizeof(path)
    ) {
        return 1;
    }

    char *parent =
        dirname(path);

    struct statvfs fs;

    if (
        statvfs(
            parent,
            &fs
        ) != 0
    ) {
        return 1;
    }

    /*
     * Directory does not exist yet, so its exact
     * allocation cannot be queried.
     *
     * Estimate one filesystem block.
     */
    *change_size =
        (off_t)fs.f_bsize;

    CALC_SIZE_DEBUG(
        "MKDIR",
        request_val
    );

    break;
}


case MKDIR_SOFT:
{
    if (request_val == NULL)
        return 1;

    char path[PATH_MAX];

    if (
        snprintf(
            path,
            sizeof(path),
            "%s",
            request_val
        ) >= (int)sizeof(path)
    ) {
        return 1;
    }

    size_t created = 0;

    while (1) {
        struct stat st;

        if (
            stat(
                path,
                &st
            ) == 0
        ) {
            if (!S_ISDIR(st.st_mode))
                return 1;

            struct statvfs fs;

            if (
                statvfs(
                    path,
                    &fs
                ) != 0
            ) {
                return 1;
            }

            *change_size =
                (off_t)created *
                (off_t)fs.f_bsize;

            CALC_SIZE_DEBUG(
                "MKDIR_SOFT",
                request_val
            );

            break;
        }

        if (errno != ENOENT)
            return 1;

        created++;

        char *parent =
            dirname(path);

        if (parent == NULL)
            return 1;

        if (strcmp(parent, path) == 0)
            return 1;

        memmove(
            path,
            parent,
            strlen(parent) + 1
        );
    }

    break;
}


case MKFILE:
{
    if (request_val == NULL)
        return 1;

    /*
     * Empty regular file:
     * no data blocks are allocated.
     */
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "MKFILE",
        request_val
    );

    break;
}


case SYMLINK:
{
    if (request_val == NULL)
        return 1;

    /*
     * Symlink allocation is ignored for now.
     * Short symlinks are commonly stored directly
     * in the inode.
     */
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "SYMLINK",
        request_val
    );

    break;
}


case CTPG_EXTRACT:
{
    if (
        catpkg_builder_ctpg_extract_size(
            ctpg_context,
            request_val,
            change_size,
            NULL
        ) != 0
    ) {
        return 1;
    }

    CALC_SIZE_DEBUG(
        "CTPG_EXTRACT",
        request_val
    );

    break;
}


case CTPG_EXTRACT_PACKAGEINFO:
{
    if (
        catpkg_builder_ctpg_extract_size(
            ctpg_context,
            CATPKG_PACKAGEINFO,
            change_size,
            NULL
        ) != 0
    ) {
        return 1;
    }

    CALC_SIZE_DEBUG(
        "CTPG_EXTRACT_PACKAGEINFO",
        CATPKG_PACKAGEINFO
    );

    break;
}


case CTPG_EXTRACT_PAYLOAD:
{
    if (
        catpkg_builder_ctpg_extract_size(
            ctpg_context,
            "/",
            change_size,
            (const char *[]) {
                CATPKG_PACKAGEINFO,
                NULL
            }
        ) != 0
    ) {
        return 1;
    }

    CALC_SIZE_DEBUG(
        "CTPG_EXTRACT_PAYLOAD",
        "/"
    );

    break;
}


case CTPG_TO_CACHE:
{
    /*
     * TODO:
     * Calculate resulting cached .catpackage
     * / zstd size.
     */
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "CTPG_TO_CACHE",
        request_val
    );

    break;
}


case CATPKG_UPDATE_DATABASE:
{
    if (request_val == NULL)
        return 1;

    off_t old_size = 0;
    off_t new_size = 0;

    struct stat st;

    if (
        stat(
            request_val,
            &st
        ) == 0
    ) {
        old_size =
            (off_t)st.st_size;
    }
    else if (errno != ENOENT) {
        return 1;
    }

    int written =
        snprintf(
            NULL,
            0,
            "CATPKG 1.1\n"
            "#catpkg-version 2.0.0-beta\n"
        );

    if (written < 0)
        return 1;

    new_size +=
        (off_t)written;

    written =
        snprintf(
            NULL,
            0,
            "-\n"
            "#type file\n"
            "#path %s\n"
            "#required catpkg\n",
            request_val
        );

    if (written < 0)
        return 1;

    new_size +=
        (off_t)written;

    DIR *dir =
        opendir(
            CATPKG_PACKAGES_PATH
        );

    if (dir == NULL)
        return 1;

    struct dirent *entry;

    while (
        (entry = readdir(dir)) != NULL
    ) {
        if (
            strcmp(
                entry->d_name,
                "."
            ) == 0 ||
            strcmp(
                entry->d_name,
                ".."
            ) == 0
        ) {
            continue;
        }

        char *package_fileinfo_path =
            make_catpkg_path(
                CATPKG_FILES_PATH
                "/"
                CATPKG_FILE,
                entry->d_name
            );

        char *package_info_path =
            make_catpkg_path(
                CATPKG_METADATA_PATH
                "/"
                CATPKG_PACKAGEINFO,
                entry->d_name
            );

        if (
            package_fileinfo_path == NULL ||
            package_info_path == NULL
        ) {
            free(
                package_fileinfo_path
            );

            free(
                package_info_path
            );

            closedir(dir);

            return 1;
        }

        struct PackageInfo *package_info =
            CATPKG_Parse(
                package_info_path
            );

        struct PackageInfo *package_fileinfo =
            CATPKG_Parse(
                package_fileinfo_path
            );

        if (
            package_info == NULL ||
            package_fileinfo == NULL
        ) {
            PackageInfo_Free(
                package_info
            );

            PackageInfo_Free(
                package_fileinfo
            );

            free(
                package_fileinfo_path
            );

            free(
                package_info_path
            );

            closedir(dir);

            return 1;
        }

        const struct PackageField *info_field =
            PackageInfo_Find(
                package_fileinfo,
                "info"
            );

        const struct PackageField *name_field =
            PackageInfo_Find(
                package_info,
                "name"
            );

        if (
            name_field == NULL ||
            name_field->value == NULL
        ) {
            PackageInfo_Free(
                package_info
            );

            PackageInfo_Free(
                package_fileinfo
            );

            free(
                package_fileinfo_path
            );

            free(
                package_info_path
            );

            closedir(dir);

            return 1;
        }

        while (info_field != NULL) {
            if (
                info_field->name != NULL &&
                info_field->value != NULL &&
                strcmp(
                    info_field->name,
                    "info"
                ) == 0
            ) {
                written =
                    snprintf(
                        NULL,
                        0,
                        "-\n"
                        "%s\n"
                        "#required %s\n",
                        info_field->value,
                        name_field->value
                    );

                if (written < 0) {
                    PackageInfo_Free(
                        package_info
                    );

                    PackageInfo_Free(
                        package_fileinfo
                    );

                    free(
                        package_fileinfo_path
                    );

                    free(
                        package_info_path
                    );

                    closedir(dir);

                    return 1;
                }

                new_size +=
                    (off_t)written;
            }

            info_field =
                info_field->next_field;
        }

        PackageInfo_Free(
            package_info
        );

        PackageInfo_Free(
            package_fileinfo
        );

        free(
            package_fileinfo_path
        );

        free(
            package_info_path
        );
    }

    closedir(dir);

    /*
     * DATABASE is rewritten completely.
     *
     * For now this remains logical file size,
     * because the future allocation cannot be
     * obtained through stat() before writing.
     */
    *change_size =
        new_size -
        old_size;

    CALC_SIZE_DEBUG(
        "CATPKG_UPDATE_DATABASE",
        request_val
    );

    break;
}


default:
    *change_size = 0;

    CALC_SIZE_DEBUG(
        "UNKNOWN/IGNORED",
        request_val
    );

    break;