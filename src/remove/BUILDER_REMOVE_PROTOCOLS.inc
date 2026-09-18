catpkg_builder_request(
    &builder,
    OPERATION_SECURE_CONTEXT,
    &opsc_context
);
catpkg_builder_request(
    &builder,
    REMOVE_BATCH_OPEN,
    NULL
);

const struct PackageField *current_db_field =
    PackageInfo_Find(database_fileinfo, "#type");

int type = 0; // 0 - Dir, 1 - File
while (current_db_field != NULL) {
    if (strcmp(current_db_field->name, "#type") == 0) {
        if(strcmp(current_db_field->value, "directory") == 0) {
            type = 0;
        } 
        else if(strcmp(current_db_field->value, "file") == 0) {
            type = 1;
        }
    }
    if (strcmp(current_db_field->name, "#path") == 0) {
        bool removable = true;

        {
            const struct PackageField *current_field =
                current_db_field->next_field;

            while (current_field != NULL &&
                   strcmp(current_field->name, "#path") != 0) {

                if (strcmp(current_field->name, "#required") == 0) {
                    if (strcmp(current_field->value, package_name) != 0)
                        removable = false;
                    break;
                }

                current_field = current_field->next_field;
            }
        }

        if (removable) {
            #ifdef CATPKG_TRANSACTION_UPDATE
                const char *path =
                    current_db_field->value;

                size_t packages_path_len =
                    strlen(CATPKG_PACKAGES_PATH);

                if (
                    !(strncmp(
                        path,
                        CATPKG_PACKAGES_PATH,
                        packages_path_len
                    ) == 0 &&
                    (
                        path[packages_path_len] == '/' ||
                        path[packages_path_len] == '\0'
                    ))
                )
                    switch (type) {
                        case 0:
                            catpkg_builder_request(
                                &builder,
                                CTPG_REMOVE_DIR_OBSOLETE,
                                current_db_field->value
                            );
                            break;

                        case 1:
                            catpkg_builder_request(
                                &builder,
                                CTPG_REMOVE_FILE_OBSOLETE,
                                current_db_field->value
                            );
                            break;
                    }
            #else
                switch (type) {
                    case 0:
                        catpkg_builder_request(
                            &builder,
                            REMOVE_DIR,
                            current_db_field->value
                        );
                        break;

                    case 1:
                        catpkg_builder_request(
                            &builder,
                            REMOVE_FILE,
                            current_db_field->value
                        );
                        break;
                }

            #endif
        }
    }
    current_db_field = current_db_field->next_field;
}

catpkg_builder_request(
    &builder,
    REMOVE_BATCH_CLOSE,
    NULL
);