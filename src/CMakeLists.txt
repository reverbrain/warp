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

add_executable(warp_stat stat.cpp)
target_link_libraries(warp_stat
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
)

if (THEVOID)
	add_executable(warp_server server.cpp)
	target_link_libraries(warp_server
		${Boost_LIBRARIES}
		${MSGPACK_LIBRARIES}
		${THEVOID_LIBRARIES}
		${SWARM_LIBRARIES}
	)
endif()


