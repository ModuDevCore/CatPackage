int catpkg_install(
    const char *path,
    bool assume_yes
)
{
    if (path == NULL) {

        fprintf(
            stderr,
            "catpkg: package path is NULL\n"
        );

        return 1;
    }


    /*
     * Check package extension.
     */

    if (!is_catpackage(path)) {

        fprintf(
            stderr,
            "catpkg: \"%s\" is not a .catpackage file\n",
            path
        );

        return 1;
    }

    /*
     * Parse PACKAGEINFO.
    */

    struct PackageInfo *info =
        CATPKG_Parse(
            path
        );

    if (info == NULL) {

        fprintf(
            stderr,
            "catpkg: failed to parse PACKAGEINFO\n"
        );

        return 1;
    }
    /*
     * Get package INFO.
     */

    const struct PackageField *name =
        PackageInfo_Find(
            info,
            "name"
        );


    const struct PackageField *version =
        PackageInfo_Find(
            info,
            "version"
        );

    const struct PackageField *sha256_catpackage =
        PackageInfo_Find(
            info,
            "SHA"
        );

    // ==============================

    /*
     * Builder.
     */

    struct CatpkgBuilder builder;
    
    catpkg_builder_init(&builder);

    catpkg_builder_request(&builder, BUILD_LOG_START, NULL);
    catpkg_builder_request(&builder, BUILD_LOG, "**PREPARATION OF THE PACKAGE**");
    char *catpkg_metadata_path =
        make_catpkg_path(
            CATPKG_METADATA_PATH,
            package_fullname
        );


    char *catpkg_files_path =
        make_catpkg_path(
            CATPKG_FILES_PATH,
            package_fullname
        );


    char *catpkg_package_path =
        make_catpkg_path(
            CATPKG_PACKAGE_PATH,
            package_fullname
        );
    
    catpkg_builder_request(&builder, MKDIR, catpkg_metadata_path);
    catpkg_builder_request(&builder, MKDIR, catpkg_files_path);
    catpkg_builder_request(&builder, MKDIR, catpkg_package_path);

   /*
     * Cache original .catpackage.
     */
    catpkg_builder_request(&builder, CTPG_OPEN, path);
    catpkg_builder_request(&builder, BUILD_LOG, "CACHING THE CURRENT PACKAGE");

    // if (catpkg_cache_package(
    //         path,
    //         package_fullname
    //     ) != 0) {

    //     fprintf(
    //         stderr,
    //         "catpkg: failed to cache package\n"
    //     );

    //     return 1;
    // }
    
    // char *cache_pkg_value_protocol = combine_strings(path, package_fullname);
    // ^ TODO: Transfer to the protocol CTPG_TO_CACHE ^
    catpkg_builder_request(&builder, CTPG_TO_CACHE, NULL);


    /*
     * Checking the integrity of commands and creating symbolic links.
     */

    catpkg_builder_request(&builder, BUILD_LOG, "Checking for the presence of commands...");

    // char *catpkg_commands_path =
    //     make_catpkg_path(
    //         CATPKG_BUILD_PATH CATPKG_PACKAGE_DATA "/.catpkg/commands",
    //         name -> value
    //     );
    // if (mkdir_p(
    //         catpkg_commands_path,
    //         0755
    //     ) != 0) {

    //     perror(
    //         "catpkg: mkdir_p temporary files path"
    //     );

    //     return 1;
    // }

    // free(catpkg_commands_path); 
    // ^ TODO: Transfer to the protocol CATPKG_MKCMD ^

    struct PackageFieldMatches commands_fields =
        PackageInfo_FindAll(
            info,
            "#command"
        );

    // for (size_t i = 0;
    //     i < commands_fields.count;
    //     i++) {


    // }    
    catpkg_builder_request(&builder, CTPG_CATPACKAGE_COMMANDS_EXTERN, NULL);

    char *catpkg_metadata_path =
        make_catpkg_path(
            CATPKG_METADATA_PATH,
            package_fullname
        );

    catpkg_builder_request(&builder, CTPG_EXTRACT_PACKAGEINFO, catpkg_metadata_path);
    catpkg_builder_request(&builder, CTPG_EXTRACT_FINAL, NULL); // At the very end

    // PackageFieldMatches_Free(
    //     &commands_fields
    // ); TODO: After Builder Apply


    catpkg_builder_request(&builder, CTPG_CLOSE, NULL);
    catpkg_builder_request(&builder, BUILD_LOG_END, NULL);

    printf("%u\n", builder.change_size);

    // if(catpkg_builder_apply(&builder) == 1)
    //     catpkg_builder_revert(&builder);
    // catpkg_builder_free(&builder);

    struct stat st;

    if (stat(path, &st) != 0) {
        perror("stat");
        return 1;
    }

    char package_size[64];
    char will_be_installed[64] = "-";
    char changing[64] = "-";
    char sign = '\0';

    catpkg_format_size(
        st.st_size,
        package_size,
        sizeof(package_size)
    );

    if (sha256_catpackage == NULL ||
            sha256_catpackage -> value == NULL ||
            catpkg_integrity_catpackage(path, sha256_catpackage -> value) == 0
    ) {
        printf("Warning: The package was modified after assembly; it is not possible to calculate the actual size. It is recommended to verify the package by reassembling it.\n");
    }
    else {
        uint64_t size = 0;
        catpkg_calc_installed(&size, info);
        
        off_t difference =  (off_t)size - st.st_size;

        sign = difference >= 0 ? '+' : '-';

        off_t absolute_difference =
            difference >= 0 ? difference : -difference;

        catpkg_format_size(
            absolute_difference,
            changing,
            sizeof(changing)
        );
        catpkg_format_size(
            (off_t)size,
            will_be_installed,
            sizeof(will_be_installed)
        );    
    }
    /*
     * Build:
     *
     * name@version
     *
     * Example:
     *
     * catpkg@1.0.0-beta
     */

    char package_fullname[256];

    int package_fullname_length =
        snprintf(
            package_fullname,
            sizeof(package_fullname),
            "%s@%s",
            name->value,
            version->value
        );


    if (package_fullname_length < 0 ||
        (size_t)package_fullname_length >=
        sizeof(package_fullname)) {

        fprintf(
            stderr,
            "catpkg: package name and version are too long\n"
        );

        PackageInfo_Free(
            info
        );

        return 1;
    }
    printf(
        ALLOW_INSTALLATION,
        package_fullname,
        package_size,
        will_be_installed,
        sign,
        changing
    );
    if(!assume_yes){
        printf("! Install it? [Y/n] ");
        if (catpkg_confirm() == 0) {
            return 1;
        }
    }

    printf("\n");

    /*
     * Create temporary directories.
     */

    if (mkdir_p(
            CATPKG_BUILD_PATH,
            0755
        ) != 0) {

        perror(
            "catpkg: mkdir_p build path"
        );

        return 1;
    }


    if (mkdir_p(
            CATPKG_TMP_FILES_PATH,
            0755
        ) != 0) {

        perror(
            "catpkg: mkdir_p temporary files path"
        );

        return 1;
    }


    if (mkdir_p(
            CATPKG_TMP_METADATA_PATH,
            0755
        ) != 0) {

        perror(
            "catpkg: mkdir_p temporary metadata path"
        );

        return 1;
    }


    if (mkdir_p(
            CATPKG_CACHE_PACKAGES,
            0755
        ) != 0) {

        perror(
            "catpkg: mkdir_p cache packages"
        );

        return 1;
    }


    /*
     * Extract .catpackage.
     */

    pid_t pid = fork();

    if (pid == -1) {

        perror(
            "catpkg: fork"
        );

        return 1;
    }


    if (pid == 0) {

        execl(
            "/usr/bin/tar",
            "tar",
            "-xf",
            path,
            "-C",
            CATPKG_BUILD_PATH,
            (char *)NULL
        );


        /*
         * Fallback.
         */

        execl(
            "/bin/tar",
            "tar",
            "-xf",
            path,
            "-C",
            CATPKG_BUILD_PATH,
            (char *)NULL
        );


        perror(
            "catpkg: execl tar"
        );

        _exit(1);
    }


    int status;

    if (waitpid(
            pid,
            &status,
            0
        ) == -1) {

        perror(
            "catpkg: waitpid"
        );

        return 1;
    }


    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {

        fprintf(
            stderr,
            "catpkg: tar failed\n"
        );

        return 1;
    }

    /*
     * Move PACKAGEINFO from the build directory
     * into temporary metadata.
     *
     * PACKAGEINFO is a FILE, therefore
     * catpkg_move_file() is used.
     */

    fflush(stdout);
    if (catpkg_move_file(
            CATPKG_BUILD_PATH "/PACKAGEINFO",
            CATPKG_TMP_METADATA_PATH "/PACKAGEINFO"
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to move PACKAGEINFO\n"
        );

        return 1;
    }


    if (name == NULL ||
        version == NULL ||
        name->value == NULL ||
        version->value == NULL) {

        fprintf(
            stderr,
            "catpkg: PACKAGEINFO is missing name or version\n"
        );

        PackageInfo_Free(
            info
        );

        return 1;
    }

    /*
     * Create package dependency directories.
     */

    struct PackageFieldMatches directories =
        PackageInfo_FindAll(
            info,
            "#folder"
        );


    for (size_t i = 0;
         i < directories.count;
         i++) {

        if (mkdir_p(
                directories.items[i]->value,
                0755
            ) != 0) {

            fprintf(
                stderr,
                "catpkg: failed to create directory: %s\n",
                directories.items[i]->value
            );

            PackageFieldMatches_Free(
                &directories
            );

            PackageInfo_Free(
                info
            );

            return 1;
        }
    }


    PackageFieldMatches_Free(
        &directories
    );


    if (name == NULL ||
        version == NULL ||
        name->value == NULL ||
        version->value == NULL) {

        fprintf(
            stderr,
            "catpkg: PACKAGEINFO is missing name or version\n"
        );

        PackageInfo_Free(
            info
        );

        return 1;
    }


    /*
     * Find package files and directories.
     */

    nftw(
        CATPKG_BUILD_PATH,
        catpkg_find_build_path,
        20,
        FTW_PHYS
    );

    /*
     * Create FileInfo metadata.
     *
     * Every file gets:
     *
     * /var/lib/catpkg/.../files/<number>
     *
     * containing:
     *
     * CATPKG 1.1
     * /path/to/file
     * 
     * OR ( symlink )
     * 
     * CATPKG 1.1
     * /path/to/target
     * /path/to/symlink
    */

    int meta_files_count = 0;

    for (size_t i = 0;
         i < nftw_build_paths_count;
         i++) {
        int len = snprintf(
            NULL,
            0,
            CATPKG_TMP_FILES_PATH "/%zu",
            i
        );


        if (len < 0) {

            fprintf(
                stderr,
                "catpkg: failed to calculate metadata path\n"
            );

            return 1;
        }


        char *filename =
            malloc(
                (size_t)len + 1
            );


        if (filename == NULL) {

            perror(
                "catpkg: malloc"
            );

            return 1;
        }


        snprintf(
            filename,
            (size_t)len + 1,
            CATPKG_TMP_FILES_PATH "/%zu",
            i
        );


        FILE *file =
            fopen(
                filename,
                "w"
            );


        if (file == NULL) {

            perror(
                "catpkg: fopen"
            );

            free(filename);

            return 1;
        }


        fprintf(
            file,
            "CATPKG 1.1\n"
        );


        fprintf(
            file,
            "%s\n",
            nftw_build_paths[i]
        );


        fclose(
            file
        );

        free(
            filename
        );
        meta_files_count++;
    }

    /*
     * Create package symlinks.
     *
     * Format:
     *
     * #symlink /path/to/link -> /path/to/target
     *
     * The symlink is first created inside BUILD_PATH.
     *
     * FileInfo:
     *
     * CATPKG 1.1
     * /path/to/link
     * /path/to/target
     */

    struct PackageFieldMatches symlinks =
        PackageInfo_FindAll(
            info,
            "#symlink"
        );


    for (size_t i = 0;
         i < symlinks.count;
         i++) {

        const char *symlink_definition =
            symlinks.items[i]->value;


        if (symlink_definition == NULL)
            continue;


        /*
         * Make a writable copy because the original
         * PackageInfo value must not be modified.
         */

        char *definition =
            strdup(symlink_definition);


        if (definition == NULL) {

            perror(
                "catpkg: strdup"
            );

            PackageFieldMatches_Free(
                &symlinks
            );

            return 1;
        }


        /*
         * Find:
         *
         * "->"
         */

        char *separator =
            strstr(
                definition,
                "->"
            );


        if (separator == NULL) {

            fprintf(
                stderr,
                "catpkg: invalid symlink definition: \"%s\"\n",
                symlink_definition
            );

            free(
                definition
            );

            PackageFieldMatches_Free(
                &symlinks
            );

            return 1;
        }


        /*
         * Split:
         *
         * /path/to/link -> /path/to/target
         *
         * into:
         *
         * path   = /path/to/link
         * target = /path/to/target
         */

        *separator = '\0';

        char *path =
            definition;

        char *target =
            separator + 2;


        /*
         * Remove leading spaces from path.
         */

        while (*path == ' ')
            path++;


        /*
         * Remove trailing spaces from path.
         */

        char *path_end =
            separator;

        while (path_end > path &&
               path_end[-1] == ' ') {

            path_end--;
        }

        *path_end = '\0';


        /*
         * Remove leading spaces from target.
         */

        while (*target == ' ')
            target++;


        /*
         * Validate paths.
         */

        if (path[0] == '\0' ||
            target[0] == '\0') {

            fprintf(
                stderr,
                "catpkg: invalid symlink definition: \"%s\"\n",
                symlink_definition
            );

            free(
                definition
            );

            PackageFieldMatches_Free(
                &symlinks
            );

            return 1;
        }


        /*
         * Create the symlink inside BUILD_PATH.
         */

        char *build_path =
            make_catpkg_path(
                CATPKG_BUILD_PATH "%s",
                path
            );


        if (build_path == NULL) {

            fprintf(
                stderr,
                "catpkg: failed to create symlink build path\n"
            );

            free(
                definition
            );

            PackageFieldMatches_Free(
                &symlinks
            );

            return 1;
        }


        /*
         * Create parent directories.
         */

        char *last_slash =
            strrchr(
                build_path,
                '/'
            );


        if (last_slash != NULL) {

            *last_slash = '\0';


            if (mkdir_p(
                    build_path,
                    0755
                ) != 0) {

                perror(
                    "catpkg: mkdir_p symlink parent"
                );

                *last_slash = '/';

                free(
                    build_path
                );

                free(
                    definition
                );

                PackageFieldMatches_Free(
                    &symlinks
                );

                return 1;
            }


            *last_slash = '/';
        }


        /*
         * Create symlink.
         *
         * target is stored exactly as specified in
         * PACKAGEINFO.
         */

        if (lstat(build_path, &st) == 0) {

            printf(
                "+ Symlink: \"%s -> %s\" already exists.\n",
                path,
                target
            );

        }
        else if (errno == ENOENT) {

            if (symlink(
                    target,
                    build_path
                ) != 0) {

                fprintf(
                    stderr,
                    "catpkg: failed to create symlink: %s -> %s: %s\n",
                    path,
                    target,
                    strerror(errno)
                );

                free(build_path);
                free(definition);
                PackageFieldMatches_Free(&symlinks);

                return 1;
            }

            printf(
                "+ Symlink: \"%s -> %s\" created successfully.\n",
                path,
                target
            );

        }
        else {

            fprintf(
                stderr,
                "catpkg: failed to check symlink \"%s\": %s\n",
                build_path,
                strerror(errno)
            );

            free(build_path);
            free(definition);
            PackageFieldMatches_Free(&symlinks);

            return 1;
        }


        free(
            build_path
        );


        /*
         * Create FileInfo metadata.
         *
         * CATPKG 1.1
         * /path/to/link
         * /path/to/target
         */

        int len =
            snprintf(
                NULL,
                0,
                CATPKG_TMP_FILES_PATH "/%zu",
                (size_t)meta_files_count
            );


        if (len < 0) {

            fprintf(
                stderr,
                "catpkg: failed to calculate metadata path\n"
            );

            free(
                definition
            );

            PackageFieldMatches_Free(
                &symlinks
            );

            return 1;
        }


        char *filename =
            malloc(
                (size_t)len + 1
            );


        if (filename == NULL) {

            perror(
                "catpkg: malloc"
            );

            free(
                definition
            );

            PackageFieldMatches_Free(
                &symlinks
            );

            return 1;
        }


        snprintf(
            filename,
            (size_t)len + 1,
            CATPKG_TMP_FILES_PATH "/%zu",
            (size_t)meta_files_count
        );


        FILE *file =
            fopen(
                filename,
                "w"
            );


        if (file == NULL) {

            perror(
                "catpkg: fopen"
            );

            free(
                filename
            );

            free(
                definition
            );

            PackageFieldMatches_Free(
                &symlinks
            );

            return 1;
        }


        fprintf(
            file,
            "CATPKG 1.1\n"
        );

        fprintf(
            file,
            "%s\n",
            path
        );

        fprintf(
            file,
            "%s\n",
            target
        );


        if (fclose(file) != 0) {

            perror(
                "catpkg: fclose"
            );

            free(
                filename
            );

            free(
                definition
            );

            PackageFieldMatches_Free(
                &symlinks
            );

            return 1;
        }


        free(
            filename
        );

        free(
            definition
        );


        meta_files_count++;
    }


    PackageFieldMatches_Free(
        &symlinks
    );
    PackageInfo_Free(
        info
    );

    /*
     * Build final paths.
     */

    char *catpkg_metadata_path =
        make_catpkg_path(
            CATPKG_METADATA_PATH,
            package_fullname
        );


    char *catpkg_files_path =
        make_catpkg_path(
            CATPKG_FILES_PATH,
            package_fullname
        );


    char *catpkg_package_path =
        make_catpkg_path(
            CATPKG_PACKAGE_PATH,
            package_fullname
        );


    if (catpkg_metadata_path == NULL ||
        catpkg_files_path == NULL ||
        catpkg_package_path == NULL) {

        free(
            catpkg_metadata_path
        );

        free(
            catpkg_files_path
        );

        free(
            catpkg_package_path
        );

        fprintf(
            stderr,
            "catpkg: failed to create paths\n"
        );

        return 1;
    }


    /*
     * Create:
     *
     * /var/lib/catpkg/name@version
     */

    if (mkdir_p(
            catpkg_package_path,
            0755
        ) != 0) {

        perror(
            "catpkg: mkdir_p package path"
        );

        free(
            catpkg_metadata_path
        );

        free(
            catpkg_files_path
        );

        free(
            catpkg_package_path
        );

        return 1;
    }


    free(
        catpkg_package_path
    );


    /*
     * 1. Deploy FileInfo directory.
     *
     * CATPKG_TMP_FILES_PATH is a DIRECTORY.
     *
     * Therefore use catpkg_move_directory().
     */

    if (catpkg_move_directory(
            CATPKG_TMP_FILES_PATH,
            catpkg_files_path
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to move file metadata directory\n"
        );

        free(
            catpkg_metadata_path
        );

        free(
            catpkg_files_path
        );

        return 1;
    }


    /*
     * 2. Deploy package metadata.
     *
     * CATPKG_TMP_METADATA_PATH is a DIRECTORY.
     *
     * Therefore use catpkg_move_directory().
     */

    if (catpkg_move_directory(
            CATPKG_TMP_METADATA_PATH,
            catpkg_metadata_path
        ) != 0) {

        fprintf(
            stderr,
            "catpkg: failed to move package metadata directory\n"
        );

        free(
            catpkg_metadata_path
        );

        free(
            catpkg_files_path
        );

        return 1;
    }


    free(
        catpkg_metadata_path
    );

    free(
        catpkg_files_path
    );


    /*
     * 3. Deploy package data.
     *
     * First create missing directories.
     */

    printf(
        "Completion...\n"
    );

    printf(
        "Creating missing directories...\n"
    );


    for (size_t i = 0;
         i < nftw_build_dirs_count;
         i++) {

        struct stat st;

        if (stat(nftw_build_dirs[i], &st) == -1) {

            if (errno != ENOENT) {
                perror("catpkg: stat");
                return 1;
            }

            if (mkdir_p(nftw_build_dirs[i], 0755) != 0) {
                perror("catpkg: mkdir_p");
                return 1;
            }

            printf(
                "+ Directory: \"%s\" created successfully.\n",
                nftw_build_dirs[i]
            );
        }
        else if (!S_ISDIR(st.st_mode)) {

            fprintf(
                stderr,
                "catpkg: \"%s\" exists but is not a directory\n",
                nftw_build_dirs[i]
            );

            return 1;
        }
    }


    printf(
        "The missing directories have been created.\n"
        "Continued deployment...\n"
    );


    /*
     * Move package files from build directory
     * to their final locations.
     *
     * Every nftw_build_paths[i] is a FILE.
     *
     * Therefore use catpkg_move_file().
     */

    for (size_t i = 0;
         i < nftw_build_paths_count;
         i++) {

        char *build_file_path =
            make_catpkg_path(
                CATPKG_BUILD_PATH "%s",
                nftw_build_paths[i]
            );


        if (build_file_path == NULL) {

            fprintf(
                stderr,
                "catpkg: failed to create build path\n"
            );

            return 1;
        }


        if (catpkg_move_file(
                build_file_path,
                nftw_build_paths[i]
            ) != 0) {

            printf(
                "%s: Deployed error.\n",
                nftw_build_paths[i]
            );

            fprintf(
                stderr,
                "catpkg: failed to move file\n"
            );


            free(
                build_file_path
            );

            return 1;
        }


        free(
            build_file_path
        );


        printf(
            "+ File: \"%s\" deployed successfully.\n",
            nftw_build_paths[i]
        );
    }


    /*
     * Installation completed.
     */

    printf(
        "The package \"%s\" has been successfully installed.\n",
        package_fullname
    );


    return 0;
}
