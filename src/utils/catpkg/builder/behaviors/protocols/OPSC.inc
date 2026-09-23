case REMOVE_DIR:
{
    if (
        request_val != NULL &&
        strcmp(
            request_val,
            path_field->value
        ) == 0
    ) {
        path_matched = 1;
        /*
         * Protected object is inside the directory
         * being removed.
         */

        if(section_package_required_field != section_first_required_field) {
            checked_request->skip = 1;
            break;
        }

        const struct PackageField *option_field = section_package_required_field;

        while(option_field != NULL) {
            if(option_field -> section > 0)
                break;
            if(strcmp(option_field -> name, "#required") != 0){
                option_field = option_field -> next_field;
                continue; 
            }

            char *copy =  strdup(option_field -> value);
            char *section = strtok(copy, "=");
            char *package_name = strtok(NULL, "=");

            if(strcmp(package_name, name_field -> value) != 0
                && strcmp(section, section_entry) == 0) {
                catpkg_build_log(
                    "ERROR OPSEC: Unable to verify operation \"%s\" \n"
                    "Unresolved dependency: \"%s\".\n",
                    builder,
                    true,
                    "REMOVE_DIR",
                    package_name
                );
                free(copy);

                return 1;
            }

            free(copy);

            option_field = option_field -> next_field;
        }

        break;
    }
    if(path_field -> next_field == NULL) {
        catpkg_build_log(
            "ERROR OPSEC: Unable to verify operation \"%s\": "
            "record \"%s\" was not found in the database. \n",
            builder,
            true,
            "REMOVE_DIR",
            request_val
        );
        return 1;
    }
}