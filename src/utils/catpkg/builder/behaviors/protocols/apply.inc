case CTPG_MKCMD:
{
    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "CTPG_MKCMD: \"%s\"\n",
        "",
        0,
        request_val
    );


    /*
     * --------------------------------------------------------
     * Read command from CTPG
     * --------------------------------------------------------
     *
     * request_val is the path inside the TAR.
     *
     * Example:
     *
     * .catpkg/commands/test.sh
     */

    char *script =
        catpkg_builder_ctpg_read_data(
            ctpg_context,
            request_val
        );

    if (script == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "CTPG_MKCMD",
            request_val
        );

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Create pipe for command metadata
     * --------------------------------------------------------
     *
     * Child:
     *
     * bash stdout
     *      ↓
     *    fd[1]
     *
     * Parent:
     *
     *    fd[0]
     *      ↓
     * name\0description\0declaration\0
     */

    int fd[2];

    if (pipe(fd) == -1) {
        perror("pipe");

        free(script);

        return 1;
    }


    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");

        close(fd[0]);
        close(fd[1]);

        free(script);

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Child
     * --------------------------------------------------------
     */

    if (pid == 0) {

        close(fd[0]);


        /*
         * Redirect stdout to metadata pipe.
         */

        if (
            dup2(
                fd[1],
                STDOUT_FILENO
            ) == -1
        ) {
            perror("dup2");

            close(fd[1]);

            _exit(1);
        }

        close(fd[1]);


        /*
         * Append catpkg metadata query to the
         * command script.
         */

        size_t script_len =
            strlen(script);

        size_t check_len =
            strlen(
                CATPKG_COMMAND_CHECK_METAINFO
            );


        char *bash_script =
            malloc(
                script_len +
                check_len +
                1
            );

        if (bash_script == NULL)
            _exit(1);


        memcpy(
            bash_script,
            script,
            script_len
        );

        memcpy(
            bash_script + script_len,
            CATPKG_COMMAND_CHECK_METAINFO,
            check_len + 1
        );


        /*
         * script does not need to be freed here.
         *
         * Successful exec replaces the entire
         * process image.
         */


        execl(
            "/usr/bin/bash",
            "bash",
            "-c",
            bash_script,
            "catpkg-command",
            (char *)NULL
        );


        /*
         * execl() failed.
         */

        perror("execl");

        free(bash_script);

        _exit(127);
    }


    /*
     * --------------------------------------------------------
     * Parent
     * --------------------------------------------------------
     */

    close(fd[1]);


    char *command_name = NULL;
    char *command_description = NULL;
    char *command_declaration = NULL;

    char *command_symlink_path = NULL;
    char *command_data_path = NULL;
    char *commands_data_path = NULL;


    /*
     * --------------------------------------------------------
     * Read command name
     * --------------------------------------------------------
     */

    if (
        read_meta_command_field(
            fd[0],
            &command_name
        ) != 0
    ) {
        fprintf(
            stderr,
            "Error: failed to read name.\n"
        );

        close(fd[0]);

        waitpid(
            pid,
            NULL,
            0
        );

        free(script);

        return 1;
    }


    if (command_name[0] == '\0') {
        fprintf(
            stderr,
            "Error: \"get-meta-command-info name\" is empty.\n"
        );

        free(command_name);

        close(fd[0]);

        waitpid(
            pid,
            NULL,
            0
        );

        free(script);

        return 1;
    }


    catpkg_build_log(
        "? Command Name: %s\n",
        builder,
        true,
        command_name
    );


    /*
     * --------------------------------------------------------
     * Read command description
     * --------------------------------------------------------
     */

    if (
        read_meta_command_field(
            fd[0],
            &command_description
        ) != 0
    ) {
        fprintf(
            stderr,
            "Error: failed to read description.\n"
        );

        free(command_name);

        close(fd[0]);

        waitpid(
            pid,
            NULL,
            0
        );

        free(script);

        return 1;
    }


    if (command_description[0] == '\0') {
        catpkg_build_log(
            "? description: Not exist.\n",
            builder,
            true
        );
    }
    else {
        catpkg_build_log(
            "? description: %s\n",
            builder,
            true,
            command_description
        );
    }


    /*
     * --------------------------------------------------------
     * Read command declaration
     * --------------------------------------------------------
     */

    if (
        read_meta_command_field(
            fd[0],
            &command_declaration
        ) != 0
    ) {
        fprintf(
            stderr,
            "Error: failed to read declaration.\n"
        );

        free(command_name);
        free(command_description);

        close(fd[0]);

        waitpid(
            pid,
            NULL,
            0
        );

        free(script);

        return 1;
    }


    if (command_declaration[0] == '\0') {
        fprintf(
            stderr,
            "Error: \"get-meta-command-info declaration\" is empty.\n"
        );

        free(command_name);
        free(command_description);
        free(command_declaration);

        close(fd[0]);

        waitpid(
            pid,
            NULL,
            0
        );

        free(script);

        return 1;
    }


    catpkg_build_log(
        "? declaration: %s\n",
        builder,
        true,
        command_declaration
    );


    close(fd[0]);


    /*
     * --------------------------------------------------------
     * Check command process
     * --------------------------------------------------------
     */

    int status;

    if (
        waitpid(
            pid,
            &status,
            0
        ) == -1
    ) {
        perror("waitpid");

        free(command_name);
        free(command_description);
        free(command_declaration);

        free(script);

        return 1;
    }


    if (
        !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0
    ) {
        fprintf(
            stderr,
            "Error: command metadata process failed.\n"
        );

        free(command_name);
        free(command_description);
        free(command_declaration);

        free(script);

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Build command paths
     * --------------------------------------------------------
     */

    commands_data_path =
        make_catpkg_path(
            CATPKG_PACKAGE_DATA
            "/.catpkg/commands",
            ctpg_context->name_field->value
        );


    command_data_path =
        make_catpkg_path(
            CATPKG_PACKAGE_DATA
            "/.catpkg/commands/%s",
            ctpg_context->name_field->value,
            basename(request_val)
        );


    command_symlink_path =
        make_catpkg_path(
            CATPKG_PACKAGE_ROOT
            "/commands/%s",
            command_name
        );

    request -> context = calloc(1, strlen(command_symlink_path) + 1);
    strcpy(request -> context, command_symlink_path);

    if (
        commands_data_path == NULL ||
        command_data_path == NULL ||
        command_symlink_path == NULL
    ) {
        fprintf(
            stderr,
            "Error: failed to create command paths.\n"
        );

        free(commands_data_path);
        free(command_data_path);
        free(command_symlink_path);

        free(command_name);
        free(command_description);
        free(command_declaration);

        free(script);

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Create commands data directory
     * --------------------------------------------------------
     */

    if (
        mkdir(
            commands_data_path,
            0755
        ) != 0 &&
        errno != EEXIST
    ) {
        perror("mkdir");

        free(commands_data_path);
        free(command_data_path);
        free(command_symlink_path);

        free(command_name);
        free(command_description);
        free(command_declaration);

        free(script);

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Write command data
     * --------------------------------------------------------
     *
     * The command was already read directly from
     * the CTPG TAR entry, so there is no reason
     * to invoke tar again.
     */

    catpkg_build_log(
        "\nExtract command %s... \n",
        builder,
        true,
        command_name
    );


    FILE *command_file =
        fopen(
            command_data_path,
            "wb"
        );

    if (command_file == NULL) {
        perror("fopen");

        free(commands_data_path);
        free(command_data_path);
        free(command_symlink_path);

        free(command_name);
        free(command_description);
        free(command_declaration);

        free(script);

        return 1;
    }


    size_t script_size =
        strlen(script);


    if (
        script_size > 0 &&
        fwrite(
            script,
            1,
            script_size,
            command_file
        ) != script_size
    ) {
        perror("fwrite");

        fclose(command_file);

        free(commands_data_path);
        free(command_data_path);
        free(command_symlink_path);

        free(command_name);
        free(command_description);
        free(command_declaration);

        free(script);

        return 1;
    }


    if (fclose(command_file) != 0) {
        perror("fclose");

        free(commands_data_path);
        free(command_data_path);
        free(command_symlink_path);

        free(command_name);
        free(command_description);
        free(command_declaration);

        free(script);

        return 1;
    }


    /*
     * Command data has now been written.
     */

    free(script);
    script = NULL;


    /*
     * --------------------------------------------------------
     * Create symbolic link
     * --------------------------------------------------------
     *
     * command_symlink_path
     *          ↓
     * command_data_path
     */

    if (
        symlink(
            command_data_path,
            command_symlink_path
        ) != 0
    ) {
        catpkg_build_log(
            "symlink: File exists\n",
            builder,
            true
        );
    }


    catpkg_build_log(
        "The symbolic link \"%s\" "
        "has been successfully created.\n",
        builder,
        true,
        command_symlink_path
    );


    /*
     * --------------------------------------------------------
     * Cleanup
     * --------------------------------------------------------
     */

    free(commands_data_path);
    free(command_data_path);
    free(command_symlink_path);

    free(command_name);
    free(command_description);
    free(command_declaration);


    break;
}
case REMOVE_BATCH_OPEN:
        size_t close_batch_index = 0;
        bool found = false;

        for (size_t ai = i + 1; ai < builder->requests_size; ai++) {
            if (builder->requests[ai] != NULL
                && builder->requests[ai]->protocol == REMOVE_BATCH_CLOSE) {

                close_batch_index = ai;
                found = true;
                break;
            }
        }

        if (!found) {
            return 1;
        }

        size_t count_batch_protocols = close_batch_index - i - 1;
        struct CatpkgRequest **remove_batch_protocols = &builder->requests[i + 1];

        // Files - first
        size_t count_batch_files_protocols = 0;

        for (size_t ai = 0; ai < count_batch_protocols; ai++) {
            if (remove_batch_protocols[ai]->protocol != REMOVE_FILE || remove_batch_protocols[ai]->protocol != CTPG_REMOVE_FILE_OBSOLETE)
                continue;

            if (ai != count_batch_files_protocols) {
                struct CatpkgRequest *tmp =
                    remove_batch_protocols[count_batch_files_protocols];

                remove_batch_protocols[count_batch_files_protocols] =
                    remove_batch_protocols[ai];

                remove_batch_protocols[ai] = tmp;
            }

            count_batch_files_protocols++;
        }

        size_t count_batch_dirs_protocols =
            count_batch_protocols - count_batch_files_protocols;

        struct CatpkgRequest **remove_batch_dirs_protocols =
            &remove_batch_protocols[count_batch_files_protocols];

        for (size_t ai = 0; ai < count_batch_dirs_protocols; ai++) {
            size_t longest_index = ai;

            for (size_t aj = ai + 1;
                 aj < count_batch_dirs_protocols;
                 aj++) {

                const char *current_path =
                    (const char *)remove_batch_dirs_protocols[aj]->value;

                const char *longest_path =
                    (const char *)remove_batch_dirs_protocols[longest_index]->value;

                if (strlen(current_path) > strlen(longest_path)) {
                    longest_index = aj;
                }
            }

            if (longest_index != ai) {
                struct CatpkgRequest *tmp =
                    remove_batch_dirs_protocols[ai];

                remove_batch_dirs_protocols[ai] =
                    remove_batch_dirs_protocols[longest_index];

                remove_batch_dirs_protocols[longest_index] =
                    tmp;
            }
        }
        CATPKG_BUILD_LOG(
            "%s",
            "",
            0,
            "REMOVE_BATCH_OPEN"
        ); 
    break;

case REMOVE_BATCH_CLOSE:
    CATPKG_BUILD_LOG(
        "%s",
        "",
        0,
        "REMOVE_BATCH_CLOSE"
    );
    break;

case OPERATION_SECURE_CONTEXT:
{
    opsec_context = request->value;

    if (
        opsec_context == NULL ||
        opsec_context->file_info == NULL ||
        opsec_context->package_info == NULL
    ) {
        return 1;
    }

    const struct PackageField *name_field =
        PackageInfo_Find(
            opsec_context->package_info,
            "name"
        );

    if (
        name_field == NULL ||
        name_field->value == NULL
    ) {
        return 1;
    }


    /*
     * First global #required field.
     */

    const struct PackageField *first_required_field =
        PackageInfo_Find(
            opsec_context->file_info,
            "#required"
        );


    /*
     * Inspect all operations which have not
     * yet been applied.
     */

    for (
        size_t ai = builder->applied_count;
        ai < builder->requests_size;
        ai++
    ) {
        struct CatpkgRequest *checked_request =
            builder->requests[ai];

        char *request_val =
            checked_request->value;

        char *request_result =
            checked_request->result;

        /*
         * Indicates that the DATABASE object
         * corresponding to this operation has
         * already been found and processed.
         */

        int path_matched = 0;


        /*
         * Iterate through DATABASE objects for
         * the current operation.
         */

        const struct PackageField *path_field =
            PackageInfo_Find(
                opsec_context->file_info,
                "#path"
            );

        while (path_field != NULL) {
            if (
                strcmp(
                    path_field->name,
                    "#path"
                ) != 0
            ) {
                path_field =
                    path_field->next_field;

                continue;
            }

            if (path_field->value == NULL) {
                path_field =
                    path_field->next_field;

                continue;
            }


            /*
             * DATABASE section corresponding
             * to the current #path.
             */

            char section_entry[64];

            snprintf(
                section_entry,
                sizeof(section_entry),
                "%u",
                path_field->section
            );


            /*
             * Find ownership information for
             * the current DATABASE section.
             */

            const struct PackageField
                *section_package_required_field = NULL;

            const struct PackageField
                *section_first_required_field = NULL;

            const struct PackageField *option_field =
                first_required_field;

            while (option_field != NULL) {
                if (option_field->section > 0)
                    break;

                if (
                    strcmp(
                        option_field->name,
                        "#required"
                    ) == 0
                ) {
                    if (option_field->value == NULL) {
                        option_field =
                            option_field->next_field;

                        continue;
                    }

                    char *copy =
                        strdup(option_field->value);

                    if (copy == NULL)
                        return 1;

                    char *section =
                        strtok(copy, "=");

                    char *package_name =
                        strtok(NULL, "=");

                    if (
                        section == NULL ||
                        package_name == NULL
                    ) {
                        free(copy);
                        return 1;
                    }

                    if (
                        strcmp(
                            section_entry,
                            section
                        ) == 0
                    ) {
                        if (
                            section_first_required_field ==
                            NULL
                        ) {
                            section_first_required_field =
                                option_field;
                        }

                        if (
                            strcmp(
                                name_field->value,
                                package_name
                            ) == 0
                        ) {
                            section_package_required_field =
                                option_field;
                        }
                    }

                    free(copy);
                }

                option_field =
                    option_field->next_field;
            }


            /*
             * Verify the current operation
             * against this DATABASE object.
             *
             * OPSC.inc may set path_matched
             * when the corresponding object
             * has been found.
             */

            switch (
                checked_request->protocol
            ) {
                #include "OPSC.inc"
            }


            /*
             * The operation has already found
             * its DATABASE object. There is no
             * reason to inspect other paths.
             */

            if (path_matched)
                break;


            path_field =
                path_field->next_field;
        }


        /*
         * Protocol-specific handling of a missing
         * DATABASE object should now be performed
         * here or through a protocol-specific flag.
         */
    }


    CATPKG_BUILD_LOG(
        "%s",
        "OK",
        0,
        "OPSC_CONTEXT"
    );

    break;
}

case DATABASE_OBSOLETE_DEPEND:
{
    CATPKG_BUILD_LOG_TREE(0);

    catpkg_build_log(
        "DATABASE_OBSOLETE_DEPEND %s\n",
        builder,
        true,
        request_val
    );


    /*
     * Validate request value.
     */

    if (
        request_val == NULL ||
        request_val[0] == '\0'
    ) {
        return 1;
    }

    /*
     * DEPENDENCIES path.
     */
    const char *dependencies_path =
        CATPKG_DATABASE_DIR_PATH "/"
        CATPKG_DEPENDENCIES;

    /*
     * Open the original DEPENDENCIES file.
     */

    // if (
    //     catpkg_builder_cache_change(
    //         CATPKG_DATABASE_DIR_PATH
    //     ) != 0
    // ) {
    //     return 1;
    // }
    
    FILE *file =
        fopen(
            dependencies_path,
            "rb"
        );

    if (file == NULL) {
        perror(
            "catpkg: failed to open DEPENDENCIES"
        );

        return 1;
    }


    /*
     * Build:
     *
     *     #required <section>=
     *
     * Including '=' prevents section "9"
     * from matching "90", "91", etc.
     */

    int prefix_length =
        snprintf(
            NULL,
            0,
            "#required %s=",
            request_val
        );

    if (prefix_length < 0) {
        fclose(file);
        return 1;
    }

    char *prefix =
        malloc(
            (size_t)prefix_length + 1
        );

    if (prefix == NULL) {
        fclose(file);
        return 1;
    }

    snprintf(
        prefix,
        (size_t)prefix_length + 1,
        "#required %s=",
        request_val
    );


    /*
     * Build temporary file path:
     *
     *     DEPENDENCIES.tmp
     *
     * The temporary file is kept in the same
     * directory so rename() can replace the original
     * file without crossing filesystems.
     */

    int temporary_length =
        snprintf(
            NULL,
            0,
            "%s.tmp",
            dependencies_path
        );

    if (temporary_length < 0) {
        free(prefix);
        fclose(file);

        return 1;
    }

    char *temporary_path =
        malloc(
            (size_t)temporary_length + 1
        );

    if (temporary_path == NULL) {
        free(prefix);
        fclose(file);

        return 1;
    }

    snprintf(
        temporary_path,
        (size_t)temporary_length + 1,
        "%s.tmp",
        dependencies_path
    );


    /*
     * Create the new DEPENDENCIES file.
     */

    FILE *temporary =
        fopen(
            temporary_path,
            "wb"
        );

    if (temporary == NULL) {
        perror(
            "catpkg: failed to create temporary "
            "DEPENDENCIES"
        );

        free(temporary_path);
        free(prefix);

        fclose(file);

        return 1;
    }


    /*
     * Copy every line except:
     *
     *     #required <section>=...
     *
     * belonging to the requested dependency section.
     */

    char *line = NULL;
    size_t line_capacity = 0;

    ssize_t line_length;

    while (
        (
            line_length =
                getline(
                    &line,
                    &line_capacity,
                    file
                )
        ) != -1
    ) {
        bool remove_line =
            (size_t)line_length >=
                (size_t)prefix_length &&

            strncmp(
                line,
                prefix,
                (size_t)prefix_length
            ) == 0;


        /*
         * Skip ownership records belonging to this
         * dependency section.
         */

        if (remove_line)
            continue;


        /*
         * Preserve every other line exactly as it was.
         */

        if (
            fwrite(
                line,
                1,
                (size_t)line_length,
                temporary
            ) != (size_t)line_length
        ) {
            free(line);

            fclose(temporary);
            fclose(file);

            remove(temporary_path);

            free(temporary_path);
            free(prefix);

            return 1;
        }
    }


    /*
     * getline() returns -1 both on EOF and on error.
     *
     * EOF is expected.
     */

    if (ferror(file)) {
        free(line);

        fclose(temporary);
        fclose(file);

        remove(temporary_path);

        free(temporary_path);
        free(prefix);

        return 1;
    }

    free(line);


    /*
     * Mark the DATABASE section as obsolete.
     *
     * Result:
     *
     *     #obsolete <section>
     */

    if (
        fprintf(
            temporary,
            "#obsolete %s\n",
            request_val
        ) < 0
    ) {
        fclose(temporary);
        fclose(file);

        remove(temporary_path);

        free(temporary_path);
        free(prefix);

        return 1;
    }


    /*
     * Flush the temporary DEPENDENCIES file before
     * replacing the original.
     */

    if (fflush(temporary) != 0) {
        fclose(temporary);
        fclose(file);

        remove(temporary_path);

        free(temporary_path);
        free(prefix);

        return 1;
    }


    /*
     * Close the temporary file.
     */

    if (fclose(temporary) != 0) {
        fclose(file);

        remove(temporary_path);

        free(temporary_path);
        free(prefix);

        return 1;
    }


    /*
     * The original file is no longer needed.
     */

    if (fclose(file) != 0) {
        remove(temporary_path);

        free(temporary_path);
        free(prefix);

        return 1;
    }


    /*
     * Atomically replace:
     *
     *     DEPENDENCIES
     *
     * with:
     *
     *     DEPENDENCIES.tmp
     */

    if (
        rename(
            temporary_path,
            dependencies_path
        ) != 0
    ) {
        perror(
            "catpkg: failed to replace DEPENDENCIES"
        );

        remove(temporary_path);

        free(temporary_path);
        free(prefix);

        return 1;
    }


    /*
     * Cleanup.
     */

    free(temporary_path);
    free(prefix);

    break;
}

case CATPKG_MERGE_DATABASE:
{
    CATPKG_BUILD_LOG_TREE(0);

    catpkg_build_log(
        "CATPKG_MERGE_DATABASE \n",
        builder,
        true
    );


    /*
     * Get DEPENDENCIES path.
     */

    char request_dir[PATH_MAX];

    int request_dir_len =
        snprintf(
            request_dir,
            sizeof(request_dir),
            "%s",
            request_val
        );

    if (
        request_dir_len < 0 ||
        (size_t)request_dir_len >= sizeof(request_dir)
    ) {
        return 1;
    }

    char *depend_path =
        make_catpkg_path(
            "%s/%s",
            dirname(request_dir),
            CATPKG_DEPENDENCIES
        );

    if (depend_path == NULL)
        return 1;


    /*
     * Cache current database state.
     */

    if (
        catpkg_builder_cache_change(
            CATPKG_DATABASE_DIR_PATH
        ) != 0
    ) {
        free(depend_path);
        return 1;
    }


    /*
     * Nothing to merge if package database
     * does not exist.
     */

    if (
        access(
            CATPKG_PACKAGES_PATH,
            F_OK
        ) != 0
    ) {
        catpkg_build_log(
            "CANNOT MERGE PACKAGES! ( SKIP ) \n",
            builder,
            true
        );

        goto merge_sync;
    }


    /*
     * Create DATABASE if it does not exist.
     */

    if (
        access(
            request_val,
            F_OK
        ) != 0
    ) {
        FILE *file =
            fopen(
                request_val,
                "w"
            );

        if (file == NULL) {
            perror("fopen");
            free(depend_path);
            return 1;
        }

        if (
            fprintf(
                file,
                "CATPKG 1.1\n"
                "#catpkg-version 2.0.0-beta\n"
                "#use-options DEPENDENCIES\n"
            ) < 0
        ) {
            fclose(file);
            free(depend_path);
            return 1;
        }

        if (fclose(file) != 0) {
            perror("fclose");
            free(depend_path);
            return 1;
        }
    }


    /*
     * DEPENDENCIES must exist before CATPKG_Parse(),
     * because DATABASE imports it through:
     *
     *     #use-options DEPENDENCIES
     */

    if (
        access(
            depend_path,
            F_OK
        ) != 0
    ) {
        FILE *file =
            fopen(
                depend_path,
                "w"
            );

        if (file == NULL) {
            perror("fopen");
            free(depend_path);
            return 1;
        }

        if (fclose(file) != 0) {
            perror("fclose");
            free(depend_path);
            return 1;
        }
    }


    /*
     * Parse DATABASE once.
     *
     * Existing DEPENDENCIES fields are also imported.
     */

    struct PackageInfo *database_fileinfo =
        CATPKG_Parse(
            request_val
        );

    if (database_fileinfo == NULL) {
        free(depend_path);
        return 1;
    }


    /*
     * Open DATABASE and DEPENDENCIES for append.
     */

    FILE *database_file =
        fopen(
            request_val,
            "a"
        );

    if (database_file == NULL) {
        perror("fopen");

        PackageInfo_Free(
            database_fileinfo
        );

        free(depend_path);

        return 1;
    }

    FILE *dependencies_file =
        fopen(
            depend_path,
            "a"
        );

    if (dependencies_file == NULL) {
        perror("fopen");

        fclose(
            database_file
        );

        PackageInfo_Free(
            database_fileinfo
        );

        free(depend_path);

        return 1;
    }


    /*
     * DATABASE entries created during this merge.
     */

    struct MergeDatabaseEntry *added_entries =
        NULL;

    size_t added_entries_count =
        0;


    /*
     * #required records created during this merge.
     */

    struct MergeDatabaseDependency *added_dependencies =
        NULL;

    size_t added_dependencies_count =
        0;


    /*
     * Find the last DATABASE section.
     */

    size_t last_database_section =
        0;

    const struct PackageField *database_field =
        PackageInfo_Find(
            database_fileinfo,
            "info"
        );

    while (database_field != NULL) {
        if (
            database_field->name != NULL &&
            strcmp(
                database_field->name,
                "info"
            ) == 0 &&
            database_field->section >
                last_database_section
        ) {
            last_database_section =
                database_field->section;
        }

        database_field =
            database_field->next_field;
    }


    /*
     * Open installed packages directory.
     */

    DIR *dir =
        opendir(
            CATPKG_PACKAGES_PATH
        );

    if (dir == NULL) {
        perror("opendir");

        fclose(
            dependencies_file
        );

        fclose(
            database_file
        );

        PackageInfo_Free(
            database_fileinfo
        );

        free(
            depend_path
        );

        return 1;
    }


    struct dirent *entry;

    size_t packages_count =
        catpkg_directory_count(
            CATPKG_PACKAGES_PATH
        );

    size_t package_count =
        0;


    /*
     * Scan installed packages.
     */

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


        /*
         * Build metadata paths.
         */

        char *package_fileinfo_path =
            make_catpkg_path(
                CATPKG_FILES_PATH "/"
                CATPKG_FILE,
                entry->d_name
            );

        char *package_info_path =
            make_catpkg_path(
                CATPKG_METADATA_PATH "/"
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

            goto merge_error;
        }


        /*
         * Parse package metadata.
         */

        struct PackageInfo *package_info =
            CATPKG_Parse(
                package_info_path
            );

        struct PackageInfo *package_fileinfo =
            CATPKG_Parse(
                package_fileinfo_path
            );

        free(
            package_fileinfo_path
        );

        free(
            package_info_path
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

            goto merge_error;
        }


        const struct PackageField *name_field =
            PackageInfo_Find(
                package_info,
                "name"
            );

        if (
            name_field == NULL ||
            name_field->value == NULL ||
            name_field->value[0] == '\0'
        ) {
            PackageInfo_Free(
                package_fileinfo
            );

            PackageInfo_Free(
                package_info
            );

            goto merge_error;
        }


        /*
         * ------------------------------------------------
         * FILEINFO
         * ------------------------------------------------
         */

        const struct PackageField *info_field =
            PackageInfo_Find(
                package_fileinfo,
                "info"
            );

        while (info_field != NULL) {

            if (
                info_field->name == NULL ||
                strcmp(
                    info_field->name,
                    "info"
                ) != 0
            ) {
                info_field =
                    info_field->next_field;

                continue;
            }


            /*
             * Read #type and #path from this section.
             */

            const char *package_type =
                NULL;

            const char *package_path =
                NULL;

            const struct PackageField *package_field =
                package_fileinfo->fields;

            while (package_field != NULL) {

                if (
                    package_field->section ==
                        info_field->section &&
                    package_field->name != NULL
                ) {
                    if (
                        strcmp(
                            package_field->name,
                            "#type"
                        ) == 0
                    ) {
                        package_type =
                            package_field->value;
                    }

                    else if (
                        strcmp(
                            package_field->name,
                            "#path"
                        ) == 0
                    ) {
                        package_path =
                            package_field->value;
                    }
                }

                package_field =
                    package_field->next_field;
            }


            if (
                package_type == NULL ||
                package_path == NULL
            ) {
                info_field =
                    info_field->next_field;

                continue;
            }


            /*
             * Get/create DATABASE object and collect
             * all existing parent directory sections.
             *
             * parent_sections belongs exclusively to
             * this iteration.
             */

            size_t database_section =
                0;

            size_t *parent_sections =
                NULL;

            size_t parent_sections_count =
                0;

            if (
                catpkg_merge_database_get_or_create_entry(
                    database_fileinfo,
                    database_file,

                    &added_entries,
                    &added_entries_count,

                    &last_database_section,

                    package_type,
                    package_path,

                    &database_section,

                    &parent_sections,
                    &parent_sections_count
                ) != 0
            ) {
                /*
                 * get_or_create_entry() guarantees that
                 * parent_sections is NULL on error.
                 */

                PackageInfo_Free(
                    package_fileinfo
                );

                PackageInfo_Free(
                    package_info
                );

                goto merge_error;
            }


            /*
             * DATABASE sections start at 1.
             *
             * DEPENDENCIES indexes DATABASE objects
             * starting at 0.
             */

            if (database_section == 0) {
                free(
                    parent_sections
                );

                PackageInfo_Free(
                    package_fileinfo
                );

                PackageInfo_Free(
                    package_info
                );

                goto merge_error;
            }


            /*
             * Require the object itself.
             */

            if (
                catpkg_merge_database_add_required(
                    database_fileinfo,
                    dependencies_file,

                    &added_dependencies,
                    &added_dependencies_count,

                    database_section - 1,
                    name_field->value
                ) != 0
            ) {
                free(
                    parent_sections
                );

                PackageInfo_Free(
                    package_fileinfo
                );

                PackageInfo_Free(
                    package_info
                );

                goto merge_error;
            }


            /*
             * Require every registered parent directory.
             */

            bool parent_error =
                false;

            for (
                size_t i = 0;
                i < parent_sections_count;
                i++
            ) {
                if (parent_sections[i] == 0) {
                    parent_error =
                        true;

                    break;
                }

                if (
                    catpkg_merge_database_add_required(
                        database_fileinfo,
                        dependencies_file,

                        &added_dependencies,
                        &added_dependencies_count,

                        parent_sections[i] - 1,
                        name_field->value
                    ) != 0
                ) {
                    parent_error =
                        true;

                    break;
                }
            }


            /*
             * parent_sections must never survive this
             * FileInfo iteration.
             */

            free(
                parent_sections
            );

            parent_sections =
                NULL;


            if (parent_error) {
                PackageInfo_Free(
                    package_fileinfo
                );

                PackageInfo_Free(
                    package_info
                );

                goto merge_error;
            }


            info_field =
                info_field->next_field;
        }


        /*
         * ------------------------------------------------
         * #persistent-file
         * ------------------------------------------------
         *
         * A persistent file may not be present in
         * FILEINFO, therefore process it separately.
         */

        const struct PackageField *persistent_file_field =
            PackageInfo_Find(
                package_info,
                "#persistent-file"
            );

        while (
            persistent_file_field != NULL
        ) {
            if (
                persistent_file_field->name == NULL ||
                strcmp(
                    persistent_file_field->name,
                    "#persistent-file"
                ) != 0
            ) {
                persistent_file_field =
                    persistent_file_field->next_field;

                continue;
            }

            if (
                persistent_file_field->value == NULL ||
                persistent_file_field->value[0] == '\0'
            ) {
                persistent_file_field =
                    persistent_file_field->next_field;

                continue;
            }


            /*
             * #persistent-file describes a file.
             */

            size_t database_section =
                0;

            size_t *parent_sections =
                NULL;

            size_t parent_sections_count =
                0;

            if (
                catpkg_merge_database_get_or_create_entry(
                    database_fileinfo,
                    database_file,

                    &added_entries,
                    &added_entries_count,

                    &last_database_section,

                    "file",
                    persistent_file_field->value,

                    &database_section,

                    &parent_sections,
                    &parent_sections_count
                ) != 0
            ) {
                PackageInfo_Free(
                    package_fileinfo
                );

                PackageInfo_Free(
                    package_info
                );

                goto merge_error;
            }


            if (database_section == 0) {
                free(
                    parent_sections
                );

                PackageInfo_Free(
                    package_fileinfo
                );

                PackageInfo_Free(
                    package_info
                );

                goto merge_error;
            }


            /*
             * Require persistent file itself.
             */

            if (
                catpkg_merge_database_add_required(
                    database_fileinfo,
                    dependencies_file,

                    &added_dependencies,
                    &added_dependencies_count,

                    database_section - 1,
                    name_field->value
                ) != 0
            ) {
                free(
                    parent_sections
                );

                PackageInfo_Free(
                    package_fileinfo
                );

                PackageInfo_Free(
                    package_info
                );

                goto merge_error;
            }


            /*
             * Require its registered parent directories.
             */

            bool parent_error =
                false;

            for (
                size_t i = 0;
                i < parent_sections_count;
                i++
            ) {
                if (parent_sections[i] == 0) {
                    parent_error =
                        true;

                    break;
                }

                if (
                    catpkg_merge_database_add_required(
                        database_fileinfo,
                        dependencies_file,

                        &added_dependencies,
                        &added_dependencies_count,

                        parent_sections[i] - 1,
                        name_field->value
                    ) != 0
                ) {
                    parent_error =
                        true;

                    break;
                }
            }


            free(
                parent_sections
            );

            parent_sections =
                NULL;


            if (parent_error) {
                PackageInfo_Free(
                    package_fileinfo
                );

                PackageInfo_Free(
                    package_info
                );

                goto merge_error;
            }


            /*
             * Add persistence policy.
             *
             * TODO:
             * This should eventually use the same
             * duplicate protection as #required.
             */

            if (
                fprintf(
                    dependencies_file,
                    "#persistent %zu=%s\n",
                    database_section - 1,
                    name_field->value
                ) < 0
            ) {
                PackageInfo_Free(
                    package_fileinfo
                );

                PackageInfo_Free(
                    package_info
                );

                goto merge_error;
            }


            persistent_file_field =
                persistent_file_field->next_field;
        }


        /*
         * Current package finished.
         */

        PackageInfo_Free(
            package_fileinfo
        );

        PackageInfo_Free(
            package_info
        );


        package_count++;

        builder_progress_bar(
            package_count,
            packages_count
        );
    }


    /*
     * ------------------------------------------------
     * Successful merge cleanup.
     * ------------------------------------------------
     */

    for (
        size_t i = 0;
        i < added_entries_count;
        i++
    ) {
        free(
            added_entries[i].type
        );

        free(
            added_entries[i].path
        );
    }

    free(
        added_entries
    );

    added_entries =
        NULL;


    for (
        size_t i = 0;
        i < added_dependencies_count;
        i++
    ) {
        free(
            added_dependencies[i].package
        );
    }

    free(
        added_dependencies
    );

    added_dependencies =
        NULL;


    closedir(
        dir
    );

    fclose(
        dependencies_file
    );

    fclose(
        database_file
    );

    PackageInfo_Free(
        database_fileinfo
    );


merge_sync:

    /*
     * Remove obsolete DATABASE records and remap
     * DEPENDENCIES section indexes.
     */

    catpkg_build_log(
        "Database synchronization...",
        builder,
        true
    );

    if (
        catpkg_database_sync_obsolete(
            request_val,
            depend_path
        ) != 0
    ) {
        free(
            depend_path
        );

        return 1;
    }

    free(
        depend_path
    );

    catpkg_build_log(
        "|       └── OK\n",
        builder,
        true
    );

    break;


merge_error:

    /*
     * ------------------------------------------------
     * Error cleanup.
     * ------------------------------------------------
     *
     * Per-entry parent_sections does not exist here:
     * every live instance is freed before jumping here.
     */


    /*
     * Free DATABASE entries created during this merge.
     */

    for (
        size_t i = 0;
        i < added_entries_count;
        i++
    ) {
        free(
            added_entries[i].type
        );

        free(
            added_entries[i].path
        );
    }

    free(
        added_entries
    );


    /*
     * Free #required records created during this merge.
     */

    for (
        size_t i = 0;
        i < added_dependencies_count;
        i++
    ) {
        free(
            added_dependencies[i].package
        );
    }

    free(
        added_dependencies
    );


    closedir(
        dir
    );

    fclose(
        dependencies_file
    );

    fclose(
        database_file
    );

    PackageInfo_Free(
        database_fileinfo
    );

    free(
        depend_path
    );

    return 1;
}

/*
 * ============================================================
 * MOVE_FILE
 * ============================================================
 *
 * value:
 *
 * source\0destination\0
 */

case MOVE_FILE: {

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_TREE(0);
        CATPKG_BUILD_LOG_PATH_ERROR("MOVE_FILE", "???");
        return 1;
    }

    char *source =
        request_val;

    char *destination =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(0);

    if (catpkg_move_file(
            source,
            destination
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "MOVE_FILE",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "MOVE_FILE",
        source,
        destination
    );

    break;
}


/*
 * ============================================================
 * MOVE_DIR
 * ============================================================
 */

case MOVE_DIR: {

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_TREE(0);
        CATPKG_BUILD_LOG_PATH_ERROR("MOVE_DIR", "???");
        return 1;
    }

    char *source =
        request_val;

    char *destination =
        request_val +
        strlen(request_val) + 1;

    CATPKG_BUILD_LOG_TREE(0);

    if (catpkg_move_directory(
            source,
            destination
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "MOVE_DIR",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "MOVE_DIR",
        source,
        destination
    );

    break;
}


/*
 * ============================================================
 * REMOVE_DIR / CTPG_REMOVE_DIR_OBSOLETE
 * ============================================================
 */

case REMOVE_DIR:
case CTPG_REMOVE_DIR_OBSOLETE:
{
    CATPKG_BUILD_LOG_TREE(0);

    const char *protocol_name =
        request->protocol == CTPG_REMOVE_DIR_OBSOLETE
            ? "CTPG_REMOVE_DIR_OBSOLETE"
            : "REMOVE_DIR";

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            "???"
        );

        return 1;
    }

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

        if (is_obsolete < 0) {
            CATPKG_BUILD_LOG_PATH_ERROR(
                protocol_name,
                request_val
            );

            return 1;
        }

        if (is_obsolete == 0) {
            CATPKG_BUILD_LOG(
                "CTPG_REMOVE_DIR_OBSOLETE: \"%s\" (It exists in the package.)",
                "NOT OBSOLETE",
                0,
                request_val
            );

            break;
        }
    }

    if (
        catpkg_builder_cache_change(
            request_val
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            request_val
        );

        return 1;
    }

    if (rmdir(request_val) != 0) {
        fprintf(
            stderr,
            "catpkg: rmdir \"%s\": ",
            request_val
        );

        perror(NULL);

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

/*
 * ============================================================
 * REMOVE_FILE / CTPG_REMOVE_FILE_OBSOLETE
 * ============================================================
 */

case REMOVE_FILE:
case CTPG_REMOVE_FILE_OBSOLETE:
{
    const char *protocol_name =
        request->protocol == CTPG_REMOVE_FILE_OBSOLETE
            ? "CTPG_REMOVE_FILE_OBSOLETE"
            : "REMOVE_FILE";

    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            "???"
        );

        return 1;
    }


    /*
     * CTPG_REMOVE_FILE_OBSOLETE:
     *
     * Remove the file only if it is obsolete.
     *
     * A file is NOT obsolete when:
     *
     * - it exists in the currently opened package payload;
     * - it is declared by #persistent-file.
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

        if (is_obsolete < 0) {
            CATPKG_BUILD_LOG_PATH_ERROR(
                protocol_name,
                request_val
            );

            return 1;
        }

        if (is_obsolete == 0) {
            CATPKG_BUILD_LOG(
                "%s",
                "NOT OBSOLETE",
                0,
                "CTPG_REMOVE_FILE_OBSOLETE: It exists in the package or is persistent."
            );

            break;
        }
    }


    /*
     * Common REMOVE_FILE implementation.
     *
     * REMOVE_FILE:
     *     always reaches this point.
     *
     * CTPG_REMOVE_FILE_OBSOLETE:
     *     reaches this point only when
     *     the file is obsolete.
     */

    if (
        catpkg_builder_cache_change(
            request_val
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            request_val
        );

        return 1;
    }

    if (unlink(request_val) != 0) {
        fprintf(
            stderr,
            "catpkg: unlink \"%s\": ",
            request_val
        );

        perror(NULL);

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

/*
 * ============================================================
 * REMOVE
 * ============================================================
 */

case REMOVE:

    CATPKG_BUILD_LOG_TREE(0);

    /*
     * TODO
     */

    CATPKG_BUILD_LOG(
        "%s",
        "TODO",
        0,
        "REMOVE"
    );

    break;


/*
 * ============================================================
 * BUILD_LOG
 * ============================================================
 */

case BUILD_LOG:
    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "BUILD_LOG",
            "???"
        );

        return 1;
    }

    catpkg_build_log("%s\n", builder, true, request_val);
    break;

