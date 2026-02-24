cmake_minimum_required(VERSION 3.25)
project(jcan LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_SOCKETCAN_DEFAULT ON)
else()
    set(_SOCKETCAN_DEFAULT OFF)
endif()
option(JCAN_ENABLE_SOCKETCAN "Build with SocketCAN support (Linux only)" ${_SOCKETCAN_DEFAULT})

find_package(OpenGL REQUIRED)

option(JCAN_ENABLE_VECTOR "Build with Vector VN1640A USB support" ON)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    libserialport
    URL https://sigrok.org/download/source/libserialport/libserialport-0.1.2.tar.gz
)
FetchContent_GetProperties(libserialport)
if(NOT libserialport_POPULATED)
    FetchContent_Populate(libserialport)
    
    set(_lsp_header "${libserialport_SOURCE_DIR}/libserialport.h")
    file(READ "${_lsp_header}" _lsp_content)
    string(REPLACE "#ifdef _MSC_VER" "#if defined(_MSC_VER) && !defined(SP_STATIC)" _lsp_content "${_lsp_content}")
    file(WRITE "${_lsp_header}" "${_lsp_content}")
endif()

set(SERIALPORT_SOURCES
    ${libserialport_SOURCE_DIR}/serialport.c
    ${libserialport_SOURCE_DIR}/timing.c
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SERIALPORT_SOURCES
        ${libserialport_SOURCE_DIR}/linux.c
        ${libserialport_SOURCE_DIR}/linux_termios.c
    )
elseif(WIN32)
    list(APPEND SERIALPORT_SOURCES ${libserialport_SOURCE_DIR}/windows.c)
