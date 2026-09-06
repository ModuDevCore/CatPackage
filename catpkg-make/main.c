#include "install.h"
#include "remove.h"
#include "build.h"
#include "database.h"
#include "utils/fs.h"
#include "catpkg/pkginfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <dirent.h>

#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdint.h>

#include <fcntl.h>
#include <ftw.h>

// Manual build of catpkg-make-x86_64
// gcc -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Wextra -Wpedantic -Iinclude -c bootstrap/bootstrap.c -o ./bootstrap.o 
// gcc -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -Wextra -Wpedantic -Iinclude catpkg-make/main.c "bootstrap.o" -static -o "catpkg-make-x86_64" 

#define GREETING \
    "Welcome to the CATPKG utility builder! Choose which build you need:\n" \
    "1) Building the catpkg-(aarch64/x86_64)@X.X.X.catpackage\n" \
    "2) Building catpkg-installer-(aarch64/x86_64)@X.X.X ( catpkg-(aarch64/x86_64)@X.X.X.catpackage build is required )\n" \
    "3) catpkg-make-(aarch64/x86_64) ( Update )\n"
#define CHOOSE_A10E \
    "Select the processor architecture:\n" \
    "1) x86_64\n" \
    "2) aarch64\n"

#define GCC_AARCH64 "aarch64-linux-gnu-gcc"
#define GCC_X86_64  "gcc"

static int remove_entry(
    const char *path,
    const struct stat *st,
    int typeflag,
    struct FTW *ftwbuf
)
{
    (void)st;
    (void)typeflag;
    (void)ftwbuf;

    return remove(path);
}

int remove_tmp_directory(void)
{
    printf(
        "[INFO] Removing the TMP directory: %s\n",
        ".tmp"
    );
    /*
     * Remove old temporary installer directory.
     */
    if (nftw(
            ".tmp",
            remove_entry,
            16,
            FTW_DEPTH | FTW_PHYS
        ) != 0) {

        if (errno != ENOENT)
            return 1;
    }

    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    int input = open(src, O_RDONLY);

    if (input < 0) {
        perror(src);
        return 1;
    }

    int output = open(
        dst,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644
    );

    if (output < 0) {
        perror(dst);
        close(input);
        return 1;
    }

    char buffer[65536];
    ssize_t bytes_read;

    while ((bytes_read = read(input, buffer, sizeof(buffer))) > 0) {
        ssize_t total_written = 0;

        while (total_written < bytes_read) {
            ssize_t bytes_written = write(
                output,
                buffer + total_written,
                bytes_read - total_written
            );

            if (bytes_written < 0) {
                perror("write");
                close(input);
                close(output);
                return 1;
            }

            total_written += bytes_written;
        }
    }

    if (bytes_read < 0) {
        perror("read");
        close(input);
        close(output);
        return 1;
    }

    if (close(input) < 0) {
        perror("close");
        close(output);
        return 1;
    }

    if (close(output) < 0) {
        perror("close");
        return 1;
    }

    return 0;
}

