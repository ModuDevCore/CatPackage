/*
 * ------------------------------------------------
 * MOVE_FILE
 * ------------------------------------------------
 */

case MOVE_FILE:
{
    if (request_val == NULL)
        goto fileinfo_error;

    char *source =
        request_val;

    char *destination =
        source +
        strlen(source) + 1;

    if (
        fprintf(
            file,
            "-\n"
            "#type file\n"
            "#path %s\n"
            "#source %s\n",
            destination,
            source
        ) < 0
    )
        goto fileinfo_error;

    CATPKG_BUILD_LOG(
        "FILEINFO: MOVE_FILE \"%s\" -> \"%s\"",
        "OK",
        0,
        CATPKG_BUILD_LOG_CTPG_ARGS(
            source,
            destination
        )
    );

    sap_section_index++;

    break;
}


/*
 * ------------------------------------------------
 * MOVE_DIR
 * ------------------------------------------------
 */

case MOVE_DIR:
{
    if (request_val == NULL)
        goto fileinfo_error;

    char *source =
        request_val;

    char *destination =
        source +
        strlen(source) + 1;

    if (
        fprintf(
            file,
            "-\n"
            "#type directory\n"
            "#path %s\n"
            "#source %s\n",
            destination,
            source
        ) < 0
    )
        goto fileinfo_error;

    CATPKG_BUILD_LOG(
        "FILEINFO: MOVE_DIR \"%s\" -> \"%s\"",
        "OK",
        0,
        CATPKG_BUILD_LOG_CTPG_ARGS(
            source,
            destination
        )
    );

    sap_section_index++;

    break;
}


/*
 * ------------------------------------------------
 * MKDIR
 * ------------------------------------------------
 */

case MKDIR:
{
    if (request_val == NULL)
        goto fileinfo_error;

    if (
        fprintf(
            file,
            "-\n"
            "#type directory\n"
            "#path %s\n",
            request_val
        ) < 0
    )
        goto fileinfo_error;

    CATPKG_BUILD_LOG(
        "FILEINFO: MKDIR \"%s\"",
        "OK",
        0,
        request_val
    );

    sap_section_index++;

    break;
}


/*
 * ------------------------------------------------
 * MKDIR_SOFT
 * ------------------------------------------------
 */

case MKDIR_SOFT:
{
    const char *path = request->value;

    if (path == NULL)
        goto fileinfo_error;

    if (
        fprintf(
            file,
            "-\n"
            "#type directory\n"
            "#path %s\n",
            path
        ) < 0
    )
        goto fileinfo_error;

    CATPKG_BUILD_LOG(
        "FILEINFO: MKDIR_SOFT \"%s\"",
        "OK",
        0,
        path
    );

    sap_section_index++;

    break;
}

/*
 * ------------------------------------------------
 * SYMLINK
 * ------------------------------------------------
 */

case SYMLINK:
{
    if (request_val == NULL)
        goto fileinfo_error;

    char *target =
        request_val;

    char *link_path =
        target +
        strlen(target) + 1;

    if (
        fprintf(
            file,
            "-\n"
            "#type symlink\n"
            "#path %s\n"
            "#target %s\n",
            link_path,
            target
        ) < 0
    )
        goto fileinfo_error;

    CATPKG_BUILD_LOG(
        "FILEINFO: SYMLINK \"%s\" -> \"%s\"",
        "OK",
        0,
        CATPKG_BUILD_LOG_CTPG_ARGS(
            target,
            link_path
        )
    );

    sap_section_index++;

    break;
}


/*
 * ------------------------------------------------
 * MKFILE
 * ------------------------------------------------
 */

case MKFILE:

    CATPKG_BUILD_LOG(
        "%s",
        "TODO",
        0,
        "FILEINFO: MKFILE"
    );

    break;


/*
 * ------------------------------------------------
 * CTPG_EXTRACT
 * ------------------------------------------------
 */

