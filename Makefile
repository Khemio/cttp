all:
	g++ src/server.cpp -o build/server -lz

run:
	build/server --directory data