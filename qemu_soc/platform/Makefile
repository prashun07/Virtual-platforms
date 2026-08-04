SYSTEMC_HOME    ?= $(HOME)/systemc/install
SYSTEMC_INCLUDE ?= $(SYSTEMC_HOME)/include
SYSTEMC_LIBDIR  ?= $(SYSTEMC_HOME)/lib

CXX      ?= clang++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
            -I$(SYSTEMC_INCLUDE) \
            -I../wrapper \
            -I../include \
            -I../../Timer
LDFLAGS  := -L$(SYSTEMC_LIBDIR) -lsystemc -pthread

TARGET := cosim_platform

.PHONY: all clean run

all: $(TARGET)

$(TARGET): cosim_main.cpp ../wrapper/cosim_server.cpp
	$(CXX) $(CXXFLAGS) cosim_main.cpp ../wrapper/cosim_server.cpp \
	    $(LDFLAGS) -o $@

run: $(TARGET)
	DYLD_LIBRARY_PATH="$(SYSTEMC_LIBDIR):$${DYLD_LIBRARY_PATH}" ./$(TARGET)

clean:
	rm -f $(TARGET)
