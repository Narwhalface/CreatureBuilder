cmake_minimum_required(VERSION 3.20)

project(CreatureBuilderOpenGLShell LANGUAGES CXX)

add_executable(CreatureBuilder WIN32
    src/engineloader.cpp
    src/main.cpp
)

target_compile_features(CreatureBuilder PRIVATE cxx_std_17)

target_compile_definitions(CreatureBuilder PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

target_link_libraries(CreatureBuilder PRIVATE
    opengl32
)

if(MSVC)
    target_compile_options(CreatureBuilder PRIVATE /W4 /permissive-)
else()
    target_compile_options(CreatureBuilder PRIVATE -Wall -Wextra -Wpedantic)
endif()