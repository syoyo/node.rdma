NODE_DIR=../../../src/node-v0.6.6
UV_DIR=${NODE_DIR}/deps/uv

all:
	#gcc -g -o client -I${UV_DIR}/include client.c ${UV_DIR}/uv.a -lrdmacm -lm -pthread -lrt
	gcc -g -o server -I${UV_DIR}/include server.c ${UV_DIR}/uv.a -lrdmacm -lm -pthread -lrt 
