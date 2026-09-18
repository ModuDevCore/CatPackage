/*
 * --------------------------------------------------------
 * Prepare transaction
 * --------------------------------------------------------
 */

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "PREPARATION OF THE PACKAGE"
);

#ifndef CATPKG_TRANSACTION_UPDATE

/*
 * CTPG context borrows `path`.
 *
 * `path` is owned by the caller, so it remains valid.
 */


catpkg_builder_request(
    &builder,
    CTPG_OPEN,
    (void *)path
);

#endif

/*
 * Cache the currently installed package if necessary.
 *
 * The protocol itself determines the size contribution.
 */

catpkg_builder_request(
    &builder,
    CTPG_TO_CACHE,
    (void *)package_fullname
);

/*
 * --------------------------------------------------------
 * Prepare package directories
 * --------------------------------------------------------
 */

catpkg_builder_request(
    &builder,
    BUILD_LOG,
    "Prepare package directories"
);

catpkg_builder_request(
    &builder,
    MKDIR_SOFT,
    metadata_path
);

catpkg_builder_request(
    &builder,
    MKDIR_SOFT,
    files_path
);

catpkg_builder_request(
    &builder,
    MKDIR_SOFT,
    package_path
);

catpkg_builder_request(
    &builder,
    MKDIR_SOFT,
    CATPKG_DATABASE_DIR_PATH
);

/*
 * --------------------------------------------------------
 * Create directories declared by #folder
 * --------------------------------------------------------
 */

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "Create directories declared by #folder"
);

const struct PackageFieldMatches folder_filds =
    PackageInfo_FindAll(
        info,
        "#folder"
    );

for (size_t i = 0; i < folder_filds.count; ++i) {
    if (
        catpkg_builder_request(
            &builder,
            MKDIR_SOFT,
            folder_filds.items[i] -> value
        ) != 0
    ) {
        goto prepare_error;
    }
}

/*
 * --------------------------------------------------------
 * Commands
 * --------------------------------------------------------
 */

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "Commands"
);

catpkg_builder_request(
    &builder,
    BUILD_LOG,
    "Checking for the presence of commands..."
);

const struct PackageField *command_field = 
    PackageInfo_Find(info, "#command");

if(command_field != NULL)
    catpkg_builder_request(
        &builder,
        MKDIR_SOFT,
        package_commands_path
    );

while(command_field != NULL) {
    if(strcmp(command_field -> name, "#command") == 0) {
        catpkg_builder_request(
            &builder,
            CTPG_MKCMD,
            command_field -> value
        );
    }
    command_field = command_field -> next_field;
}

    

// catpkg_builder_request(
//     &builder,
//     CATPKG_MKCMD,
//     NULL
// );

/*
 * --------------------------------------------------------
 * Create #symlink entries
 *
 * The protocol receives:
 *
 *     target\0link
 *
 * The allocated value belongs to us and is freed only
 * after apply/revert.
 * --------------------------------------------------------
 */

const struct PackageFieldMatches symlink_fields =
    PackageInfo_FindAll(
        info,
        "#symlink"
    );

for (size_t i = 0; i < symlink_fields.count; ++i) {

    const char *separator =
        strstr(symlink_fields.items[i] -> value, " -> ");

    if (separator == NULL)
        goto prepare_error;

    size_t target_size =
        (size_t)(separator - symlink_fields.items[i] -> value);

    const char *link =
        separator + 4;

    size_t value_size =
        target_size +
        1 +
        strlen(link) +
        1;

    char *value = malloc(value_size);

    if (value == NULL)
        goto prepare_error;

    memcpy(
        value,
        symlink_fields.items[i] -> value,
        target_size
    );

    value[target_size] = '\0';

    memcpy(
        value + target_size + 1,
        link,
        strlen(link) + 1
    );

    if (
        catpkg_builder_request(
            &builder,
            SYMLINK,
            value
        ) != 0
    ) {
        free(value);
        goto prepare_error;
    }
}

/*
 * --------------------------------------------------------
 * Extract package payload
 * --------------------------------------------------------
 */

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "Extarct package payload..."
);

catpkg_builder_request(
    &builder,
    CTPG_EXTRACT_PAYLOAD,
    NULL
);

/*
 * --------------------------------------------------------
 * Extract PACKAGEINFO
 * --------------------------------------------------------
 */

#ifdef CATPKG_TRANSACTION_UPDATE

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "Update PACKAGEINFO"
);

update_packageinfo_args[0] = REMOVE_VAL(path);
update_packageinfo_args[1] = INSTALL_VAL(package_info_path);

if (
    strcmp(
        package_fullname,
        REMOVE_VAL(package_fullname)
    ) != 0
) {
    catpkg_builder_request(
        &builder,
        CTPG_UPDATE_PACKAGEINFO,
        &update_packageinfo_args
    );
    
    catpkg_builder_request(
        &builder,
        CATPKG_REMOVE_PACKAGE_RECORDS,
        REMOVE_VAL(package_fullname)
    );
}

#else

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "Extract PACKAGEINFO"
);
catpkg_builder_request(
    &builder,
    CTPG_EXTRACT_PACKAGEINFO,
    package_info_path
);

#endif



/*
 * --------------------------------------------------------
 * Generate FileInfo
 * --------------------------------------------------------
 */

catpkg_builder_request(
    &builder,
    BUILD_LOG_SEP,
    "Generate FileInfo"
);

catpkg_builder_request(
    &builder,
    CATPKG_CREATE_FILEINFO,
    fileinfo_path
);

catpkg_builder_request(
    &builder,
    CATPKG_UPDATE_DATABASE,
    CATPKG_DATABASE_DIR_PATH "/" CATPKG_DATABASE
);

#ifndef CATPKG_TRANSACTION_UPDATE

catpkg_builder_request(
    &builder,
    CTPG_CLOSE,
    NULL
);

#endif