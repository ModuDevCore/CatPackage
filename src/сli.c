#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "configuration.h"
#include "install.h"
#include "verify.h"
#include "remove.h"
#include "database.h"
#include "cli.h"
#include "build.h"
#include "update.h"

#include "catpkg/pkginfo.h"

enum Command {
    COMMAND_UNKNOWN = 0,
    COMMAND_INSTALL,
    COMMAND_REMOVE,
    COMMAND_VERIFY,
    COMMAND_DATABASE,
    COMMAND_BUILD,
    COMMAND_UPDATE,
    COMMAND_HELP,
};


static int catpkg_parse_arch(
    const char *option
)
{
    if (strcmp(option, "--aarch64") == 0)
        return 1;

    if (strcmp(option, "--x86_64") == 0)
        return 2;

    return 0;
}

static enum Command catpkg_parse_command(
    const char *command
)
{
    if (strcmp(command, "install") == 0)
        return COMMAND_INSTALL;

    if (strcmp(command, "remove") == 0)
        return COMMAND_REMOVE;

    if (strcmp(command, "verify") == 0)
        return COMMAND_VERIFY;

    if (strcmp(command, "database") == 0)
        return COMMAND_DATABASE;

    if (strcmp(command, "build") == 0)
        return COMMAND_BUILD;
    if (strcmp(command, "update") == 0)
        return COMMAND_UPDATE;
    if (strcmp(command, "help") == 0)
        return COMMAND_HELP;

    return COMMAND_UNKNOWN;
}

void print_package_separator(
    const char *action,
    const char *package_fullname
)
{
    const int width = 60;

    char text[256];

    snprintf(
        text,
        sizeof(text),
        "  %s %s  ",
        action,
        package_fullname
    );

    int text_len = (int)strlen(text);

    if (text_len >= width) {
        printf("%s\n", text);
        return;
    }

    int remaining = width - text_len;

    int left = remaining / 2;
    int right = remaining - left;

    for (int i = 0; i < left; i++)
        putchar('-');

    fputs(text, stdout);

    for (int i = 0; i < right; i++)
        putchar('-');

    putchar('\n');
}

/*
 * Check whether the current process has
 * root privileges.
 */
static int catpkg_require_root(void)
{
    if (geteuid() == 0)
        return 0;

    fprintf(
        stderr,
        "catpkg: this command must be run as root.\n"
    );

    fprintf(
        stderr,
        "catpkg: please run it with sudo or as root.\n"
    );

    return 1;
}


int catpkg_cli(
    int argc,
    char **argv
)
{
    if (argc < 2) {

        fprintf(
            stderr,
            CATPKG_HELP
        );

        return 1;
    }


    enum Command command =
        catpkg_parse_command(argv[1]);


    switch (command) {

        case COMMAND_INSTALL:

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg install <package>\n"
                );

                return 1;
            }


            if (catpkg_require_root() != 0)
                return 1;


            return catpkg_install(
                argv[2],
                false
            );        
        case COMMAND_UPDATE:

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg update <package>\n"
                );

                return 1;
            }


            if (catpkg_require_root() != 0)
                return 1;


            return catpkg_update(
                argv[2]
            );


        case COMMAND_REMOVE:

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg remove <package>\n"
                );

                return 1;
            }


            if (catpkg_require_root() != 0)
                return 1;


            return catpkg_remove(
                argv[2],
                false
            );


        case COMMAND_VERIFY:

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg verify <package>\n"
                );

                return 1;
            }


            return catpkg_verify(
                argv[2]
            );


        case COMMAND_DATABASE:

            /*
             * Database commands will be handled here.
             */

            if (argc < 3) {

                fprintf(
                    stderr,
                    "Usage: catpkg database <command>\n"
                );

                return 1;
            }


            if (strcmp(argv[2], "list") == 0) {
                struct PackageMatches packages =
                    catpkg_find_package("");


                for (
                    size_t p_i = 0;
                    p_i < packages.count;
                    p_i++
                ) {
                    struct PackageMatches versions =
                        catpkg_find_version(
                            packages.items[p_i]
                        );

                    char package_fullname[256];

                    snprintf(
                        package_fullname,
                        sizeof(package_fullname),
                        "%s@%s",
                        packages.items[p_i],
                        versions.items[0]
                    );

                    struct PackageInfo *info = catpkg_find_package_info(package_fullname);
                    const struct PackageField *description_field = PackageInfo_Find(info, "description");
                    struct PackageFieldMatches dependency_fields = PackageInfo_FindAll(info, "dependency");

                    print_package_separator("", "catpkg@1.0.0-beta");

                    printf(
                        "Name: %s\n",
                        packages.items[p_i]
                    );
                    printf(
                        "Version:"
                    );


                    for (
                        size_t v_i = 0;
                        v_i < versions.count;
                        v_i++
                    ) {

                        printf(
                            " %s",
                            versions.items[v_i]
                        );
                    }


                    printf(
                        "\n"
                    );

                    printf("Description: ");
                    if(description_field != NULL)
                        printf("%s\n", description_field -> value);
                    else
                        printf("\n");

                    printf("Dependencies: ");
                    for (
                        size_t dp_i = 0;
                        dp_i < dependency_fields.count;
                        dp_i++
                    ) {
                        if(dependency_fields.items[dp_i] != NULL)
                            printf("%s ", dependency_fields.items[dp_i] -> value);
                    }
                    PackageFieldMatches_Free(&dependency_fields);

                    printf(
                        "\n"
                    );
                    printf(
                        "\n"
                    );

                    PackageMatches_Free(
                        &versions
                    );
                    PackageInfo_Free(
                        info
                    );
                }

                PackageMatches_Free(
                    &packages
                );
            }


            return 0;


        case COMMAND_BUILD:
            int arch = 0;
            if(argc > 2) {
                arch = catpkg_parse_arch(argv[2]);
            }
            if(arch == 1)
                return catpkg_build("aarch64");
            else if(arch ==2)
                return catpkg_build("x86_64");
            else
                return catpkg_build(NULL);

        case COMMAND_HELP:
            printf(CATPKG_HELP);
            return 0;

        case COMMAND_UNKNOWN:
        default:

            fprintf(
                stderr,
                "catpkg: unknown command \"%s\"\nTry \"catpkg help\"\n",
                argv[1]
            );

            return 1;
    }
}