/*
 * ============================================================
 * BUILD_LOG_SEP
 * ============================================================
 */

case BUILD_LOG_SEP:
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
        return 1;

    size_t width = ws.ws_col;

    size_t title_length = strlen(request_val);

    /*
     * "\n" + '-' + "**" + request_val + "**" + '-' + "\n"
     */
    size_t prefix_size = 4 + title_length;

    if (width <= prefix_size)
        return 1;

    size_t dash_count = width - prefix_size;

    size_t left_dashes = dash_count / 2;
    size_t right_dashes = dash_count - left_dashes;

    size_t buffer_size =
        1 +
        left_dashes +
        2 +
        title_length +
        2 +
        right_dashes +
        1 +
        1;

    char *buffer = malloc(buffer_size);

    if (buffer == NULL)
        return 1;

    char *ptr = buffer;

    *ptr++ = '|';
    *ptr++ = '\n';

    memset(ptr, '-', left_dashes);
    ptr += left_dashes;

    *ptr++ = '*';
    *ptr++ = '*';

    memcpy(ptr, request_val, title_length);
    ptr += title_length;

    *ptr++ = '*';
    *ptr++ = '*';

    memset(ptr, '-', right_dashes);
    ptr += right_dashes;

    *ptr++ = '\n';
    *ptr = '\0';

    catpkg_build_log(buffer, builder, true);

    free(buffer);

    break;

