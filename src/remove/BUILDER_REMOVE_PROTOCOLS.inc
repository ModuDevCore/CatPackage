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
    PackageInfo_Find(
        database_fileinfo,
        "#use-options"
    );

size_t *remove_sections = NULL;
size_t remove_sections_count = 0;

int type = 0; // 0 - Dir, 1 - File

while (current_db_field != NULL) {

    /*
     * #required <section>=<package>
     */

    if (
        strcmp(
            current_db_field->name,
            "#required"
        ) == 0
    ) {
        char value[PATH_MAX];

        strncpy(
            value,
            current_db_field->value,
            sizeof(value) - 1
        );

        value[sizeof(value) - 1] = '\0';

        char *section_text =
            strtok(value, "=");

        char *required_package =
            strtok(NULL, "=");

        if (
            section_text == NULL ||
            required_package == NULL
        ) {
            current_db_field =
                current_db_field->next_field;

            continue;
        }


        /*
         * This dependency belongs to the package
         * being removed/updated.
         */

        if (
            strcmp(
                required_package,
                package_name
            ) == 0
        ) {
            char *end = NULL;

            errno = 0;

            unsigned long long section_number =
                strtoull(
                    section_text,
                    &end,
                    10
                );

            if (
                errno != 0 ||
                end == section_text ||
                *end != '\0' ||
                section_number > SIZE_MAX - 1
            ) {
                free(remove_sections);
                return 1;
            }


            /*
             * Save corresponding DATABASE section.
             *
             * DEPENDENCIES:
             *
             *     0 -> DATABASE PackageField.section 1
             */

            size_t database_section =
                (size_t)section_number + 1;

            size_t *tmp =
                realloc(
                    remove_sections,
                    (remove_sections_count + 1) *
                        sizeof(*remove_sections)
                );

            if (tmp == NULL) {
                free(remove_sections);
                return 1;
            }

            remove_sections = tmp;

            remove_sections[
                remove_sections_count++
            ] = database_section;


#ifndef CATPKG_TRANSACTION_UPDATE

            /*
             * REMOVE
             *
             * The package loses ownership of every
             * DATABASE section it owns.
             *
             * DATABASE_OBSOLETE_DEPEND receives the
             * DEPENDENCIES section number.
             */

            if (
                catpkg_builder_request_copy(
                    &builder,
                    DATABASE_OBSOLETE_DEPEND,
                    section_text
                ) != 0
            ) {
                free(remove_sections);
                return 1;
            }

#endif
        }

        current_db_field =
            current_db_field->next_field;

        continue;
    }


    /*
     * Check whether current DATABASE section belongs
     * to the package being removed/updated.
     */

    bool removable = false;

    size_t dependency_section = 0;

    for (
        size_t i = 0;
        i < remove_sections_count;
        i++
    ) {
        if (
            remove_sections[i] ==
            current_db_field->section
        ) {
            removable = true;

            /*
             * DATABASE section 1 -> DEPENDENCIES 0.
             */

            dependency_section =
                remove_sections[i] - 1;

            break;
        }
    }

    if (!removable) {
        current_db_field =
            current_db_field->next_field;

        continue;
    }


    /*
     * Get filesystem object type.
     */

    if (
        strcmp(
            current_db_field->name,
            "#type"
        ) == 0
    ) {
        if (
            strcmp(
                current_db_field->value,
                "directory"
            ) == 0
        ) {
            type = 0;
        }

        else if (
            strcmp(
                current_db_field->value,
                "file"
            ) == 0
        ) {
            type = 1;
        }
    }


    /*
     * Handle filesystem object.
     */

    else if (
        strcmp(
            current_db_field->name,
            "#path"
        ) == 0
    ) {

#ifdef CATPKG_TRANSACTION_UPDATE

        /*
         * UPDATE
         *
         * Package metadata stored under
         * CATPKG_PACKAGES_PATH is replaced by the
         * update process itself, so don't treat it
         * as an obsolete payload object here.
         */

        size_t packages_path_len =
            strlen(CATPKG_PACKAGES_PATH);

        bool package_metadata =
            strncmp(
                current_db_field->value,
                CATPKG_PACKAGES_PATH,
                packages_path_len
            ) == 0 &&
            (
                current_db_field->value[
                    packages_path_len
                ] == '/' ||

                current_db_field->value[
                    packages_path_len
                ] == '\0'
            );

        if (!package_metadata) {

            switch (type) {

                case 0:
                    if (
                        catpkg_builder_request(
                            &builder,
                            CTPG_REMOVE_DIR_OBSOLETE,
                            current_db_field->value
                        ) != 0
                    ) {
                        free(remove_sections);
                        return 1;
                    }
                    break;


                case 1:
                    if (
                        catpkg_builder_request(
                            &builder,
                            CTPG_REMOVE_FILE_OBSOLETE,
                            current_db_field->value
                        ) != 0
                    ) {
                        free(remove_sections);
                        return 1;
                    }
                    break;
            }


            /*
             * IMPORTANT:
             *
             * Do NOT queue DATABASE_OBSOLETE_DEPEND
             * here unconditionally.
             *
             * CTPG_REMOVE_*_OBSOLETE determines during
             * Apply() whether the object is actually:
             *
             *     OBSOLETE
             *
             * or:
             *
             *     NOT OBSOLETE
             *
             * Therefore the dependency must only be
             * obsoleted when that protocol actually
             * removes the object.
             *
             * See note below.
             */
        }

#else

        /*
         * REMOVE
         *
         * Ownership has already been scheduled for
         * removal above through
         * DATABASE_OBSOLETE_DEPEND.
         *
         * Here we remove the filesystem object.
         */

        switch (type) {

            case 0:
                if (
                    catpkg_builder_request(
                        &builder,
                        REMOVE_DIR,
                        current_db_field->value
                    ) != 0
                ) {
                    free(remove_sections);
                    return 1;
                }
                break;


            case 1:
                if (
                    catpkg_builder_request(
                        &builder,
                        REMOVE_FILE,
                        current_db_field->value
                    ) != 0
                ) {
                    free(remove_sections);
                    return 1;
                }
                break;
        }

#endif
    }

    current_db_field =
        current_db_field->next_field;
}

free(remove_sections);

catpkg_builder_request(
    &builder,
    REMOVE_BATCH_CLOSE,
    NULL
);