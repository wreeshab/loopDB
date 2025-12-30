CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -pthread

SERVER = server
CLIENT = client

all: $(SERVER) $(CLIENT)

$(SERVER): server.cpp hashmap.cpp hashmap.hpp avl_tree.cpp avl_tree.hpp util.hpp
	$(CXX) $(CXXFLAGS) -o $(SERVER) server.cpp hashmap.cpp avl_tree.cpp

$(CLIENT): client.cpp
	$(CXX) $(CXXFLAGS) -o $(CLIENT) client.cpp

clean:
	rm -f $(SERVER) $(CLIENT)