/*
 * ============================================================
 * MKDIR
 * ============================================================
 */

case MKDIR:

    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "MKDIR",
            "???"
        );

        return 1;
    }

    if (mkdir(
            request_val,
            0755
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: mkdir \"%s\": ",
            request_val
        );

        perror(NULL);

        CATPKG_BUILD_LOG_PATH_ERROR(
            "MKDIR",
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "MKDIR",
        request_val
    );

    break;


/*
 * ============================================================
 * MKDIR_SOFT
 * ============================================================
 */

case MKDIR_SOFT:
{
    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "MKDIR_SOFT",
            "???"
        );

        return 1;
    }

    CATPKG_BUILD_LOG(
        "MKDIR_SOFT_START: \"%s\"",
        "",
        0,
        request_val
    );


    /*
     * Context is a single allocation.
     *
     * Layout:
     *
     * [MkdirSoftContext]
     * [path1\0]
     * [path2\0]
     * [...]
     *
     * Therefore the whole context can always be
     * released with:
     *
     * free(request->context);
     */

    struct MkdirSoftContext *context =
        calloc(
            1,
            sizeof(*context)
        );

    if (context == NULL) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "MKDIR_SOFT: calloc"
        );

        return 1;
    }


    /*
     * Store the context immediately.
     *
     * realloc() may change its address later, so
     * request->context must be updated after every
     * successful realloc().
     */

    request->context =
        context;


    char path[PATH_MAX];

    int path_length =
        snprintf(
            path,
            sizeof(path),
            "%s",
            request_val
        );

    if (
        path_length < 0 ||
        path_length >= (int)sizeof(path)
    ) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "MKDIR_SOFT: path too long"
        );

        goto mkdir_soft_error;
    }


    char *p = path;

    if (*p == '/')
        p++;


    for (;;) {

        char *slash =
            strchr(p, '/');

        if (slash != NULL)
            *slash = '\0';


        struct stat st;

        if (stat(path, &st) != 0) {

            if (errno != ENOENT) {

                fprintf(
                    stderr,
                    "catpkg: stat \"%s\": ",
                    path
                );

                perror(NULL);

                CATPKG_BUILD_LOG_PATH_ERROR(
                    "MKDIR",
                    path
                );

                goto mkdir_soft_error;
            }


            /*
             * Directory does not exist.
             * Create it.
             */

            if (mkdir(path, 0755) != 0) {

                fprintf(
                    stderr,
                    "catpkg: mkdir \"%s\": ",
                    path
                );

                perror(NULL);

                CATPKG_BUILD_LOG_PATH_ERROR(
                    "MKDIR",
                    path
                );

                goto mkdir_soft_error;
            }


            /*
             * The directory now belongs to this
             * MKDIR_SOFT operation.
             *
             * Save its path directly inside the
             * context allocation.
             */

            size_t path_size =
                strlen(path) + 1;

            /*
             * Overflow protection.
             */

            if (
                context->data_size >
                SIZE_MAX -
                    sizeof(*context) -
                    path_size
            ) {
                CATPKG_BUILD_LOG(
                    "%s",
                    "ERROR",
                    0,
                    "MKDIR_SOFT: context size overflow"
                );

                goto mkdir_soft_error;
            }

            size_t new_context_size =
                sizeof(*context) +
                context->data_size +
                path_size;


            struct MkdirSoftContext *tmp =
                realloc(
                    context,
                    new_context_size
                );

            if (tmp == NULL) {

                CATPKG_BUILD_LOG(
                    "%s",
                    "ERROR",
                    0,
                    "MKDIR_SOFT: realloc"
                );

                goto mkdir_soft_error;
            }


            /*
             * realloc() may have moved the block.
             */

            context = tmp;

            request->context =
                context;


            /*
             * Append:
             *
             * path\0
             */

            memcpy(
                context->data +
                    context->data_size,
                path,
                path_size
            );

            context->data_size +=
                path_size;

            context->created_paths_size++;


            CATPKG_BUILD_LOG_PATH_OK(
                "MKDIR",
                path
            );
        }
        else if (!S_ISDIR(st.st_mode)) {

            fprintf(
                stderr,
                "catpkg: \"%s\" is not a directory\n",
                path
            );

            CATPKG_BUILD_LOG_PATH_ERROR(
                "MKDIR",
                path
            );

            goto mkdir_soft_error;
        }


        if (slash == NULL)
            break;


        /*
         * Restore slash removed for the current
         * path component.
         */

        *slash = '/';

        p = slash + 1;

        while (*p == '/')
            p++;
    }


    if (context->created_paths_size == 0) {
        CATPKG_BUILD_LOG(
            "%s",
            "EXIST",
            0,
            "MKDIR_SOFT_END"
        );
    }
    else {
        CATPKG_BUILD_LOG(
            "%s",
            "OK",
            0,
            "MKDIR_SOFT_END"
        );
    }

    break;


