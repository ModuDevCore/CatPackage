#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "catpkg/pkginfo.h"
#include "configuration.h"

static int catpkg_parse_v1_packageinfo(
    FILE *file,
    struct PackageInfo *info
)
{
    char buffer[PATH_MAX];
    long bytepos = 0;
    long field_bytepos = 0;

    struct PackageField *last_field = NULL;

    /*
     * CATPKG 1.0 — PackageInfo
     *
     * name
     * -
     * version
     * -
     * description
     * -
     * SHA
     * -
     * size
     * -
     * dependency
     * dependency
     * -
     * #folder <path>
     * #symlink <link> <target>
     * #command ./path/to/command/script
     */


    /*
     * Helper for adding a field.
     */

    #define ADD_FIELD(field_name, field_value, field_bytepos)  \
        do {                                                   \
            struct PackageField *field = malloc(              \
                sizeof(*field)                                 \
            );                                                 \
                                                               \
            if (field == NULL)                                 \
                return 1;                                      \
                                                               \
            field->name = strdup(field_name);                  \
            field->value = strdup(field_value);                \
            field->bytepos = field_bytepos;                    \
            field->next_field = NULL;                          \
                                                               \
            if (field->name == NULL ||                         \
                field->value == NULL) {                        \
                free(field->name);                             \
                free(field->value);                            \
                free(field);                                   \
                return 1;                                      \
            }                                                  \
                                                               \
            if (info->fields == NULL)                          \
                info->fields = field;                          \
            else                                               \
                last_field->next_field = field;                \
                                                               \
            last_field = field;                                \
            info->fields_count++;                              \
        } while (0)


    /*
     * Read package name.
     */

    field_bytepos = bytepos;

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package name\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] == '\0') {

        fprintf(
            stderr,
            "catpkg: package name cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD("name", buffer, field_bytepos);


    /*
     * Separator after package name.
     */

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing separator after package name\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (strcmp(buffer, "-") != 0) {

        fprintf(
            stderr,
            "catpkg: expected separator after package name\n"
        );

        return 1;
    }


    /*
     * Read package version.
     */

    field_bytepos = bytepos;

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package version\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] == '\0' ||
        strcmp(buffer, "-") == 0) {

        fprintf(
            stderr,
            "catpkg: package version cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD("version", buffer, field_bytepos);


    /*
     * Separator after package version.
     */

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing separator after package version\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (strcmp(buffer, "-") != 0) {

        fprintf(
            stderr,
            "catpkg: expected separator after package version\n"
        );

        return 1;
    }


    /*
     * Read package architecture.
     */

    field_bytepos = bytepos;

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package architecture\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] != '-') {

        ADD_FIELD(
            "architecture",
            buffer,
            field_bytepos
        );

        if (fgets(buffer, sizeof(buffer), file) == NULL) {

            fprintf(
                stderr,
                "catpkg: missing separator after package architecture\n"
            );

            return 1;
        }

        bytepos += (long)strlen(buffer);

        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strcmp(buffer, "-") != 0) {

            fprintf(
                stderr,
                "catpkg: expected separator after package architecture\n"
            );

            return 1;
        }
    }


    /*
     * Read package description.
     *
     * "-" means empty description.
     */

    field_bytepos = bytepos;

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package description\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (strcmp(buffer, "-") == 0) {

        ADD_FIELD(
            "description",
            "",
            field_bytepos
        );

    } else {

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


        /*
         * Separator after description.
         */

        if (fgets(buffer, sizeof(buffer), file) == NULL) {

            fprintf(
                stderr,
                "catpkg: missing separator after package description\n"
            );

            return 1;
        }

        bytepos += (long)strlen(buffer);

        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strcmp(buffer, "-") != 0) {

            fprintf(
                stderr,
                "catpkg: expected separator after package description\n"
            );

            return 1;
        }
    }


    /*
     * Read package SHA.
     */

    field_bytepos = bytepos;

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package SHA\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] == '\0' ||
        strcmp(buffer, "-") == 0) {

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


    /*
     * Separator after SHA.
     */

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing separator after package SHA\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (strcmp(buffer, "-") != 0) {

        fprintf(
            stderr,
            "catpkg: expected separator after package SHA\n"
        );

        return 1;
    }


    /*
     * Read package size.
     */

    field_bytepos = bytepos;

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package size\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] == '\0' ||
        strcmp(buffer, "-") == 0) {

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


    /*
     * Separator after size.
     */

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing separator after package size\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (strcmp(buffer, "-") != 0) {

        fprintf(
            stderr,
            "catpkg: expected separator after package size\n"
        );

        return 1;
    }


    /*
     * Dependencies.
     *
     * An immediately following "-" means
     * that the dependency list is empty.
     */

    while (1) {

        field_bytepos = bytepos;

        if (fgets(buffer, sizeof(buffer), file) == NULL)
            break;

        bytepos += (long)strlen(buffer);

        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strcmp(buffer, "-") == 0)
            break;

        if (buffer[0] == '\0')
            continue;

        ADD_FIELD(
            "dependency",
            buffer,
            field_bytepos
        );
    }


    /*
     * Filesystem operations.
     */

    char *operations = NULL;
    size_t operations_size = 0;
    long operations_bytepos = bytepos;

    while (1) {

        field_bytepos = bytepos;

        if (fgets(buffer, sizeof(buffer), file) == NULL)
            break;

        bytepos += (long)strlen(buffer);

        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (buffer[0] == '\0')
            continue;



        /*
         * #folder
         */

        if (strncmp(buffer, "#folder ", 8) == 0) {
            ADD_FIELD(
                "#folder",
                buffer + 8,
                field_bytepos
            );
        }

        /*
         * #persistent-file
         */

        else if (strncmp(buffer, "#persistent-file ", 17) == 0) {

            ADD_FIELD(
                "#persistent-file",
                buffer + 17,
                field_bytepos
            );
        }
        /*
         * #symlink
         */

        else if (strncmp(buffer, "#symlink ", 9) == 0) {

            ADD_FIELD(
                "#symlink",
                buffer + 9,
                field_bytepos
            );
        }


        /*
         * #command
         */

        else if (strncmp(buffer, "#command ", 9) == 0) {

            ADD_FIELD(
                "#command",
                buffer + 9,
                field_bytepos
            );
        }


        /*
         * Unknown operation.
         */

        else {

            fprintf(
                stderr,
                "catpkg: unknown filesystem operation: \"%s\"\n",
                buffer
            );

            free(operations);

            return 1;
        }


        /*
         * Append operation to combined field.
         */

        size_t length = strlen(buffer);

        char *tmp = realloc(
            operations,
            operations_size + length + 2
        );

        if (tmp == NULL) {

            free(operations);

            return 1;
        }

        operations = tmp;

        memcpy(
            operations + operations_size,
            buffer,
            length
        );

        operations_size += length;

        operations[operations_size++] = '\n';
        operations[operations_size] = '\0';
    }


    /*
     * Add combined operations field.
     */

    if (operations == NULL) {

        ADD_FIELD(
            "operations",
            "",
            operations_bytepos
        );

    } else {

        struct PackageField *field = malloc(
            sizeof(*field)
        );

        if (field == NULL) {

            free(operations);

            return 1;
        }

        field->name = strdup("operations");
        field->value = operations;
        field->bytepos = operations_bytepos;
        field->next_field = NULL;

        if (field->name == NULL) {

            free(field->value);
            free(field);

            return 1;
        }

        if (info->fields == NULL)
            info->fields = field;
        else
            last_field->next_field = field;

        last_field = field;
        info->fields_count++;
    }


    #undef ADD_FIELD

    return 0;
}


