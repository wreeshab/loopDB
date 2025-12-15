CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -pthread

SERVER = server
CLIENT = client

all: $(SERVER) $(CLIENT)

$(SERVER): server.cpp
	$(CXX) $(CXXFLAGS) -o $(SERVER) server.cpp

$(CLIENT): client.cpp
	$(CXX) $(CXXFLAGS) -o $(CLIENT) client.cpp

clean:
	rm -f $(SERVER) $(CLIENT)
