cmake_minimum_required(VERSION 3.10)
project(ngTerm VERSION 1.0)

# Add this to your CMakeLists.txt
if(UNIX)
    set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE PATH "Installation prefix")
    set(CMAKE_INSTALL_BINDIR "bin" CACHE PATH "Binary installation directory")
    set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "Include installation directory")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM3 REQUIRED gtkmm-3.0)
pkg_check_modules(VTE REQUIRED vte-2.91)
pkg_check_modules(JSON REQUIRED nlohmann_json)

# Include directories
include_directories(${GTKMM3_INCLUDE_DIRS})
include_directories(${VTE_INCLUDE_DIRS})
include_directories(${JSON_INCLUDE_DIRS})

# Source files
set(SOURCES
    main.cpp
    main.h
    Connections.cpp
    Connections.h
    Folders.cpp
    Folders.h
    Ssh.cpp
    Ssh.h
    Config.cpp
    Config.h
    TreeModelColumns.h
)

# Generate icondata.h
add_custom_command(
    OUTPUT icondata.h
    COMMAND bash -c "echo \"// Generated icon data - DO NOT EDIT\" > icondata.h && \
                     echo \"\" >> icondata.h && \
                     echo \"#include <gdk/gdk.h>\" >> icondata.h && \
                     echo \"\" >> icondata.h && \
                     for file in images/*.png; do \
                         base=\$(basename \"\${file}\" .png | sed 's/^images_//'); \
                         xxd -n \"\${base}_png\" -i \"\${file}\" >> icondata.h; \
                         echo \"\" >> icondata.h; \
                     done"
    DEPENDS images/*.png
)

# Add source files
set(SOURCES ${SOURCES} icondata.h)

# Add executable
add_executable(ngTerm ${SOURCES})

# Link libraries
target_link_libraries(ngTerm
    ${GTKMM3_LIBRARIES}
    ${VTE_LIBRARIES}
    ${JSON_LIBRARIES}
    -lstdc++fs
    -luuid
)

# Install rules
install(TARGETS ngTerm DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES icondata.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
