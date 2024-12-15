
CXX = g++
CXXFLAGS = -std=c++17 -O2
TARGET = simplebench

# Source files
SRCS = mainCode.cpp

OBJS = $(SRCS:.cpp=.o)

# Default rule
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean