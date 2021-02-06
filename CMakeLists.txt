cmake_minimum_required(VERSION 3.7)

project(yac8)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/modules)
find_package(SDL2 REQUIRED)

include_directories(${SDL2_INCLUDE_DIR})

file(GLOB yac8_SRC "./**.cpp" "./imgui/*.cpp")

add_executable(yac8  ${yac8_SRC})
target_link_libraries(yac8 ${SDL2_LIBRARY})

# Copy SDL2 DLLs to output folder on Windows
if(WIN32)
    foreach(DLL ${SDL2_DLLS})
        add_custom_command(TARGET yac8 POST_BUILD COMMAND
                ${CMAKE_COMMAND} -E copy_if_different ${DLL} $<TARGET_FILE_DIR:yac8>)
    endforeach()
endif()