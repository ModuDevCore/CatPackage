#ifndef CATPKG_PACKAGEMANAGER_CONFIGURATION_H
#define CATPKG_PACKAGEMANAGER_CONFIGURATION_H

#define CATPKG_PACKAGEINFO "PACKAGEINFO"

#define CATPKG_PACKAGE_ROOT "/var/lib/catpkg"

#define CATPKG_METADATA_PATH CATPKG_PACKAGE_ROOT "/%s/metadata"
#define CATPKG_FILES_PATH CATPKG_PACKAGE_ROOT "/%s/files"
#define CATPKG_PACKAGE_PATH CATPKG_PACKAGE_ROOT "/%s"
#define CATPKG_BUILD_PATH "/tmp/catpkg/build"
#define CATPKG_TMP_FILES_PATH "/tmp/catpkg/files"
#define CATPKG_TMP_METADATA_PATH "/tmp/catpkg/metadata"
#define CATPKG_CACHE_CATPACKAGE "%s.catpackage"
#define CATPKG_CACHE_CATPACKAGE_ZSTD "%s.catpackage.zst"
#define CATPKG_CACHE_PACKAGES "/var/cache/catpkg/packages"

#define CATPKG_HELP \
    "Usage: catpkg <command> [options]\n" \
    "\n" \
    "Commands:\n" \
    "  install <package>   Install a package\n" \
    "  remove <package>    Remove a package\n" \
    "  verify <package>    Verify a package\n" \
    "  database <command>  Manage package database\n" \
    "  build               Build a .catpackage\n"
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
    "Additional space: %s\n" \
    "Changing the size: %s%s\n"
#define ALLOW_REMOVE \
    "========================================\n" \
    "Removal: %s\n" \
    "Will be released: %s\n"
#endif