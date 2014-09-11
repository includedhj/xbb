target=xbb_server

INCLUDE=-I
LDFLAGS=-L.
LDLIBS = -lpthread -ljson_linux-gcc-3.4.5_libmt

CXXFLAGS=-g -Wall -W -Winline -Wno-unused-parameter -Wno-unused-function

SOURCES := $(wildcard *.cpp)
HEADERS := *.h 
OBJS := $(SOURCES:.cpp=.o)

all :  $(target) output
.PHONY : all clean output

$(target) : $(OBJS) 
	g++ $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS) 

output:
	mkdir -p output
	mkdir -p output/bin
	cp -rf $(target) output/bin

clean:
	-rm -rf $(OBJS) 
	-rm -rf $(target)
	-rm -rf output
