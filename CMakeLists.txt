#   设置CMake的最低版本和项目名称
cmake_minimum_required(VERSION 3.0)
project(mymuduo C CXX)

#生成debug版本，可以进行gdb调试
set(CMAKE_BUILD_TYPE "Debug")

# 添加搜索路径
include_directories(
    ${PROJECT_SOURCE_DIR}/src/base
    ${PROJECT_SOURCE_DIR}/src/net
    ${PROJECT_SOURCE_DIR}/src/net/poller
    ${PROJECT_SOURCE_DIR}/src/timer
    ${PROJECT_SOURCE_DIR}/src/logger
    ${PROJECT_SOURCE_DIR}/src/net/http
    ${PROJECT_SOURCE_DIR}/src/memory
    )

# 包含这些目录下的.cc文件
aux_source_directory(${PROJECT_SOURCE_DIR}/src/base SRC_BASE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/net SRC_NET)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/net/poller SRC_POLLER)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/timer SRC_TIMER)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/logger SRC_LOG)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/http SRC_HTTP)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/memory SRC_MEMORY)

# 设置编译选项
set(CXX_FLAGS -g -Wall -std=c++11)

# 生成动态库 mymuduo
add_library(mymuduo SHARED 
            ${SRC_BASE}
            ${SRC_NET}
            ${SRC_POLLER}
            ${SRC_TIMER}
            ${SRC_LOG}
            ${SRC_HTTP}
            ${SRC_MEMORY}
            )

# 目标动态库所需连接的库（这里需要连接libpthread.so）
target_link_libraries(mymuduo pthread)

# 设置生成动态库的路径，放在根目录的lib文件夹下面
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置安装的默认路径
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})

# 将目录安装到 include/mymuduo 下（只安装头文件）
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/ DESTINATION include/mymuduo
        FILES_MATCHING PATTERN "*.h")



add_subdirectory(src/http)
add_subdirectory(src/memory)

#   加载example
add_subdirectory(example)