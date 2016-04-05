#include <stdio.h>
#include <signal.h>
#include <unistd.h>


int flag = 0;

void sigHandler(int sig, siginfo_t *si, void * g) {
    if (flag) return;
    flag = 1;
    if (sig == SIGUSR1) printf("SIGUSR1 ");
    if (sig == SIGUSR2) printf("SIGUSR2 ");
    printf("%d\n", si->si_pid);
}


int main(){
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = sigHandler;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    sleep(10);
    if (!flag) {
        puts("No signals were caught");
    }

    return 0;
}
