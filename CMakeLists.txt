cmake_minimum_required(VERSION 3.15)
project(ScreenCaptureAddon LANGUAGES CXX C OBJCXX) # OBJCXX for macOS .mm files

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# --- N-API Configuration ---
# Option 1: Try to find NodeJS package (works with newer CMake and if Node dev files are setup)
# find_package(NodeJS) # This is the ideal, modern CMake way if available
# if(NOT NodeJS_FOUND)
    # Option 2: Manual path (user might need to set NODE_INCLUDE_DIR)
    # This is often how node-gyp would provide it.
    # You might need to get this from environment variable or a build script.
    # Example: set(NODE_INCLUDE_DIR "$ENV{NODE_INCLUDE_DIR}" CACHE PATH "Path to Node.js headers")

    # Fallback search paths for NODE_INCLUDE_DIR
    if(DEFINED ENV{NODE_INCLUDE_PATH}) # Common env var set by some tools or users
        set(NODE_INCLUDE_DIR $ENV{NODE_INCLUDE_PATH})
    elseif(DEFINED ENV{NODE_GYP_INCLUDE_DIRS}) # If node-gyp environment variable is available
        # NODE_GYP_INCLUDE_DIRS might be like "<(module_root_dir)/node_modules/node-addon-api" or a direct path
        # This needs careful parsing. For simplicity, assume it's a direct path or a parent that contains "node.h"
        # A more robust way would be to parse it properly if it's a complex string.
        # This is a simplified attempt:
        string(REPLACE "<(module_root_dir)/" "" NODE_INCLUDE_DIR_TMP $ENV{NODE_GYP_INCLUDE_DIRS})
        if(IS_DIRECTORY ${NODE_INCLUDE_DIR_TMP})
            set(NODE_INCLUDE_DIR ${NODE_INCLUDE_DIR_TMP})
        else()
            # Try to find node.h in standard locations as a last resort
            find_path(NODE_INCLUDE_DIR node.h
                PATHS $ENV{NVM_INC} # NVM path
                      /usr/include/node /usr/local/include/node # Linux common paths
                      /opt/homebrew/opt/node/include/node # macOS Apple Silicon Homebrew
                      /usr/local/opt/node/include/node    # macOS Intel Homebrew
                # Windows paths are very variable, often handled by node-gyp setting direct env vars
            )
        endif()
    else()
        find_path(NODE_INCLUDE_DIR node.h
            PATHS $ENV{NVM_INC}
                  /usr/include/node /usr/local/include/node
                  /opt/homebrew/opt/node/include/node
                  /usr/local/opt/node/include/node
        )
    endif()

    if(NOT NODE_INCLUDE_DIR AND EXISTS "$ENV{HOME}/.node-gyp") # Common place node-gyp downloads headers
         file(GLOB NODE_GYP_INCLUDE_CANDIDATES "$ENV{HOME}/.node-gyp/*/include/node")
         if(NODE_GYP_INCLUDE_CANDIDATES)
            list(GET NODE_GYP_INCLUDE_CANDIDATES 0 NODE_INCLUDE_DIR) # Get the first one found
         endif()
    endif()


    if(NOT NODE_INCLUDE_DIR)
        message(WARNING "NODE_INCLUDE_DIR not found. N-API compilation might fail. "
                        "Set NODE_INCLUDE_DIR, NODE_INCLUDE_PATH, or ensure Node.js development files are in standard paths or use node-gyp.")
    else()
         message(STATUS "Using Node.js include directory: ${NODE_INCLUDE_DIR}")
    endif()
# endif()

# --- Source Files ---
set(COMMON_SOURCES
    screen_capture_api.h
    napi_screen_capture.cpp
)

set(MACOS_SOURCES
    sc_session_macos.h
    sc_session_macos.mm
    sc_platform_macos.mm
)

set(WINDOWS_SOURCES
    sc_session_windows.h
    sc_session_windows.cpp
    sc_platform_windows.cpp
)

set(LINUX_SOURCES
    sc_session_linux.h
    sc_session_linux.cpp
    sc_platform_linux.cpp
)

# --- Target Definition ---
# The output name should be <module_name>.node
add_library(screen_capture_addon SHARED ${COMMON_SOURCES})
set_target_properties(screen_capture_addon PROPERTIES PREFIX "" SUFFIX ".node")


