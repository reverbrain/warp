add_executable(warp_zpack pack.cpp)
target_link_libraries(warp_zpack
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
)

add_executable(warp_spell spell.cpp)
target_link_libraries(warp_spell
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
endif()


