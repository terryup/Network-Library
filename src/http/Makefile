PROJECT_PATH=/home/zixuanhuang/webserver/
HEADER_PATH=-I ${PROJECT_PATH}/include/mymuduo/base \
			-I ${PROJECT_PATH}/include/mymuduo/net \
			-I ${PROJECT_PATH}/include/mymuduo/net/poller \
			-I ${PROJECT_PATH}/include/mymuduo/loggerLIBS=-lmymuduo_base -lmymuduo_net -lpthread
LIB_PATH=-lmymuduo -lpthread
CFLAGS= -g -Wall ${LIB_PATH} ${HEADER_PATH}

all:server test

server: WebServer.cc HttpResponse_test.cc HttpConText.cc HttpServer_test.cc
	g++ WebServer.cc HttpResponse_test.cc HttpConText.cc HttpServer_test.cc ${CFLAGS} -o WebServer

clean:
	rm -r WebServer