static int catpkg_parse_v1_fileinfo(
    FILE *file,
    struct PackageInfo *info
)
{
    char buffer[PATH_MAX];
    long bytepos = 0;
    long field_bytepos = 0;

    struct PackageField *last_field = NULL;

    /*
     * CATPKG 1.1 — FileInfo
     *
     * CATPKG 1.1
     * #catpkg-version 2.0.0-beta
     * -
     * #type file
     * #path /usr/bin/catpkg
     * -
     * #type directory
     * #path /var/cache/catpkg/packages
     */


    /*
     * Helper for adding a field.
     */

    #define ADD_FIELD(field_name, field_value, field_bytepos)  \
        do {                                                   \
            struct PackageField *field = malloc(              \
                sizeof(*field)                                 \
            );                                                 \
                                                               \
            if (field == NULL)                                 \
                return 1;                                      \
                                                               \
            field->name = strdup(field_name);                  \
            field->value = strdup(field_value);                \
            field->bytepos = field_bytepos;                    \
            field->next_field = NULL;                          \
                                                               \
            if (field->name == NULL ||                         \
                field->value == NULL) {                        \
                free(field->name);                             \
                free(field->value);                            \
                free(field);                                   \
                return 1;                                      \
            }                                                  \
                                                               \
            if (info->fields == NULL)                          \
                info->fields = field;                          \
            else                                               \
                last_field->next_field = field;                \
                                                               \
            last_field = field;                                \
            info->fields_count++;                              \
        } while (0)


    /*
     * Read catpkg version.
     */

    field_bytepos = bytepos;

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing catpkg version\n"
        );

        return 1;
    }

    bytepos += (long)strlen(buffer);

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (strncmp(buffer, "#catpkg-version ", 16) != 0) {

        fprintf(
            stderr,
            "catpkg: missing #catpkg-version\n"
        );

        return 1;
    }

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


    /*
     * Read FileInfo sections.
     */

    char *section = NULL;
    size_t section_size = 0;

    long section_bytepos = 0;


    /*
     * Add current section.
     */

    #define ADD_SECTION()                                      \
        do {                                                   \
            if (section != NULL && section_size > 0) {         \
                ADD_FIELD(                                     \
                    "info",                                    \
                    section,                                   \
                    section_bytepos                            \
                );                                             \
            }                                                  \
                                                               \
            free(section);                                     \
            section = NULL;                                    \
            section_size = 0;                                  \
            section_bytepos = 0;                               \
        } while (0)


    while (1) {

        field_bytepos = bytepos;

        if (fgets(buffer, sizeof(buffer), file) == NULL)
            break;

        bytepos += (long)strlen(buffer);

        buffer[strcspn(buffer, "\r\n")] = '\0';


        /*
         * Section separator.
         */

        if (strcmp(buffer, "-") == 0) {

            ADD_SECTION();

            continue;
        }


        /*
         * Ignore empty lines.
         */

        if (buffer[0] == '\0')
            continue;


        /*
         * Save the first byte position of the section.
         */

        if (section == NULL)
            section_bytepos = field_bytepos;


        /*
         * Add the original line to the section.
         */

        size_t line_length = strlen(buffer);

        char *tmp = realloc(
            section,
            section_size + line_length + 2
        );

        if (tmp == NULL) {

            free(section);

            return 1;
        }

        section = tmp;

        memcpy(
            section + section_size,
            buffer,
            line_length
        );

        section_size += line_length;

        section[section_size++] = '\n';
        section[section_size] = '\0';


        /*
         * #type
         */

        if (strncmp(buffer, "#type ", 6) == 0) {

            if (buffer[6] == '\0') {

                fprintf(
                    stderr,
                    "catpkg: invalid FileInfo type: \"%s\"\n",
                    buffer
                );

                free(section);

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

        if (strncmp(buffer, "#path ", 6) == 0) {

            if (buffer[6] == '\0') {

                fprintf(
                    stderr,
                    "catpkg: invalid FileInfo path: \"%s\"\n",
                    buffer
                );

                free(section);

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
         * #required
         */

        if (strncmp(buffer, "#required ", 10) == 0) {

            if (buffer[10] == '\0') {

                fprintf(
                    stderr,
                    "catpkg: invalid FileInfo required: \"%s\"\n",
                    buffer
                );

                free(section);

                return 1;
            }

            ADD_FIELD(
                "#required",
                buffer + 10,
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

        free(section);

        return 1;
    }


    /*
     * Store the final section.
     *
     * There may be no trailing '-' after it.
     */

    ADD_SECTION();


    #undef ADD_SECTION
    #undef ADD_FIELD

    return 0;
}

const struct PackageField *PackageInfo_Find(
    const struct PackageInfo *info,
    const char *name
)
{
    if (info == NULL || name == NULL)
        return NULL;

    const struct PackageField *field = info->fields;

    while (field != NULL) {

        if (
            field->name != NULL &&
            strcmp(field->name, name) == 0
        ) {
            return field;
        }

        field = field->next_field;
    }

    return NULL;
}


struct PackageFieldMatches PackageInfo_FindAll(
    const struct PackageInfo *info,
    const char *name
)
{
    struct PackageFieldMatches matches = {
        .items = NULL,
        .count = 0
    };

    if (info == NULL || name == NULL)
        return matches;

    const struct PackageField *field = info->fields;

    while (field != NULL) {

        if (
            field->name != NULL &&
            strcmp(field->name, name) == 0
        ) {
            const struct PackageField **tmp = realloc(
                matches.items,
                (matches.count + 1) *
                sizeof(*matches.items)
            );

            if (tmp == NULL) {
                PackageFieldMatches_Free(&matches);
                return matches;
            }

            matches.items = tmp;

            matches.items[matches.count] = field;
            matches.count++;
        }

        field = field->next_field;
    }

    return matches;
}

struct PackageInfo *CATPKG_Parse(
    const char *filePath
)
{
    if (filePath == NULL)
        return NULL;
    
    FILE *file;

    if (is_catpackage(filePath)) {
        char command[PATH_MAX];

        snprintf(
            command,
            sizeof(command),
            "tar -xOf '%s' '%s'",
            filePath,
            CATPKG_PACKAGEINFO
        );

        file = popen(command, "r");
    } else {
        file = fopen(filePath, "r");
    }

    if (file == NULL) {
        perror("catpkg: failed to open PACKAGEINFO");
        return NULL;
    }

    if (file == NULL) {
        perror("catpkg: fopen PACKAGEINFO");
        return NULL;
    }

    struct PackageInfo *info = malloc(
        sizeof(*info)
    );

    if (info == NULL) {
        perror("catpkg: malloc");
        if (is_catpackage(filePath))
            pclose(file);
        else
            fclose(file);
        return NULL;
    }

    info->fields = NULL;
    info->fields_count = 0;

    char format[256];

    if (fgets(format, sizeof(format), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: empty PACKAGEINFO\n"
        );

        if (is_catpackage(filePath))
            pclose(file);
        else
            fclose(file);
        PackageInfo_Free(info);

        return NULL;
    }

    format[strcspn(format, "\r\n")] = '\0';


    /*
     * Reading the package information.
     *
     * Parsing versions:
     *
     * CATPKG V1:
     * Native line-by-line parser with separators "-".
     *
     * CATPKG 1.0 — PackageInfo
     * CATPKG 1.1 — FileInfo
     * CATPKG 2.0 — Reserved for future format.
     */


    if (strcmp(format, "CATPKG 1.0") == 0) {

        /*
         * CATPKG 1.0 — PackageInfo
         */

        if (catpkg_parse_v1_packageinfo(
                file,
                info
            ) != 0) {

            fprintf(
                stderr,
                "catpkg: invalid CATPKG 1.0 PACKAGEINFO\n"
            );

            if (is_catpackage(filePath))
                pclose(file);
            else
                fclose(file);
            PackageInfo_Free(info);

            return NULL;
        }

    }
    else if (strcmp(format, "CATPKG 1.1") == 0) {

        /*
         * CATPKG 1.1 — FileInfo
         */

        if (catpkg_parse_v1_fileinfo(
                file,
                info
            ) != 0) {

            fprintf(
                stderr,
                "catpkg: invalid CATPKG 1.1 FILEINFO\n"
            );

            if (is_catpackage(filePath))
                pclose(file);
            else
                fclose(file);
            PackageInfo_Free(info);

            return NULL;
        }

    }
    else if (strcmp(format, "CATPKG 2.0") == 0) {

        fprintf(
            stderr,
            "catpkg: CATPKG 2.0 is not supported yet\n"
        );

        if (is_catpackage(filePath))
            pclose(file);
        else
            fclose(file);
        PackageInfo_Free(info);

        return NULL;
    }
    else {

        /*
         * Unknown format.
         */

        fprintf(
            stderr,
            "catpkg: unsupported PACKAGEINFO format: %s\n",
            format
        );

        if (is_catpackage(filePath))
            pclose(file);
        else
            fclose(file);
        PackageInfo_Free(info);

        return NULL;
    }


    if (is_catpackage(filePath))
        pclose(file);
    else
        fclose(file);

    return info;
}

void PackageFieldMatches_Free(
    struct PackageFieldMatches *matches
)
{
    if (matches == NULL)
        return;

    free(matches->items);

    matches->items = NULL;
    matches->count = 0;
}


void PackageInfo_Free(
    struct PackageInfo *info
)
{
    if (info == NULL)
        return;

    struct PackageField *field = info->fields;

    while (field != NULL) {

        struct PackageField *next = field->next_field;

        free(field->name);
        free(field->value);
        free(field);

        field = next;
    }

    free(info);
}


int is_catpackage(
    const char *path
)
{
    if (path == NULL)
        return 0;

    const char *extension = ".catpackage";

    size_t path_len = strlen(path);
    size_t extension_len = strlen(extension);

    if (path_len < extension_len)
        return 0;

    return strcmp(
        path + path_len - extension_len,
        extension
    ) == 0;
}