/*
 * value:
 *
 * source\0destination\0
 */

case MOVE_FILE: {

    if (request_val == NULL)
        return 1;

    char *source =
        request_val;

    char *destination =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(1);

    if (catpkg_move_file(
            destination,
            source
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "REVERT_MOVE_FILE",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "REVERT_MOVE_FILE",
        source,
        destination
    );

    break;
}


case MOVE_DIR: {

    if (request_val == NULL)
        return 1;

    char *source =
        request_val;

    char *destination =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(1);

    if (catpkg_move_directory(
            destination,
            source
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "REVERT_MOVE_DIR",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "REVERT_MOVE_DIR",
        source,
        destination
    );

    break;
}

case REMOVE_DIR:
case CTPG_REMOVE_DIR_OBSOLETE:
{
    const char *protocol_name =
        request->protocol == CTPG_REMOVE_DIR_OBSOLETE
            ? "REVERT_CTPG_REMOVE_DIR_OBSOLETE"
            : "REVERT_REMOVE_DIR";

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
            CATPKG_BUILD_LOG(
                "REVERT_CTPG_REMOVE_DIR_OBSOLETE: \"%s\" Nothing to restore.",
                "( NOT REMOVED )",
                0,
                request_val
            );

            break;
        }
    }

    CATPKG_BUILD_LOG_TREE(1);

    if (
        catpkg_builder_cache_extract(
            request_val,
            "/"
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
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
            ? "REVERT_CTPG_REMOVE_FILE_OBSOLETE"
            : "REVERT_REMOVE_FILE";

    if (request_val == NULL)
        return 1;


    /*
     * CTPG_REMOVE_FILE_OBSOLETE:
     *
     * If the file is NOT obsolete, APPLY did not
     * remove it.
     *
     * This includes files which:
     *
     * - exist in the package payload;
     * - are declared by #persistent-file.
     *
     * Therefore there is nothing to restore.
     */

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
            CATPKG_BUILD_LOG(
                "%s",
                "NOT REMOVED",
                0,
                "REVERT_CTPG_REMOVE_FILE_OBSOLETE: Nothing to restore."
            );

            break;
        }
    }


    /*
     * The file was actually removed.
     * Restore it from the builder cache.
     */

    CATPKG_BUILD_LOG_TREE(1);

    if (
        catpkg_builder_cache_extract(
            request_val,
            "/"
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        protocol_name,
        request_val
    );

    break;
}


case REMOVE:

    /*
     * TODO
     */

    break;

case MKDIR:

    if (request_val == NULL)
        return 1;

    CATPKG_BUILD_LOG_TREE(1);

    if (catpkg_remove_directory(
            request_val
        ) != 0) {

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_MKDIR",
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "REVERT_MKDIR",
        request_val
    );

    break;


case MKFILE:

    /*
     * TODO
     */

    break;


case SYMLINK: {

    if (request_val == NULL)
        return 1;

    char *link_path =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(1);

    if (unlink(link_path) != 0) {

        fprintf(
            stderr,
            "catpkg: unlink \"%s\": ",
            link_path
        );

        perror(NULL);

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_SYMLINK",
            link_path
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "REVERT_SYMLINK",
        link_path
    );

    break;
}


case SU:

    CATPKG_BUILD_LOG_TREE(1);

    if (seteuid(
            request->previous_euid
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: seteuid(%u): ",
            (unsigned int)
            request->previous_euid
        );

        perror(NULL);

        CATPKG_BUILD_LOG_PATH_ERROR(
            "REVERT_SU",
            "ERROR"
        );

        return 1;
    }

    char uid_buffer[32];

    snprintf(
        uid_buffer,
        sizeof(uid_buffer),
        "%u",
        (unsigned int)
        request->previous_euid
    );

    CATPKG_BUILD_LOG_PATH_OK(
        "REVERT_SU",
        uid_buffer
    );

    break;


case BUILD_LOG_START:

    CATPKG_BUILD_LOG_TREE(1);
    CATPKG_BUILD_LOG(
        "BUILD_LOG_END",
        "",
        1,
        ""
    );

    build_log_active = 0;

    break;


case BUILD_LOG_END:

    build_log_active = 1;

    CATPKG_BUILD_LOG_TREE(1);
    CATPKG_BUILD_LOG(
        "BUILD_LOG_START",
        "",
        1,
        ""
    );

    break;


case NONE:

    break;