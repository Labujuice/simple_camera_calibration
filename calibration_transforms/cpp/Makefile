CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++11 `pkg-config --cflags opencv4`
LIBS = `pkg-config --libs opencv4`

OBJS = CameraCalibration.o verify.o
TARGET = verify_cpp

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
