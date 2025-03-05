
# Mongoose
set(MONGOOSE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/mongoose)
add_library(mongoose STATIC ${MONGOOSE_DIR}/mongoose.c)
target_include_directories(mongoose PUBLIC ${MONGOOSE_DIR})
target_compile_options(mongoose PRIVATE -ffunction-sections -fdata-sections -DMG_ENABLE_PACKED_FS=1)

# GoldHEN SDK
set(GoldHEN_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/GoldHEN_Plugins_SDK)
set(GoldHEN_SDK_TARGET ${GoldHEN_SDK_DIR}/libGoldHEN_Hook.a PARENT_SCOPE)
file(GLOB GoldHEN_SOURCE_FILES ${GoldHEN_SDK_DIR}/source/*.c ${GoldHEN_SDK_DIR}/include/*.c ${GoldHEN_SDK_DIR}/crt/*.c)
set(GoldHEN_SDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/GoldHEN_Plugins_SDK)
set(GoldHEN_SDK_BUIILD_FLAG "-j1")
if (DEBUG_SDK)
    list(APPEND GoldHEN_SDK_BUIILD_FLAG DEBUGFLAGS=1)
endif ()
add_custom_target(goldhen_sdk
    COMMAND ${CMAKE_MAKE_PROGRAM} libGoldHEN_Hook.a ${GoldHEN_SDK_BUIILD_FLAG}
    WORKING_DIRECTORY ${GoldHEN_SDK_DIR}
    DEPENDS ${GoldHEN_SOURCE_FILES}
)

# A fake target to make IDE happy
file(GLOB GoldHEN_SOURCE_FAKE ${GoldHEN_SDK_DIR}/source/*.c)
add_library(GoldHEN_SDK_FAKE STATIC ${GoldHEN_SOURCE_FILES})
target_include_directories(GoldHEN_SDK_FAKE PUBLIC ${GoldHEN_SDK_DIR}/include)