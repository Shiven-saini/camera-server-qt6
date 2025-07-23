cmake_minimum_required(VERSION 3.16)
project(CameraServerQt6 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find Qt6 package (modified for MinGW) ---

# This makes CMake automatically handle MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Attempt to find Qt6. If this fails, we will search common MinGW paths.
find_package(Qt6 COMPONENTS Core Widgets Network)

# If Qt6 was not found automatically, search for it in common MinGW locations
if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found automatically. Searching common MinGW directories...")

    # List of potential installation paths for MinGW versions of Qt6
    # You can add your specific path here if it's not listed.
    set(QT6_MINGW_SEARCH_PATHS
        "C:/Qt/6.8.0/mingw_64"
        "C:/Qt/6.7.0/mingw_64"
        "C:/Qt/6.6.0/mingw_64"
        "C:/Qt/6.5.3/mingw_64"
        "C:/Qt/6.5.0/mingw_64"
        "C:/Qt6"
    )

    # Search the paths
    foreach(QT_PATH ${QT6_MINGW_SEARCH_PATHS})
        if(EXISTS "${QT_PATH}/lib/cmake/Qt6")
            message(STATUS "Found potential Qt6 MinGW installation at: ${QT_PATH}")
            # Prepend the path to CMAKE_PREFIX_PATH to give it priority
            set(CMAKE_PREFIX_PATH "${QT_PATH}" ${CMAKE_PREFIX_PATH})
            
            # Try to find the package again with the new hint
            find_package(Qt6 COMPONENTS Core Widgets Network)
            
            if(Qt6_FOUND)
                break() # Exit the loop if Qt is found
            endif()
        endif()
    endforeach()
endif()

# If Qt6 is still not found after searching, fail with a clear error
if(NOT Qt6_FOUND)
    message(FATAL_ERROR 
        "Qt6 for MinGW was not found.\n"
        "Please ensure Qt for MinGW is installed and that the CMAKE_PREFIX_PATH variable is set correctly.\n"
        "Example of how to run CMake with the correct path:\n"
        "cmake -S . -B build -G \"MinGW Makefiles\" -DCMAKE_PREFIX_PATH=C:/Qt/6.5.3/mingw_64"
    )
endif()

message(STATUS "Successfully found Qt version: ${Qt6_VERSION}")
message(STATUS "Using Qt from path: ${Qt6_DIR}")

# --- Project Sources and Executable ---

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CameraConfig.cpp
    src/CameraManager.cpp
    src/PortForwarder.cpp
    src/WindowsService.cpp
    src/SystemTrayManager.cpp
    src/Logger.cpp
    src/ConfigManager.cpp
)

# Header files
set(HEADERS
    include/MainWindow.h
    include/CameraConfig.h
    include/CameraManager.h
    include/PortForwarder.h
    include/WindowsService.h
    include/SystemTrayManager.h
    include/Logger.h
    include/ConfigManager.h
)

# Resource files
set(RESOURCES
    resources/resources.qrc
)

# Include directories
include_directories(include)

# Create executable
add_executable(CameraServerQt6 ${SOURCES} ${HEADERS} ${RESOURCES})

# Link Qt6 libraries
target_link_libraries(CameraServerQt6 PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)

# Copy Qt6 DLLs to the output directory using windeployqt (Corrected Version)
if(WIN32)
    # Find the windeployqt executable reliably
    find_program(
        WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS ${Qt6_DIR}/../../../bin # Qt6_DIR is set by find_package(Qt6)
        REQUIRED
    )

    message(STATUS "Found windeployqt at: ${WINDEPLOYQT_EXECUTABLE}")

    # Add the post-build command to run windeployqt
    add_custom_command(TARGET CameraServerQt6 POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:CameraServerQt6>
        COMMENT "Deploying Qt DLLs to the output directory..."
        VERBATIM
    )
endif()


# Set output directory
set_target_properties(CameraServerQt6 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Copy Qt6 DLLs to the output directory using windeployqt
if(WIN32)
    add_custom_command(TARGET CameraServerQt6 POST_BUILD
        COMMAND ${Qt6_HOST_PATH}/bin/windeployqt.exe $<TARGET_FILE:CameraServerQt6>
        COMMENT "Deploying Qt DLLs for MinGW..."
    )
endif()
