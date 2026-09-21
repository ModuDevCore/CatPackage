#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>  
#include <stdint.h>

#include "catpkg/pkginfo.h"
#include "catpkg/path.h"
#include "configuration.h"


/*
 * Generic PackageInfo reader.
 *
 * Allows the parser to read exactly the same way from:
 *
 *     FILE *
 *
 * or:
 *
 *     const char *
 */

enum PackageReaderType {
    PACKAGE_READER_FILE,
    PACKAGE_READER_STRING
};

struct PackageReader {
    char path[PATH_MAX];
    enum PackageReaderType type;

    union {
        FILE *file;

        struct {
            const char *data;
            size_t position;
        } string;
    } source;

    /*
     * Absolute byte position.
     *
     * For FILE this normally starts at 0.
     *
     * For a string containing an already extracted section,
     * this may start at the original section byte position.
     */
    long bytepos;
};


int get_file_path(FILE *file, char *buffer, size_t size)
{
    int fd = fileno(file);

    char proc_path[64];

    snprintf(
        proc_path,
        sizeof(proc_path),
        "/proc/self/fd/%d",
        fd
    );

    ssize_t len = readlink(
        proc_path,
        buffer,
        size - 1
    );

    if (len == -1)
        return 1;

    buffer[len] = '\0';

    return 0;
}

/*
 * Initialize reader from FILE.
 */

static void package_reader_init_file(
    struct PackageReader *reader,
    FILE *file,
    long bytepos
)
{

    reader->type = PACKAGE_READER_FILE;
    reader->source.file = file;
    reader->bytepos = bytepos;

    if (
        get_file_path(
            file,
            reader->path,
            sizeof(reader->path)
        ) != 0
    ) {
        reader->path[0] = '\0';
    }
}


/*
 * Initialize reader from string.
 */

static void package_reader_init_string(
    struct PackageReader *reader,
    const char *data,
    long bytepos
)
{
    reader->type = PACKAGE_READER_STRING;

    reader->source.string.data = data;
    reader->source.string.position = 0;

    reader->bytepos = bytepos;
}


/*
 * Read one line.
 *
 * Behaviour is intentionally similar to fgets().
 *
 * The newline is preserved in buffer if it exists.
 *
 * reader->bytepos is automatically moved by the amount
 * of bytes actually read.
 */

static char *package_reader_getline(
    struct PackageReader *reader,
    char *buffer,
    size_t buffer_size
)
{
    if (
        reader == NULL ||
        buffer == NULL ||
        buffer_size == 0
    )
        return NULL;

    if (reader->type == PACKAGE_READER_FILE) {

        if (
            fgets(
                buffer,
                buffer_size,
                reader->source.file
            ) == NULL
        )
            return NULL;
    }

    else if (reader->type == PACKAGE_READER_STRING) {

        const char *data =
            reader->source.string.data;

        size_t position =
            reader->source.string.position;

        if (
            data == NULL ||
            data[position] == '\0'
        )
            return NULL;

        size_t i = 0;

        while (
            data[position] != '\0' &&
            i + 1 < buffer_size
        ) {
            char c = data[position++];

            buffer[i++] = c;

            if (c == '\n')
                break;
        }

        buffer[i] = '\0';

        reader->source.string.position =
            position;
    }

    else {
        return NULL;
    }

    /*
     * Advance absolute position exactly once.
     */

    reader->bytepos +=
        (long)strlen(buffer);

    return buffer;
}

static int package_reader_seek(
    struct PackageReader *reader,
    long bytepos
)
{
    if (reader == NULL)
        return 1;

    if (reader->type == PACKAGE_READER_FILE) {

        if (
            fseek(
                reader->source.file,
                bytepos,
                SEEK_SET
            ) != 0
        )
            return 1;
    }

    else if (reader->type == PACKAGE_READER_STRING) {

        long base =
            reader->bytepos -
            (long)reader->source.string.position;

        if (bytepos < base)
            return 1;

        size_t position =
            (size_t)(bytepos - base);

        if (
            reader->source.string.data == NULL ||
            position > strlen(reader->source.string.data)
        )
            return 1;

        reader->source.string.position =
            position;
    }

    else {
        return 1;
    }

    reader->bytepos = bytepos;

    return 0;
}

