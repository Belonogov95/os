#include "main.h"


#ifndef MY_BUFFER_H
#define MY_BUFFER_H

struct Buffer {
    deque < char > deq;
    int cap;
    int readFD, writeFD;
    int epfd;
    bool inEpollR, inEpollW;
    Buffer(int cap, int readFD, int writeFD, int epfd);
    void bufRead();
    void bufWrite();
};



#endif