mkdir_soft_error:

    /*
     * Do NOT free context here.
     *
     * Directories may already have been created.
     * The context contains all paths successfully
     * created before the error and is preserved
     * for REVERT.
     *
     * The entire context is a single allocation,
     * therefore its final cleanup requires only:
     *
     * free(request->context);
     */

    request->context =
        context;

    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "%s",
        "ERROR",
        0,
        "MKDIR_SOFT_END"
    );

    return 1;
}


/*
 * ============================================================
 * MKFILE
 * ============================================================
 */

case MKFILE:

    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "%s",
        "TODO",
        0,
        "MKFILE"
    );

    break;


/*
 * ============================================================
 * SYMLINK
 * ============================================================
 *
 * value:
 *
 * target\0link_path\0
 */

case SYMLINK: {

    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "SYMLINK",
            "???",
            "???"
        );

        return 1;
    }

    char *target =
        request_val;

    char *link_path =
        request_val +
        strlen(request_val) + 1;

    if (symlink(
            target,
            link_path
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: symlink \"%s\": ",
            link_path
        );

        perror(NULL);

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "SYMLINK",
            target,
            link_path
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "SYMLINK",
        target,
        link_path
    );

    break;
}


/*
 * ============================================================
 * SU
 * ============================================================
 */

case SU: {

    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {

        CATPKG_BUILD_LOG_PATH_ERROR(
            "SU",
            "???"
        );

        return 1;
    }

    uid_t uid;

    if (catpkg_parse_uid(
            request_val,
            &uid
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: invalid UID \"%s\"\n",
            request_val
        );

        CATPKG_BUILD_LOG_PATH_ERROR(
            "SU",
            request_val
        );

        return 1;
    }

    request->previous_euid =
        geteuid();

    if (seteuid(uid) != 0) {

        fprintf(
            stderr,
            "catpkg: seteuid(%u): ",
            (unsigned int)uid
        );

        perror(NULL);

        CATPKG_BUILD_LOG_PATH_ERROR(
            "SU",
            request_val
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "SU",
        request_val
    );

    break;
}


/*
 * ============================================================
 * BUILD_LOG_START
 * ============================================================
 */

case BUILD_LOG_START:

    build_log_active = 1;

    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "BUILD_LOG_START",
        "",
        0,
        ""
    );

    break;


