enum CatpkgProtocol {
    OPERATION_SECURE_CONTEXT,

    MOVE_FILE,
    MOVE_DIR,
    REMOVE_BATCH_OPEN, // Manages and groups the REMOVE_DIR and REMOVE_FILE protocols.
    REMOVE_BATCH_CLOSE,
    REMOVE_DIR,
    CTPG_REMOVE_DIR_OBSOLETE,
    REMOVE_FILE,
    CTPG_REMOVE_FILE_OBSOLETE,
    REMOVE,
    MKDIR,
    MKDIR_SOFT, // Gently restores the parent folders for the created package, records the existing ones in the context, and, after restoration, if an error occurs, will skip the already existing folders. !! THERE MAY BE PROBLEMS !!

    MKFILE,
    SYMLINK,

    CATPKG_MERGE_DATABASE,
    DATABASE_OBSOLETE_DEPEND,
    CATPKG_REMOVE_PACKAGE_RECORDS,
    FILEINFO_EXCLUDE,
    CATPKG_CREATE_FILEINFO,

	CTPG_OPEN,
	CTPG_INFO,
    CTPG_MKCMD,
    CTPG_READFILE,
    CTPG_EXTRACT,
    CTPG_TO_CACHE,
    CTPG_EXTRACT_PAYLOAD,
    CTPG_EXTRACT_PACKAGEINFO,
    CTPG_UPDATE_PACKAGEINFO,
	CTPG_CLOSE,

    SU,

    BUILD_LOG_START,
    BUILD_LOG,
    BUILD_LOG_SEP,
	BUILD_LOG_END,

    NONE
};