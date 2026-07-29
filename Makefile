CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pthread

all: sender receiver

sender: sender.cpp protocol.h
	$(CXX) $(CXXFLAGS) sender.cpp -o sender

receiver: receiver.cpp protocol.h
	$(CXX) $(CXXFLAGS) receiver.cpp -o receiver

clean:
	rm -f sender receiver

.PHONY: all clean