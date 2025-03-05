cmake_minimum_required(VERSION 3.10)

###################################################################

set(OO_PS4_TOOLCHAIN $ENV{OO_PS4_TOOLCHAIN})
set(PS4 TRUE)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(TARGET x86_64-pc-freebsd-elf)
set(CMAKE_SYSTEM_VERSION 12)
set(CMAKE_CROSSCOMPILING 1)

find_program(CLANG clang)
if (NOT CLANG)
    message(FATAL_ERROR "clang not Found")
endif ()

find_program(CLANG_PP clang++)
if (NOT CLANG_PP)
    message(FATAL_ERROR "clang++ not Found")
endif ()

find_program(LLD_LINKER ld.lld)
if (NOT LLD_LINKER)
    message(FATAL_ERROR "ld.lld not Found")
endif ()

find_program(LLVM_AR llvm-ar)
if (NOT LLVM_AR)
    message(FATAL_ERROR "llvm-ar not Found")
endif ()

find_program(LLD_RANLIB llvm-ranlib)
if (NOT LLD_RANLIB)
    message(FATAL_ERROR "llvm-ranlib not Found")
endif ()

find_program(LLD_STRIP llvm-strip)
if (NOT LLD_STRIP)
    message(FATAL_ERROR "llvm-strip not Found")
endif ()

set(CMAKE_ASM_COMPILER ${CLANG} CACHE PATH "")
set(CMAKE_C_COMPILER ${CLANG} CACHE PATH "")
set(CMAKE_CXX_COMPILER ${CLANG_PP} CACHE PATH "")
set(CMAKE_LINKER ${LLD_LINKER} CACHE PATH "")
set(CMAKE_AR ${LLVM_AR} CACHE PATH "")
set(CMAKE_RANLIB ${LLD_RANLIB} CACHE PATH "")
set(CMAKE_STRIP ${LLD_STRIP} CACHE PATH "")

# We use the linker directly instead of using the llvm wrapper.
# CMake uses `-Xlinker` for passing llvm linker flags
# added via `add_link_options(... "LINKER:...")`.
# Force the correct linker flag generation:
macro(reset_linker_wrapper_flag)
    set(CMAKE_ASM_LINKER_WRAPPER_FLAG "")
    set(CMAKE_C_LINKER_WRAPPER_FLAG "")
    set(CMAKE_CXX_LINKER_WRAPPER_FLAG "")
endmacro()
variable_watch(CMAKE_ASM_LINKER_FLAG reset_linker_wrapper_flag)
variable_watch(CMAKE_C_LINKER_WRAPPER_FLAG reset_linker_wrapper_flag)
variable_watch(CMAKE_CXX_LINKER_WRAPPER_FLAG reset_linker_wrapper_flag)

set(CMAKE_LIBRARY_ARCHITECTURE x86_64 CACHE INTERNAL "abi")

set(CMAKE_FIND_ROOT_PATH ${OO_PS4_TOOLCHAIN})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Shared libs not available")

###################################################################

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_ASM_FLAGS_INIT
        "-target x86_64-pc-freebsd12-elf \
   -D__PS4__ -fPIC -funwind-tables \
   -isysroot ${OO_PS4_TOOLCHAIN} -isystem ${OO_PS4_TOOLCHAIN}/include")

set(CMAKE_C_FLAGS_INIT "${CMAKE_ASM_FLAGS_INIT}")

set(CMAKE_C_STANDARD_LIBRARIES "-lkernel -lSceLibcInternal")

set(CMAKE_EXE_LINKER_FLAGS_INIT
        "-m elf_x86_64 -pie --eh-frame-hdr -e _init \
   --script ${OO_PS4_TOOLCHAIN}/link.x \
   -L${OO_PS4_TOOLCHAIN}/lib")

set(CMAKE_ASM_LINK_EXECUTABLE
        "<CMAKE_LINKER> -o <TARGET> <CMAKE_ASM_LINK_FLAGS> <LINK_FLAGS> --start-group \
   <OBJECTS> <LINK_LIBRARIES> --end-group")

set(CMAKE_C_LINK_EXECUTABLE
        "<CMAKE_LINKER> -o <TARGET> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> \
  --start-group \
     ${OO_PS4_TOOLCHAIN}/lib/crt1.o ${OO_PS4_TOOLCHAIN}/lib/crti.o \
     <OBJECTS> <LINK_LIBRARIES> -lc \
     ${OO_PS4_TOOLCHAIN}/lib/crtn.o \
  --end-group")

function(add_prx project)
    add_custom_command(
            OUTPUT "${project}.prx"
            COMMAND ${CMAKE_COMMAND} -E env "OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN}" "${OO_PS4_TOOLCHAIN}/bin/linux/create-fself" "-in=${project}" "-out=${project}.oelf" "--lib=${project}.prx" "--paid" "0x3800000000000011"
            VERBATIM
            DEPENDS "${project}"
    )
    add_custom_target(
            "${project}_prx" ALL
            DEPENDS "${project}.prx"
    )
endfunction()