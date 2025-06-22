//BASCIALL till now the server only support simple hashtable queries. THE GOAL IS TO EXTEND IT TO 
// RANGE/RANK QUERIES.
//then we would have
// A set of name/score pairs
// Efficient support for ranked/range access by score → AVL Tree
// Efficient lookup by name → Hash Table

// we now have 2 types of queries, one that can be handledd with simple hashtable and 
// one where we need to have a sorted set, these types of queries are rank or range queries
// for this, now we have 3 values, keys(str), value(str), score(double). hash table still
// works with key value pair, but the score is used in avl tree to handle range, rank queries

//so the idea is if the user if want to use only the hash table he can do GET, SET , DEL ,KEYS Commands.
// but if we need to also use the sorted set queries he can do commands like ZADD, etc
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
#include "avl.h"
#include "range.h"
#include "helper.h"

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
// SERIALIZATION/////////////////////////////////////////////////////
//   nil       int64           str                   array
// ┌─────┐   ┌─────┬─────┐   ┌─────┬─────┬─────┐   ┌─────┬─────┬─────┐
// │ tag │   │ tag │ int │   │ tag │ len │ ... │   │ tag │ len │ ... │
// └─────┘   └─────┴─────┘   └─────┴─────┴─────┘   └─────┴─────┴─────┘
//    1B        1B    8B        1B    4B   ...        1B    4B   ...
// Integers and lengths are encoded in little-endian, which is just memcpying values on all relevant platforms.
// error code for TAG_ERR
enum {
    ERR_UNKNOWN = 1,    // unknown command
    ERR_TOO_BIG = 2,    // response too big
    ERR_BAD_ARGUMENT = 3,
    ERR_FUCK_YOU = 9,
    ERR_BAD_TYPE = 4,
};

// data types of serialized data
enum {
    TAG_NIL = 0,    // nil
    TAG_ERR = 1,    // error code + msg
    TAG_STR = 2,    // string
    TAG_INT = 3,    // int64
    TAG_DBL = 4,    // double
    TAG_ARR = 5,    // array
};
typedef vector<uint8_t> Buffer;
// help functions for the serialization
static void buf_append_u8(Buffer &buf, uint8_t data) {
    buf.push_back(data);
}
static void buf_append_u32(Buffer &buf, uint32_t data) {
    buf_append(buf, (const uint8_t *)&data, 4);
}
static void buf_append_i64(Buffer &buf, int64_t data) {
    buf_append(buf, (const uint8_t *)&data, 8);
}
static void buf_append_dbl(Buffer &buf, double data) {
    buf_append(buf, (const uint8_t *)&data, 8);
}


// append serialized data types to the back
static void out_nil(Buffer &out) {
    buf_append_u8(out, TAG_NIL);
}
static void out_str(Buffer &out, const char *s, size_t size) {
    buf_append_u8(out, TAG_STR);
    buf_append_u32(out, (uint32_t)size);
    buf_append(out, (const uint8_t *)s, size);
}
static void out_int(Buffer &out, int64_t val) {
    buf_append_u8(out, TAG_INT);
    buf_append_i64(out, val);
}
static void out_dbl(Buffer &out, double val) {
    buf_append_u8(out, TAG_DBL);
    buf_append_dbl(out, val);
}
static void out_err(Buffer &out, uint32_t code, const std::string &msg) {
    buf_append_u8(out, TAG_ERR);
    buf_append_u32(out, code);
    buf_append_u32(out, (uint32_t)msg.size());
    buf_append(out, (const uint8_t *)msg.data(), msg.size());
}

//DOUBT ?? /////
static void out_arr(Buffer &out, uint32_t n) {
    buf_append_u8(out, TAG_ARR);
    buf_append_u32(out, n);
}


//two types of values, string and sorted set. the design is hard to understand
enum {
    T_INIT  = 0,
    T_STR   = 1,    // string
    T_ZSET  = 2,    // sorted set
};

//common interface.
//strings use Entry.str
//sorted sets use Entry.zset
//members in the set are stored as ZNode(with score + name which is intrusive i.e contains node and avlnode)
//now fast access by name in set ZSet.hmap, ZSet.root fast rank/range queries 
struct Entry{
    HNode node;
    string key;
    //value
    uint32_t type = 0;
    //one of the following
    //can be optimized by using only one of the fileds
    string value;
    ZSet zset;

};


/////////////////////////////////////////////
// handle delete new according to the type need differnte function
static Entry *entry_new(uint32_t type) {
    Entry *ent = new Entry();
    ent->type = type;
    return ent;
}

