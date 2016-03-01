#include <stdio.h>
#include <unistd.h>

#define T 1024

char buff1[T];


int main() {
    for (;;) {
        int cnt = read(0, buff1, T);
        if (cnt == 0) break;
        int l = 0;
        for (;l < cnt;) {
            int d = write(1, buff1 + l, cnt - l);
            l += d;
        }
    }


    return 0;
}













