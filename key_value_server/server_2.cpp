//goal : use the custome hash logic in place of std: unordered_map 
//todo:learn, understand and implement the custome hash table
//todo:use that in place of map.


// stdlib
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
#include <stddef.h>
// C++
#include <map>
#include <vector>
#include <string>
#include <iostream>
///////////////////////
// define custome status codes
#define RES_NF 2
#define RES_ERR 1
#define RES_OK 0



////////////////////////////////////
#include "hash.h"

//////////////////////////////////


using namespace std;

const size_t k_max_msg = 32 << 20;  // likely larger than the kernel buffer
const size_t k_max_args = 200 * 1000;

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

static void msg_errno(const char *msg) {
    fprintf(stderr, "[errno:%d] %s\n", errno, msg);
}

static void die(const char *msg) {
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}
// append to the back
//8 bits 1 byte per vector entry
static void
buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
    //insert from data to data + len
}

// remove from the front
static void buf_consume(std::vector<uint8_t> &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}
//todo 1
// curr is a reference (&) to a pointer (*) to const uint8_t
// i.e., you cannot modify the data that curr points to, but you can change 
// what address curr holds
static bool read_32(const uint8_t *&curr, const uint8_t *&end , uint32_t& x){
    if(curr + 4 > end)return false;
    memcpy(&x , curr, 4);
    curr = curr + 4;
    return true;
}

static bool
read_str(const uint8_t *&cur, const uint8_t *end, size_t n, string &out) {
    if (cur + n > end) {
        return false;
    }
    out.assign(cur, cur + n);
    cur = cur +  n;
    return true;
}


// +------+-----+------+-----+------+-----+-----+------+
// | nstr | len | str1 | len | str2 | ... | len | strn |
// +------+-----+------+-----+------+-----+-----+------+

static int32_t parse_request(const uint8_t* data, size_t size, vector<string>& out){
    const uint8_t* end = data + size;
    uint32_t number_of_strings = 0;
    if(!read_32(data, end, number_of_strings)){
        return -1;
    }
     if (number_of_strings > k_max_args) {
        return -1;  // safety limit
    }
    //use a safety limimt. k_max_args
    while(out.size() < number_of_strings){
        uint32_t l = 0;
        if(!read_32(data, end, l)){
            return -1;
        }
        out.push_back(string());
        if(!read_str(data ,end, l, out.back())){
            return -1;
        }
    }
    if(data != end){
        return -1;
    }
    return 0;

}
//todo 2:
// process the command and generate response
struct Response{
    uint32_t status = RES_OK;
    vector<uint8_t> data;
};
/////////////////////////////////////////////////////////////////////////
// map<string, string> store;
static struct{
    HMAP db;
}store;
// use of intrusive data structure
// structure in data 
// embed the HNode lpayload

// NEEED NEEEEEED
// a hash function (!obviously)
// a equality logic.
// data with structure in it(here HNode embeeded in it)

// in memory, an Entry object looks like:
// [ key ][ val ][ hnode ]
struct Entry{
    string key;
    string val;
    // for intrusive hashtable
    HNode hnode;
    // Now, each Entry is the payload, and HMap only knows about HNode 
    // pointers, which are part of the entry
};
// this is the coolest sht 
// BEHOLD THE FAMOUS POINTER WIZARD MAGIC FROM THE LINUX KERNEL
// calculates the address of the containing structure given a pointer to one of its members.
// char cast to interpret it as bytes now pointer arithematics works with bytes because char is one byte
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

// he have HNode* but we need to compare the keys 
// hnode is always at the same offset from the start of Entry, we can use the macro:
bool EQ(HNode* first , HNode* second){
    Entry* f = container_of(first, Entry, hnode);
    Entry* s = container_of(second, Entry, hnode);
    return f->key == s->key;
}

// FNV hash
static uint64_t str_hash(const uint8_t *data, size_t len){
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}


/////////////////////////////////////////////////