/*
 * ============================================================
 * BUILD_LOG_END
 * ============================================================
 */

case BUILD_LOG_END:

    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "BUILD_LOG_END",
        "",
        0,
        ""
    );

    build_log_active = 0;

    break;


/*
 * ============================================================
 * CTPG_OPEN
 * ============================================================
 */

case CTPG_OPEN:

    CATPKG_BUILD_LOG_TREE(0);

    if (
        request_val == NULL ||
        ctpg_context_init(
            &ctpg_context,
            request_val
        ) != 0
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            "CTPG_OPEN",
            request_val != NULL
                ? request_val
                : "???"
        );

        return 1;
    }

    CATPKG_BUILD_LOG_PATH_OK(
        "CTPG_OPEN",
        request_val
    );

    break;


/*
 * ============================================================
 * CTPG_EXTRACT_PAYLOAD
 * ============================================================
 */

case CTPG_EXTRACT_PAYLOAD:
{
    CATPKG_BUILD_LOG_TREE(0);    

    if (
        ctpg_context == NULL ||
        ctpg_context->path == NULL
    ) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_EXTRACT_PAYLOAD: invalid context"
        );

        return 1;
    }

    char **excludes = NULL;
    size_t excludes_size = 0;

    const struct PackageField *option_field =
        ctpg_context->command_field;

    char **tmp = (char **) realloc(excludes, ++excludes_size * sizeof(*excludes));
    if (tmp == NULL) {
        printf("Memory Reallocation Failed\n");
        return 1;
    }
    else
        excludes = tmp;
    
    excludes[excludes_size - 1] = CATPKG_PACKAGEINFO;

    while(option_field != NULL) {
        if(strcmp(option_field -> name, "#command") == 0) {
            tmp = (char **) realloc(excludes, ++excludes_size * sizeof(*excludes));
            if (tmp == NULL){
                printf("Memory Reallocation Failed\n");
                return 1;
            }
            else
                excludes = tmp;

            excludes[excludes_size - 1] = option_field -> value;
        }

        option_field = option_field->next_field;
    }

    tmp = (char **) realloc(excludes, ++excludes_size * sizeof(*excludes));
    if (tmp == NULL) {
        printf("Memory Reallocation Failed\n");
        return 1;
    }
    else
        excludes = tmp;

    excludes[excludes_size - 1] = NULL;

    if (catpkg_builder_catpackage_extract(
            ctpg_context,
            "/",
            "/",
            excludes
        ) != 0) {
        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_EXTRACT_PAYLOAD"
        );

        return 1;
    }

    free(excludes);

    CATPKG_BUILD_LOG(
        "%s",
        "OK",
        0,
        "CTPG_EXTRACT_PAYLOAD"
    );

    break;
}

