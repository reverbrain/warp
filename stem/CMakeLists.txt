FILE(GLOB stemmers "libstemmer_c/src_c/*.c" libstemmer_c/runtime/api.c libstemmer_c/runtime/utilities.c libstemmer_c/libstemmer/libstemmer.c)
FILE(GLOB headers "libstemmer_c/include/*.h")

add_library(warp_stem SHARED ${stemmers})

set_target_properties(warp_stem PROPERTIES
	VERSION ${WARP_VERSION_MAJOR}
	SOVERSION ${WARP_VERSION_FULL}
)

install(FILES ${headers} DESTINATION include/warp)
install(TARGETS warp_stem
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    BUNDLE DESTINATION library
)
