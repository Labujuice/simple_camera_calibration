CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -std=c99
CXXFLAGS = -Wall -Wextra -O2 -std=c++11 `pkg-config --cflags opencv4`
LIBS = `pkg-config --libs opencv4` -lm -lstdc++

OBJS = camera_calibration.o cv_wrapper.o verify.o
TARGET = verify_c

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

cv_wrapper.o: cv_wrapper.cpp cv_wrapper.h
	$(CXX) $(CXXFLAGS) -c cv_wrapper.cpp -o cv_wrapper.o

clean:
	rm -f $(OBJS) $(TARGET)
