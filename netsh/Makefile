

all: netsh

netsh: netsh.cpp buffer.cpp main.h buffer.h
	g++ -std=c++11 -o netsh netsh.cpp buffer.cpp  -D_GLIBCXX_DEBUG -fsanitize=address -Wall


clean:
	rm netsh 
