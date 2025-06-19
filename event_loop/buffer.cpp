#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
// system
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
// C++
#include <vector>
#define int uint8_t


class Buffer{
    private:
    size_t size;
    int* data_start;
    int* data_end;
    int* buff_start;
    int* buff_end;

    void alloc(){

    }
    public:
    Buffer(size_t sz) : size(sz) {
        data_start = new int[size];
        buff_start = data_start;
    }
    ~Buffer() {
        delete[] data_start;
    }

    int* data(){
        return buff_start;
    }
    size_t size(){
        return size;
    }
    int& operator[](size_t index) {
        // optionally add bounds check
        return buff_start[index];
    }
    void push_back(const uint8_t *data, size_t len){

    }
    void erase(size_t n){

    }




};