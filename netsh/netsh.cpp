#include "main.h"
#include "buffer.h"

map < int, string > fdMean;


string getFdMean(int fd) {
    if (fdMean.count(fd) == 0) return "null";
    return fdMean[fd];
}

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
    FILE * f = fopen("/tmp/netsh.pid", "w");        
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
    //db2("insert ", getFdMean(sfd));
    epollMask[sfd] = mask;
}

void delEpoll(int epfd, int sfd) {
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, sfd, NULL) == -1) printError("epoll del");
    //db2("remove ", getFdMean(sfd));
    epollMask.erase(sfd);
}

void modEpoll(int epfd, int sfd, int what, int val) {
    if (!(epollMask.count(sfd) == 1)) {
        db(sfd);
        db2(what, val);
    }
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
    int c1 = 0;
    //int c2 = 0;

    for (int i = 0; i < (int)s.size();) {
        for (;i < (int)s.size() && s[i] == ch; i++);
        if (i == (int)s.size()) break;

        int j = i;
        for (; i < (int)s.size() && (s[i] != ch || c1 % 2 == 1); i++) {
            c1 += s[i] == '\'';
        }
        res.pb(s.substr(j, i - j));
    }
    return res;
}

struct Task {
    int cnt, sfd, lfd, rfd; 
    bool finished;
    shared_ptr < Buffer > rightBuf;
    Task(int cnt, int sfd, int lfd, int rfd): cnt(cnt), sfd(sfd), lfd(lfd), rfd(rfd), finished(0) { }
};


map < int, int > taskId;
vector < Task > task;

int superPipe[2];

void sigHandler(int, siginfo_t *si, void *) {
    int pid = si->si_pid;
    db(taskId.size());
    for (auto x: taskId)
        cerr << "(" << x.fr << ", " << x.sc << ")" << "   ";
    cerr << endl;
    db(pid);
    int tid;
    if (taskId.count(pid) == 0) {
        assert(!task.empty());
        tid = (int)task.size() - 1; 
    }
    else 
        tid = taskId[pid];
    //assert(taskId.count(pid) == 1);
    task[tid].cnt--;
    assert(task[tid].cnt >= 0);
    if (task[tid].cnt == 0) {
        char s[20];
        assert(tid < (int)task.size());
        sprintf(s, "%d ", tid);
        int g = write(superPipe[1], s, strlen(s));
        assert((int)strlen(s) == g);
    }
}


void finishTask(int tid) {
    if (task[tid].finished) return;
    //cerr << "sock: " << task[tid].sfd << " " << task[tid].lfd << " " << task[tid].rfd << endl;
    int epfd = task[tid].rightBuf->epfd;
    if (epollMask.count(task[tid].lfd) == 1) delEpoll(epfd, task[tid].lfd);
    if (epollMask.count(task[tid].rfd) == 1) delEpoll(epfd, task[tid].rfd);
    if (epollMask.count(task[tid].sfd) == 1) delEpoll(epfd, task[tid].sfd);

    //delEpoll(epfd, task[tid].rfd);
    //delEpoll(epfd, task[tid].sfd);
    if (close(task[tid].lfd) == -1) printError("close"); 
    if (close(task[tid].rfd) == -1) printError("close");
    if (close(task[tid].sfd) == -1) printError("close");
    task[tid].finished = 1;
}


bool checkEnd(int tid) {
    bool flagOK = 1;
    db(tid);
    //cerr << "checkEnd tid read write: " << tid << " " << task[tid].rightBuf->readFD << " " << task[tid].rightBuf->writeFD << endl;
    if (task[tid].rightBuf->readFD != -1){
        task[tid].rightBuf->bufRead();
        flagOK &= (int)task[tid].rightBuf->deq.size() < task[tid].rightBuf->cap;
    }
    if (task[tid].rightBuf->writeFD != -1) {
        task[tid].rightBuf->bufWrite();
        flagOK &= (int)task[tid].rightBuf->deq.empty();
    }
    return flagOK;
}

void remFD(shared_ptr < Buffer > buf, int fd) {
    if (buf->readFD == fd) {
        buf->readFD = -1;
        buf->inEpollR = 0;
    }
    if (buf->writeFD == fd) {
        buf->writeFD = -1;
        buf->inEpollW = 0; 
    }
}


map < int, int > cntFail;

