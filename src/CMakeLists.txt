if (THEVOID)
	add_executable(warp_server server.cpp)
	target_link_libraries(warp_server
		${Boost_LIBRARIES}
		${MSGPACK_LIBRARIES}
		${THEVOID_LIBRARIES}
		${SWARM_LIBRARIES}
		pthread
	)
endif()


