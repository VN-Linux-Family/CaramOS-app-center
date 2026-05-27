cmake_minimum_required(VERSION 3.16)
project(app-explorer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─── Dependencies ────────────────────────────────────────────────────────────

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# ─── Sources ─────────────────────────────────────────────────────────────────

set(SOURCES
    src/main.cpp
    src/theme_config.cpp
    src/package_manager.cpp
    src/ui_widgets.cpp
    src/main_window.cpp
)

set(HEADERS
    include/theme_config.hpp
    include/package_manager.hpp
    include/ui_widgets.hpp
)

# ─── Executable ──────────────────────────────────────────────────────────────

add_executable(app-explorer ${SOURCES} ${HEADERS})

target_include_directories(app-explorer PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GTK3_INCLUDE_DIRS}
)

target_link_libraries(app-explorer PRIVATE
    ${GTK3_LIBRARIES}
    pthread
)

target_compile_options(app-explorer PRIVATE
    ${GTK3_CFLAGS_OTHER}
    -Wall
    -Wextra
    -Wno-deprecated-declarations
)

# ─── Install ─────────────────────────────────────────────────────────────────

install(TARGETS app-explorer DESTINATION bin)

install(FILES
    data/themes/classic-ivory.cfg
    data/themes/forest-dark.cfg
    DESTINATION share/app-explorer/themes
)

install(FILES data/app-explorer.desktop
    DESTINATION share/applications
    OPTIONAL
)

install(DIRECTORY data/packages/
    DESTINATION share/app-explorer/packages
    OPTIONAL
    FILES_MATCHING PATTERN "*.aepkg"
)

# ─── Compile definitions ─────────────────────────────────────────────────────

target_compile_definitions(app-explorer PRIVATE
    AE_VERSION="${PROJECT_VERSION}"
    AE_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/app-explorer"
)
