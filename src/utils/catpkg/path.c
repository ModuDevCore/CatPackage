#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

char *make_catpkg_path(
    const char *format,
    ...
)
{
    va_list args;
    va_list args_copy;

    /*
     * First pass: calculate required string length.
     */
    va_start(args, format);
    va_copy(args_copy, args);

    int len = vsnprintf(
        NULL,
        0,
        format,
        args
    );

    va_end(args);

    if (len < 0) {
        va_end(args_copy);
        return NULL;
    }

    char *path = malloc(
        (size_t)len + 1
    );

    if (path == NULL) {
        va_end(args_copy);
        return NULL;
    }

    /*
     * Second pass: actually write the formatted string.
     */
    vsnprintf(
        path,
        (size_t)len + 1,
        format,
        args_copy
    );

    va_end(args_copy);

    return path;
}