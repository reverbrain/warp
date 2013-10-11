cmake_minimum_required (VERSION 2.6)
project (warp)

add_definitions(-g -std=c++11 -W -Wall -Wextra -fstack-protector-all)

find_package(Boost REQUIRED COMPONENTS system thread locale program_options regex)

INCLUDE(cmake/locate_library.cmake)
LOCATE_LIBRARY(FANN "fann.h" "fann")

include_directories(${PROJECT_SOURCE_DIR}/include
	${Boost_INCLUDE_DIRS}
	${FANN_INCLUDE_DIRS}
)

link_directories(
	${Boost_LIBRARY_DIRS}
	${FANN_LIBRARY_DIRS}
)

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

add_executable(warp_trie_test trie.cpp)
target_link_libraries(warp_trie_test
	${Boost_LIBRARIES}
)