elseif(APPLE)
    list(APPEND SERIALPORT_SOURCES ${libserialport_SOURCE_DIR}/macosx.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    list(APPEND SERIALPORT_SOURCES ${libserialport_SOURCE_DIR}/freebsd.c)
endif()

set(_sp_config_dir "${CMAKE_CURRENT_BINARY_DIR}/sp_config")
file(MAKE_DIRECTORY "${_sp_config_dir}")
set(_sp_cfg "")
string(APPEND _sp_cfg "#define SP_PRIV\n#define SP_API\n")
if(UNIX)
    string(APPEND _sp_cfg "#define HAVE_SYS_FILE_H 1\n")
    string(APPEND _sp_cfg "#define HAVE_FLOCK 1\n")
    string(APPEND _sp_cfg "#define HAVE_CLOCK_GETTIME 1\n")
    string(APPEND _sp_cfg "#define HAVE_REALPATH 1\n")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(APPEND _sp_cfg "#define HAVE_STRUCT_TERMIOS2 1\n")
    string(APPEND _sp_cfg "#define HAVE_STRUCT_TERMIOS_C_ISPEED 0\n")
    string(APPEND _sp_cfg "#define HAVE_STRUCT_TERMIOS_C_OSPEED 0\n")
    string(APPEND _sp_cfg "#define HAVE_STRUCT_TERMIOS2_C_ISPEED 1\n")
    string(APPEND _sp_cfg "#define HAVE_STRUCT_TERMIOS2_C_OSPEED 1\n")
    string(APPEND _sp_cfg "#define HAVE_DECL_BOTHER 1\n")
    string(APPEND _sp_cfg "#define HAVE_TERMIOS2_SPEED 1\n")
    string(APPEND _sp_cfg "#define HAVE_STRUCT_SERIAL_STRUCT 1\n")
endif()
file(WRITE "${_sp_config_dir}/config.h" "${_sp_cfg}")

add_library(serialport STATIC ${SERIALPORT_SOURCES})
target_include_directories(serialport PUBLIC ${libserialport_SOURCE_DIR})
target_include_directories(serialport PRIVATE ${_sp_config_dir})
target_compile_definitions(serialport PUBLIC SP_STATIC)
target_compile_definitions(serialport PRIVATE LIBSERIALPORT_ATBUILD)
if(WIN32)
target_compile_definitions(serialport PRIVATE LIBSERIALPORT_MSBUILD) 
target_compile_definitions(serialport PRIVATE strdup=_strdup) 
endif()
if(WIN32)
    target_link_libraries(serialport PRIVATE cfgmgr32 setupapi)
elseif(APPLE)
    target_link_libraries(serialport PRIVATE "-framework IOKit" "-framework CoreFoundation")
endif()

FetchContent_Declare(
    libusb
    GIT_REPOSITORY https://github.com/libusb/libusb.git
    GIT_TAG        v1.0.27
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(libusb)
if(NOT libusb_POPULATED)
    FetchContent_Populate(libusb)
endif()

set(LIBUSB_SOURCES
    ${libusb_SOURCE_DIR}/libusb/core.c
    ${libusb_SOURCE_DIR}/libusb/descriptor.c
    ${libusb_SOURCE_DIR}/libusb/hotplug.c
    ${libusb_SOURCE_DIR}/libusb/io.c
    ${libusb_SOURCE_DIR}/libusb/strerror.c
    ${libusb_SOURCE_DIR}/libusb/sync.c
)
if(UNIX)
    list(APPEND LIBUSB_SOURCES
        ${libusb_SOURCE_DIR}/libusb/os/events_posix.c
        ${libusb_SOURCE_DIR}/libusb/os/threads_posix.c
    )
elseif(WIN32)
    list(APPEND LIBUSB_SOURCES
        ${libusb_SOURCE_DIR}/libusb/os/events_windows.c
        ${libusb_SOURCE_DIR}/libusb/os/threads_windows.c
    )
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND LIBUSB_SOURCES
        ${libusb_SOURCE_DIR}/libusb/os/linux_usbfs.c
        ${libusb_SOURCE_DIR}/libusb/os/linux_netlink.c
    )
elseif(WIN32)
    list(APPEND LIBUSB_SOURCES
        ${libusb_SOURCE_DIR}/libusb/os/windows_common.c
        ${libusb_SOURCE_DIR}/libusb/os/windows_usbdk.c
        ${libusb_SOURCE_DIR}/libusb/os/windows_winusb.c
    )
elseif(APPLE)
    list(APPEND LIBUSB_SOURCES
        ${libusb_SOURCE_DIR}/libusb/os/darwin_usb.c
    )
endif()

set(_lusb_config_dir "${CMAKE_CURRENT_BINARY_DIR}/lusb_config")
file(MAKE_DIRECTORY "${_lusb_config_dir}")
set(_lusb_cfg "")
if(UNIX)
    string(APPEND _lusb_cfg "#define PLATFORM_POSIX 1\n")
    string(APPEND _lusb_cfg "#define HAVE_CLOCK_GETTIME 1\n")
    string(APPEND _lusb_cfg "#define HAVE_EVENTFD 1\n")
    string(APPEND _lusb_cfg "#define HAVE_TIMERFD 1\n")
    string(APPEND _lusb_cfg "#define HAVE_PIPE2 1\n")
    string(APPEND _lusb_cfg "#define HAVE_NFDS_T 1\n")
    string(APPEND _lusb_cfg "#define HAVE_SYS_TIME_H 1\n")
    string(APPEND _lusb_cfg "#define _GNU_SOURCE 1\n")
    string(APPEND _lusb_cfg "#define DEFAULT_VISIBILITY __attribute__ ((visibility (\"default\")))\n")
    string(APPEND _lusb_cfg "#define PRINTF_FORMAT(a, b) __attribute__ ((__format__ (__printf__, a, b)))\n")
elseif(WIN32)
    string(APPEND _lusb_cfg "#define PLATFORM_WINDOWS 1\n")
    string(APPEND _lusb_cfg "#define DEFAULT_VISIBILITY /**/\n")
    string(APPEND _lusb_cfg "#define PRINTF_FORMAT(a, b) /**/\n")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(APPEND _lusb_cfg "#define HAVE_PTHREAD_SETNAME_NP 1\n")
    string(APPEND _lusb_cfg "#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1\n")
endif()
string(APPEND _lusb_cfg "#define ENABLE_LOGGING 1\n")
file(WRITE "${_lusb_config_dir}/config.h" "${_lusb_cfg}")

add_library(usb_static STATIC ${LIBUSB_SOURCES})
target_include_directories(usb_static PUBLIC ${libusb_SOURCE_DIR}/libusb)
target_include_directories(usb_static PRIVATE ${_lusb_config_dir})
if(UNIX)
    target_link_libraries(usb_static PRIVATE pthread)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(usb_static PRIVATE rt)
endif()
if(WIN32)
    target_compile_definitions(usb_static PRIVATE _WIN32_WINNT=0x0600 _TIMESPEC_DEFINED)
endif()

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8-docking
)
FetchContent_MakeAvailable(imgui)

