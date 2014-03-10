cmake_minimum_required (VERSION 2.6)
project (warp)

add_definitions(-g -std=c++0x -W -Wall -Wextra -fstack-protector-all)

find_package(Boost REQUIRED COMPONENTS system thread locale program_options regex)

INCLUDE(cmake/locate_library.cmake)
LOCATE_LIBRARY(MSGPACK "msgpack.hpp" "msgpack")

option(THEVOID "Build thevoid server for lexical parsing" OFF)
if (THEVOID)
	LOCATE_LIBRARY(THEVOID "thevoid/server.hpp" "thevoid")
	LOCATE_LIBRARY(SWARM "swarm/http_response.hpp" "swarm")
	LOCATE_LIBRARY(SWARM_URLFETCHER "swarm/http_response.hpp" "swarm_urlfetcher")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include
	${Boost_INCLUDE_DIRS}
	${MSGPACK_INCLUDE_DIRS}
	${THEVOID_INCLUDE_DIRS}
	${SWARM_INCLUDE_DIRS}
)

link_directories(
	${Boost_LIBRARY_DIRS}
	${MSGPACK_LIBRARY_DIRS}
	${THEVOID_LIBRARY_DIRS}
	${SWARM_LIBRARY_DIRS}
)

add_subdirectory(src)

FILE(GLOB headers
	"${PROJECT_SOURCE_DIR}/include/warp/*.hpp"
)
install(FILES ${headers} DESTINATION include/warp)
