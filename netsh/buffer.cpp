#include "buffer.h"



Buffer::Buffer(int cap):cap(cap), readFD(-1), writeFD(-1) { }


void Buffer::bufRead() {
    assert(readFD != -1);
    if ((int)deq.size() < cap) {
        int mx = cap - deq.size();
        string s(mx, 0);
        int len = read(readFD, (void *)s.data(), mx);
        if (len == -1) printError("read"); // todo change
        for (int i = 0; i < len; i++)
            deq.push_back(s[i]);
        if (writeFD != -1 && len > 0) bufWrite();
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
        if (readFD != -1 && len > 0) bufRead();
    }
}