static void entry_del(Entry *ent) {
    if (ent->type == T_ZSET) {
        zset_clear(&ent->zset);
    }
    delete ent;
}

//////////////////////////////////////////////////
// equality comparison for the top-level hashstable
static bool eq_f(HNode *node, HNode *key) {
    struct Entry *ent = container_of(node, struct Entry, node);
    struct LookupKey *keydata = container_of(key, struct LookupKey, node);
    return ent->key == keydata->key;
}




///A void* is a generic pointer — it can hold the address of any data type.
// But it has no type information, so you can't dereference it without a cast.
// use of callbacks, new stuff, basically a fuction is jut another piece of binary data in the memory, its reference
// can be passed as a pointer, then this function can be called by any other function, function calling a function.
static bool node_callback(HNode* node, void* arg){
    // no copyying
    Buffer& out = *(Buffer *)arg;
    string &s = container_of(node, Entry, node)->key;
    // gets the s.data pointer char to the first char of the string
    out_str(out, s.data(), s.size());
    return true;
}
//helper

// global states
// no matter the query type every shit key value pair is stored in here
static struct {
    HMAP db;    // top-level hashtable
} store;



struct LookupKey {
    struct HNode node;  // hashtable node
    string key;
};

// ZADD KEY SCORE NAME
static void do_add(vector<string>& cmd, Buffer &out){
    double score = 0;
    if(!str2dbl(cmd[2], score)){
        return out_err(out, ERR_BAD_ARGUMENT, "excepting a float");
    }
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.next = NULL;
    key.node.hcode = str_hash((uint8_t*)key.key.data(), key.key.size());
    //check if already present
    HNode *node = hm_lookup(&store.db, &key.node, &eq_f);
    Entry* ent = NULL;
    if(node){
        ent = container_of(node, Entry, node);
        if(ent->type != T_ZSET){
            return out_err(out, ERR_BAD_ARGUMENT, "expecting set, key already present in HashMap");
        }
        //if a t_zset then update the value
    }else{
        //insert into hashmap global one
       ent = entry_new(T_ZSET);
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        hm_insert(&store.db, &ent->node);
    }
    const string &name = cmd[3];
    bool added = zset_insert(&ent->zset, name.data(), name.size(), score );
    return out_int(out, (int64_t)added);

    
}

static void do_keys(Buffer& out){
    out_arr(out, (uint32_t)hm_size(&store.db));
    hm_foreach(&store.db, &node_callback, (void*)&out);

}

static size_t arr_res_start(Buffer& out){
    out.push_back(TAG_ARR);
    buf_append_u32(out, 0); //later filled by end, size not known
    return out.size() - 4;
}
static void arr_res_end(Buffer& out, size_t ctx , uint32_t siz){
    assert(out[ctx - 1] == TAG_ARR);
    memcpy(&out[ctx], &siz, 4);
}
static const ZSet k_empty_zset;
static ZSet *expect_zset(std::string &s) {
    LookupKey key;
    key.key.swap(s);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
    HNode *hnode = hm_lookup(&store.db, &key.node, &eq_f);
    if (!hnode) {   // a non-existent key is treated as an empty zset
        return (ZSet *)&k_empty_zset;
    }
    Entry *ent = container_of(hnode, Entry, node);
    return ent->type == T_ZSET ? &ent->zset : NULL;
}

static void do_zquery(vector<string>& cmd, Buffer& out){
    //parse the request.
    //ZQUERY key score name offset limit returns:
    //A sorted list of at most limit (score, name) pairs starting from the offset-th element at or after (score, name) in the tree's sorted order.
    

    //seek/search the given node
    // parse args
    double score = 0;
    if (!str2dbl(cmd[2], score)) {
        return out_err(out, ERR_BAD_ARGUMENT, "expect fp number");
    }
    const std::string &name = cmd[3];
    int64_t offset = 0, limit = 0;
    if (!str2int(cmd[4], offset) || !str2int(cmd[5], limit)) {
        return out_err(out, ERR_BAD_ARGUMENT, "expect int");
    }

    // get the zset
    ZSet *zset = expect_zset(cmd[1]);
    if (!zset) {
        return out_err(out, ERR_BAD_TYPE, "expect zset");
    }

    // seek to the key
    if (limit <= 0) {
        return out_arr(out, 0);
    }
    ZNode *znode = zset_search(zset, score, name.data(), name.size());
    znode = znode_offset(znode, offset);

    // output
    size_t ctx = arr_res_start(out);
    int64_t n = 0;
    while (znode && n < limit) {
        out_str(out, znode->name, znode->len);
        out_dbl(out, znode->score);
        znode = znode_offset(znode, +1);
        n += 2;
    }
    arr_res_end(out, ctx, (uint32_t)n);
    //go to offset

    

    //iterate limit elements
    //out them to an array, array length not known so similar to response start end, make array start end function
    size_t ctx = arr_res_start(out);
    
    arr_res_end(out, ctx, (uint32_t)n);

}

