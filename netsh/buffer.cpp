#include "buffer.h"
#include "netsh.h"

Buffer::Buffer(int cap, int readFD, int writeFD, int epfd):cap(cap), readFD(readFD), writeFD(writeFD), epfd(epfd) {
    inEpollR = readFD != -1;
    inEpollW = writeFD != -1;
}

void Buffer::bufRead() {
    assert(readFD != -1);
    if ((int)deq.size() < cap) {
        int mx = cap - deq.size();
        string s(mx, 0);
        int len = read(readFD, (void *)s.data(), mx);
        //db2("read len: ", len);
        if (len == 0) {
            delEpoll(epfd, readFD);
            readFD = -1;
        }
        if (len == -1) {
            if (errno != EAGAIN) 
                printError("read"); // todo change
        }
        for (int i = 0; i < len; i++)
            deq.push_back(s[i]);
        if ((int)deq.size() < cap && inEpollR == 0) {
            modEpoll(epfd, readFD, EPOLLIN, EPOLLIN);
            inEpollR = 1;
        }
        if (writeFD != -1 && len > 0) bufWrite();
    }
    else {
        if (inEpollR) {
            modEpoll(epfd, readFD, EPOLLIN, 0);
            inEpollR = 0;
        }
    }
}

void Buffer::bufWrite() {
    assert(writeFD != -1);
    if (!deq.empty()) {
        string s(deq.begin(), deq.end());
        int len = write(writeFD, s.data(), s.size());
        if (len == -1) printError("write");
        for (int i = 0; i < len; i++)
            deq.pop_back();
        
        if (!deq.empty() && inEpollW == 0) {
            modEpoll(epfd, writeFD, EPOLLOUT, EPOLLOUT); 
            inEpollW = 1;
        }
        if (readFD != -1 && len > 0) bufRead();
    }
    else {
        if (inEpollW) {
            modEpoll(epfd, writeFD, EPOLLOUT, 0); 
            inEpollW = 0;
        }
    }
}


