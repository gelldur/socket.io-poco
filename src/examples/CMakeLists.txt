

SET(CMAKE_BUILD_TYPE Debug)

SET(source_files
		main.cpp
		TestEndpointTarget.cpp
		TestTarget.cpp
		)
INCLUDE_DIRECTORIES(${INCLUDE_SOCKETIOPOCO})

ADD_EXECUTABLE(socketiopoco_example ${source_files})
TARGET_LINK_LIBRARIES(socketiopoco_example socketiopoco_shared)

INSTALL(TARGETS socketiopoco_example DESTINATION bin)
