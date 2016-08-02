add_executable(warp_language_detector detector.cpp)
target_link_libraries(warp_language_detector
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${RIBOSOME_LIBRARIES}
	pthread
)

if (THEVOID)
	add_executable(warp_server server.cpp)
	target_link_libraries(warp_server
		${Boost_LIBRARIES}
		${MSGPACK_LIBRARIES}
		${RIBOSOME_LIBRARIES}
		${SWARM_LIBRARIES}
		${THEVOID_LIBRARIES}
		pthread
		warp_stem
	)

	install(TARGETS	warp_server
		RUNTIME DESTINATION bin COMPONENT runtime
	)
endif()

install(TARGETS	warp_language_detector
	RUNTIME DESTINATION bin COMPONENT runtime
)
