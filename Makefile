src = $(wildcard ./example/*.cpp)
inc = ./include/
target = ./a.out

ALL:$(target)

CC = g++
CXXFLAGS = -std=c++11 

$(target):$(src)
	$(CC) $^ $(CXXFLAGS) -I $(inc) -lpthread -o $@

clean:
	-rm -rf $(target)

.PHONY: clean ALL