CXX = g++
CXXFLAGS = -Wall -Wextra
CPP_STD = --std=c++20
GTK_CFLAGS = `pkg-config --cflags gtkmm-4.0`
GTK_LIBS = `pkg-config --libs gtkmm-4.0`

SIM = tr1_sim

SRCS = $(wildcard src/*.cpp)
GUI_SRCS = $(wildcard src/gui/*.cpp)
SIM_SRCS = main.cpp $(SRCS) $(GUI_SRCS) 
CHANNEL_SRCS = channel.cpp $(SRCS)
PEER_SRCS = peer.cpp $(SRCS)

SIM_OBJS = $(SIM_SRCS:.cpp=.o)
CHANNEL_OBJS = $(CHANNEL_SRCS:.cpp=.o)
PEER_OBJS = $(PEER_SRCS:.cpp=.o)

OBJS = $(SIM_OBJS) $(CHANNEL_OBJS) $(PEER_OBJS)
NUM_OBJS = $(words $(sort $(OBJS)))
N = 

all: $(SIM) channel peer
	@rm -f $(OBJS)

$(SIM): $(OBJS)
	@echo "Linkando tr1_sim...    "
	@$(CXX) $(CXXFLAGS) $(CPP_STD) -o $@ $(SIM_OBJS) $(GTK_LIBS) 1>/dev/null

channel: $(CHANNEL_OBJS)
	@echo "Linkando channel...    "
	@$(CXX) $(CXXFLAGS) $(CPP_STD) -o $@ $^ 1>/dev/null

peer: $(PEER_OBJS)
	@echo "Linkando peer...    "
	@$(CXX) $(CXXFLAGS) $(CPP_STD) -o $@ $^ 1>/dev/null

%.o : %.cpp
	@printf "Compilando... [$(words $(N))/$(NUM_OBJS)]\r" 
	@$(CXX) $(CXXFLAGS) $(CPP_STD) -Iinclude/ -Iinclude/gui -c $< -o $@ $(GTK_CFLAGS) 1>/dev/null
	$(eval N := $(N) \*)

clean:
	@rm -f $(OBJS) $(SIM) channel peer