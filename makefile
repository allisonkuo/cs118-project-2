mainmake: server.cpp receiver.cpp
	g++ server.cpp -o server
	g++ receiver.cpp -o receiver
