cmake_minimum_required (VERSION 2.6)
project (wookie)

add_definitions(-g -std=c++11 -W -Wall -Wextra -fstack-protector-all)

find_package(Boost REQUIRED COMPONENTS system thread locale program_options regex)

include_directories(${PROJECT_SOURCE_DIR}/include
	${Boost_INCLUDE_DIRS}
)

link_directories(
	${Boost_LIBRARY_DIRS}
)

add_executable(lemmer_parser parser.cpp)
target_link_libraries(lemmer_parser
	${Boost_LIBRARIES}
)
