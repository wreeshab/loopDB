#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

using namespace std;

void die(const char *msg){
    int err = errno;
    fprintf(stderr , "[%d] %s\n", err, msg);
    abort();
}
static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void do_something(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        msg("read() error");
        return;
    }
    fprintf(stderr, "client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}


int main(){

    int sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockFd < 0){
        die("socket() failed");
    }

    int val = 1;
    if(setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0){
        die("setsockopt() failed");
    }   

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr =  htonl(0); // wildcard address

    int rv = bind(sockFd , (struct sockaddr*)&addr, sizeof(addr));
    if(rv < 0){
        die("bind() failed");
    }

    rv = listen ( sockFd , 10);
    if(rv< 0){
        die("listen() failed");
    }
    while(1){
        struct sockaddr_in clientAddr = {};
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        int connFd = accept(sockFd , (struct sockaddr*) &clientAddr, &clientAddrLen);
        if(connFd < 0){
            continue;
        }

        do_something(connFd);
        close(connFd);
    }
}