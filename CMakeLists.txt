cmake_minimum_required(VERSION 3.25)
project(jcan LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Options ──────────────────────────────────────────────────────────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_SOCKETCAN_DEFAULT ON)
else()
    set(_SOCKETCAN_DEFAULT OFF)
endif()
option(JCAN_ENABLE_SOCKETCAN "Build with SocketCAN support (Linux only)" ${_SOCKETCAN_DEFAULT})

# ── System dependencies ──────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(SERIALPORT REQUIRED IMPORTED_TARGET libserialport)
find_package(OpenGL REQUIRED)
pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)

# ── FetchContent ─────────────────────────────────────────────────────────────
include(FetchContent)

# Dear ImGui — immediate mode GUI
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8-docking
)
FetchContent_MakeAvailable(imgui)

# dbcppp — DBC file parser
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

# nativefiledialog-extended — cross-platform file picker
FetchContent_Declare(
    nfd
    GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nfd)

# ── ImGui static library (GLFW + OpenGL3 backend) ────────────────────────────
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
target_link_libraries(imgui_impl PUBLIC PkgConfig::GLFW OpenGL::GL)

# ── jcan core library ────────────────────────────────────────────────────────
add_library(jcan_core STATIC
    src/discovery.cpp
)
target_include_directories(jcan_core PUBLIC src)
target_link_libraries(jcan_core PUBLIC PkgConfig::SERIALPORT)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(jcan_core PUBLIC JCAN_HAS_SOCKETCAN=1)
endif()

if(WIN32)
    target_compile_definitions(jcan_core PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# ── CLI executable (Phase 1 validation) ──────────────────────────────────────
add_executable(jcan_cli src/main.cpp)
target_link_libraries(jcan_cli PRIVATE jcan_core)

# ── GUI executable (Phase 2) ─────────────────────────────────────────────────
add_executable(jcan_gui src/gui_main.cpp)
target_link_libraries(jcan_gui PRIVATE jcan_core imgui_impl dbcppp nfd)