case CTPG_EXTRACT:
{
    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL ||
        request_val == NULL
    ) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "FILEINFO: CTPG_EXTRACT: invalid context"
        );

        goto fileinfo_error;
    }

    char *path_prefix =
        catpkg_normalize_tar_path(
            request_val
        );

    if (path_prefix == NULL) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "FILEINFO: CTPG_EXTRACT: normalize path"
        );

        goto fileinfo_error;
    }

    char *destination =
        request_val +
        strlen(request_val) + 1;

    FILE *tarfile =
        fopen(
            ctpg_context->path,
            "rb"
        );

    if (tarfile == NULL) {

        perror(
            "catpkg: failed to open package"
        );

        free(path_prefix);

        goto fileinfo_error;
    }

    struct header_posix_ustar hpu;

    while (
        ctpg_read_tar_header(
            &hpu,
            tarfile
        ) == 0
    ) {

        char size_string[13];

        memcpy(
            size_string,
            hpu.size,
            sizeof(hpu.size)
        );

        size_string[
            sizeof(hpu.size)
        ] = '\0';

        off_t data_size =
            (off_t)strtoull(
                size_string,
                NULL,
                8
            );

        off_t padding =
            (512 - (data_size % 512)) % 512;

        if (
            fseek(
                tarfile,
                (long)data_size,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR data"
            );

            fclose(tarfile);
            free(path_prefix);

            goto fileinfo_error;
        }

        if (
            fseek(
                tarfile,
                (long)padding,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR padding"
            );

            fclose(tarfile);
            free(path_prefix);

            goto fileinfo_error;
        }

        char *correct_path =
            catpkg_normalize_tar_path(
                hpu.name
            );

        if (correct_path == NULL) {

            CATPKG_BUILD_LOG(
                "%s",
                "ERROR",
                0,
                "FILEINFO: CTPG_EXTRACT: invalid TAR path"
            );

            fclose(tarfile);
            free(path_prefix);

            goto fileinfo_error;
        }

        size_t prefix_len =
            strlen(path_prefix);

        if (
            is_excluded_tar_path(
                correct_path,
                (const char *[]) {
                    CATPKG_PACKAGEINFO,
                    NULL
                }
            ) == 0 &&
            (
                prefix_len == 0 ||
                (
                    strncmp(
                        correct_path,
                        path_prefix,
                        prefix_len
                    ) == 0 &&
                    (
                        correct_path[prefix_len] == '\0' ||
                        correct_path[prefix_len] == '/'
                    )
                )
            )
        ) {

            const char *relative_path =
                correct_path;

            if (prefix_len > 0) {

                relative_path =
                    correct_path +
                    prefix_len;

                while (
                    *relative_path == '/'
                )
                    relative_path++;
            }

            char *exported_to =
                make_catpkg_path(
                    "%s/%s",
                    destination,
                    relative_path
                );

            if (exported_to == NULL) {

                free(correct_path);
                fclose(tarfile);
                free(path_prefix);

                goto fileinfo_error;
            }

            switch (
                hpu.typeflag[0]
            ) {

                case '\0':
                case '0':

                    fprintf(
                        file,
                        "-\n"
                        "#type file\n"
                        "#path %s\n",
                        exported_to
                    );

                    CATPKG_BUILD_LOG(
                        "FILEINFO: CTPG_EXTRACT file \"%s\"",
                        "OK",
                        0,
                        exported_to
                    );

                    sap_section_index++;

                    break;


                case '2':

                    fprintf(
                        file,
                        "-\n"
                        "#type symlink\n"
                        "#path %s\n"
                        "#target %s\n",
                        exported_to,
                        hpu.linkname
                    );

                    CATPKG_BUILD_LOG(
                        "FILEINFO: CTPG_EXTRACT symlink \"%s\"",
                        "OK",
                        0,
                        exported_to
                    );

                    sap_section_index++;

                    break;

                default:

                    CATPKG_BUILD_LOG(
                        "FILEINFO: CTPG_EXTRACT unsupported type \"%c\"",
                        "SKIP",
                        0,
                        hpu.typeflag[0]
                    );

                    break;
            }

            free(exported_to);
        }

        free(correct_path);
    }

    fclose(tarfile);
    free(path_prefix);

    CATPKG_BUILD_LOG(
        "%s",
        "OK",
        0,
        "FILEINFO: CTPG_EXTRACT"
    );

    break;
}


/*
 * ------------------------------------------------
 * CTPG_EXTRACT_PAYLOAD
 * ------------------------------------------------
 */

