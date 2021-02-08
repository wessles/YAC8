cmake_minimum_required(VERSION 3.7)

project(yac8)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/modules)
find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)

add_library(GLAD "extern/glad/src/glad.c")
target_include_directories(GLAD PUBLIC "extern/glad/include")

file(GLOB imgui_SRC
        "./imgui/*.cpp"
        "./imgui/backends/imgui_impl_opengl3.cpp"
        "./imgui/backends/imgui_impl_sdl.cpp"
        )

file(GLOB yac8_SRC "*.cpp")
add_executable(yac8  ${yac8_SRC} ${imgui_SRC})
target_include_directories(yac8 PUBLIC ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} "extern/glad/include")
target_link_libraries(yac8 ${SDL2_LIBS} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(yac8 ${OPENGL_LIBRARIES} GLAD)
target_compile_definitions(yac8 PUBLIC
        GL_GLEXT_PROTOTYPES=1
        WINDOWS_IGNORE_PACKING_MISMATCH)

# Copy SDL2 DLLs to output folder on Windows
if(WIN32)
    foreach(DLL ${SDL2_DLLS})
        add_custom_command(TARGET yac8 POST_BUILD COMMAND
                ${CMAKE_COMMAND} -E copy_if_different ${DLL} $<TARGET_FILE_DIR:yac8>)
    endforeach()
endif()