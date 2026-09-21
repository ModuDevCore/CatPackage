/*
 * Section 0 — package name.
 */

case 0: {

    if (strcmp(buffer, "-") == 0) {
        current_section++;
        continue;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: package name cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD(
        "name",
        buffer,
        field_bytepos
    );

    continue;
}


/*
 * Section 1 — package version.
 */

case 1: {

    if (strcmp(buffer, "-") == 0) {
        current_section++;
        continue;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: package version cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD(
        "version",
        buffer,
        field_bytepos
    );

    continue;
}


/*
 * Section 2 — architecture.
 *
 * Empty section means that architecture
 * is not specified.
 */

case 2: {

    if (strcmp(buffer, "-") == 0) {
        current_section++;
        continue;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: invalid empty package architecture\n"
        );

        return 1;
    }

    ADD_FIELD(
        "architecture",
        buffer,
        field_bytepos
    );

    continue;
}


/*
 * Section 3 — description.
 *
 * Empty section means empty description.
 */

case 3: {

    if (strcmp(buffer, "-") == 0) {

        /*
         * Preserve the old behaviour:
         * "-" directly in place of description
         * means an empty description.
         */

        ADD_FIELD(
            "description",
            "",
            field_bytepos
        );

        current_section++;

        continue;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: invalid empty package description\n"
        );

        return 1;
    }

    ADD_FIELD(
        "description",
        buffer,
        field_bytepos
    );

    continue;
}


/*
 * Section 4 — SHA.
 */

case 4: {

    if (strcmp(buffer, "-") == 0) {
        current_section++;
        continue;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: package SHA cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD(
        "SHA",
        buffer,
        field_bytepos
    );

    continue;
}


/*
 * Section 5 — package size.
 */

case 5: {

    if (strcmp(buffer, "-") == 0) {
        current_section++;
        continue;
    }

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: package size cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD(
        "size",
        buffer,
        field_bytepos
    );

    continue;
}


/*
 * Section 6 — dependencies.
 */

case 6: {

    if (strcmp(buffer, "-") == 0) {
        current_section++;
        continue;
    }

    if (buffer[0] == '\0')
        continue;

    ADD_FIELD(
        "dependency",
        buffer,
        field_bytepos
    );

    continue;
}


/*
 * Section 7 — filesystem operations.
 */

case 7: {

    if (buffer[0] == '\0')
        continue;


    /*
     * Keep complete original section.
     */

    APPEND_SECTION_LINE();


    /*
     * #folder
     */

    if (
        strncmp(
            buffer,
            "#folder ",
            8
        ) == 0
    ) {
        if (buffer[8] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid folder operation: \"%s\"\n",
                buffer
            );

            free(section);

            return 1;
        }

        ADD_FIELD(
            "#folder",
            buffer + 8,
            field_bytepos
        );

        continue;
    }


    /*
     * #persistent-file
     */

    if (
        strncmp(
            buffer,
            "#persistent-file ",
            17
        ) == 0
    ) {
        if (buffer[17] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid persistent-file operation: \"%s\"\n",
                buffer
            );

            free(section);

            return 1;
        }

        ADD_FIELD(
            "#persistent-file",
            buffer + 17,
            field_bytepos
        );

        continue;
    }


    /*
     * #symlink
     */

    if (
        strncmp(
            buffer,
            "#symlink ",
            9
        ) == 0
    ) {
        if (buffer[9] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid symlink operation: \"%s\"\n",
                buffer
            );

            free(section);

            return 1;
        }

        ADD_FIELD(
            "#symlink",
            buffer + 9,
            field_bytepos
        );

        continue;
    }


    /*
     * #command
     */

    if (
        strncmp(
            buffer,
            "#command ",
            9
        ) == 0
    ) {
        if (buffer[9] == '\0') {
            fprintf(
                stderr,
                "catpkg: invalid command operation: \"%s\"\n",
                buffer
            );

            free(section);

            return 1;
        }

        ADD_FIELD(
            "#command",
            buffer + 9,
            field_bytepos
        );

        continue;
    }


    /*
     * Unknown operation.
     */

    fprintf(
        stderr,
        "catpkg: unknown filesystem operation: \"%s\"\n",
        buffer
    );

    free(section);

    return 1;
}


/*
 * Invalid section.
 */

default: {
    fprintf(
        stderr,
        "catpkg: invalid PackageInfo section: %zu\n",
        current_section
    );

    free(section);

    return 1;
}