int main(int argc, char * argv[]){
    //string s = "  grep 'model name'  ";
    //auto res = split(s, ' ');
    //for (auto x: res)
        //db(x);
    //return 0;

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = sigHandler;
    sigaction(SIGCHLD, &sa, NULL);

    int port;
    if (argc != 2) printError("Usage: ./netsh port");
    if (sscanf(argv[1], "%d", &port) != 1) printError("Usage: ./netsh port");

    //createDemon();
    //printPid(); 
    int sfd = createSocket(port); 
    makeSocketNonBlocking(sfd);
    int epfd = epoll_create(1);
    if (epfd == -1) printError("epoll create");
    fdMean[epfd] = "epoll fd";
    fdMean[sfd] = "main socket";

    addEpoll(epfd, sfd, EPOLLIN); 

    if (pipe2(superPipe, O_CLOEXEC) == -1) printError("superPipe");

    fdMean[superPipe[0]] = "superPipe read";
    fdMean[superPipe[1]] = "superPipe write";

    //db2(superPipe[0], superPipe[1]);

    addEpoll(epfd, superPipe[0], EPOLLIN);

    for (int iter = 0;; iter++) {
        //cerr << endl;
        //db(iter);
        const int MAX_EVENTS = 1;
        epoll_event events[MAX_EVENTS];
        //db("before wait");
        int res = epoll_wait(epfd, events, MAX_EVENTS, -1);
        //db("after wait");
        //db2("epoll : ", res);
        if (res == -1) {
            if (errno == EINTR)  {
                db("EINTR");
                continue; }
            printError("epoll_wait");
        } 

        assert(res == 1);
        int fd = events[0].data.fd;
        int mask = events[0].events;
        //db2(mask, fdMean[fd]);

        if (fd == superPipe[0]) {
            //db("in superPipe");
            char buf[20];
            read(superPipe[0], buf, sizeof(buf));
            int tid = -1;
            if (sscanf(buf, "%d", &tid) != 1) printError("scanf tid");
            db2(tid, task.size());
            assert(tid < (int)task.size());
            assert(task[tid].cnt == 0);
            //db("CLOSE!!!");
            if (checkEnd(tid))
                finishTask(tid);
            continue;
        }

        if ((mask & EPOLLERR) || (mask & EPOLLHUP)) {
            cntFail[fd]++;
        }

        if (mask & EPOLLERR) mask ^= EPOLLERR;
            
        //if (mask == EPOLLHUP) {
        if (cntFail[fd] > 5) {
            //db2("clean HUP ERR", (mask == EPOLLERR));
            //db2("\t\t\t\t\tso many fail", fd);
            //continue;
            delEpoll(epfd, fd);

            if (q2.count(mp(fd, EPOLLIN)) == 1) remFD(q2[mp(fd, EPOLLIN)], fd);
            if (q2.count(mp(fd, EPOLLOUT)) == 1) remFD(q2[mp(fd, EPOLLOUT)], fd);
            //modEpoll(epfd, fd, EPOLLIN | EPOLLOUT, 0);
            continue;
        }

        if (mask & EPOLLHUP) mask ^= EPOLLHUP;

        if (mask == 0) {
            continue;
        }

        db(mask);
        assert(mask == EPOLLIN || mask == EPOLLOUT || mask == (EPOLLIN | EPOLLOUT));
        if (fd == sfd) {
            sockaddr_in client;
            socklen_t sz = sizeof(client);
            int nfd = accept(sfd, (sockaddr*)&client, &sz);
            //db2("-----from accept: ", nfd);
            makeSocketNonBlocking(nfd);
            fdMean[nfd] = "child socket";
            
            //db("!!!!!!!!!!");
            //char tmp[100];
            //sprintf(tmp, "hello\n");
            //int res = write(nfd, tmp, strlen(tmp));
            //db(res);
            //close(nfd);
            //exit(0);

            addEpoll(epfd, nfd, EPOLLIN);
            //db(nfd);
            q1[nfd] = shared_ptr < Buffer > (new Buffer(CAP, nfd, -1, epfd, -1));
        }
        else if (q1.count(fd) == 1) {
            auto bufLeft = q1[fd];
            bufLeft->bufRead();
            bool flag = 0;
            for (auto x: bufLeft->deq) 
                flag |= x == '\n';
            if (flag) {
                string s(bufLeft->deq.begin(), bufLeft->deq.end());
                //buf->unSubscribe();
                q1.erase(fd);
                auto r1 = split(s, '\n');
                vector < string > r2 = split(r1[0], '|');
                //db2(bufLeft->deq.size(), r1[0].size());
                for (int i = 0; i < (int)r1[0].size() + 1; i++)
                    bufLeft->deq.pop_front();

                vector < vector < string > > commands;
                for (auto vec: r2) 
                    commands.pb(split(vec, ' '));

                vector < pair < int, int > > pipes;
                int k = commands.size();
                assert(k >= 1);

                //cerr << "========\n";
                //for (auto cc: commands) {
                    //for (auto x: cc)
                        //cerr << x << "!!";
                    //cerr << endl;
                //}

                for (auto & cc: commands) {
                    for (auto & x: cc) {
                        //db(x);
                        //db2((int)x[0], (int)'\'');
                        if (x[0] == '\'') x.erase(x.begin());
                        if (x.back() == '\'') x.pop_back();

                        if (x[0] == '"') x.erase(x.begin());
                        if (x.back() == '"') x.pop_back();

                    }
                }

                cerr << "========\n";
                for (auto cc: commands) {
                    for (auto x: cc)
                        cerr << x << "!!";
                    cerr << endl;
                }

                for (int i = 0; i < k + 1; i++) {
                    int p[2];
                    if (pipe2(p, O_CLOEXEC) == -1) printError("pipe");
                    pipes.pb(mp(p[0], p[1]));
                }
                makeSocketNonBlocking(pipes[0].sc);
                makeSocketNonBlocking(pipes[k].fr);

                //cerr << "pipes:\n";
                //for (auto x: pipes)
                    //db2(x.fr, x.sc);

                task.pb(Task(k, fd, pipes[0].sc, pipes[k].fr)); 
                int tid = task.size() - 1;
             
                for (int i = 0; i < k; i++) {
                    int childPid = fork(); 
                    if (childPid == -1) printError("fork in for");
                    if (childPid == 0) {
                        //db(getpid());

                        dup2(pipes[i].fr, STDIN_FILENO);
                        dup2(pipes[i + 1].sc, STDOUT_FILENO);
                        //for (auto pp: pipes) {
                            //if (close(pp.fr) == -1) printError("close in for 1");
                            //if (close(pp.sc) == -1) printError("close in for 2");
                        //}

                        char ** tmp = new char * [commands[i].size() + 1];
                        for (int j = 0; j < (int)commands[i].size(); j++)    {
                            char * ptr = new char[commands[i][j].size() + 1];
                            strcpy(ptr, commands[i][j].data());
                            tmp[j] = ptr;
                        }
                        tmp[commands[i].size()] = NULL;
                        execvp(commands[i][0].data(), tmp);
                        exit(0);
                    } 
                    taskId[childPid] = tid;
                }

                for (int i = 0; i < k + 1; i++) {
                    if (i != k && close(pipes[i].fr) == -1) printError("close in for 1");
                    if (i != 0 && close(pipes[i].sc) == -1) printError("close in for 2");
                }

                //shared_ptr < Buffer > bufLeft(new Buffer(CAP, fd, pipes[0].sc, epfd));
                bufLeft->writeFD = pipes[0].sc;
                bufLeft->inEpollW = 1;
                shared_ptr < Buffer > bufRight(new Buffer(CAP, pipes[k].fr, fd, epfd, tid));
                
                task.back().rightBuf = bufRight;  
                
                // add to left buffer 
                int posBack = -1;
                for (int i = 0; i < (int)s.size(); i++)
                    if (s[i] == '\n') {
                        posBack = i + 1;
                        break;
                    }
                assert(posBack != -1);
                //for (int i = posBack; i < (int)s.size(); i++)
                    //bufLeft->deq.pb(s[i]);

                fdMean[pipes[0].sc] = "left pipe write";
                fdMean[pipes[k].fr] = "right pipe read";

                q2[mp(pipes[0].sc, EPOLLOUT)] = bufLeft;
                q2[mp(fd, EPOLLIN)] = bufLeft;
                q2[mp(fd, EPOLLOUT)] = bufRight;
                q2[mp(pipes[k].fr, EPOLLIN)] = bufRight;
                db2(pipes[k].fr, pipes[0].sc);
                addEpoll(epfd, pipes[0].sc, EPOLLOUT);
                addEpoll(epfd, pipes[k].fr, EPOLLIN);
                
                modEpoll(epfd, fd, EPOLLOUT, EPOLLOUT); 
            }
        }
        else {
            for (int i = 0; i <= 2; i++) {
                if (((mask >> i) & 1)) {
                    int mmask = (1 << i);
                    if (q2.count(mp(fd, mmask)) == 1) {
                        shared_ptr < Buffer > buf = q2[mp(fd, mmask)]; 
                        if (mmask & EPOLLIN) buf->bufRead();  
                        if (mmask & EPOLLOUT) buf->bufWrite();

                        if (buf->tid != -1 && task[buf->tid].cnt == 0) {
                            if (checkEnd(buf->tid))
                                finishTask(buf->tid);
                        }
                    }
                }
            }
        }
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


