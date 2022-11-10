cmake_minimum_required(VERSION 3.3 FATAL_ERROR)

set(project_name "AlphaNoGo")
project(${project_name} CXX)

######################### Options  #########################
option(USE_BOTZONE_ONLINE "Use Botzone Online" ON)
option(BUILD_LIBRARY "Build Library" OFF)
option(BUILD_WITH_TORCH "Build with Torch" OFF)

# https://wiki.botzone.org.cn/index.php?title=Bot#.E9.95.BF.E6.97.B6.E8.BF.90.E8.A1.8C
option(NO_BUILD_LONGRUN "Bot will not support long run" OFF)

############################################################
add_executable(${project_name}
        ./src/main.cpp
        )

target_include_directories(${project_name} PRIVATE ./deps)
target_include_directories(${project_name} PRIVATE ./include)

# Maximum CXX Version for Botzone G++ Compiler is CXX 17
set_property(TARGET ${project_name} PROPERTY CXX_STANDARD 17)

if (USE_BOTZONE_ONLINE)
    target_compile_definitions(${project_name} PRIVATE _BOTZONE_ONLINE)
endif ()

if (NO_BUILD_LONGRUN)
    target_compile_definitions(${project_name} PRIVATE _NOT_BOTZONE_LONGRUN)
endif ()

if (BUILD_LIBRARY)
    target_compile_definitions(${project_name} PRIVATE BUILD_SHARED)
    add_library(${project_name}_shared SHARED ./src/main.cpp)
endif ()

if (BUILD_WITH_TORCH)
    target_compile_definitions(${project_name} PRIVATE _WITH_TORCH)
    find_package(Torch REQUIRED PATHS ./deps/libtorch)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
    target_link_libraries(${project_name} "${TORCH_LIBRARIES}")
    if (MSVC)
        file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
        add_custom_command(TARGET ${project_name}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${TORCH_DLLS}
                $<TARGET_FILE_DIR:${project_name}>)
    endif ()
endif ()

