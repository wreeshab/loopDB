clean: 
	rm -f server client
server:
	g++ -o server server.cpp -pthread && ./server
client:
	g++ -o client client.cpp -pthread && ./client
