cmake_minimum_required(VERSION 3.10)

option(DEVICE_IP "Set your PS4 IP address to send the PRX directly to the GoldHEN plugin folder")
option(DEBUG_SDK "Enable debug output for GoldHEN SDK" OFF)

project(remote_pad C)

add_subdirectory(lib)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/static.c
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND gzip -k -c client/index.html > client/index.html.gz
    COMMAND cc -o ${CMAKE_BINARY_DIR}/pack lib/mongoose/test/pack.c
    COMMAND ${CMAKE_BINARY_DIR}/pack client/index.html.gz > ${CMAKE_BINARY_DIR}/static.c
    DEPENDS client/index.html
)

set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> -o <TARGET> <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES>")
file(GLOB SOURCE_FILES src/*.c src/pad/*.c)
add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${CMAKE_BINARY_DIR}/static.c)
target_include_directories(${PROJECT_NAME} PRIVATE include lib/GoldHEN_Plugins_SDK/include)
target_link_libraries(${PROJECT_NAME} SceSysmodule SceUserService ScePad SceNet mongoose ${GoldHEN_SDK_TARGET})
target_link_options(${PROJECT_NAME} PRIVATE --gc-sections)
add_dependencies(${PROJECT_NAME} goldhen_sdk)
add_prx(${PROJECT_NAME})

if (DEVICE_IP)
    add_custom_target(${PROJECT_NAME}_send_prx
        COMMAND curl -T ${PROJECT_NAME}.prx ftp://${DEVICE_IP}:2121/data/GoldHEN/plugins/
        DEPENDS ${PROJECT_NAME}_prx
    )
endif ()