#define CATPKG_REWRITE_FIELD(field, new_value)                          \
    do {                                                                \
        if ((field) == NULL ||                                          \
            (field)->value == NULL ||                                   \
            (new_value) == NULL)                                        \
            goto rewrite_error;                                         \
                                                                        \
        size_t _old_size = strlen((field)->value);                      \
        size_t _new_size = strlen((new_value));                         \
                                                                        \
        /*                                                              \
         * We cannot overwrite the following bytes of PACKAGEINFO.      \
         */                                                             \
        if (_new_size > _old_size)                                      \
            goto rewrite_error;                                         \
                                                                        \
        if (fseek(                                                       \
                file,                                                   \
                (field)->bytepos + byteheader,                          \
                SEEK_SET                                                \
            ) != 0) {                                                   \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
        /* Clear the old value. */                                      \
                                                                        \
        for (size_t _i = 0; _i < _old_size; _i++) {                    \
            if (fputc(' ', file) == EOF)                                \
                goto rewrite_error;                                     \
        }                                                               \
                                                                        \
        /* Return to the beginning of the value. */                     \
                                                                        \
        if (fseek(                                                       \
                file,                                                   \
                (field)->bytepos + byteheader,                          \
                SEEK_SET                                                \
            ) != 0) {                                                   \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
        /* Write the new value. */                                      \
                                                                        \
        if (fwrite(                                                      \
                (new_value),                                            \
                1,                                                      \
                _new_size,                                              \
                file                                                    \
            ) != _new_size) {                                           \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
    } while (0)


#define CATPKG_REWRITE_OPTION_FIELD(field, new_name, new_value)          \
    do {                                                                 \
        if ((field) == NULL ||                                          \
            (field)->name == NULL ||                                    \
            (field)->value == NULL ||                                   \
            (new_name) == NULL ||                                       \
            (new_value) == NULL)                                        \
            goto rewrite_error;                                         \
                                                                        \
        size_t _old_size =                                               \
            strlen((field)->name) +                                     \
            1 +                                                         \
            strlen((field)->value);                                     \
                                                                        \
        size_t _new_size =                                               \
            strlen((new_name)) +                                        \
            1 +                                                         \
            strlen((new_value));                                        \
                                                                        \
        /*                                                              \
         * The new option line must fit into the old option line.        \
         */                                                             \
        if (_new_size > _old_size)                                      \
            goto rewrite_error;                                         \
                                                                        \
        if (fseek(                                                       \
                file,                                                   \
                (field)->bytepos + byteheader,                          \
                SEEK_SET                                                \
            ) != 0) {                                                   \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
        /* Clear the old option line. */                                \
                                                                        \
        for (size_t _i = 0; _i < _old_size; _i++) {                    \
            if (fputc(' ', file) == EOF)                                \
                goto rewrite_error;                                     \
        }                                                               \
                                                                        \
        /* Return to the beginning of the option line. */               \
                                                                        \
        if (fseek(                                                       \
                file,                                                   \
                (field)->bytepos + byteheader,                          \
                SEEK_SET                                                \
            ) != 0) {                                                   \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
        /* Write: #option value */                                      \
                                                                        \
        if (fprintf(                                                     \
                file,                                                   \
                "%s %s",                                                \
                (new_name),                                             \
                (new_value)                                             \
            ) < 0) {                                                    \
            goto rewrite_error;                                         \
        }                                                               \
                                                                        \
    } while (0)


#define CATPKG_CLEAR_OPTION_FIELD(field)                                 \
    do {                                                                 \
        if ((field) == NULL ||                                           \
            (field)->name == NULL ||                                     \
            (field)->value == NULL)                                      \
            goto rewrite_error;                                          \
                                                                         \
        size_t _old_size =                                                \
            strlen((field)->name) +                                      \
            1 +                                                          \
            strlen((field)->value);                                      \
                                                                         \
        if (fseek(                                                        \
                file,                                                    \
                (field)->bytepos,                                        \
                SEEK_SET                                                 \
            ) != 0) {                                                    \
            goto rewrite_error;                                          \
        }                                                                \
                                                                         \
        for (size_t _i = 0; _i < _old_size; _i++) {                     \
            if (fputc(' ', file) == EOF)                                 \
                goto rewrite_error;                                      \
        }                                                                \
                                                                         \
    } while (0)


#define FIELD_IS(field, field_name, field_value)                         \
    ((field) != NULL &&                                                 \
     (field)->name != NULL &&                                           \
     (field)->value != NULL &&                                          \
     strcmp((field)->name, (field_name)) == 0 &&                        \
     strcmp((field)->value, (field_value)) == 0)


#define FIELD_NAME(field, field_name)                                    \
    ((field) != NULL &&                                                 \
     (field)->name != NULL &&                                           \
     strcmp((field)->name, (field_name)) == 0)


case CTPG_UPDATE_PACKAGEINFO:
{
    char **args = (char **)request->value;

    const char *protocol_name =
        "CTPG_UPDATE_PACKAGEINFO";

    if (
        args == NULL ||
        args[0] == NULL ||
        args[1] == NULL
    ) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            "???"
        );

        return 1;
    }

    // Parse copy old PackageInfo to new.

    FILE *src = fopen(args[0], "rb");
    FILE *dst = fopen(args[1], "wb");

    char buffer[4096];
    size_t size;

    while ((size = fread(buffer, 1, sizeof(buffer), src)) > 0)
        fwrite(buffer, 1, size, dst);

    fclose(src);
    fclose(dst);

    struct PackageInfo *removed_info =
        CATPKG_Parse(args[0]);

    if (removed_info == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            args[0]
        );

        return 1;
    }

    // Parse PackageInfo

    struct PackageInfo *info =
        CATPKG_Parse(args[1]);

    if (info == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            args[1]
        );

        PackageInfo_Free(removed_info);

        return 1;
    }

    FILE *file =
        fopen(args[1], "r+");

    if (file == NULL) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            args[1]
        );

        PackageInfo_Free(info);
        PackageInfo_Free(removed_info);

        return 1;
    }

    long byteheader = 11;

    const struct PackageField *current_field =
        info->fields;

    while (current_field != NULL) {

        if (FIELD_NAME(
                current_field,
                "version"
            )) {

                        if (
                ctpg_context == NULL ||
                ctpg_context->version_field == NULL ||
                ctpg_context->version_field->value == NULL
            ) {
                goto rewrite_error;
            }

            CATPKG_REWRITE_FIELD(
                current_field,
                ctpg_context->version_field->value
            );
        }

        else if (FIELD_NAME(
                     current_field,
                     "description"
                 )) {
                        if (
                ctpg_context == NULL ||
                ctpg_context->description_field == NULL
            ) {
                goto rewrite_error;
            }

            const char *new_description =
                ctpg_context->description_field->value;

            if (new_description == NULL)
                new_description = "";

            if (
                current_field->value != NULL &&
                current_field->value[0] != '\0'
            ) {
                CATPKG_REWRITE_FIELD(
                    current_field,
                    new_description
                );
            }

            else {

                if (
                    fseek(
                        file,
                        current_field->bytepos +
                            byteheader,
                        SEEK_SET
                    ) != 0
                ) {
                    goto rewrite_error;
                }

                if (
                    fprintf(
                        file,
                        "%s\n-",
                        new_description
                    ) < 0
                ) {
                    goto rewrite_error;
                }
            }
        }

        else if (FIELD_NAME(
                     current_field,
                     "SHA"
                 )) {

            if (
                ctpg_context == NULL ||
                ctpg_context->sha256_field == NULL ||
                ctpg_context->sha256_field->value == NULL
            ) {
                goto rewrite_error;
            }

            CATPKG_REWRITE_FIELD(
                current_field,
                ctpg_context->sha256_field->value
            );
        }

        else if (FIELD_NAME(
                     current_field,
                     "size"
                 )) {

                        if (removed_info == NULL)
                goto rewrite_error;

            const struct PackageField *removed_size_field =
                PackageInfo_Find(
                    removed_info,
                    "size"
                );
            
            if (
                removed_size_field == NULL ||
                removed_size_field->value == NULL
            ) {
                goto rewrite_error;
            }
            
            errno = 0;

            char *old_end = NULL;

            unsigned long long old_size =
                strtoull(
                    removed_size_field->value,
                    &old_end,
                    10
                );

                        while (
                old_end != NULL &&
                (
                    *old_end == ' ' ||
                    *old_end == '\t'
                )
            ) {
                old_end++;
            }

            if (
                errno != 0 ||
                old_end == removed_size_field->value ||
                *old_end != '\0'
            ) {
                goto rewrite_error;
            }
            
            off_t new_size =
                (off_t)old_size +
                builder->change_size;

            
            printf(
                "old_size = %llu\n"
                "builder->change_size = %lld\n"
                "new_size = %lld\n",
                old_size,
                (long long)builder->change_size,
                (long long)new_size
            );
            if (new_size < 0)
                goto rewrite_error;

            char new_size_string[32];
            
            int new_size_length =
                snprintf(
                    new_size_string,
                    sizeof(new_size_string),
                    "%lld",
                    (long long)new_size
                );

                        if (
                new_size_length < 0 ||
                (size_t)new_size_length >=
                    sizeof(new_size_string)
            ) {
                goto rewrite_error;
            }

            CATPKG_REWRITE_FIELD(
                current_field,
                new_size_string
            );
                    }

        else if (FIELD_NAME(
                     current_field,
                     "dependency"
                 )) {

                        const struct PackageField *new_field =
                ctpg_context->dependency_field;

            const struct PackageField *old_field =
                current_field;

            while (
                old_field != NULL &&
                FIELD_NAME(
                    old_field,
                    "dependency"
                ) &&
                new_field != NULL &&
                FIELD_NAME(
                    new_field,
                    "dependency"
                )
            ) {
                CATPKG_REWRITE_FIELD(
                    old_field,
                    new_field->value
                );

                old_field =
                    old_field->next_field;

                new_field =
                    new_field->next_field;
            }

            while (
                old_field != NULL &&
                FIELD_NAME(
                    old_field,
                    "dependency"
                )
            ) {
                CATPKG_REWRITE_FIELD(
                    old_field,
                    ""
                );

                old_field =
                    old_field->next_field;
            }

            if (
                new_field != NULL &&
                FIELD_NAME(
                    new_field,
                    "dependency"
                )
            ) {
                goto rewrite_error;
            }

            current_field =
                old_field;

            continue;
        }

        else if (FIELD_NAME(
                     current_field,
                     "#folder"
                 )) {

                        const struct PackageField *new_field =
                ctpg_context->folder_field;

            const struct PackageField *old_field =
                current_field;

            while (
                old_field != NULL &&
                FIELD_NAME(
                    old_field,
                    "#folder"
                ) &&
                new_field != NULL &&
                FIELD_NAME(
                    new_field,
                    "#folder"
                )
            ) {
                CATPKG_REWRITE_OPTION_FIELD(
                    old_field,
                    new_field->name,
                    new_field->value
                );

                old_field =
                    old_field->next_field;

                new_field =
                    new_field->next_field;
            }

            while (
                old_field != NULL &&
                FIELD_NAME(
                    old_field,
                    "#folder"
                )
            ) {
                            CATPKG_CLEAR_OPTION_FIELD(
                    old_field
                );

                old_field =
                    old_field->next_field;
            }

            if (
                new_field != NULL &&
                FIELD_NAME(
                    new_field,
                    "#folder"
                )
            ) {
                goto rewrite_error;
            }

            current_field =
                old_field;

            continue;
        }

        else if (FIELD_NAME(
                     current_field,
                     "#symlink"
                 )) {
            const struct PackageField *new_field =
                ctpg_context->symlink_field;

            const struct PackageField *old_field =
                current_field;

            while (
                old_field != NULL &&
                FIELD_NAME(
                    old_field,
                    "#symlink"
                ) &&
                new_field != NULL &&
                FIELD_NAME(
                    new_field,
                    "#symlink"
                )
            ) {
                CATPKG_REWRITE_OPTION_FIELD(
                    old_field,
                    new_field->name,
                    new_field->value
                );

                old_field =
                    old_field->next_field;

                new_field =
                    new_field->next_field;
            }

            while (
                old_field != NULL &&
                FIELD_NAME(
                    old_field,
                    "#symlink"
                )
            ) {
                CATPKG_CLEAR_OPTION_FIELD(
                    old_field
                );

                old_field =
                    old_field->next_field;
            }

            if (
                new_field != NULL &&
                FIELD_NAME(
                    new_field,
                    "#symlink"
                )
            ) {
                goto rewrite_error;
            }

            current_field =
                old_field;

            continue;
        }

        else if (FIELD_NAME(
                     current_field,
                     "#command"
                 )) {

            const struct PackageField *new_field =
                ctpg_context->command_field;

            const struct PackageField *old_field =
                current_field;

            while (
                old_field != NULL &&
                FIELD_NAME(
                    old_field,
                    "#command"
                ) &&
                new_field != NULL &&
                FIELD_NAME(
                    new_field,
                    "#command"
                )
            ) {
                CATPKG_REWRITE_OPTION_FIELD(
                    old_field,
                    new_field->name,
                    new_field->value
                );

                old_field =
                    old_field->next_field;

                new_field =
                    new_field->next_field;
            }

            while (
                old_field != NULL &&
                FIELD_NAME(
                    old_field,
                    "#command"
                )
            ) {
                CATPKG_CLEAR_OPTION_FIELD(
                    old_field
                );

                old_field =
                    old_field->next_field;
            }

            if (
                new_field != NULL &&
                FIELD_NAME(
                    new_field,
                    "#command"
                )
            ) {
                goto rewrite_error;
            }

            current_field =
                old_field;

            continue;
        }

        current_field =
            current_field->next_field;
    }

    if (fclose(file) != 0) {
        CATPKG_BUILD_LOG_PATH_ERROR(
            protocol_name,
            args[1]
        );

        PackageInfo_Free(info);
        PackageInfo_Free(removed_info);

        return 1;
    }

    PackageInfo_Free(info);
    PackageInfo_Free(removed_info);

    CATPKG_BUILD_LOG_PATH_OK(
        protocol_name,
        args[1]
    );

    break;

