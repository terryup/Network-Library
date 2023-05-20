set(MEMORY_SRCS
  CentralCache.cc
  PageCache.cc
  ThreadCache.cc
  test.cc
)

add_executable(MemoryPool ${MEMORY_SRCS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/src/memory)

target_link_libraries(MemoryPool mymuduo)