static char *package_reader_read_section(
    struct PackageReader *reader
)
{
    if (reader == NULL)
        return NULL;

    long start_bytepos = reader->bytepos;

    char buffer[PATH_MAX];

    char *content = NULL;
    size_t content_size = 0;

    while (
        package_reader_getline(
            reader,
            buffer,
            sizeof(buffer)
        ) != NULL
    ) {
        /*
         * "-" belongs to the separator,
         * not to section content.
         */

        char *line_end = strpbrk(
            buffer,
            "\r\n"
        );

        size_t line_length =
            line_end != NULL
                ? (size_t)(line_end - buffer)
                : strlen(buffer);

        if (
            line_length == 1 &&
            buffer[0] == '-'
        ) {
            break;
        }


        /*
         * Keep original content exactly as
         * package_reader_getline() returned it.
         */

        size_t length = strlen(buffer);

        char *tmp = realloc(
            content,
            content_size + length + 1
        );

        if (tmp == NULL) {
            free(content);

            package_reader_seek(
                reader,
                start_bytepos
            );

            return NULL;
        }

        content = tmp;

        memcpy(
            content + content_size,
            buffer,
            length
        );

        content_size += length;
        content[content_size] = '\0';
    }


    /*
     * Return reader to the beginning
     * of the section.
     */

    if (
        package_reader_seek(
            reader,
            start_bytepos
        ) != 0
    ) {
        free(content);
        return NULL;
    }


    /*
     * Empty section.
     */

    if (content == NULL) {
        content = strdup("");

        if (content == NULL)
            return NULL;
    }

    return content;
}

/*
 * CATPKG 1.0 — PackageInfo
 */

