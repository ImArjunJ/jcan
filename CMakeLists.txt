cmake_minimum_required(VERSION 3.25)
project(jcan LANGUAGES C CXX)

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

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SERIALPORT IMPORTED_TARGET libserialport)
endif()
if(TARGET PkgConfig::SERIALPORT)
    add_library(serialport ALIAS PkgConfig::SERIALPORT)
else()
    find_path(SERIALPORT_INCLUDE_DIR libserialport.h)
    find_library(SERIALPORT_LIBRARY NAMES serialport libserialport)
    if(SERIALPORT_INCLUDE_DIR AND SERIALPORT_LIBRARY)
        add_library(serialport UNKNOWN IMPORTED)
        set_target_properties(serialport PROPERTIES
            IMPORTED_LOCATION "${SERIALPORT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SERIALPORT_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR
            "libserialport not found.\n"
            "  arch:   pacman -S libserialport\n"
            "  debian: apt install libserialport-dev\n"
            "  brew:   brew install libserialport\n"
            "  win:    vcpkg install libserialport\n"
            "Or set -DSERIALPORT_INCLUDE_DIR=... -DSERIALPORT_LIBRARY=...")
    endif()
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
FetchContent_MakeAvailable(dbcppp)

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

if(WIN32)
    target_compile_definitions(jcan_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

add_executable(jcan_cli src/main.cpp)
target_link_libraries(jcan_cli PRIVATE jcan_core)

add_executable(jcan_gui src/gui_main.cpp)
target_link_libraries(jcan_gui PRIVATE jcan_core imgui_impl dbcppp nfd)