case CTPG_EXTRACT_PAYLOAD:
{
    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL
    ) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "FILEINFO: CTPG_EXTRACT_PAYLOAD: invalid context"
        );

        goto fileinfo_error;
    }

    FILE *tarfile =
        fopen(
            ctpg_context->path,
            "rb"
        );

    if (tarfile == NULL) {

        perror(
            "catpkg: failed to open package"
        );

        goto fileinfo_error;
    }

    struct header_posix_ustar hpu;

    while (
        ctpg_read_tar_header(
            &hpu,
            tarfile
        ) == 0
    ) {

        char size_string[13];

        memcpy(
            size_string,
            hpu.size,
            sizeof(hpu.size)
        );

        size_string[
            sizeof(hpu.size)
        ] = '\0';

        off_t data_size =
            (off_t)strtoull(
                size_string,
                NULL,
                8
            );

        off_t padding =
            (512 - (data_size % 512)) % 512;

        if (
            fseek(
                tarfile,
                (long)data_size,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR data"
            );

            fclose(tarfile);

            goto fileinfo_error;
        }

        if (
            fseek(
                tarfile,
                (long)padding,
                SEEK_CUR
            ) != 0
        ) {

            perror(
                "catpkg: failed to skip TAR padding"
            );

            fclose(tarfile);

            goto fileinfo_error;
        }

        char *correct_path =
            catpkg_normalize_tar_path(
                hpu.name
            );

        if (correct_path == NULL) {

            fclose(tarfile);

            goto fileinfo_error;
        }

        if (
            is_excluded_tar_path(
                correct_path,
                (const char *[]) {
                    CATPKG_PACKAGEINFO,
                    NULL
                }
            ) == 0
        ) {

            char *exported_to =
                make_catpkg_path(
                    "/%s",
                    correct_path
                );

            if (exported_to == NULL) {

                free(correct_path);
                fclose(tarfile);

                goto fileinfo_error;
            }

            switch (
                hpu.typeflag[0]
            ) {

                case '\0':
                case '0':

                    fprintf(
                        file,
                        "-\n"
                        "#type file\n"
                        "#path %s\n",
                        exported_to
                    );

                    CATPKG_BUILD_LOG(
                        "FILEINFO: CTPG_EXTRACT_PAYLOAD file \"%s\"",
                        "OK",
                        0,
                        exported_to
                    );

                    sap_section_index++;

                    break;


                case '2':

                    fprintf(
                        file,
                        "-\n"
                        "#type symlink\n"
                        "#path %s\n"
                        "#target %s\n",
                        exported_to,
                        hpu.linkname
                    );

                    CATPKG_BUILD_LOG(
                        "FILEINFO: CTPG_EXTRACT_PAYLOAD symlink \"%s\"",
                        "OK",
                        0,
                        exported_to
                    );

                    sap_section_index++;

                    break;

                default:

                    CATPKG_BUILD_LOG(
                        "FILEINFO: CTPG_EXTRACT_PAYLOAD unsupported type \"%c\"",
                        "SKIP",
                        0,
                        hpu.typeflag[0]
                    );

                    break;
            }

            free(exported_to);
        }

        free(correct_path);
    }

    fclose(tarfile);

    CATPKG_BUILD_LOG(
        "%s",
        "OK",
        0,
        "FILEINFO: CTPG_EXTRACT_PAYLOAD"
    );

    break;
}


/*
 * ------------------------------------------------
 * CTPG_EXTRACT_PACKAGEINFO
 * ------------------------------------------------
 */

case CTPG_EXTRACT_PACKAGEINFO:
{
    if (ctpg_context == NULL) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "FILEINFO: PACKAGEINFO: invalid context"
        );

        goto fileinfo_error;
    }

    char *catpkg_PACKAGEINFO_path =
        make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            ctpg_context->full_name
        );

    if (catpkg_PACKAGEINFO_path == NULL)
        goto fileinfo_error;

    if (
        fprintf(
            file,
            "-\n"
            "#type file\n"
            "#path %s\n",
            catpkg_PACKAGEINFO_path
        ) < 0
    ) {

        free(catpkg_PACKAGEINFO_path);

        goto fileinfo_error;
    }

    CATPKG_BUILD_LOG(
        "FILEINFO: PACKAGEINFO \"%s\"",
        "OK",
        0,
        catpkg_PACKAGEINFO_path
    );

    free(catpkg_PACKAGEINFO_path);

    sap_section_index++;

    break;
}


/*
 * ------------------------------------------------
 * Non-persistent protocols
 * ------------------------------------------------
 */

case REMOVE_DIR:
case REMOVE_FILE:
case REMOVE:
case SU:
case BUILD_LOG:
case BUILD_LOG_START:
case BUILD_LOG_END:
case CTPG_OPEN:
case CTPG_INFO:
case CTPG_CLOSE:

    break;


default:

    CATPKG_BUILD_LOG(
        "FILEINFO: unsupported protocol %d",
        "SKIP",
        0,
        request->protocol
    );

    break;