CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -O2 -Iinclude

SRC_COMMON = src/common.cpp
SRC_BUFFER = src/buffer.cpp
SRC_SERVER = src/server.cpp
SRC_CLIENT = src/client.cpp

all: server client

server: $(SRC_SERVER) $(SRC_COMMON) $(SRC_BUFFER)
	$(CXX) $(CXXFLAGS) -o server $(SRC_SERVER) $(SRC_COMMON) $(SRC_BUFFER)

client: $(SRC_CLIENT) $(SRC_COMMON)
	$(CXX) $(CXXFLAGS) -o client $(SRC_CLIENT) $(SRC_COMMON)

clean:
	rm -f server client

.PHONY: all clean
