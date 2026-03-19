CXX      := g++
CXXFLAGS := -std=c++23 -O2 -Wall -Wextra
LDFLAGS  := -lzstd

SRCS := main.cpp net.cpp sender.cpp receiver.cpp compress.cpp
OBJS := $(SRCS:.cpp=.o)
BIN  := ftransfer

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(BIN)

.PHONY: all clean
