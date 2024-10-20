CXX = g++
CFLAGS = -g -O2 -Wall -fPIC -Wno-deprecated

INC = -I ../../include
LIB = -L ../../lib -llreactor -lpthread
OBJS = $(addsuffix .o, $(basename $(wildcard *.cc)))

all:
	$(CXX) -o udpClient $(CFLAGS) udpClient.cpp $(INC) $(LIB)
	$(CXX) -o udpServer $(CFLAGS) udpServer.cpp $(INC) $(LIB)
clean:
	-rm -f *.o  udpClient udpServer
