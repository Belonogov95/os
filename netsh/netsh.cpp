#include "main.h"
#include "buffer.h"

void printError(string s) {
    perror(s.data());
    exit(1);
}

void createDemon() {
    int cpid = fork();
    if (cpid == -1) printError("can't make first fork");
    if (cpid > 0) exit(0);

    if (setsid() == -1) printError("can't change session");

    cpid = fork();
    if (cpid == -1) printError("can't make second fork");    
    if (cpid > 0) exit(0);
}

void printPid() {
    FILE * f = fopen("tmp/netsh.pid", "w");        
    fprintf(f, "%d\n", getpid());
    fclose(f);
}

int createSocket(int port) {

    int sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sfd == -1) printError("socket");

    int one = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) == -1) printError("setsockopt");

    struct sockaddr_in addr = { 
        .sin_family = AF_INET, 
        .sin_port = htons(port), 
        .sin_addr = {.s_addr = INADDR_ANY}};


    if (bind(sfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) printError("bind");
    if (listen(sfd, LISTEN_BACKLOG) == -1) printError("listen");

    return sfd;
}

void makeSocketNonBlocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) printError("fcntl get");
    flags |= O_NONBLOCK;
    if (fcntl(sfd, F_SETFL, flags) == -1) printError("fcntl set");
}


void addInEpoll(int epfd, int sfd) {
    epoll_event event;
    event.data.fd = sfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, & event);
}

int main(int argc, char * argv[]){
    int port;
    if (argc != 2) printError("Usage: ./netsh port");
    if (sscanf(argv[1], "%d", &port) != 1) printError("Usage: ./netsh port");

    //createDemon();
    //printPid(); 
    int sfd = createSocket(port); 
    makeSocketNonBlocking(sfd);
    int epfd = epoll_create(1);
    if (epfd == -1) printError("epoll");
    addInEpoll(epfd, sfd); 


    while (true) {
        const int MAX_EVENTS = 1;
        epoll_event events[MAX_EVENTS];
        int res = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (res == -1) {
            if (errno == EINTR) 
                continue;
            printError("epoll_wait");
        } 
        assert(res == 1);
        int fd = events[0].data.fd;
        if (fd == sfd) {
            struct sockaddr_in client;
            socklen_t sz = sizeof(client);
            int nfd = accept(sfd, (struct sockaddr*)&client, &sz);

            //db("before");
            //db("after");
            if (nfd == -1) printError("accept");

            printf("accept = %d\n", fd);
            printf("from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

            char buffer[100];

            int len = read(fd, buffer, 100); 
            db(len);
            buffer[len] = 0;
            fprintf(stderr, "%s", buffer);
            return fd;


        }
    }







    return 0;
}

