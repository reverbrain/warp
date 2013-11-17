cmake_minimum_required (VERSION 2.6)
project (warp)

add_definitions(-g -std=c++11 -W -Wall -Wextra -fstack-protector-all)

find_package(Boost REQUIRED COMPONENTS system thread locale program_options regex)

INCLUDE(cmake/locate_library.cmake)
LOCATE_LIBRARY(MSGPACK "msgpack.hpp" "msgpack")

option(FANN "Build with FANN (Fast Artificial Neural Network) support" OFF)
if(FANN)
	LOCATE_LIBRARY(FANN "fann.h" "fann")
endif()

option(THEVOID "Build thevoid server for lexical parsing" OFF)
if (THEVOID)
	LOCATE_LIBRARY(THEVOID "thevoid/server.hpp" "thevoid")
	LOCATE_LIBRARY(SWARM "swarm/http_response.hpp" "swarm")
	LOCATE_LIBRARY(SWARM_URLFETCHER "swarm/http_response.hpp" "swarm_urlfetcher")
endif()

include_directories(${PROJECT_SOURCE_DIR}/include
	${Boost_INCLUDE_DIRS}
	${FANN_INCLUDE_DIRS}
	${MSGPACK_INCLUDE_DIRS}
	${THEVOID_INCLUDE_DIRS}
	${SWARM_INCLUDE_DIRS}
)

link_directories(
	${Boost_LIBRARY_DIRS}
	${FANN_LIBRARY_DIRS}
	${MSGPACK_LIBRARY_DIRS}
	${THEVOID_LIBRARY_DIRS}
	${SWARM_LIBRARY_DIRS}
)

if (FANN)
	add_executable(warp_ann_zparser ann_zparser.cpp)
	target_link_libraries(warp_ann_zparser
		${Boost_LIBRARIES}
	)

	add_executable(warp_ann_learn ann_learn.cpp)
	target_link_libraries(warp_ann_learn
		${Boost_LIBRARIES}
		${FANN_LIBRARIES}
	)

	add_executable(warp_ann_check ann_check.cpp)
	target_link_libraries(warp_ann_check
		${Boost_LIBRARIES}
		${FANN_LIBRARIES}
	)
endif()

add_executable(warp_trie_test trie.cpp)
target_link_libraries(warp_trie_test
	${Boost_LIBRARIES}
)

add_executable(warp_zpack pack.cpp)
target_link_libraries(warp_zpack
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
)

option(THEVOID "Build thevoid server for lexical parsing" OFF)
if (THEVOID)
	add_executable(warp_server server.cpp)
	target_link_libraries(warp_server
		${Boost_LIBRARIES}
		${MSGPACK_LIBRARIES}
		${THEVOID_LIBRARIES}
		${SWARM_LIBRARIES}
		${SWARM_URLFETCHER_LIBRARIES}
		)

	locate_library(LIBEV "ev++.h" "ev" "libev")
	include_directories(
	    ${LIBEV_INCLUDE_DIRS}
	)
	link_directories(
	    ${LIBEV_LIBRARY_DIRS}
	)

message("libev include dirs: " ${LIBEV_INCLUDE_DIRS})

	add_executable(warp_client client.cpp)
	target_link_libraries(warp_client
		${LIBEV_LIBRARIES}
		${Boost_LIBRARIES}
		${MSGPACK_LIBRARIES}
		${SWARM_LIBRARIES}
		${SWARM_URLFETCHER_LIBRARIES}
		)
endif()

FILE(GLOB headers
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
install(FILES ${headers} DESTINATION include/warp)
