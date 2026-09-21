#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>

#include "configuration.h"
#include "install.h"
#include "verify.h"
#include "remove.h"
#include "database.h"
#include "cli.h"
#include "build.h"
#include "update.h"

#include "catpkg/pkginfo.h"
#include "catpkg/path.h"
#include "catpkg/command.h"

enum Command {
    COMMAND_UNKNOWN = 0,
    COMMAND_INSTALL,
    COMMAND_REMOVE,
    COMMAND_VERIFY,
    COMMAND_DATABASE,
    COMMAND_BUILD,
    COMMAND_UPDATE,
    COMMAND_HELP,
    COMMAND_BY_PACKAGE
};

struct CatpkgBuiltinCommand {
    const char *declaration;
    const char *description;
};

static const struct CatpkgBuiltinCommand builtin_commands[] = {
    {
        "install <package>",
        "Install a package"
    },
    {
        "remove <package>",
        "Remove a package"
    },
    {
        "verify <package>",
        "Verify a package"
    },
    {
        "database <command>",
        "Manage package database"
    },
    {
        "build",
        "Build a .catpackage"
    },
    {
        "update <package>",
        "Update a package"
    },
    {
        "help",
        "Show this help"
    }
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
    
    char *catpkg_command_path =
        make_catpkg_path(
            CATPKG_COMMANDS_DIR_PATH "/%s",
            command
        );

    if(access(catpkg_command_path, F_OK) == 0)
        return COMMAND_BY_PACKAGE;

    free(catpkg_command_path);

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
    enum Command command;
    if (argc < 2)
        command = COMMAND_HELP;
    else
        command = catpkg_parse_command(argv[1]);

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

                    print_package_separator("", package_fullname);

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
        {
            printf("%s", CATPKG_HELP);

            DIR *dir =
                opendir(CATPKG_COMMANDS_DIR_PATH);

            if (dir != NULL) {
                struct dirent *entry;

                while ((entry = readdir(dir)) != NULL) {
                    if (
                        strcmp(entry->d_name, ".") == 0 ||
                        strcmp(entry->d_name, "..") == 0
                    ) {
                        continue;
                    }

                    char *command_declaration = NULL;
                    char *command_description = NULL;

                    int fd[2];

                    if (pipe(fd) == -1) {
                        perror("pipe");
                        return 1;
                    }

                    pid_t pid = fork();

                    if (pid == -1) {
                        perror("fork");

                        close(fd[0]);
                        close(fd[1]);

                        return 1;
                    }


                    /*
                     * --------------------------------------------------------
                     * Child
                     * --------------------------------------------------------
                     */

                    if (pid == 0) {

                        char *command_path =
                            make_catpkg_path(
                                CATPKG_COMMANDS_DIR_PATH "/%s",
                                entry->d_name
                            );
                        close(fd[0]);

                        if (dup2(fd[1], STDOUT_FILENO) == -1) {
                            perror("dup2");

                            close(fd[1]);
                            _exit(1);
                        }

                        close(fd[1]);


                        /*
                         * $0 = command_path
                         *
                         * Load command script and request:
                         *
                         * declaration\0description\0
                         */

                        execl(
                            "/usr/bin/bash",
                            "bash",
                            "-c",
                            "source \"$0\"; "
                            CATPKG_COMMAND_DECLARATION_DESCRIPTION_METAINFO,
                            command_path,
                            (char *)NULL
                        );

                        free(command_path);

                        perror("execl");
                        _exit(127);
                    }


                    /*
                     * --------------------------------------------------------
                     * Parent
                     * --------------------------------------------------------
                     */

                    close(fd[1]);


                    /*
                     * declaration\0
                     */
                    if (
                        read_meta_command_field(
                            fd[0],
                            &command_declaration
                        ) != 0
                    ) {
                        close(fd[0]);

                        waitpid(pid, NULL, 0);

                        return 1;
                    }


                    /*
                     * description\0
                     */
                    if (
                        read_meta_command_field(
                            fd[0],
                            &command_description
                        ) != 0
                    ) {
                        free(command_declaration);

                        close(fd[0]);

                        waitpid(pid, NULL, 0);

                        return 1;
                    }


                    close(fd[0]);


                    int status;

                    if (waitpid(pid, &status, 0) == -1) {
                        perror("waitpid");

                        free(command_declaration);
                        free(command_description);

                        return 1;
                    }

                    if (
                        !WIFEXITED(status) ||
                        WEXITSTATUS(status) != 0
                    ) {
                        free(command_declaration);
                        free(command_description);

                        return 1;
                    }


                    /*
                     * Metadata is ready.
                     */

                    printf(
                        "  %-22s %s\n",
                        command_declaration,
                        command_description
                    );


                    free(command_declaration);
                    free(command_description);
                }

                closedir(dir);
            }

            size_t count =
                sizeof(builtin_commands) /
                sizeof(builtin_commands[0]);

            for (size_t i = 0; i < count; i++) {
                printf(
                    "  %-22s %s\n",
                    builtin_commands[i].declaration,
                    builtin_commands[i].description
                );
            }



            return 0;
        }

        case COMMAND_BY_PACKAGE:
        {
            pid_t pid = fork();

            if (pid == -1) {
                perror("fork");
                return 1;
            }


            /*
             * --------------------------------------------------------
             * Child
             * --------------------------------------------------------
             */

            if (pid == 0) {

                char *command_path = 
                    make_catpkg_path(
                        CATPKG_COMMANDS_DIR_PATH "/%s",
                        argv[1]
                    );


                /*
                 * bash -c:
                 *
                 *   $0 = command_path
                 *   $1 = argv[2]
                 *   $2 = argv[3]
                 *   ...
                 *
                 * First source the command file,
                 * then execute the command function.
                 */

                size_t command_argc =
                    argc >= 2
                        ? (size_t)(argc - 2)
                        : 0;


                /*
                 * argv:
                 *
                 * [0] bash
                 * [1] -c
                 * [2] source "$0"; execute "$@"
                 * [3] command_path
                 * [4...] command arguments
                 * [last] NULL
                 */

                char **bash_argv =
                    malloc(
                        (
                            4 +
                            command_argc +
                            1
                        ) * sizeof(*bash_argv)
                    );

                if (bash_argv == NULL)
                    _exit(1);


                size_t i = 0;

                bash_argv[i++] = "bash";
                bash_argv[i++] = "-c";

                bash_argv[i++] =
                    "source \"$0\"; "
                    CATPKG_COMMAND_EXECUTE;

                /*
                 * Becomes $0 inside bash.
                 */
                bash_argv[i++] = command_path;


                /*
                 * Everything after:
                 *
                 * catpkg <command> ...
                 *
                 * becomes $@.
                 */
                for (int j = 2; j < argc; j++)
                    bash_argv[i++] = argv[j];


                bash_argv[i] = NULL;


                execv(
                    "/usr/bin/bash",
                    bash_argv
                );


                /*
                 * execv() only returns on error.
                 */

                perror("execv");

                free(bash_argv);
                free(command_path);

                _exit(127);
            }


            /*
             * --------------------------------------------------------
             * Parent
             * --------------------------------------------------------
             */

            int status;

            if (
                waitpid(
                    pid,
                    &status,
                    0
                ) == -1
            ) {
                perror("waitpid");
                return 1;
            }


            if (WIFEXITED(status))
                return WEXITSTATUS(status);


            if (WIFSIGNALED(status)) {
                fprintf(
                    stderr,
                    "catpkg: command terminated by signal %d\n",
                    WTERMSIG(status)
                );

                return 128 + WTERMSIG(status);
            }


            return 1;
        }

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