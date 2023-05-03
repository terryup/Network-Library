# 加载子目录
set(
    testserver.cc
)

add_executable(echoServer testserver.cc)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/example)

target_link_libraries(echoServer mymuduo)