all : clean echo_client_server

echo_client_server: echo.o
	g++ -g -o echo_client echo_client.o -pthread
	g++ -g -o echo_server echo_server.o -pthread

echo.o:
	g++ -g -c -o echo_client.o echo_client.cpp
	g++ -g -c -o echo_server.o echo_server.cpp

clean:
	rm -f echo_client
	rm -f echo_server
	rm -f *.o
