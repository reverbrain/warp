cmake_minimum_required (VERSION 2.6)
project (lemmer)

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

add_executable(lemmer_parser parser.cpp)
target_link_libraries(lemmer_parser
	${Boost_LIBRARIES}
)

add_executable(lemmer_ann nnet.cpp)
target_link_libraries(lemmer_ann
	${Boost_LIBRARIES}
	${FANN_LIBRARIES}
)
