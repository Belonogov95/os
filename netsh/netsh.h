
#ifndef NETSH_H
#define NETSH_H
    
void addEpoll(int epfd, int sfd, int mask);

void delEpoll(int epfd, int sfd);

void modEpoll(int epfd, int sfd, int what, int val);
//// 'void'

#endif

