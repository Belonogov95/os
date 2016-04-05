#include "main.h"


#ifndef MY_BUFFER_H
#define MY_BUFFER_H

struct Buffer {
    deque < char > deq;
    int cap;
    int readFD, writeFD;
    int epfd;
    bool inEpollR, inEpollW;
    int tid;
    Buffer(int cap, int readFD, int writeFD, int epfd, int tid);
    void bufRead();
    void bufWrite();
    void unSubscribe();
};



#endif

