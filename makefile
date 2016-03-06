mainmake: server.cpp receiver.cpp
	g++ server.cpp -o server -lpthread
	g++ receiver.cpp -o receiver
