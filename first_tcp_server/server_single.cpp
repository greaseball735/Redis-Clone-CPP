#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <cassert>

static int32_t read_full(int fd, char* buf, size_t n){
    while(n > 0){
        ssize_t rv = read(fd, buf , n);
        if(rv <= 0 ){
            return -1;
        }
        assert((size_t)rv <= n);
        n = n - (size_t)rv;
        buf = buf + rv;

    }
    return 0;
}
static int32_t write_full(int fd , char* buf , size_t n){
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}


//static int32_t one_request()

static void msg(const char *msg){
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg){
    int err = errno;
    fprintf(stderr , "[%d] %s\n", err , msg);


}


static void do_something(int connfd){
    char read_buf[64] = {};
    ssize_t n = read(connfd, read_buf, sizeof(read_buf) - 1);
    if(n < 0){
        msg("read() error");
        return;
    }
    fprintf(stderr , "client has said %s\n", read_buf);
    char wbuf[] = "suffering";
    write(connfd, wbuf , strlen(wbuf));
}



int main(){

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int val = 1;
    
    //When a TCP server shuts down, its sockets don’t instantly disappear from the OS. T
    //he kernel keeps them around for a little while in a state called TIME_WAIT
    //So when you restart your server immediately, and try to bind to the same IP + port, you get this error:

    
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    //bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = htonl(0);
    int rv = bind(fd, (const struct sockaddr *)&addr , sizeof(addr));
    if(rv){
        die("bind()");
    }
    //listedn

    rv = listen(fd, SOMAXCONN);
    if(rv){
        die("listen()");
    }
    while(1){
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(fd , (struct sockaddr *)&client_addr, &addrlen);
        if(connfd < 0){
            continue;
        }
        do_something(connfd);
        close(connfd);


    }
    return 0;
}