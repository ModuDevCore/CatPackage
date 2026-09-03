#include <stdio.h>
#include <string.h>
#include "cli.h"

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "list") == 0) {

        catpkg_cli(
            3,
            (char *[]) {
                argv[0],
                "database",
                "list",
                NULL
            }
        );

        return 0;
    }

    catpkg_cli(argc, argv);

    return 0;
}