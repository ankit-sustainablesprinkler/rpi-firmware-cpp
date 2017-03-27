CXXFLAGS=-std=c++11 -Os
LDLIBS=-lwiringPi -llcm
BIN_DIR=bin/
INC=$(addprefix -I, lcm/)

OBJ=$(addprefix $(BIN_DIR),sensor_node.o sensor.o moving_average.o crc16.o)

all: $(OBJ)
	g++ $(CXXFLAGS) $(LDLIBS) -o sensor_node $(OBJ)

$(BIN_DIR)sensor_node.o: sensor_node.cpp
	g++ $(CXXFLAGS) $(INC) -c sensor_node.cpp -o $(BIN_DIR)sensor_node.o
$(BIN_DIR)sensor.o: sensor.cpp
	g++ $(CXXFLAGS) $(INC) -c sensor.cpp -o $(BIN_DIR)sensor.o
$(BIN_DIR)moving_average.o: moving_average.cpp
	g++ $(CXXFLAGS) $(INC) -c moving_average.cpp -o $(BIN_DIR)moving_average.o
$(BIN_DIR)crc16.o: crc16.cpp
	g++ $(CXXFLAGS) $(INC) -c crc16.cpp -o $(BIN_DIR)crc16.o