static int catpkg_parse_v1_packageinfo_reader(
    struct PackageReader *reader,
    struct PackageInfo *info
)
{
    char buffer[PATH_MAX];

    long field_bytepos = 0;

    struct PackageField *last_field = NULL;

    size_t current_section = 0;

    char *section = NULL;
    size_t section_size = 0;
    long section_bytepos = 0;


    /*
     * CATPKG 1.0 — PackageInfo
     *
     * Section 0:
     * name
     * -
     *
     * Section 1:
     * version
     * -
     *
     * Section 2:
     * architecture
     * -
     *
     * Section 3:
     * description
     * -
     *
     * Section 4:
     * SHA
     * -
     *
     * Section 5:
     * size
     * -
     *
     * Section 6:
     * dependency
     * dependency
     * -
     *
     * Section 7:
     * #folder <path>
     * #persistent-file <path>
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
            field->section = current_section;                  \
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
     * Store a combined section field.
     *
     * Used by sections whose original contents must also
     * be available as one PackageField.
     */

    #define ADD_SECTION(field_name)                             \
        do {                                                   \
            if (section == NULL) {                             \
                ADD_FIELD(                                     \
                    field_name,                                \
                    "",                                        \
                    section_bytepos                            \
                );                                             \
            }                                                  \
            else {                                             \
                ADD_FIELD(                                     \
                    field_name,                                \
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


    /*
     * Append current line to combined section data.
     */

    #define APPEND_SECTION_LINE()                              \
        do {                                                   \
            if (section == NULL)                               \
                section_bytepos = field_bytepos;               \
                                                               \
            size_t line_length = strlen(buffer);               \
                                                               \
            char *tmp = realloc(                               \
                section,                                       \
                section_size + line_length + 2                 \
            );                                                 \
                                                               \
            if (tmp == NULL) {                                 \
                free(section);                                 \
                return 1;                                      \
            }                                                  \
                                                               \
            section = tmp;                                     \
                                                               \
            memcpy(                                            \
                section + section_size,                        \
                buffer,                                        \
                line_length                                    \
            );                                                 \
                                                               \
            section_size += line_length;                       \
            section[section_size++] = '\n';                    \
            section[section_size] = '\0';                      \
        } while (0)


    /*
     * Read PackageInfo.
     */

    while (1) {

        field_bytepos = reader->bytepos;

        if (
            package_reader_getline(
                reader,
                buffer,
                sizeof(buffer)
            ) == NULL
        ) {
            /*
             * Operations are the final section and therefore
             * do not require a trailing separator.
             */

            if (current_section == 7) {
                if (section == NULL)
                    section_bytepos = field_bytepos;

                ADD_SECTION("operations");
            }

            break;
        }

        buffer[strcspn(buffer, "\r\n")] = '\0';


        switch (current_section) {
            #include "pkginfo/SHEME/SHEME_PACKAGE_INFO_READER.inc"
        }
    }


    #undef APPEND_SECTION_LINE
    #undef ADD_SECTION
    #undef ADD_FIELD

    return 0;
}


/*
 * Compatibility wrapper.
 *
 * Existing code can continue calling:
 *
 *     catpkg_parse_v1_packageinfo(file, info);
 */

static int catpkg_parse_v1_packageinfo(
    FILE *file,
    struct PackageInfo *info
)
{
    struct PackageReader reader;

    /*
     * CATPKG_Parse() has already consumed:
     *
     * CATPKG 1.0\n
     *
     * Do not use ftell(), because file may be a pipe
     * returned by popen().
     */

    long bytepos =
        (long)strlen("CATPKG 1.0\n");

    package_reader_init_file(
        &reader,
        file,
        bytepos
    );

    return catpkg_parse_v1_packageinfo_reader(
        &reader,
        info
    );
}


/*
 * CATPKG 1.1 — FileInfo
 */

static int catpkg_parse_v1_fileinfo_reader(
    struct PackageReader *reader,
    struct PackageInfo *info
)
{
    char buffer[PATH_MAX];

    long field_bytepos = 0;

    struct PackageField *last_field = NULL;

    size_t current_section = 0;


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
            field->section = current_section;                  \
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
     * Read sections.
     */

    while (1) {

        field_bytepos = reader->bytepos;

        if (
            package_reader_getline(
                reader,
                buffer,
                sizeof(buffer)
            ) == NULL
        ) {
            break;
        }

        buffer[strcspn(buffer, "\r\n")] = '\0';


        /*
         * Parse according to current section.
         */

        switch (current_section) {
            #include "pkginfo/SHEME/SHEME_FILE_INFO_READER.inc"
        }
    }


    #undef ADD_FIELD

    return 0;
}


/*
 * Compatibility wrapper for FILE *.
 */

static int catpkg_parse_v1_fileinfo(
    FILE *file,
    struct PackageInfo *info
)
{
    struct PackageReader reader;

    long bytepos =
        (long)strlen("CATPKG 1.0\n");

    package_reader_init_file(
        &reader,
        file,
        bytepos
    );

    return catpkg_parse_v1_fileinfo_reader(
        &reader,
        info
    );
}

static int PackageInfo_UpdateSection(
    char *path,
    struct PackageInfo *info,
    size_t arg_section
)
{
    switch (info->patch) {

        case 1:
        {
            /*
             * Find root "info" field of requested section.
             */

            struct PackageField *section_field = NULL;

            struct PackageField *field =
                info->fields;

            while (field != NULL) {

                if (
                    field->section == arg_section &&
                    strcmp(field->name, "info") == 0
                ) {
                    section_field = field;
                    break;
                }

                field = field->next_field;
            }

            if (section_field == NULL) {
                fprintf(
                    stderr,
                    "catpkg: FileInfo section %zu not found\n",
                    arg_section
                );

                return 1;
            }


            /*
             * Save old section information.
             */

            long section_start =
                section_field->bytepos;

            size_t old_section_size =
                strlen(section_field->value);


            /*
             * Find field before this section.
             *
             * All FieldOptions of the section are located
             * before the root "info" field.
             */

            struct PackageField *before_section = NULL;

            field = info->fields;

            while (
                field != NULL &&
                field->section != arg_section
            ) {
                before_section = field;
                field = field->next_field;
            }


            /*
             * Remove old FieldOptions of this section,
             * but preserve the root "info" field.
             */

            field = (
                before_section != NULL
                    ? before_section->next_field
                    : info->fields
            );

            while (
                field != NULL &&
                field->section == arg_section
            ) {
                struct PackageField *next =
                    field->next_field;

                if (field != section_field) {
                    free(field->name);
                    free(field->value);
                    free(field);

                    info->fields_count--;
                }

                field = next;
            }


            /*
             * Field after the complete old section.
             */

            struct PackageField *after_section =
                section_field->next_field;


            /*
             * Temporarily detach root section field.
             *
             * New FieldOptions will be inserted before it.
             */

            if (before_section != NULL)
                before_section->next_field =
                    section_field;
            else
                info->fields =
                    section_field;

            section_field->next_field =
                after_section;


            /*
             * Open updated FileInfo.
             */

            FILE *file = fopen(path, "r");

            if (file == NULL) {
                perror("fopen");
                return 1;
            }

            if (
                fseek(
                    file,
                    section_start,
                    SEEK_SET
                ) != 0
            ) {
                perror("fseek");

                fclose(file);
                return 1;
            }


            /*
             * Create reader starting exactly at the
             * beginning of requested section.
             */

            struct PackageReader local_reader;
            struct PackageReader *reader = &local_reader;

            package_reader_init_file(
                reader,
                file,
                section_start
            );



            char buffer[PATH_MAX];

            long field_bytepos = 0;

            size_t current_section =
                arg_section;

            char *section = NULL;

            size_t section_size = 0;

            long section_bytepos =
                section_start;


            /*
             * New fields must be inserted immediately
             * before the existing root "info" field.
             */

            struct PackageField *last_field =
                before_section;


            /*
             * Helper for adding a FieldOption.
             */

            #define ADD_FIELD(field_name, field_value, field_bytepos)  \
                do {                                                   \
                    struct PackageField *new_field = malloc(          \
                        sizeof(*new_field)                             \
                    );                                                 \
                                                                       \
                    if (new_field == NULL) {                           \
                        free(section);                                 \
                        fclose(file);                                  \
                        return 1;                                      \
                    }                                                  \
                                                                       \
                    new_field->name = strdup(field_name);              \
                    new_field->value = strdup(field_value);            \
                    new_field->bytepos = field_bytepos;                \
                    new_field->section = arg_section;                  \
                    new_field->next_field = section_field;             \
                                                                       \
                    if (                                               \
                        new_field->name == NULL ||                     \
                        new_field->value == NULL                       \
                    ) {                                                \
                        free(new_field->name);                         \
                        free(new_field->value);                        \
                        free(new_field);                               \
                        free(section);                                 \
                        fclose(file);                                  \
                        return 1;                                      \
                    }                                                  \
                                                                       \
                    if (last_field == NULL)                            \
                        info->fields = new_field;                      \
                    else                                               \
                        last_field->next_field = new_field;            \
                                                                       \
                    last_field = new_field;                            \
                                                                       \
                    info->fields_count++;                              \
                } while (0)


            /*
             * Replace existing root "info" field instead
             * of creating another one.
             */

            #define ADD_SECTION()                                      \
                do {                                                   \
                    char *new_value;                                   \
                                                                       \
                    if (section != NULL)                               \
                        new_value = strdup(section);                    \
                    else                                               \
                        new_value = strdup("");                         \
                                                                       \
                    if (new_value == NULL) {                           \
                        free(section);                                 \
                        fclose(file);                                  \
                        return 1;                                      \
                    }                                                  \
                                                                       \
                    free(section_field->value);                        \
                                                                       \
                    section_field->value = new_value;                  \
                    section_field->bytepos = section_bytepos;          \
                    section_field->section = arg_section;              \
                                                                       \
                    if (last_field == NULL)                            \
                        info->fields = section_field;                  \
                    else                                               \
                        last_field->next_field = section_field;        \
                                                                       \
                    section_field->next_field = after_section;         \
                                                                       \
                    free(section);                                     \
                    section = NULL;                                    \
                    section_size = 0;                                  \
                    section_bytepos = 0;                               \
                } while (0)


            /*
             * Read only requested section.
             */

            while (1) {

                field_bytepos =
                    reader -> bytepos;

                if (
                    package_reader_getline(
                        reader,
                        buffer,
                        sizeof(buffer)
                    ) == NULL
                ) {
                    /*
                     * EOF also closes the final section.
                     */

                    ADD_SECTION();

                    break;
                }

                buffer[
                    strcspn(buffer, "\r\n")
                ] = '\0';


                /*
                 * Parse requested FileInfo section.
                 *
                 * arg_section > 0 enters default case.
                 */
                struct PackageField *section_field = NULL;
                
                switch (current_section) {

                    #include "pkginfo/SHEME/SHEME_FILE_INFO_READER.inc"

                }


                /*
                 * The scheme increments current_section
                 * after encountering '-'.
                 *
                 * Therefore requested section is finished.
                 */

                if (current_section != arg_section)
                    break;
            }


            #undef ADD_SECTION
            #undef ADD_FIELD


            fclose(file);


            /*
             * Calculate physical section size change.
             *
             * section_field->value contains every physical
             * line of the section including '\n', but not
             * the '-' separator.
             */

            size_t new_section_size =
                strlen(section_field->value);

            off_t section_delta =
                (off_t)new_section_size -
                (off_t)old_section_size;


            /*
             * Shift all following fields.
             */

            if (section_delta != 0) {

                field =
                    section_field->next_field;

                while (field != NULL) {

                    field->bytepos +=
                        section_delta;

                    field =
                        field->next_field;
                }
            }

            break;
        }
    }

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

            info->patch = 0;
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

            info->patch = 1;
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