/*
 * Section 0
 *
 * Global FileInfo options.
 *
 * Currently:
 *
 * #catpkg-version <version>
 * #use-options <path>
 */

case 0: {

    /*
     * End of global section.
     *
     * The next section is an info section.
     */

    if (strcmp(buffer, "-") == 0) {
        current_section++;

        long section_bytepos =
            reader->bytepos;

        char *content =
            package_reader_read_section(
                reader
            );

        if (content == NULL)
            return 1;

        ADD_FIELD(
            "info",
            content,
            section_bytepos
        );

        free(content);

        continue;
    }


    /*
     * Ignore empty lines.
     */

    if (buffer[0] == '\0')
        continue;


    /*
     * #catpkg-version
     */

    if (
        strncmp(
            buffer,
            "#catpkg-version ",
            16
        ) == 0
    ) {
        if (buffer[16] == '\0') {
            fprintf(
                stderr,
                "catpkg: catpkg version cannot be empty\n"
            );

            return 1;
        }

        ADD_FIELD(
            "#catpkg-version",
            buffer + 16,
            field_bytepos
        );

        continue;
    }


    /*
     * #use-options
     */

    if (
        strncmp(
            buffer,
            "#use-options ",
            13
        ) == 0
    ) {
        if (buffer[13] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid options path: \"%s\"\n",
                buffer
            );

            return 1;
        }

        ADD_FIELD(
            "#use-options",
            buffer + 13,
            field_bytepos
        );


        /*
         * Resolve options path relative
         * to the current FileInfo.
         */

        char directory[PATH_MAX];

        strncpy(
            directory,
            reader->path,
            sizeof(directory) - 1
        );

        directory[
            sizeof(directory) - 1
        ] = '\0';

        char *directory_path =
            dirname(directory);

        char *options_path =
            make_catpkg_path(
                "%s/%s",
                directory_path,
                buffer + 13
            );

        if (options_path == NULL)
            return 1;


        /*
         * Open options file.
         */

        FILE *file =
            fopen(
                options_path,
                "r"
            );

        if (file == NULL) {
            fprintf(
                stderr,
                "#use-options: cannot open options file \"%s\"\n",
                options_path
            );

            free(options_path);

            return 1;
        }

        free(options_path);


        /*
         * Initialize options reader.
         */

        struct PackageReader options_reader;

        package_reader_init_file(
            &options_reader,
            file,
            0
        );

        char option_buffer[PATH_MAX];


        /*
         * #use-options scope.
         */

        while (
            package_reader_getline(
                &options_reader,
                option_buffer,
                sizeof(option_buffer)
            ) != NULL
        ) {
            option_buffer[
                strcspn(
                    option_buffer,
                    "\r\n"
                )
            ] = '\0';


            /*
             * Ignore empty lines.
             */

            if (option_buffer[0] == '\0')
                continue;


            /*
             * #required <section>=<path>
             */

             if (
                strncmp(
                    option_buffer,
                    "#required ",
                    10
                ) == 0
            ) {
                char *declaration =
                    option_buffer + 10;

                char *separator =
                    strchr(
                        declaration,
                        '='
                    );

                if (
                    separator == NULL ||
                    separator == declaration ||
                    separator[1] == '\0'
                ) {
                    fprintf(
                        stderr,
                        "#use-options: invalid FileInfo required: \"%s\"\n"
                        "Declaration: {section}={path}\n",
                        declaration
                    );

                    fclose(file);
                    return 1;
                }

                /*
                 * Validate section number without modifying
                 * the original declaration.
                 */

                errno = 0;

                char *end = NULL;

                unsigned long long section_number =
                    strtoull(
                        declaration,
                        &end,
                        10
                    );

                if (
                    errno != 0 ||
                    end == declaration ||
                    end != separator ||
                    section_number > SIZE_MAX
                ) {
                    fprintf(
                        stderr,
                        "#use-options: invalid FileInfo section in: \"%s\"\n"
                        "Declaration: {section}={path}\n",
                        declaration
                    );

                    fclose(file);
                    return 1;
                }

                /*
                 * Keep the complete declaration:
                 *
                 * #required 0=catpkg
                 *
                 * PackageField:
                 *   name    = "#required"
                 *   value   = "0=catpkg"
                 *   section = section of #use-options
                 */

                ADD_FIELD(
                    "#required",
                    declaration,
                    options_reader.bytepos
                );

                continue;
            }

            /*
             * #persistent <section>=<path>
             */

             if (
                strncmp(
                    option_buffer,
                    "#persistent ",
                    12
                ) == 0
            ) {
                char *declaration =
                    option_buffer + 12;

                char *separator =
                    strchr(
                        declaration,
                        '='
                    );

                if (
                    separator == NULL ||
                    separator == declaration ||
                    separator[1] == '\0'
                ) {
                    fprintf(
                        stderr,
                        "#use-options: invalid FileInfo persistent: \"%s\"\n"
                        "Declaration: {section}={path}\n",
                        declaration
                    );

                    fclose(file);
                    return 1;
                }

                /*
                 * Validate section number without modifying
                 * the original declaration.
                 */

                errno = 0;

                char *end = NULL;

                unsigned long long section_number =
                    strtoull(
                        declaration,
                        &end,
                        10
                    );

                if (
                    errno != 0 ||
                    end == declaration ||
                    end != separator ||
                    section_number > SIZE_MAX
                ) {
                    fprintf(
                        stderr,
                        "#use-options: invalid FileInfo section in: \"%s\"\n"
                        "Declaration: {section}={path}\n",
                        declaration
                    );

                    fclose(file);
                    return 1;
                }

                /*
                 * Keep the complete declaration:
                 *
                 * #persistent 0=catpkg
                 *
                 * PackageField:
                 *   name    = "#persistent"
                 *   value   = "0=catpkg"
                 *   section = section of #use-options
                 */

                ADD_FIELD(
                    "#persistent",
                    declaration,
                    options_reader.bytepos
                );

                continue;
            }

            /*
             * #obsolete <section>=<path>
             */

             if (
                strncmp(
                    option_buffer,
                    "#obsolete ",
                    10
                ) == 0
            ) {
                char *declaration =
                    option_buffer + 10;
                /*
                 * Keep the complete declaration:
                 *
                 * #obsolete 0
                 *
                 * PackageField:
                 *   name    = "#obsolete"
                 *   value   = "0"
                 *   section = section of #use-options
                 */

                ADD_FIELD(
                    "#obsolete",
                    declaration,
                    options_reader.bytepos
                );

                continue;
            }

            /*
             * Unknown external option.
             */

            fprintf(
                stderr,
                "#use-options: unknown option: \"%s\"\n",
                option_buffer
            );

            fclose(file);
            return 1;
        }

        fclose(file);
        continue;
    }


    /*
     * Unknown global FileInfo option.
     */

    fprintf(
        stderr,
        "catpkg: unknown FileInfo global option: \"%s\"\n",
        buffer
    );

    return 1;
}


