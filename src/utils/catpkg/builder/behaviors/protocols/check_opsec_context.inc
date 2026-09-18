case REMOVE_FILE:
case REMOVE_DIR:
{
    const struct PackageField *current_field =
        PackageInfo_Find(file_info, "#path");

    const struct PackageField *name_field =
        PackageInfo_Find(info, "name");

    while (current_field != NULL) {
        if (strcmp(current_field->name, "#path") != 0) {
            current_field = current_field->next_field;
            continue;
        }

        if (strcmp(current_field->value, request_val) != 0) {
            current_field = current_field->next_field;
            continue;
        }

        /*
         * We found the FileInfo entry for request_val.
         * Check its #required fields until the next #path.
         */
        bool is_first_required = false;

        current_field = current_field->next_field;

        while (current_field != NULL &&
               strcmp(current_field->name, "#path") != 0) {

            if (strcmp(current_field->name, "#required") == 0) {
                if (!is_first_required) {
                    if (strcmp(current_field->value, name_field->value) == 0)
                        is_first_required = true;
                } else {
                    error->code = CATPKG_DEPENDENCY_ERROR;
                    error->path = request_val;
                    error->required_by = current_field->value;

                    return error->code;
                }
            }

            current_field = current_field->next_field;
        }

        break;
    }

    break;
}

default:
    /*
     * All other operations whose behavior is not specified depending
     * on the context are performed without additional restrictions.
     */
    break;