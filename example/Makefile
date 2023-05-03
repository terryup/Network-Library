PROJECT_PATH=/home/zixuanhuang/webserver/
HEADER_PATH=-I ${PROJECT_PATH}/include/mymuduo/base \
			-I ${PROJECT_PATH}/include/mymuduo/net \
			-I ${PROJECT_PATH}/include/mymuduo/net/poller \
			-I ${PROJECT_PATH}/include/mymuduo/logger
LIB_PATH=-lmymuduo -lpthread
CFLAGS= -g -Wall ${LIB_PATH} ${HEADER_PATH}

all: testserver

testserver: testserver.cc
	g++ testserver.cc ${CFLAGS} -o testserver

clean:
	rm -r testserver