rewrite_error:

    fclose(file);

    CATPKG_BUILD_LOG_PATH_ERROR(
        protocol_name,
        args[1]
    );

    PackageInfo_Free(info);
    PackageInfo_Free(removed_info);

    return 1;
}


#undef FIELD_IS
#undef FIELD_NAME

#undef CATPKG_CLEAR_OPTION_FIELD
#undef CATPKG_REWRITE_OPTION_FIELD
#undef CATPKG_REWRITE_FIELD

/*
 * ============================================================
 * CTPG_EXTRACT_PACKAGEINFO
 * ============================================================
 */

case CTPG_EXTRACT_PACKAGEINFO:
{
    CATPKG_BUILD_LOG_TREE(0);

    if (ctpg_context == NULL) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_EXTRACT_PACKAGEINFO: invalid context"
        );

        return 1;
    }

    char *catpkg_metadata_path =
        make_catpkg_path(
            CATPKG_METADATA_PATH,
            ctpg_context->full_name
        );

    if (catpkg_metadata_path == NULL) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_EXTRACT_PACKAGEINFO: path"
        );

        return 1;
    }

    if (catpkg_builder_catpackage_extract(
            ctpg_context,
            CATPKG_PACKAGEINFO,
            catpkg_metadata_path,
            NULL
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "CTPG_EXTRACT_PACKAGEINFO",
            CATPKG_PACKAGEINFO,
            catpkg_metadata_path
        );

        free(catpkg_metadata_path);

        return 1;
    }

    catpkg_build_log(
        "Saving the current package size...",
        builder,
        true
    );

    size_t absolute =
        (size_t)(
            builder->change_size < 0
                ? -builder->change_size
                : builder->change_size
        );

    char new_value[32];

    int new_value_length =
        snprintf(
            new_value,
            sizeof(new_value),
            "%zu",
            absolute
        );

    if (
        new_value_length < 0 ||
        (size_t)new_value_length >= sizeof(new_value)
    ) {
        free(catpkg_metadata_path);
        return 1;
    }

    char *path_packageinfo =
        make_catpkg_path(
            CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO,
            ctpg_context->full_name
        );

    if (path_packageinfo == NULL) {
        free(catpkg_metadata_path);
        return 1;
    }

    FILE *file =
        fopen(path_packageinfo, "r+");

    free(path_packageinfo);

    if (file == NULL) {
        free(catpkg_metadata_path);
        return 1;
    }

    /*
     * size_field itself must exist because it contains
     * the absolute byte position of the size field.
     */

    if (ctpg_context->size_field == NULL) {
        fclose(file);
        free(catpkg_metadata_path);
        return 1;
    }

    /*
     * bytepos is now absolute from the beginning of the file,
     * so no CATPKG header offset is required.
     */

    if (fseek(
            file,
            ctpg_context->size_field->bytepos,
            SEEK_SET
        ) != 0) {

        fclose(file);
        free(catpkg_metadata_path);
        return 1;
    }

    /*
     * No size value:
     *
     * -
     * -
     *
     * Replace it with:
     *
     * <size>
     * -
     */

    if (
        ctpg_context->size_field->value == NULL ||
        ctpg_context->size_field->value[0] == '\0'
    ) {

        if (fprintf(
                file,
                "%s\n-",
                new_value
            ) < 0) {

            fclose(file);
            free(catpkg_metadata_path);
            return 1;
        }

    } else {

        size_t old_size =
            strlen(
                ctpg_context->size_field->value
            );

        size_t new_size =
            (size_t)new_value_length;

        /*
         * Clear the old value.
         */

        for (size_t i = 0; i < old_size; i++) {

            if (fputc(' ', file) == EOF) {

                fclose(file);
                free(catpkg_metadata_path);
                return 1;
            }
        }

        /*
         * Return to the absolute size byte position.
         */

        if (fseek(
                file,
                ctpg_context->size_field->bytepos,
                SEEK_SET
            ) != 0) {

            fclose(file);
            free(catpkg_metadata_path);
            return 1;
        }

        /*
         * Write new size.
         */

        if (fwrite(
                new_value,
                1,
                new_size,
                file
            ) != new_size) {

            fclose(file);
            free(catpkg_metadata_path);
            return 1;
        }
    }

    if (fclose(file) != 0) {
        free(catpkg_metadata_path);
        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "CTPG_EXTRACT_PACKAGEINFO",
        CATPKG_PACKAGEINFO,
        catpkg_metadata_path
    );

    free(catpkg_metadata_path);

    break;
}


