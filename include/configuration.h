#ifndef CATPKG_PACKAGEMANAGER_CONFIGURATION_H
#define CATPKG_PACKAGEMANAGER_CONFIGURATION_H

#define CATPKG_PACKAGEINFO "PACKAGEINFO"
#define CATPKG_FILE "FILEINFO"
#define CATPKG_DATABASE "DATABASE"
#define CATPKG_DEPENDENCIES "DEPENDENCIES"

#define CATPKG_PACKAGE_DATA "/var/lib/%s"

#define CATPKG_PACKAGE_ROOT "/var/lib/catpkg"

#define CATPKG_PACKAGES_PATH CATPKG_PACKAGE_ROOT "/packages"
#define CATPKG_DATABASE_DIR_PATH "/var/lib/catpkg-database"
#define CATPKG_COMMANDS_DIR_PATH CATPKG_PACKAGE_ROOT "/commands"
#define CATPKG_PACKAGE_PATH CATPKG_PACKAGES_PATH "/%s"
#define CATPKG_METADATA_PATH CATPKG_PACKAGE_PATH "/metadata"
#define CATPKG_FILES_PATH CATPKG_PACKAGE_PATH "/files"
#define CATPKG_BUILD_PATH "/tmp/catpkg/build"
#define CATPKG_TMP_FILES_PATH "/tmp/catpkg/files"
#define CATPKG_TMP_METADATA_PATH "/tmp/catpkg/metadata"
#define CATPKG_CACHE_CATPACKAGE "%s.catpackage"
#define CATPKG_CACHE_CATPACKAGE_ZSTD "%s.catpackage.zst"
#define CATPKG_CACHE "/var/cache/catpkg"
#define CATPKG_CACHE_PACKAGES CATPKG_CACHE "/packages"

#define CATPKG_COMMAND_EXECUTE \
    "\n" \
    "execute \"$@\""

#define CATPKG_COMMAND_CHECK_METAINFO \
    "\n" \
    "get-meta-command-info name;" \
    "printf '\\0';" \
    "get-meta-command-info description;" \
    "printf '\\0';" \
    "get-meta-command-info declaration;" \
    "printf '\\0';"

#define CATPKG_COMMAND_DECLARATION_DESCRIPTION_METAINFO \
    "\n" \
    "get-meta-command-info declaration;" \
    "printf '\\0';" \
    "get-meta-command-info description;" \
    "printf '\\0';"

#define CATPKG_HELP \
    "Usage: catpkg <command> [options]\n" \
    "\n" \
    "Commands:\n"
#define ALLOW_INSTALLATION \
    "========================================\n" \
    "Installing: %s\n" \
    "Package size: %s\n" \
    "Will be installed: %s\n" \
    "Changing the size: %c%s\n"
#define ALLOW_UPDATE \
    "========================================\n" \
    "Update: %s\n" \
    "Package size: %s\n" \
    "Additional space: %s\n"
#define ALLOW_REMOVE \
    "========================================\n" \
    "Removal: %s\n" \
    "Will be released: %s\n"
#endif