static void do_request(vector<string>& cmd, Response& out){
    if(cmd.size() == 2 && cmd[0] == "get"){
        Entry key;
        key.key.swap(cmd[1]);
        key.hnode.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
        // hashtable lookup
        HNode *node = hm_lookup(&store.db, &key.hnode, &EQ);
        if (!node) {
            out.status = RES_NF;
            return;
        }
        // copy the value
        const std::string &val = container_of(node, Entry, hnode)->val;
        assert(val.size() <= k_max_msg);
        out.data.assign(val.begin(), val.end());

    }else if (cmd.size() == 3 && cmd[0] == "set") {
        Entry key;
        key.key.swap(cmd[1]);
        key.hnode.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
        // hashtable lookup
        HNode *node = hm_lookup(&store.db, &key.hnode, &EQ);
        if (!node) {
            // not found create a key value pair.
            Entry *ent = new Entry();
            ent->key.swap(cmd[1]);
            ent->val.swap(cmd[2]);
            ent->hnode.hcode = key.hnode.hcode;
            hm_insert(&store.db, &ent->hnode);
        }else{
            container_of(node, Entry, hnode)->val = cmd[2];

        }
    } else if (cmd.size() == 2 && cmd[0] == "del") {
         // a dummy `Entry` just for the lookup
        Entry key;
        key.key.swap(cmd[1]);
        key.hnode.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
        // hashtable delete
        HNode *node = hm_delete(&store.db, &key.hnode, &EQ);
        if (node) { // deallocate the pair
            delete container_of(node, Entry, hnode);
        }
    } else {
        out.status = RES_ERR;       // unrecognized command
    }
}

//todo 3:
//append the response to the output buffer
static void make_response(const Response& res, vector<uint8_t> &out){
    uint32_t response_len = 4 + (uint32_t)res.data.size();
    buf_append(out, (const uint8_t* )&response_len, 4);
    buf_append(out, (const uint8_t* )&res.status, 4);
    buf_append(out, res.data.data(), res.data.size());
}
//  there is room for improvement: the response data is copied twice, 
// first from the key value to Response::data, then from Response::data 
// to Conn::outgoing. Exercise: Optimize the code so that the response 
// data goes directly to Conn::outgoing.


//make non blocking modify flag
static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl error");
    }
}

//kernel buffer

//connection struct for the application
struct Conn {
    int fd = -1;
    // application's intention, for the event loop
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    // buffered input and output
    // Since reads are now non-blocking, we cannot just wait for n bytes while parsing the protocol;
    // the read_full() function is now irrelevant
  
    std::vector<uint8_t> incoming;  // data to be parsed by the application
    std::vector<uint8_t> outgoing;  // responses generated by the application
};


// application callback when the listening socket is ready
//callback means give control to the application
static Conn *handle_accept(int fd) {
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
        //create a connection fd, the original socket is fd, create a new connection fd
    //fd argument in the listening socket which is the first one in the list passed to poll
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0){
        msg_errno("accept() error");
        return NULL;
    }
    //to be consistent use of unit32 vs int 
    uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "new client from %u.%u.%u.%u:%u\n",
        ip & 255, (ip >> 8) & 255, (ip >> 16) & 255, ip >> 24,
        ntohs(client_addr.sin_port)
    );

    // set the new connection fd to nonblocking mode
    fd_set_nb(connfd);

    // create a `struct Conn`
    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true;
    return conn;
}

// process 1 request if there is enough data
//Returns true if a request was processed, false otherwis
//Network data can arrive in chunks. try_one_request allows the
// server to process requests only when a full request has arrived.

//non blocking read 
//add data to incoming
//try to parse accumalated, not enough data, do nothing
//process
//remove form incoming


//similarly write data is received from the application.
//we cant write at will only can write if the socket is free , can take multiple iterations.
static bool try_one_request(Conn *conn) {
    // try to parse the protocol: message header
    if (conn->incoming.size() < 4) {
        return false;   // want read
    }

    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);

       //validate messsage length
    if (len > k_max_msg) {
        msg("too long");
        conn->want_close = true;
        return false;   // want close
    }

    // message body
    ///Returning false signals that there isn’t enough data to continue processing]
    //(e.g., parsing a message).
    //wait more
    if (4 + len > conn->incoming.size()) {
        return false;   // want read
    }
      //incoming is a unit8 vector, request is pointer to 4th element actual start of data
    const uint8_t *request = &conn->incoming[4];
    vector<string> commands;
    uint32_t p = parse_request(request, (size_t)len, commands);
    if(p < 0){
        msg("bad request");
        conn->want_close = true;
        return false;   // want close
        // return false;
    }
    for(string s : commands){
        cout << s << endl;
    }
    Response res;
    do_request(commands, res);
    make_response(res, conn->outgoing);
    

    ///////////---------------------------------LOGIC LOGIC LOGIC------------------
    // got one request, do some application logic
    //%.*s prints a string, but you can specify the maximum number of characters to print
    printf("client says: len:%d data:%.*s\n",
        len, len < 100 ? len : 100, request);
        //----------------------------------------------------------------------------

    // // generate the response (echo)
    // //respond with the same mesage for now
    // buf_append(conn->outgoing, (const uint8_t *)&len, 4);
    // buf_append(conn->outgoing, request, len);

    // // application logic done! remove the request message.
    buf_consume(conn->incoming, 4 + len);
    // Q: Why not just empty the buffer? See the explanation of "pipelining".
    return true;        // success
}