set(build_kcd OFF CACHE BOOL "" FORCE)
set(build_tools OFF CACHE BOOL "" FORCE)
set(build_tests OFF CACHE BOOL "" FORCE)
set(build_examples OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    dbcppp
    GIT_REPOSITORY https://github.com/xR3b0rn/dbcppp.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES "third-party/boost"
)
FetchContent_GetProperties(dbcppp)
if(NOT dbcppp_POPULATED)
    FetchContent_Populate(dbcppp)
    # dbcppp hardcodes SHARED — patch to STATIC for a self-contained binary
    file(READ "${dbcppp_SOURCE_DIR}/CMakeLists.txt" _dbcppp_cmake)
    string(REPLACE "add_library(\${PROJECT_NAME} SHARED" "add_library(\${PROJECT_NAME} STATIC" _dbcppp_cmake "${_dbcppp_cmake}")
    file(WRITE "${dbcppp_SOURCE_DIR}/CMakeLists.txt" "${_dbcppp_cmake}")
    add_subdirectory(${dbcppp_SOURCE_DIR} ${dbcppp_BINARY_DIR})
endif()

if(TARGET dbcppp AND WIN32)
    target_compile_definitions(dbcppp PUBLIC DBCPPP_EXPORT)
endif()

FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nfd)

add_library(imgui_impl STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_impl PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_impl PUBLIC glfw OpenGL::GL)

add_library(jcan_core STATIC
    src/discovery.cpp
)
target_include_directories(jcan_core PUBLIC src)
target_link_libraries(jcan_core PUBLIC serialport)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(jcan_core PUBLIC JCAN_HAS_SOCKETCAN=1)
endif()

if(JCAN_ENABLE_VECTOR)
    target_compile_definitions(jcan_core PUBLIC JCAN_HAS_VECTOR=1)
    target_compile_definitions(jcan_core PUBLIC JCAN_HAS_KVASER=1)

    if(NOT WIN32)
        set(JCAN_FW_MAIN_BIN "${CMAKE_CURRENT_SOURCE_DIR}/firmware/vn1640a_main_fw.bin")
        set(JCAN_FW_FPGA_BIN "${CMAKE_CURRENT_SOURCE_DIR}/firmware/vn1640a_fpga.bin")
        set(JCAN_ASM_RODATA_SECTION ".rodata")
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/src/firmware_blobs.S.in
            ${CMAKE_CURRENT_BINARY_DIR}/firmware_blobs.S
            @ONLY
        )
        target_sources(jcan_core PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/firmware_blobs.S)
        target_link_libraries(jcan_core PUBLIC usb_static)
    endif()
endif()

if(WIN32)
    target_compile_definitions(jcan_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(jcan_cli src/main.cpp)
target_link_libraries(jcan_cli PRIVATE jcan_core)

add_executable(jcan_gui src/gui_main.cpp)
target_link_libraries(jcan_gui PRIVATE jcan_core imgui_impl dbcppp nfd)
if(WIN32)
    target_link_options(jcan_gui PRIVATE "LINKER:/STACK:8388608")
endif()
