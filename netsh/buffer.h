#include "main.h"


#ifndef MY_BUFFER_H
#define MY_BUFFER_H

struct Buffer {
    deque < char > deq;
    int cap;
    int readFD, writeFD;
    Buffer(int cap);
    void bufRead();
    void bufWrite();
};



#endif