int catpkg_embed_binary(
    const char *input_path,
    FILE *output
)
{
    if (input_path == NULL || output == NULL) {
        errno = EINVAL;
        return 1;
    }

    FILE *input = fopen(input_path, "rb");

    if (input == NULL)
        return 1;

    if (fseek(input, 0, SEEK_END) != 0) {
        fclose(input);
        return 1;
    }

    long file_size = ftell(input);

    if (file_size < 0) {
        fclose(input);
        return 1;
    }

    rewind(input);

    size_t package_size = (size_t)file_size;

    if (fprintf(
        output,
        "#include <stddef.h>\n"
        "#include <stdint.h>\n\n"
        "const uint8_t catpkg_embedded_package[] = {\n"
    ) < 0) {
        fclose(input);
        return 1;
    }

    uint8_t buffer[4096];
    size_t offset = 0;
    size_t read;

    while ((read = fread(buffer, 1, sizeof(buffer), input)) > 0) {

        for (size_t i = 0; i < read; i++) {

            if (fprintf(
                output,
                "    0x%02X%s",
                buffer[i],
                ((offset + 1) % 12 == 0) ? "\n" : ""
            ) < 0) {
                fclose(input);
                return 1;
            }

            /*
             * Добавляем запятую между элементами.
             */
            if (offset + 1 < package_size) {
                if (fputc(',', output) == EOF) {
                    fclose(input);
                    return 1;
                }
            }

            offset++;
        }
    }

    if (ferror(input)) {
        fclose(input);
        return 1;
    }

    fclose(input);

    if (fprintf(
        output,
        "\n};\n\n"
        "const size_t catpkg_embedded_package_size = %zu;\n",
        package_size
    ) < 0) {
        return 1;
    }

    return 0;
}
int bootstrap_update(char* CC) {
    printf("Building bootstrap...\n");

    char compiler[PATH_MAX];

    snprintf(
        compiler,
        sizeof(compiler),
        "/usr/bin/%s",
        CC
    );

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execl(
            compiler,
            CC,
            "-D_DEFAULT_SOURCE",
            "-D_XOPEN_SOURCE=700",
            "-std=c23",
            "-Wextra",
            "-Wpedantic",
            "-Iinclude",
            "-c",
            "bootstrap/bootstrap.c",
            "-o",
            "./bootstrap.o",
            (char *)NULL
        );

        perror("gcc");
        _exit(127);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    if (!WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {

        fprintf(stderr, "gcc: compilation failed\n");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    printf(
        "========================================\n"
    );

    printf(
        "        CATPKG MAKE\n"
    );
    printf(
        "The CATPKG utility builder.\n"
    );

    printf(
        "========================================\n"
    );

    printf(GREETING);

    printf("Enter a number: ");

    char c = getchar();

    while (getchar() != '\n')
    ;
    if (mkdir(".tmp", 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return 1;
    }

    switch (c) {

        case '1':
        {
            /*
             * Remove leftovers from a previous installer run.
             */
            if (remove_tmp_directory() != 0) {

                fprintf(
                    stderr,
                    "[FATAL] Failed to remove old temporary directory.\n"
                );

                return 1;
            }

            if (mkdir(".tmp", 0755) != 0 && errno != EEXIST) {
                perror("mkdir");
                return 1;
            }

            if (mkdir(".tmp/usr", 0755) != 0 && errno != EEXIST) {
                perror("mkdir");
                return 1;
            }

            if (mkdir(".tmp/usr/bin", 0755) != 0 && errno != EEXIST) {
                perror("mkdir");
                return 1;
            }
            chdir(".tmp");

            printf(CHOOSE_A10E);
            
            printf("Enter a number: ");

            char cc = getchar();

            while (getchar() != '\n')
                ;

            printf("Building \"catpkg-(aarch64/x86_64)@X.X.X.catpackage\"...\n");
            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
                return 1;
            }

            if (pid == 0) {
                switch (cc) {
                    case '1':
                    execl(
                        "/bin/sh",
                        "sh",
                        "-c",
                        GCC_X86_64 " -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE "
                        "-Os -ffunction-sections -fdata-sections "
                        "-Wextra -Wpedantic -I../include "
                        "../src/*.c "
                        "../src/utils/*.c "
                        "../src/utils/catpkg/*.c "
                        "-Wl,--gc-sections "
                        "-static "
                        "-o usr/bin/catpkg",
                        (char *)NULL
                    );                
                        break;

                    case '2':
                        execl(
                            "/bin/sh",
                            "sh",
                            "-c",
                            GCC_AARCH64 " -std=c23 -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE "
                            "-Os -ffunction-sections -fdata-sections "
                            "-Wextra -Wpedantic -I../include "
                            "../src/*.c "
                            "../src/utils/*.c "
                            "../src/utils/catpkg/*.c "
                            "-Wl,--gc-sections "
                            "-static "
                            "-o usr/bin/catpkg",
                            (char *)NULL
                        );  
                        break;

                    default:
                        printf("Invalid architecture.\n");
                        break;
                }

                perror("gcc");
                _exit(127);
            }

            int status;

            if (waitpid(pid, &status, 0) < 0) {
                perror("waitpid");
                return 1;
            }

            if (!WIFEXITED(status) ||
                WEXITSTATUS(status) != 0) {

                fprintf(stderr, "gcc: compilation failed\n");
                return 1;
            }

            if (copy_file(
                "../build-data/PACKAGEINFO",
                "PACKAGEINFO"
            ) != 0) {
                return 1;
            }
        switch (cc) {
                case '1':
                catpkg_build("x86_64");            
                    break;

                case '2':
                catpkg_build("aarch64");             
                    break;

                default:
                    printf("Invalid architecture.\n");
                    break;
            }

            printf("Compilation successful.\n");

            break;
        }

        case '2':
        {
                printf(CHOOSE_A10E);
                
                printf("Enter a number: ");

                char cc = getchar();

                while (getchar() != '\n')
                    ;

                switch (cc) {
                    case '1':
                        bootstrap_update(GCC_X86_64);
                        break;

                    case '2':
                        bootstrap_update(GCC_AARCH64);
                        break;

                    default:
                        printf("Invalid architecture.\n");
                        break;
                }

                printf("Search for “catpkg” and “.catpackage” package in the .tmp folder...\n");
                // Searching for the “cc” architecture package
                DIR *dir = opendir(".tmp");
                char catpkg_path[PATH_MAX] = "";

                if (dir == NULL) {
                    perror("opendir");
                    return 1;
                }

                struct dirent *entry;

                while ((entry = readdir(dir)) != NULL) {

                    if (strcmp(entry->d_name, ".") == 0 ||
                        strcmp(entry->d_name, "..") == 0)
                        continue;

                    const char *extension = strrchr(entry->d_name, '.');

                    if (extension != NULL &&
                        strcmp(extension, ".catpackage") == 0) {
                        snprintf(
                            catpkg_path,
                            sizeof(catpkg_path),
                            ".tmp/%s",
                            entry->d_name
                        );
                        struct PackageInfo *info = CATPKG_Parse(catpkg_path);
                        if(info == NULL) {
                            catpkg_path[0] = '\0';
                            continue;
                        }
                        const struct PackageField *name_field = PackageInfo_Find(info, "name");
                        const struct PackageField *architecture_field = PackageInfo_Find(info, "architecture");

                        if(name_field == NULL ||
                            architecture_field == NULL ||
                            name_field -> value == NULL || 
                            architecture_field -> value == NULL) {
                            catpkg_path[0] = '\0';
                            continue;
                        }
                        if (strcmp(name_field->value, "catpkg") == 0) {
                            switch (cc) {
                                case '1':
                                    if (strcmp(architecture_field->value, "x86_64") != 0)
                                        catpkg_path[0] = '\0';
                                    break;

                                case '2':
                                    if (strcmp(architecture_field->value, "aarch64") != 0)
                                        catpkg_path[0] = '\0';
                                    break;

                                default:
                                    printf("Invalid architecture.\n");
                                    break;
                            }
                        }
                        else {
                            catpkg_path[0] = '\0';
                        }
                    }
                }

                closedir(dir);
                
                if(catpkg_path[0] == '\0') {
                    fprintf(stderr, "Error: “catpkg” was not found in the .tmp folder; first, build the package.\nRecommendation: Run “catpkg-make” again and select option 1.\n");
                    return 1;
                }

                printf("The package \"%s\" has been found.\n", catpkg_path);
                int pipefd[2];

                if (pipe(pipefd) != 0) {
                    perror("pipe");
                    return 1;
                }

                pid_t pid = fork();

                if (pid < 0) {
                    perror("fork");
                    return 1;
                }

                if (pid == 0) {
                    close(pipefd[1]);

                    if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                        perror("dup2");
                        _exit(1);
                    }

                    close(pipefd[0]);

                    int devnull = open("/dev/null", O_WRONLY);

                    if (devnull < 0) {
                        perror("open");
                        _exit(1);
                    }

                    if (dup2(devnull, STDOUT_FILENO) < 0) {
                        perror("dup2");
                        _exit(1);
                    }

                    close(devnull);

                    switch (cc) {
                        case '1':
                            execl(
                                "/usr/bin/" GCC_X86_64,
                                GCC_X86_64,
                                "-D_DEFAULT_SOURCE",
                                "-D_XOPEN_SOURCE=700",
                                "-std=c23",
                                "-Wextra",
                                "-Wpedantic",
                                "-Iinclude",
                                "installer/main.c",
                                "bootstrap.o",
                                "-x", "c",
                                "-",
                                "-static",
                                "-o",
                                "catpkg-installer-x86_64",
                                (char *)NULL
                            );
                            break;

                        case '2':
                            execl(
                                "/usr/bin/" GCC_AARCH64,
                                GCC_AARCH64,
                                "-D_DEFAULT_SOURCE",
                                "-D_XOPEN_SOURCE=700",
                                "-std=c23",
                                "-Wextra",
                                "-Wpedantic",
                                "-Iinclude",
                                "installer/main.c",
                                "bootstrap.o",
                                "-x", "c",
                                "-",
                                "-static",
                                "-o",
                                "catpkg-installer-aarch64",
                                (char *)NULL
                            );
                            break;

                        default:
                            _exit(1);
                    }

                    perror("execl");
                    _exit(1);
                }
                close(pipefd[0]);

                FILE *gcc_stdin = fdopen(pipefd[1], "w");

                if (gcc_stdin == NULL) {
                    perror("fdopen");
                    close(pipefd[1]);
                    return 1;
                }

                if (catpkg_embed_binary(
                        catpkg_path,
                        gcc_stdin
                    ) != 0) {

                    perror("catpkg_embed_binary");

                    fclose(gcc_stdin);
                    return 1;
                }

                fclose(gcc_stdin);

                waitpid(pid, NULL, 0);

                printf("Compilation successful.\n");
            break;
        }

        case '3':
        {
                printf(CHOOSE_A10E);
                
                printf("Enter a number: ");

                char cc = getchar();

                while (getchar() != '\n')
                    ;

                switch (cc) {
                    case '1':
                        bootstrap_update(GCC_X86_64);
                        break;

                    case '2':
                        bootstrap_update(GCC_AARCH64);
                        break;

                    default:
                        printf("Invalid architecture.\n");
                        break;
                }

                printf("Building catpkg-make...\n");
                pid_t pid = fork();

                if (pid < 0) {
                    perror("fork");
                    return 1;
                }

                if (pid == 0) {
                    switch (cc) {
                        case '1':
                            execl(
                                "/usr/bin/" GCC_X86_64,
                                GCC_X86_64,
                                "-D_DEFAULT_SOURCE",
                                "-D_XOPEN_SOURCE=700",
                                "-std=c23",
                                "-Wextra",
                                "-Wpedantic",
                                "-Iinclude",
                                "catpkg-make/main.c",
                                "bootstrap.o",
                                "-static",
                                "-o",
                                "catpkg-make-x86_64",
                                (char *)NULL
                            );                        
                            break;

                        case '2':
                            execl(
                                "/usr/bin/" GCC_AARCH64,
                                GCC_AARCH64,
                                "-D_DEFAULT_SOURCE",
                                "-D_XOPEN_SOURCE=700",
                                "-std=c23",
                                "-Wextra",
                                "-Wpedantic",
                                "-Iinclude",
                                "catpkg-make/main.c",
                                "bootstrap.o",
                                "-static",
                                "-o",
                                "catpkg-make-aarch64",
                                (char *)NULL
                            );
                            break;

                        default:
                            printf("Invalid architecture.\n");
                            break;
                    }

                    perror("gcc");
                    _exit(127);
                }

                int status;

                if (waitpid(pid, &status, 0) < 0) {
                    perror("waitpid");
                    return 1;
                }

                if (!WIFEXITED(status) ||
                    WEXITSTATUS(status) != 0) {

                    fprintf(stderr, "gcc: compilation failed\n");
                    return 1;
                }

                printf("Compilation successful.\n");
            break;
        }

        default:
            printf("Unknown option.\n");
            break;
    }

    return 0;
}