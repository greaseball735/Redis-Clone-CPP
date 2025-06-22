#pragma once

#include <stdint.h>
#include <stddef.h>
#include <math.h> 
#include <string>

// intrusive data structure
// this is the coolest sht 
// BEHOLD THE FAMOUS POINTER WIZARD MAGIC FROM THE LINUX KERNEL
// calculates the address of the containing structure given a pointer to one of its members.
// char cast to interpret it as bytes now pointer arithematics works with bytes because char is one byte
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))


// FNV hash
static uint64_t str_hash(const uint8_t *data, size_t len){
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

static bool str2dbl(const std::string &s, double &out) {
    char *endp = NULL;
    out = strtod(s.c_str(), &endp);
    return endp == s.c_str() + s.size() && !isnan(out);
}

static bool str2int(const std::string &s, int64_t &out) {
    char *endp = NULL;
    out = strtoll(s.c_str(), &endp, 10);
    return endp == s.c_str() + s.size();
}