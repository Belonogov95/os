#include "main.h"
#include "buffer.h"

void printError(string s) {
    perror(s.data());
    exit(0);
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

map < int, int > epollMask;

void addEpoll(int epfd, int sfd, int mask) {
    epoll_event event;
    event.data.fd = sfd;
    event.events = mask;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, & event) == -1) printError("epoll add");
    epollMask[sfd] = mask;
}

void delEpoll(int epfd, int sfd) {
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, sfd, NULL) == -1) printError("epoll del");
    epollMask.erase(sfd);
}

void modEpoll(int epfd, int sfd, int what, int val) {
    assert(epollMask.count(sfd) == 1);
    int oldMask = epollMask[sfd];
    int mask = ((oldMask & what) ^ oldMask) | val;
    epoll_event event;
    event.events = mask;
    event.data.fd = sfd;
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, sfd, &event) == -1) printError("epoll ctl");
    epollMask[sfd] = mask;
}


map < int, shared_ptr < Buffer > > q1;
map < pair < int, int >, shared_ptr < Buffer > > q2;

vector < string > split(string s, char ch) {
    vector < string > res;
    for (int i = 0; i < (int)s.size();) {
        for (;i < (int)s.size() && s[i] == ch; i++);
        if (i == (int)s.size()) break;

        int j = i;
        for (; i < (int)s.size() && s[i] != ch; i++);
        res.pb(s.substr(j, i - j));
    }
    return res;
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
    if (epfd == -1) printError("epoll create");

    addEpoll(epfd, sfd, EPOLLIN); 

    db2(EPOLLIN, EPOLLOUT);

    while (true) {
        cerr << endl << endl;
        const int MAX_EVENTS = 1;
        epoll_event events[MAX_EVENTS];
        db("before epoll_wait");
        int res = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (res == -1) {
            if (errno == EINTR) 
                continue;
            printError("epoll_wait");
        } 
        assert(res == 1);
        int fd = events[0].data.fd;
        int mask = events[0].events;
        cerr << "mask: " << mask << "    "; db2(fd, sfd);
        assert(mask == EPOLLIN || mask == EPOLLOUT);
        if (fd == sfd) {
            struct sockaddr_in client;
            socklen_t sz = sizeof(client);
            int nfd = accept(sfd, (struct sockaddr*)&client, &sz);

            addEpoll(epfd, nfd, EPOLLIN);
            db(nfd);
            q1[nfd] = shared_ptr < Buffer > (new Buffer(CAP, nfd, -1, epfd));
        }
        else if (q1.count(fd) == 1) {
            auto buf = q1[fd];
            buf->bufRead();
            bool flag = 0;
            for (auto x: buf->deq) 
                flag |= x == '\n';
            if (flag) {
                string s(buf->deq.begin(), buf->deq.end());
                auto r1 = split(s, '\n');
                vector < string > r2 = split(r1[0], '|');
                vector < vector < string > > commands;
                for (auto vec: r2) {
                    commands.pb(split(vec, ' '));
                }
                vector < pair < int, int > > pipes;
                int k = commands.size();
                assert(k >= 1);
                cerr << "========\n";
                for (auto cc: commands) {
                    for (auto x: cc)
                        cerr << x << " ";
                    cerr << endl;
                }

                for (int i = 0; i < k + 1; i++) {
                    int p[2];
                    if (pipe(p) == -1) printError("pipe");
                    pipes.pb(mp(p[0], p[1]));
                }
                //cerr << "pipes:\n";
                //for (auto x: pipes)
                    //db2(x.fr, x.sc);

                db(getpid());
                for (int i = 0; i < k; i++) {
                    int res = fork(); 
                    if (res == -1) printError("fork in for");
                    if (res == 0) {
                        //db(getpid());
                        dup2(pipes[i].fr, STDIN_FILENO);
                        dup2(pipes[i + 1].sc, STDOUT_FILENO);
                        for (auto pp: pipes) {
                            if (close(pp.fr) == -1) printError("close in for 1");
                            if (close(pp.sc) == -1) printError("close in for 2");
                        }
                        char ** tmp = new char * [commands[i].size() + 1];
                        for (int j = 0; j < (int)commands[i].size(); j++)    {
                            char * ptr = new char[commands[i][j].size()];
                            strcpy(ptr, commands[i][j].data());
                            tmp[j] = ptr;
                        }
                        tmp[commands[i].size()] = NULL;
                        //int mm = commands[i].size();
                        //db(mm);
                        //for (int j = 0; j <= mm; j++) {
                            //long long g = (long long)tmp[j];
                            //db(g);
                        //}

                        execvp(commands[i][0].data(), tmp);
                        exit(0);
                    } 
                }

                for (int i = 0; i < k + 1; i++) {
                    if (i != k && close(pipes[i].fr) == -1) printError("close in for 1");
                    if (i != 0 && close(pipes[i].sc) == -1) printError("close in for 2");
                }

                shared_ptr < Buffer > bufLeft(new Buffer(CAP, fd, pipes[0].sc, epfd));
                shared_ptr < Buffer > bufRight(new Buffer(CAP, pipes[k].fr, fd, epfd));
                 
                // add to left buffer 
                int posBack = -1;
                for (int i = 0; i < (int)s.size(); i++)
                    if (s[i] == '\n') {
                        posBack = i + 1;
                        break;
                    }
                assert(posBack != -1);
                for (int i = posBack; i < (int)s.size(); i++)
                    bufLeft->deq.pb(s[i]);


                q2[mp(pipes[0].sc, EPOLLOUT)] = bufLeft;
                q2[mp(fd, EPOLLIN)] = bufLeft;
                q2[mp(fd, EPOLLOUT)] = bufRight;
                q2[mp(pipes[k].fr, EPOLLIN)] = bufRight;
                db2(epfd, pipes[0].sc);
                db2(epfd, pipes[k].fr);
                addEpoll(epfd, pipes[0].sc, EPOLLOUT);
                addEpoll(epfd, pipes[k].fr, EPOLLIN);
            }
        }
        else if (q2.count(mp(fd, mask)) == 1) {
            shared_ptr < Buffer > buf = q2[mp(fd, mask)]; 
            if (mask == EPOLLIN)
                buf->bufRead();  
            else
                buf->bufWrite();
        }
        else 
            assert(false);
    }

    return 0;
}



 
            //if (nfd == -1) printError("accept");

            //printf("accept = %d\n", fd);
            //printf("from %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

            //char buffer[100];

            //int len = read(fd, buffer, 100); 
            //db(len);
            //buffer[len] = 0;
            //fprintf(stderr, "%s", buffer);
            //return fd;


