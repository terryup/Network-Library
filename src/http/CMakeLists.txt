set(HTTP_SRCS
  HttpResponse_test.cc
  HttpServer_test.cc
  HttpContext.cc
  WebServer.cc
  mian.cc
)

add_executable(WebServer ${HTTP_SRCS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/src/http)

target_link_libraries(WebServer mymuduo)