// application callback when the socket is writable
static void handle_write(Conn *conn) {
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());

    ///KERNEL BUFFER IS FULL CANT WRITE RN
    ///WILL DO IN NEXT ITERATION
    
    if (rv < 0 && errno == EAGAIN) {
        return; // actually not ready
    }
    if (rv < 0) {
        msg_errno("write() error");
        conn->want_close = true;    // error handling
        return;
    }

    // remove written data from `outgoing`
    buf_consume(conn->outgoing, (size_t)rv);

    // update the readiness intention
    if (conn->outgoing.size() == 0) {   // all data written
        conn->want_read = true;
        conn->want_write = false;      //change intention application does this 
    } // else: want write
}

// application callback when the socket is readable
static void handle_read(Conn *conn) {
    // read some data
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv < 0 && errno == EAGAIN) {
        return; // actually not ready
    }
    // handle IO error
    if (rv < 0) {
        msg_errno("read() error");
        conn->want_close = true;
        return; // want close
    }
    // handle EOF
    if (rv == 0) {
        if (conn->incoming.size() == 0) {
            msg("client closed");
        } else {
            msg("unexpected EOF");
        }
        conn->want_close = true;
        return; // want close
    }
    // got some new data
    buf_append(conn->incoming, buf, (size_t)rv);

    // parse requests and generate responses
    while (try_one_request(conn)) {}
    // Q: Why calling this in a loop? See the explanation of "pipelining".

    // update the readiness intention
    if (conn->outgoing.size() > 0) {    // has a response
        conn->want_read = false;
        conn->want_write = true;
        // The socket is likely ready to write in a request-response protocol,
        // try to write it without waiting for the next iteration.
        return handle_write(conn);
    }   // else: want read
}

int main() {
    // the listening socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }
    int val = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0);    // wildcard address 0.0.0.0
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // set the listen fd to nonblocking mode
    fd_set_nb(fd);

    // listen
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    // a map of all client connections, keyed by fd
    std::vector<Conn *> fd2conn;
    // the event loop
    //------------------------------------------------------
    std::vector<struct pollfd> poll_args;
    while (true) {
        // prepare the arguments of the poll()
        poll_args.clear();
        // put the listening sockets in the first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);
        // the rest are connection sockets
        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }
            // always poll() for error
            struct pollfd pfd = {conn->fd, POLLERR, 0};
            // poll() flags from the application's intent
            if (conn->want_read) {
                pfd.events |= POLLIN;
            }
            if (conn->want_write) {
                pfd.events |= POLLOUT;
            }
            poll_args.push_back(pfd);
        }

        // wait for readiness
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
        if (rv < 0 && errno == EINTR) {
            continue;   // not an error
        }
        if (rv < 0) {
            die("poll");
        }

        // handle the listening socket
        if (poll_args[0].revents) {
            if (Conn *conn = handle_accept(fd)) {
                // put it into the map
                 //can simply append since its a vector
                if (fd2conn.size() <= (size_t)conn->fd) {
                    fd2conn.resize(conn->fd + 1);
                }
                assert(!fd2conn[conn->fd]);
                fd2conn[conn->fd] = conn;
            }
        }

        // handle connection sockets
          //poll changed the state of these poll_args, communicate throught POLLIN, POLLOUT etc
        for (size_t i = 1; i < poll_args.size(); ++i) { // note: skip the 1st
            uint32_t ready = poll_args[i].revents;
            if (ready == 0) {
                continue;
            }

            Conn *conn = fd2conn[poll_args[i].fd];
            if (ready & POLLIN) {
                assert(conn->want_read);
                handle_read(conn);  // application logic
            }
            if (ready & POLLOUT) {
                assert(conn->want_write);
                handle_write(conn); // application logic
            }

            // close the socket from socket error or application logic
            if ((ready & POLLERR) || conn->want_close) {
                (void)close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }   // for each connection sockets
    }   // the event loop
    return 0;
}