# --- Platform Specific Configuration ---
if(CMAKE_SYSTEM_NAME MATCHES "Darwin") # macOS
    target_sources(screen_capture_addon PRIVATE ${MACOS_SOURCES})
    target_compile_options(screen_capture_addon PRIVATE -fobjc-arc) # Enable ARC for .mm files

    # For system frameworks
    target_link_libraries(screen_capture_addon PRIVATE
        "-framework Foundation"
        "-framework AVFoundation"
        "-framework CoreGraphics"
        "-framework IOSurface"
        "-framework ScreenCaptureKit"
        "-framework AppKit"
    )
    # Set minimum macOS version for ScreenCaptureKit
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.3" CACHE STRING "Minimum macOS deployment version")


elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_sources(screen_capture_addon PRIVATE ${WINDOWS_SOURCES})
    target_link_libraries(screen_capture_addon PRIVATE
        dxgi.lib
        d3d11.lib
        ole32.lib   # For CoInitializeEx
        dwmapi.lib  # For DwmGetWindowAttribute
        # user32.lib # For EnumWindows, GetDC etc. (often implicitly linked by MSVC for GUI apps)
        # gdi32.lib  # For BitBlt, CreateDIBSection etc. (often implicitly linked)
    )
    # Add MSVC specific compile options if needed, e.g. /EHsc (usually default for C++)


elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_sources(screen_capture_addon PRIVATE ${LINUX_SOURCES})
    # Use pkg-config to find Linux libraries
    find_package(PkgConfig REQUIRED)

    # For D-Bus (Gio, GLib, GObject)
    pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
    pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
    pkg_check_modules(GOBJECT REQUIRED IMPORTED_TARGET gobject-2.0)

    # For PipeWire
    pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

    if (TARGET PkgConfig::GIO AND TARGET PkgConfig::GLIB AND TARGET PkgConfig::GOBJECT AND TARGET PkgConfig::PIPEWIRE)
        message(STATUS "Found Gio, GLib, GObject, and PipeWire via PkgConfig")
        target_link_libraries(screen_capture_addon PRIVATE
            PkgConfig::GIO
            PkgConfig::GLIB
            PkgConfig::GOBJECT
            PkgConfig::PIPEWIRE
            pthread # For std::thread
            dl      # For dlopen if used
        )
    else()
        message(WARNING "One or more Linux dependencies (Gio, GLib, GObject, PipeWire) not found via PkgConfig as IMPORTED_TARGETs. Trying legacy variable method.")
        # Fallback to direct linking if targets are not created (older pkg-config modules might just set vars)
        if(GIO_FOUND AND GLIB_FOUND AND GOBJECT_FOUND AND PIPEWIRE_FOUND)
             include_directories(screen_capture_addon PRIVATE
                ${GIO_INCLUDE_DIRS}
                ${GLIB_INCLUDE_DIRS}
                ${GOBJECT_INCLUDE_DIRS}
                ${PIPEWIRE_INCLUDE_DIRS}
            )
            target_link_libraries(screen_capture_addon PRIVATE
                ${GIO_LIBRARIES}
                ${GLIB_LIBRARIES}
                ${GOBJECT_LIBRARIES}
                ${PIPEWIRE_LIBRARIES}
                pthread dl
            )
             message(STATUS "Linking Linux dependencies using legacy PkgConfig variables.")
        else()
            message(FATAL_ERROR "Cannot find or link critical Linux dependencies (Gio, GLib, GObject, PipeWire) using PkgConfig variables.")
        endif()
    endif()
    # The __has_include in sc_session_linux.cpp handles conditional compilation internally
    # based on header presence, which is simpler than managing defines from CMake for this case.

else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()


# --- Include Directories for N-API and our headers ---
target_include_directories(screen_capture_addon PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" # So our .h files are found
)
if(NODE_INCLUDE_DIR)
    target_include_directories(screen_capture_addon PRIVATE "${NODE_INCLUDE_DIR}")
# elseif(NodeJS_FOUND AND NodeJS_INCLUDE_DIRS) # From find_package(NodeJS)
#    target_include_directories(screen_capture_addon PRIVATE "${NodeJS_INCLUDE_DIRS}")
endif()


# --- Installation (Optional, for packaging or system-wide install) ---
# install(TARGETS screen_capture_addon
#         LIBRARY DESTINATION your_lib_install_path # e.g., lib or lib/node_modules/...
#         RUNTIME DESTINATION your_bin_install_path # e.g., bin, if it were an executable
# )

# --- Output ---
message(STATUS "Screen capture addon target: screen_capture_addon.node")
# Using CMAKE_LIBRARY_OUTPUT_DIRECTORY which is set by default or can be overridden.
# CMAKE_BINARY_DIR is the top-level build directory.
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
message(STATUS "Output directory for libraries: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "Build directory: ${CMAKE_BINARY_DIR}")
