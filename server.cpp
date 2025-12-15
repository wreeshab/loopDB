#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <assert.h>
#include <iostream>

using namespace std;

void die(const char *msg)
{
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}
static void msg(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
}

const size_t k_max_msg = 4096;

// returns 0 on success, -1 on error
static int32_t read_full(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0)
        {
            return -1; // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

// returns 0 on success, -1 on error
static int32_t write_all(int fd, const char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0)
        {
            return -1; // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int one_request(int connfd)
{
    // assume 4 byte header that indicates message length

    char readBuffer[4 + k_max_msg];
    errno = 0;
    // reading the 4 byte header
    int err = read_full(connfd, readBuffer, 4);
    if (err < 0)
    {
        if(errno == 0){
            msg("EOF detected, closing connection");
            return -1;
        }
        msg("read_full() failed");
        return -1;
    }

    uint32_t msgLen = 0;
    memcpy(&msgLen, readBuffer, 4);
    if (msgLen > k_max_msg)
    {
        msg("message too long");
        return -1;
    }

    // reading the message body
    err = read_full(connfd, readBuffer + 4, msgLen);
    if (err < 0)
    {
        msg("read_full() failed");
        return -1;
    }

    // do something with the message
    fprintf(stderr, "client says : %.*s\n", msgLen, readBuffer + 4);

    const char reply[] = "world";
    char wbuf[4 + sizeof(reply)];
    int wLen = (uint32_t)strlen(reply);
    memcpy(wbuf, &wLen, 4);
    memcpy(&wbuf[4], reply, wLen);
    return write_all(connfd, wbuf, 4 + wLen);
}



int main()
{

    int sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0)
    {
        die("socket() failed");
    }

    int val = 1;
    if (setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
    {
        die("setsockopt() failed");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(0); // wildcard address

    int rv = bind(sockFd, (struct sockaddr *)&addr, sizeof(addr));
    if (rv < 0)
    {
        die("bind() failed");
    }

    rv = listen(sockFd, 10);
    if (rv < 0)
    {
        die("listen() failed");
    }
    while (1)
    {
        struct sockaddr_in clientAddr = {};
        socklen_t clientAddrLen = sizeof(clientAddr);

        int connFd = accept(sockFd, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (connFd < 0)
        {
            continue;
        }

        while (true)
        {
            int32_t err = one_request(connFd);
            if (err)
            {
                break;
            }
        }
        close(connFd);
    }
}