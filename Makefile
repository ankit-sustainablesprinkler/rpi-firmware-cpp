#CPPFLAGS+=-O3 -I/opt/rootfs-raspbian/usr/include/ -I/opt/rootfs-raspbian/usr/local/include/
INC=$(addprefix -I, ./ lcm/ modem/include/ bin_protocol/include/ sensor/lcm/ sensor/)
CXXFLAGS=-std=c++11 -Os
LDLIBS=-lwiringPi -llcm -lstdc++
MODEM_LIBS=-lboost_system
LDFLAGS=-pthread
# -L/opt/rootfs-raspbian/usr/lib/arm-linux-gnueabihf -L/opt/rootfs-raspbian/lib/arm-linux-gnueabihf  -L/opt/rootfs-raspbian/usr/local/lib -Xlinker -rpath-link=/opt/rootfs-raspbian/lib/arm-linux-gnueabihf/ -Xlinker -rpath-link=/opt/rootfs-raspbian/usr/lib/arm-linux-gnueabihf/
BIN_DIR=bin/
OBJ=$(addprefix $(BIN_DIR),functions.o base64.o bin_protocol.o crc16.o modem.o modem_driver.o serial_driver.o)
SCHEDULE_OBJ=$(addprefix $(BIN_DIR),main.o schedule.o) $(OBJ)
MODEM_OBJ=$(addprefix $(BIN_DIR),modem_test.o modem.o modem_driver.o serial_driver.o)
TEST_OBJ=$(addprefix $(BIN_DIR),test.o schedule.o) $(OBJ)

all: main logger get_log
	
main: $(SCHEDULE_OBJ)
	g++ $(CXXFLAGS) $(MODEM_LIBS) $(LDFLAGS) $(LDLIBS) -o s3-main $(SCHEDULE_OBJ)
	
modem: $(MODEM_OBJ)
	g++ $(CXXFLAGS) $(MODEM_LIBS) $(LDFLAGS) $(LDLIBS) -o modem_test $(MODEM_OBJ) -Wl,-Map=output.map

test: $(TEST_OBJ)
	g++ $(CXXFLAGS) $(MODEM_LIBS) $(LDFLAGS) $(LDLIBS) -o test $(TEST_OBJ)

logger: logger.cpp
	g++ $(CXXFLAGS) $(INC) -llcm -o logger logger.cpp
get_log: get_log.cpp
	g++ $(CXXFLAGS) $(INC) -llcm -o get_log get_log.cpp
	

$(BIN_DIR)main.o: main.cpp
	g++ $(CXXFLAGS) $(INC) -c main.cpp -o $(BIN_DIR)main.o
$(BIN_DIR)functions.o: functions.cpp
	g++ $(CXXFLAGS) $(INC) -c functions.cpp -o $(BIN_DIR)functions.o
$(BIN_DIR)base64.o: bin_protocol/base64.cpp
	g++ $(CXXFLAGS) $(INC) -c bin_protocol/base64.cpp -o $(BIN_DIR)base64.o
$(BIN_DIR)bin_protocol.o: bin_protocol/bin_protocol.cpp
	g++ $(CXXFLAGS) $(INC) -c bin_protocol/bin_protocol.cpp -o $(BIN_DIR)bin_protocol.o
$(BIN_DIR)crc16.o: bin_protocol/crc16.cpp
	g++ $(CXXFLAGS) $(INC) -c bin_protocol/crc16.cpp -o $(BIN_DIR)crc16.o

$(BIN_DIR)schedule.o: schedule.cpp
	g++ $(CXXFLAGS) $(INC) -c schedule.cpp -o $(BIN_DIR)schedule.o
	

$(BIN_DIR)modem_test.o: modem/modem_test.cpp
	g++ $(CXXFLAGS) $(INC) -c modem/modem_test.cpp -o $(BIN_DIR)modem_test.o
	
$(BIN_DIR)modem.o: modem/modem.cpp
	g++ $(CXXFLAGS) $(INC) -c modem/modem.cpp -o $(BIN_DIR)modem.o
	
$(BIN_DIR)modem_driver.o: modem/modem_driver.cpp
	g++ $(CXXFLAGS) $(INC) -c modem/modem_driver.cpp -o $(BIN_DIR)modem_driver.o
	
$(BIN_DIR)serial_driver.o: modem/serial_driver.cpp
	g++ $(CXXFLAGS) $(INC) -c modem/serial_driver.cpp -o $(BIN_DIR)serial_driver.o

$(BIN_DIR)test.o: testing/test.cpp
	g++ $(CXXFLAGS) $(INC) -c testing/test.cpp -o $(BIN_DIR)test.o
