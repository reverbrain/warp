cmake_minimum_required (VERSION 2.6)
project (warp)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" WARP_VERSION_MAJOR "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+$)" WARP_VERSION_MINOR "${DEBFULLVERSION}")
SET(WARP_VERSION_FULL ${DEBFULLVERSION})

set(CMAKE_CXX_FLAGS "-g -std=c++0x -W -Wall -Wextra -fstack-protector-all")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

find_package(Boost REQUIRED COMPONENTS system program_options thread)

INCLUDE(cmake/locate_library.cmake)
LOCATE_LIBRARY(MSGPACK "msgpack.hpp" "msgpack")

option(THEVOID "Build thevoid server for lexical parsing" ON)
if (THEVOID)
	LOCATE_LIBRARY(SWARM "swarm/http_response.hpp" "swarm")
	LOCATE_LIBRARY(THEVOID "thevoid/server.hpp" "thevoid")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include
	${Boost_INCLUDE_DIRS}
	${MSGPACK_INCLUDE_DIRS}
	${THEVOID_INCLUDE_DIRS}
	${SWARM_INCLUDE_DIRS}
	${THEVOID_INCLUDE_DIRS}
	#${PROJECT_SOURCE_DIR}/stem/libstemmer_c/include
)

link_directories(
	${Boost_LIBRARY_DIRS}
	${MSGPACK_LIBRARY_DIRS}
	${THEVOID_LIBRARY_DIRS}
	${SWARM_LIBRARY_DIRS}
	${THEVOID_LIBRARY_DIRS}
	${cld_LIBRARY_DIRS}
)

add_subdirectory(stem)
add_subdirectory(src)

FILE(GLOB headers
	"${PROJECT_SOURCE_DIR}/include/warp/*.hpp"
	"${PROJECT_SOURCE_DIR}/stem/include/stem/*.hpp"
	"${PROJECT_SOURCE_DIR}/stem/include/stem/*.h"
)
install(FILES ${headers} DESTINATION include/warp)
