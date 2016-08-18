add_executable(warp_zpack pack.cpp)
target_link_libraries(warp_zpack
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${RIBOSOME_LIBRARIES}
	${ROCKSDB_LIBRARIES}
	pthread
	warp_stem
)

add_executable(warp_html html.cpp)
target_link_libraries(warp_html
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${RIBOSOME_LIBRARIES}
	${ROCKSDB_LIBRARIES}
	pthread
)

add_executable(warp_wikipedia wikipedia.cpp)
target_link_libraries(warp_wikipedia
	${Boost_LIBRARIES}
	${EXPAT_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${RIBOSOME_LIBRARIES}
	${ROCKSDB_LIBRARIES}
	pthread
)

add_executable(warp_fuzzy_search fuzzy_search.cpp)
target_link_libraries(warp_fuzzy_search
	${Boost_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${RIBOSOME_LIBRARIES}
	${ROCKSDB_LIBRARIES}
	pthread
	warp_stem
)


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
		${ROCKSDB_LIBRARIES}
		${SWARM_LIBRARIES}
		${THEVOID_LIBRARIES}
		pthread
		warp_stem
	)

	install(TARGETS	warp_server
		RUNTIME DESTINATION bin COMPONENT runtime
	)
endif()

install(TARGETS	warp_language_detector warp_fuzzy_search warp_wikipedia warp_zpack
	RUNTIME DESTINATION bin COMPONENT runtime
)