/*
 * ============================================================
 * CTPG_INFO
 * ============================================================
 */

case CTPG_INFO:

    CATPKG_BUILD_LOG_TREE(0);

    if (
        ctpg_context == NULL ||
        request->result == NULL
    ) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_INFO"
        );

        return 1;
    }

    *(struct PackageInfo **)request->result =
        ctpg_context->pkginfo;

    CATPKG_BUILD_LOG(
        "CTPG_INFO\n|   Package: %s@%s",
        "OK",
        0,
        CATPKG_BUILD_LOG_CTPG_ARGS(
            ctpg_context->name_field->value,
            ctpg_context->version_field->value
        )
    );

    break;


/*
 * ============================================================
 * CTPG_EXTRACT
 * ============================================================
 *
 * value:
 *
 * source\0destination\0
 */

case CTPG_EXTRACT: {

    CATPKG_BUILD_LOG_TREE(0);

    if (
        request_val == NULL ||
        ctpg_context == NULL
    ) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_EXTRACT"
        );

        return 1;
    }

    char *source =
        request_val;

    char *destination =
        source +
        strlen(source) + 1;

    if (catpkg_builder_catpackage_extract(
            ctpg_context,
            source,
            destination,
            NULL
        ) != 0) {

        CATPKG_BUILD_LOG_MOVE_ERROR(
            "CTPG_EXTRACT",
            source,
            destination
        );

        return 1;
    }

    CATPKG_BUILD_LOG_MOVE_OK(
        "CTPG_EXTRACT",
        source,
        destination
    );

    break;
}


/*
 * ============================================================
 * CTPG_CLOSE
 * ============================================================
 */

case CTPG_CLOSE:

    CATPKG_BUILD_LOG_TREE(0);

    if (ctpg_context == NULL) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CTPG_CLOSE: no context"
        );

        return 1;
    }

    ctpg_context_close(
        &ctpg_context
    );

    CATPKG_BUILD_LOG(
        "%s",
        "OK",
        0,
        "CTPG_CLOSE"
    );

    break;


/*
 * ============================================================
 * CATPKG_CREATE_FILEINFO
 * ============================================================
 */

case CATPKG_CREATE_FILEINFO:
{
    CATPKG_BUILD_LOG_TREE(0);

    if (request_val == NULL) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CATPKG_CREATE_FILEINFO: invalid path"
        );

        return 1;
    }

    CATPKG_BUILD_LOG(
        "CATPKG_CREATE_FILEINFO_START: \"%s\"",
        "",
        0,
        request_val
    );

    FILE *file =
        fopen(
            request_val,
            "w"
        );

    if (file == NULL) {

        perror(
            "catpkg: fopen FileInfo"
        );

        CATPKG_BUILD_LOG_PATH_ERROR(
            "CATPKG_CREATE_FILEINFO",
            request_val
        );

        return 1;
    }

    if (
        fprintf(
            file,
            "CATPKG 1.1\n"
            "#catpkg-version 2.0.0-beta\n"
        ) < 0
    ) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CATPKG_CREATE_FILEINFO: header"
        );

        goto fileinfo_error;
    }

    fprintf(
        file,
        "-\n"
        "#type file\n"
        "#path %s\n",
        request_val
    );
    size_t sap_section_index =
        0;

    for(size_t ai = 0; ai < builder->applied_count; ai++ ) {
        struct CatpkgRequest *request =
            builder->requests[ai];

        if (request == NULL) {

            CATPKG_BUILD_LOG(
                "%s",
                "ERROR",
                0,
                "CATPKG_CREATE_FILEINFO: NULL request"
            );

            goto fileinfo_error;
        }

        char *request_val =
            request->value;

        switch (request->protocol) {
            #include "CATPKG_CREATE_FILEINFO.inc"
        }
    }

    if (fclose(file) != 0) {

        CATPKG_BUILD_LOG(
            "%s",
            "ERROR",
            0,
            "CATPKG_CREATE_FILEINFO: fclose"
        );

        return 1;
    }

    CATPKG_BUILD_LOG(
        "CATPKG_CREATE_FILEINFO_END: %zu objects",
        "OK",
        0,
        sap_section_index
    );

    break;


fileinfo_error:

    fclose(file);

    CATPKG_BUILD_LOG(
        "%s",
        "ERROR",
        0,
        "CATPKG_CREATE_FILEINFO"
    );

    return 1;
}

case CATPKG_REMOVE_PACKAGE_RECORDS:
{
    char *metadata_path = make_catpkg_path(CATPKG_METADATA_PATH, request_val);
    char *files_path = make_catpkg_path(CATPKG_FILES_PATH, request_val);
    char *package_path = make_catpkg_path(CATPKG_PACKAGE_PATH, request_val);

    char *info_path = make_catpkg_path(CATPKG_METADATA_PATH "/" CATPKG_PACKAGEINFO, request_val);
    char *fileinfo_path = make_catpkg_path(CATPKG_FILES_PATH "/" CATPKG_FILE, request_val);
    
    if (unlink(info_path) != 0){
        return 1;
    }
    if (unlink(fileinfo_path) != 0)
        return 1;

    if (rmdir(metadata_path) != 0)
        return 1;
    if (rmdir(files_path) != 0)
        return 1;
    if (rmdir(package_path) != 0)
        return 1;

    CATPKG_BUILD_LOG_PATH_OK(
        "CATPKG_REMOVE_PACKAGE_RECORDS",
        request_val
    );
    break;
}
/*
 * ============================================================
 * NONE
 * ============================================================
 */

case NONE:

    CATPKG_BUILD_LOG_TREE(0);

    CATPKG_BUILD_LOG(
        "%s",
        "SKIP",
        0,
        "NONE"
    );

    break;