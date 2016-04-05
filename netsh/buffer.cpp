#include "buffer.h"
#include "netsh.h"

Buffer::Buffer(int cap, int readFD, int writeFD, int epfd, int tid):cap(cap), readFD(readFD), writeFD(writeFD), epfd(epfd), tid(tid) {
    inEpollR = readFD != -1;
    inEpollW = writeFD != -1;
}

void Buffer::bufRead() {
    assert(readFD != -1);
    if ((int)deq.size() < cap) {
        int mx = cap - deq.size();
        string s(mx, 0);
        int len = read(readFD, (void *)s.data(), mx);
        db2("read len: ", len);
        if (len == 0) {
            modEpoll(epfd, readFD, EPOLLIN, 0);
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
            //db("+++++sub");
            inEpollR = 1;
        }
        if (writeFD != -1 && len > 0) bufWrite();
    }
    else {
        //db(inEpollR);
        if (inEpollR) {
            modEpoll(epfd, readFD, EPOLLIN, 0);
            //db("-----unsub");
            inEpollR = 0;
        }
    }
}

void Buffer::bufWrite() {
    //db("bufWrite");
    assert(writeFD != -1);
    db(deq.size());
    if (!deq.empty()) {
        string s(deq.begin(), deq.end());
        db(s.size());
        int len = write(writeFD, s.data(), s.size());
        db2("write len: ", len);
        if (len == -1) printError("write");
        for (int i = 0; i < len; i++)
            deq.pop_front();
        
        if (!deq.empty() && inEpollW == 0) {
            //db("+++++sub");
            modEpoll(epfd, writeFD, EPOLLOUT, EPOLLOUT); 
            inEpollW = 1;
        }
        if (readFD != -1 && len > 0) bufRead();
    }
    else {
        if (inEpollW) {
            //db("-----unsub");
            modEpoll(epfd, writeFD, EPOLLOUT, 0); 
            inEpollW = 0;
        }
    }
    //db("endBufWriter");
}

//void Buffer::unSubscribe() {
    //if (readFD != -1) {
        //delEpoll(epfd, readFD);
        //if (close(readFD) == -1) printError("close in buf 2");
        //readFD = -1;
    //}
    //if (writeFD != -1) {
        //delEpoll(epfd, writeFD);
        //if (close(writeFD) == -1) printError("close in buf 3");
        //writeFD = -1;
    //}
//}