/////////////////////////////////////////////////

static void do_request(vector<string>& cmd, Buffer& out){
    if(cmd.size() == 2 && cmd[0] == "GET"){
        LookupKey key;
        key.key.swap(cmd[1]);
        key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
        // hashtable lookup
        HNode *node = hm_lookup(&store.db, &key.node, &eq_f);
        if (!node) {
            out_nil(out);
            return;
        }
        // copy the value
        Entry* e = container_of(node, Entry, node);
        // assert(val.size() <= k_max_msg);
        if(e->type != T_STR){
            return out_err(out, ERR_BAD_TYPE, "not a string value");
        }
        return out_str(out, e->value.data(), e->value.size());

    }else if (cmd.size() == 3 && cmd[0] == "SET") {
        LookupKey key;
        key.key.swap(cmd[1]);
        key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
        // hashtable lookup
        HNode *node = hm_lookup(&store.db, &key.node, &eq_f);
        if (!node) {
            // not found create a key value pair.
            Entry *ent = entry_new(T_STR);

            /////////STUPID BUG WARNING. SWAP ACTUALLY 
            // Important: swap() doesn't copy or assign characters — it just swaps the internal pointers 
            // (and size/capacity metadata) between the two strings.

            // ent->key.swap(cmd[1]);
            // This moves cmd[1] into key.key. Now cmd[1] is empty
            // so using this doest work but using key.key works.
            // wasted 3 FUCKING HOURS DEBUGGING THIS SHIT. WHY LORD WHY. I THOUGH SWAP just swaps?! i just heard it is fast so i used it without actually
            // knowing how it swaps, SO HALF KNOWING IS NO KNOWING
            
            
            ent->key.swap(key.key);
            ent->value.swap(cmd[2]);
            ent->node.hcode = key.node.hcode;
            hm_insert(&store.db, &ent->node);
            out_nil(out);
        }else{
            // found, update the value
        Entry *ent = container_of(node, Entry, node);
        if (ent->type != T_STR) {
            return out_err(out, ERR_BAD_TYPE, "a non-string value exists");
        }
        ent->value.swap(cmd[2]);
        out_nil(out);

        }
    } else if (cmd.size() == 2 && cmd[0] == "DEL") {
         // a dummy `Entry` just for the lookup
        LookupKey key;
        key.key.swap(cmd[1]);
        key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());
        // hashtable delete
        HNode *node = hm_delete(&store.db, &key.node, &eq_f);
        if (node) { // deallocate the pair
            entry_del(container_of(node, Entry, node));
        }
        return out_int(out, node ? 1 : 0);
    } else if(cmd.size() == 1 && cmd[0] == "KEYS"){
        do_keys(out);
    
    }else if(cmd.size() == 4 && cmd[0] == "ZADD"){
        do_add(cmd, out);
    }
    else {
        out_err(out, ERR_UNKNOWN, "unknown command");      // unrecognized command
    }
}

//todo 3:
//append the response to the output buffer
// static void make_response(const Buffer& res, vector<uint8_t> &out){
//     uint32_t response_len = 4 + (uint32_t)res.data.size();
//     buf_append(out, (const uint8_t* )&response_len, 4);
//     buf_append(out, (const uint8_t* )&res.status, 4);
//     buf_append(out, res.data.data(), res.data.size());
// }
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

static
void start_response(Buffer& out, size_t *header){
    *header = out.size();
    buf_append_u32(out, 0);
}
static size_t response_size(Buffer &out, size_t header) {
    return out.size() - header - 4;
}
void end_response(Buffer& out, size_t header){
    size_t len = out.size() - header - 4;
    if(len > k_max_msg){
        out.resize(header + 4);
        out_err(out, ERR_TOO_BIG, "response is too big");
        len = response_size(out, header);
    }
    uint32_t msgsiz = (uint32_t)len;
    memcpy(&out[header], &msgsiz, 4);
}

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
    // Response res;
    size_t header_pos = 0;
    start_response(conn->outgoing, &header_pos);
    do_request(commands, conn->outgoing);
    end_response(conn->outgoing, header_pos);
    // make_response(res, conn->outgoing);
    

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






