SET(EXE_NAME KGEConfig)

SET(KGECONFIG_SOURCE_FILES
	${KLAYGE_PROJECT_DIR}/Core/Src/KGEConfig/KGEConfig.cpp
)

SET(KGECONFIG_HEADER_FILES
	${KLAYGE_PROJECT_DIR}/Core/Src/KGEConfig/resource.h
)

SET(KGECONFIG_RESOURCE_FILES
	${KLAYGE_PROJECT_DIR}/Core/Src/KGEConfig/KGEConfig.rc
	${KLAYGE_PROJECT_DIR}/klayge_logo.ico
)

SOURCE_GROUP("Source Files" FILES ${KGECONFIG_SOURCE_FILES})
SOURCE_GROUP("Header Files" FILES ${KGECONFIG_HEADER_FILES})
SOURCE_GROUP("Resource Files" FILES ${KGECONFIG_RESOURCE_FILES})

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${KLAYGE_PROJECT_DIR}/../KFL/Include)
INCLUDE_DIRECTORIES(${KLAYGE_PROJECT_DIR}/Core/Include)
LINK_DIRECTORIES(${KLAYGE_PROJECT_DIR}/../KFL/lib/${KLAYGE_PLATFORM_NAME})
IF(KLAYGE_PLATFORM_DARWIN OR KLAYGE_PLATFORM_LINUX)
	LINK_DIRECTORIES(${KLAYGE_BIN_DIR})
ELSE()
	LINK_DIRECTORIES(${KLAYGE_OUTPUT_DIR})
ENDIF()

ADD_EXECUTABLE(${EXE_NAME} WIN32
	${KGECONFIG_SOURCE_FILES} ${KGECONFIG_HEADER_FILES} ${KGECONFIG_RESOURCE_FILES}
	${KLAYGE_PROJECT_DIR}/bin/KlayGE.cfg
)
ADD_DEPENDENCIES(${EXE_NAME} AllInEngine)

SET_TARGET_PROPERTIES(${EXE_NAME} PROPERTIES
	PROJECT_LABEL ${EXE_NAME}
	DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
	OUTPUT_NAME ${EXE_NAME}${KLAYGE_OUTPUT_SUFFIX}
)

TARGET_LINK_LIBRARIES(${EXE_NAME}
	debug KlayGE_Core${KLAYGE_OUTPUT_SUFFIX}_d optimized KlayGE_Core${KLAYGE_OUTPUT_SUFFIX}
	debug KFL${KLAYGE_OUTPUT_SUFFIX}_d optimized KFL${KLAYGE_OUTPUT_SUFFIX}
)


ADD_POST_BUILD(${EXE_NAME} "")


INSTALL(TARGETS ${EXE_NAME}
	RUNTIME DESTINATION ${KLAYGE_BIN_DIR}
	LIBRARY DESTINATION ${KLAYGE_BIN_DIR}
	ARCHIVE DESTINATION ${KLAYGE_OUTPUT_DIR}
)

IF(MSVC)
	CREATE_VCPROJ_USERFILE(${EXE_NAME})
ENDIF()

SET_TARGET_PROPERTIES(${EXE_NAME} PROPERTIES FOLDER "Engine/KGEConfig")
