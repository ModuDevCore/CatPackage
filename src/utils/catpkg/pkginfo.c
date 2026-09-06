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
     */


    /*
     * Helper for adding a field.
     */

    #define ADD_FIELD(field_name, field_value)                 \
        do {                                                   \
            struct PackageField *tmp = realloc(               \
                info->fields,                                 \
                (info->fields_count + 1) *                    \
                sizeof(*info->fields)                          \
            );                                                 \
                                                               \
            if (tmp == NULL)                                   \
                return 1;                                      \
                                                               \
            info->fields = tmp;                                \
                                                               \
            info->fields[info->fields_count].name =           \
                strdup(field_name);                            \
                                                               \
            info->fields[info->fields_count].value =          \
                strdup(field_value);                           \
                                                               \
            if (info->fields[info->fields_count].name == NULL \
                ||                                             \
                info->fields[info->fields_count].value == NULL) \
                return 1;                                      \
                                                               \
            info->fields_count++;                             \
        } while (0)


    /*
     * Read package name.
     */

    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        
        fprintf(
            stderr,
            "catpkg: missing package name\n"
        );

        return 1;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] == '\0') {

        fprintf(
            stderr,
            "catpkg: package name cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD("name", buffer);

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

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package version\n"
        );

        return 1;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] == '\0' ||
        strcmp(buffer, "-") == 0) {

        fprintf(
            stderr,
            "catpkg: package version cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD("version", buffer);


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

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package architecture\n"
        );

        return 1;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if(buffer[0] != '-') {
        ADD_FIELD("architecture", buffer);

        if (fgets(buffer, sizeof(buffer), file) == NULL) {
            fprintf(
                stderr,
                "catpkg: missing separator after package architecture\n"
            );

            return 1;
        }
        
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

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package description\n"
        );

        return 1;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (strcmp(buffer, "-") == 0) {

        ADD_FIELD("description", "");

    } else {

        if (buffer[0] == '\0') {

            fprintf(
                stderr,
                "catpkg: invalid empty package description\n"
            );

            return 1;
        }

        ADD_FIELD("description", buffer);


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

    if (fgets(buffer, sizeof(buffer), file) == NULL) {

        fprintf(
            stderr,
            "catpkg: missing package SHA\n"
        );

        return 1;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] == '\0' ||
        strcmp(buffer, "-") == 0) {

        fprintf(
            stderr,
            "catpkg: package SHA cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD("SHA", buffer);


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

    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fprintf(
            stderr,
            "catpkg: missing package size\n"
        );

        return 1;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] == '\0' ||
        strcmp(buffer, "-") == 0) {

        fprintf(
            stderr,
            "catpkg: package size cannot be empty\n"
        );

        return 1;
    }

    ADD_FIELD("size", buffer);


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

    while (fgets(buffer, sizeof(buffer), file) != NULL) {

        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (strcmp(buffer, "-") == 0)
            break;

        if (buffer[0] == '\0')
            continue;

        ADD_FIELD("dependency", buffer);
    }

    /*
     * Filesystem operations.
     */

    char *operations = NULL;
    size_t operations_size = 0;

    while (fgets(buffer, sizeof(buffer), file) != NULL) {

        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (buffer[0] == '\0')
            continue;


        /*
         * #folder
         */

        if (strncmp(buffer, "#folder ", 8) == 0) {

            ADD_FIELD(
                "#folder",
                buffer + 8
            );
        }


        /*
         * #symlink
         */

        else if (strncmp(buffer, "#symlink ", 9) == 0) {

            ADD_FIELD(
                "#symlink",
                buffer + 9
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
         * Append operation to the combined field.
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

    if (operations == NULL)
        ADD_FIELD("operations", "");
    else {

        struct PackageField *tmp = realloc(
            info->fields,
            (info->fields_count + 1) *
            sizeof(*info->fields)
        );

        if (tmp == NULL) {

            free(operations);

            return 1;
        }

        info->fields = tmp;

        info->fields[info->fields_count].name =
            strdup("operations");

        info->fields[info->fields_count].value =
            operations;

        if (info->fields[info->fields_count].name == NULL) {

            free(
                info->fields[info->fields_count].value
            );

            return 1;
        }

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

    /*
     * CATPKG 1.1 — FileInfo
     *
     * Format:
     *
     * CATPKG 1.1
     * /usr/bin/catpkg
     *
     * SAP section 1 — file path
     */


    /*
     * SAP section 1: file path.
     */

    if (fgets(buffer, sizeof(buffer), file) == NULL)
        return 1;

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (buffer[0] == '\0') {
        fprintf(
            stderr,
            "catpkg: file path cannot be empty\n"
        );

        return 1;
    }

    info->fields = realloc(
        info->fields,
        (info->fields_count + 1) *
        sizeof(*info->fields)
    );

    if (info->fields == NULL)
        return 1;

    info->fields[info->fields_count].name =
        strdup("path");

    info->fields[info->fields_count].value =
        strdup(buffer);

    if (info->fields[info->fields_count].name == NULL ||
        info->fields[info->fields_count].value == NULL)
        return 1;

    info->fields_count++;


    /*
     * There must be no additional non-empty data.
     */

    while (fgets(buffer, sizeof(buffer), file) != NULL) {

        buffer[strcspn(buffer, "\r\n")] = '\0';

        if (buffer[0] != '\0') {
            fprintf(
                stderr,
                "catpkg: unexpected data in CATPKG 1.1 FILEINFO\n"
            );

            return 1;
        }
    }

    return 0;
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


    fclose(file);

    return info;
}


const struct PackageField *PackageInfo_Find(
    const struct PackageInfo *info,
    const char *name
)
{
    if (info == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < info->fields_count; i++) {

        if (info->fields[i].name == NULL)
            continue;

        if (strcmp(
                info->fields[i].name,
                name
            ) == 0) {

            return &info->fields[i];
        }
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

    for (size_t i = 0; i < info->fields_count; i++) {

        if (info->fields[i].name == NULL)
            continue;

        if (strcmp(
                info->fields[i].name,
                name
            ) != 0) {

            continue;
        }

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

        matches.items[matches.count] =
            &info->fields[i];

        matches.count++;
    }

    return matches;
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

    for (size_t i = 0; i < info->fields_count; i++) {

        free(info->fields[i].name);
        free(info->fields[i].value);
    }

    free(info->fields);
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