/*
 * Sections 1..N
 *
 * FileInfo entry.
 */

default: {

    /*
     * End of current info section.
     *
     * Read the next section in advance,
     * store its raw content as "info",
     * then return the reader to its start.
     */

    if (strcmp(buffer, "-") == 0) {
        current_section++;

        long section_bytepos =
            reader->bytepos;

        char *content =
            package_reader_read_section(
                reader
            );

        if (content == NULL)
            return 1;

        ADD_FIELD(
            "info",
            content,
            section_bytepos
        );

        free(content);

        continue;
    }


    /*
     * Ignore empty lines.
     */

    if (buffer[0] == '\0')
        continue;


    /*
     * #type
     */

    if (
        strncmp(
            buffer,
            "#type ",
            6
        ) == 0
    ) {
        if (buffer[6] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid FileInfo type: \"%s\"\n",
                buffer
            );

            return 1;
        }

        ADD_FIELD(
            "#type",
            buffer + 6,
            field_bytepos
        );

        continue;
    }


    /*
     * #path
     */

    if (
        strncmp(
            buffer,
            "#path ",
            6
        ) == 0
    ) {
        if (buffer[6] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid FileInfo path: \"%s\"\n",
                buffer
            );

            return 1;
        }

        ADD_FIELD(
            "#path",
            buffer + 6,
            field_bytepos
        );

        continue;
    }

    /*
     * #path
     */

    if (
        strncmp(
            buffer,
            "#target ",
            8
        ) == 0
    ) {
        if (buffer[8] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid FileInfo target: \"%s\"\n",
                buffer
            );

            return 1;
        }

        ADD_FIELD(
            "#target",
            buffer + 8,
            field_bytepos
        );

        continue;
    }

    /*
     * Unknown FileInfo option.
     */

    fprintf(
        stderr,
        "catpkg: unknown FileInfo option: \"%s\"\n",
        buffer
    );

    return 1;
}