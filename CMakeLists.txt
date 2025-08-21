#------------------------------------------------------------------------------
# Editor/CMakeLists.txt
#
# Nightbloom Editor - Development tools and content creation
#------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)
project(NightbloomEditor VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find the Nightbloom Engine in sibling directory
get_filename_component(NIGHTBLOOM_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../NightBloom/Build" ABSOLUTE)
set(NightbloomEngine_DIR "${NIGHTBLOOM_ENGINE_DIR}")
message(STATUS "Looking for NightbloomEngine in: ${NightbloomEngine_DIR}")

find_package(NightbloomEngine REQUIRED)

# Gather source files
file(GLOB_RECURSE EDITOR_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.hpp"
)

# Create executable
add_executable(NightbloomEditor ${EDITOR_SOURCES})

# Set output name
set_target_properties(NightbloomEditor PROPERTIES OUTPUT_NAME "Editor")

if(WIN32)
    set_target_properties(NightbloomEditor PROPERTIES
        LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
endif()

# Link with engine
target_link_libraries(NightbloomEditor
    PRIVATE
        Nightbloom::NightbloomEngine
)

# Set as startup project in Visual Studio
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NightbloomEditor)

# Copy assets to output directory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
    add_custom_command(TARGET NightbloomEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets
        $<TARGET_FILE_DIR:NightbloomEditor>/Assets
    )
endif()

# Copy editor-specific resources
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/EditorResources")
    add_custom_command(TARGET NightbloomEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/EditorResources
        $<TARGET_FILE_DIR:NightbloomEditor>/EditorResources
    )
endif()

# Group files in IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${EDITOR_SOURCES})