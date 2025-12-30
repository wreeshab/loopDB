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
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <map>

#include "util.hpp"
#include "hashmap.hpp"
#include "avl_tree.hpp"

using namespace std;





static void fd_set_nonblocking(int fd)
{
    // file descriptor control.
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
    {
        die("fcntl(F_GETFL) failed");
    }
    // add non blocking flag.
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        die("fcntl(F_SETFL) failed");
    }
}

static Conn *handle_accept_new_client(int sockFd)
{
    struct sockaddr_in client_addr = {};
    socklen_t client_len = sizeof(client_addr);
    int connfd = accept(sockFd, (struct sockaddr *)&client_addr, &client_len);
    if (connfd < 0)
    {
        die("accept() failed");
        return nullptr;
    }
    uint32_t client_ip = client_addr.sin_addr.s_addr;
    uint16_t client_port = client_addr.sin_port;

    fprintf(stderr, "Accepted new connection from %u.%u.%u.%u:%u , connfd=%d\n",
            (client_ip >> 0) & 0xFF,
            (client_ip >> 8) & 0xFF,
            (client_ip >> 16) & 0xFF,
            (client_ip >> 24) & 0xFF,
            ntohs(client_port),
            connfd);

    // set connfd to non-blocking mode.
    fd_set_nonblocking(connfd);

    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true; // initially we want to read from the client.
    return conn;
}

static void append_to_buffer(vector<uint8_t> &buffer, const uint8_t *data, size_t len)
{
    buffer.insert(buffer.end(), data, data + len);
}

// helper functions for serializing data into buffer.
static void append_to_buffer_u8(vector<uint8_t> & buffer, uint8_t data){
    append_to_buffer(buffer, (const uint8_t*)&data, 1);
}

static void append_to_buffer_u32(vector<uint8_t> & buffer, uint32_t data){
    append_to_buffer(buffer, (const uint8_t*)&data, 4);
}

static void append_to_buffer_u64(vector<uint8_t> &buffer, uint64_t data){
    append_to_buffer(buffer, (const uint8_t*)& data, 8);
}

static void append_to_buffer_dbl(vector<uint8_t> &buffer, double data){
    append_to_buffer(buffer, (const uint8_t*)&data, 4);
}

// helper function for serializing the response.
static void out_nil(vector<uint8_t> &buffer){
    append_to_buffer_u8(buffer, TAG_NIL);
}

static void out_str(vector<uint8_t> &buffer , const char* s , size_t size){
    append_to_buffer_u8(buffer, TAG_STR);
    append_to_buffer_u32(buffer, (uint32_t)size);
    append_to_buffer(buffer, (const uint8_t*)s, size);
}

static void out_int(vector<uint8_t> &buffer, int64_t val){
    append_to_buffer_u8(buffer, TAG_INT);
    append_to_buffer_u64(buffer, val);
}

static void out_err(vector<uint8_t> &buffer, uint32_t code , const char* msg , size_t msg_len){
    append_to_buffer_u8(buffer, TAG_ERR);
    append_to_buffer_u32(buffer, code);
    append_to_buffer_u32(buffer, (uint32_t) msg_len);
    append_to_buffer(buffer, (const uint8_t*) msg , msg_len);
}

static void out_dbl(vector<uint8_t> &buffer, double val){
    append_to_buffer_u8(buffer, TAG_DBL);
    append_to_buffer_dbl(buffer, val);
}

// just inits the array.
static void out_array_header(vector<uint8_t> &buffer, uint32_t count){
    append_to_buffer_u8(buffer, TAG_ARR);
    append_to_buffer_u32(buffer, count);
}


// TODO: optimise this later (o(n) right now), replace this with a ring buffer or use begin and end pointers/offsets.
static void buffer_consume(vector<uint8_t> &buffer, size_t n){
    buffer.erase(buffer.begin() , buffer.begin() + n);
}

static int32_t read_u32(const uint8_t * &curr, const uint8_t* end , uint32_t &out){
    if(curr + 4 > end){
        return -1;
    }
    memcpy (&out, curr ,4);
    curr+=4;
    return 0;
}

static int32_t read_str(const uint8_t* &curr , const uint8_t*end , size_t n ,string &out){
    if(curr  + n> end){
        return -1;
    }
    out.assign(curr, curr+ n );
    curr += n ;
    return 0;
}

static int32_t parse_request(const uint8_t* data , size_t size, vector<string> & out)
{
    const uint8_t *end = data + size;
    uint32_t nstr  = 0; 
    if(read_u32(data , end , nstr) < 0){
        return -1;
    }
    if(nstr> k_max_args){
        return -1;
    }

    while(out.size() < nstr){
        uint32_t len = 0; // length of one request in the nstr requests.

        if(read_u32(data , end , len)){
            return -1;
        }

        out.push_back(string()); // push one empty string.
        if(read_str(data , end , len , out.back()) < 0){
            return -1;
        }
    }
    if(data != end){
        return -1;
    }
    return 0;
}


// static HashMap g_db;
static struct {
    HashMap hmap;
    AVLTree avl;
} g_db;

static bool entry_eq(HashNode *lhs, HashNode *rhs) {
    struct Entry *le = container_of(lhs,  Entry, node);
    struct Entry *re = container_of(rhs,  Entry, node);
    return le->key == re->key;
}

// FNV hash
static uint64_t str_hash(const uint8_t *data, size_t len) {
    uint32_t h = 0x811C9DC5;
    for (size_t i = 0; i < len; i++) {
        h = (h + data[i]) * 0x01000193;
    }
    return h;
}

static bool cb_keys(HashNode* node, void* arg) {
    // Pointer-style (valid, but more verbose)
    // vector<uint8_t> *out = (vector<uint8_t> *)arg;

    // Reference-style (preferred)
    vector<uint8_t> &out = *(vector<uint8_t> *)arg;

    const std::string &key = container_of(node, Entry, node)->key;
    out_str(out, key.data(), key.size());
    return true;   // continue iteration
}

static void do_keys (vector<string> &cmd,  vector<uint8_t> &out){
    // init the array header.
    out_array_header(out, hmap_size(&g_db.hmap));
    hmap_for_each_key(&g_db.hmap, &cb_keys , (void *) &out);
}

static void do_get(vector<string> &cmd,  vector<uint8_t> &out) {
    Entry key;
    key.key.swap(cmd[1]);
    key.node.hCode = str_hash(
        (uint8_t *)key.key.data(), key.key.size());

    HashNode *node = hmap_lookup(&g_db.hmap, &key.node, entry_eq);
    if (!node) {
        return out_nil(out);
    }

    const string &val = container_of(node, Entry, node)->val;
    return out_str(out, val.data(), val.size());
}

static void do_set(vector<string> &cmd,      vector<uint8_t> &out) {
    Entry probe;
    probe.key.swap(cmd[1]);
    probe.node.hCode = str_hash(
        (uint8_t *)probe.key.data(), probe.key.size());

    HashNode *node = hmap_lookup(&g_db.hmap, &probe.node, entry_eq);
    if (node) {
        container_of(node, Entry, node)->val.swap(cmd[2]);
    } else {
        Entry *ent = new Entry();
        ent->key.swap(probe.key);
        ent->val.swap(cmd[2]);
        ent->node.hCode = probe.node.hCode;
        hmap_insert(&g_db.hmap, &ent->node);
    }

    return out_nil(out);
}


static void do_del(vector<string> &cmd,  vector<uint8_t> &out) {
    Entry probe;
    probe.key.swap(cmd[1]);
    probe.node.hCode = str_hash(
        (uint8_t *)probe.key.data(), probe.key.size());

    HashNode *node = hmap_delete(&g_db.hmap, &probe.node, entry_eq);
    if (node) {
        delete container_of(node, Entry, node);
    }

    return out_nil(out);
}

// AVL Tree command handlers
static void do_avl_keys(vector<string> &cmd, vector<uint8_t> &out) {
    (void)cmd; // unused parameter
    out_array_header(out, g_db.avl.size());
    g_db.avl.for_each([&out](const string& key, const string& val) {
        (void)val; // unused parameter
        out_str(out, key.data(), key.size());
    });
}

static void do_avl_get(vector<string> &cmd, vector<uint8_t> &out) {
    string* val = g_db.avl.lookup(cmd[1]);
    if (!val) {
        return out_nil(out);
    }
    return out_str(out, val->data(), val->size());
}

static void do_avl_set(vector<string> &cmd, vector<uint8_t> &out) {
    g_db.avl.insert(cmd[1], cmd[2]);
    return out_nil(out);
}

static void do_avl_del(vector<string> &cmd, vector<uint8_t> &out) {
    g_db.avl.remove(cmd[1]);
    return out_nil(out);
}


static void do_request(vector<string> &cmd,  vector<uint8_t> &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        return do_get(cmd, out);
    }
    else if (cmd.size() == 3 && cmd[0] == "set") {
        return do_set(cmd, out);
    }
    else if (cmd.size() == 2 && cmd[0] == "del") {
        return do_del(cmd, out);
    }else if (cmd.size() ==1 && cmd[0] == "keys"){
        return do_keys(cmd, out);
    }
    // AVL tree commands
    else if (cmd.size() == 2 && cmd[0] == "avl_get") {
        return do_avl_get(cmd, out);
    }
    else if (cmd.size() == 3 && cmd[0] == "avl_set") {
        return do_avl_set(cmd, out);
    }
    else if (cmd.size() == 2 && cmd[0] == "avl_del") {
        return do_avl_del(cmd, out);
    }
    else if (cmd.size() == 1 && cmd[0] == "avl_keys") {
        return do_avl_keys(cmd, out);
    }
    else {
        string err_msg = "unknown command";
        return out_err(out, ERR_UNKNOWN, err_msg.data(), err_msg.size());
        
    }
}


// static void serialise_response(const Response &resp, vector<uint8_t> &out){
//     uint32_t resp_len = 4 + (uint32_t) resp.payload.size(); // 4 for status , and then payload.
//     append_to_buffer(out, (const uint8_t *)&resp_len, 4 );
//     append_to_buffer(out , (const uint8_t *)& resp.status , 4);
//     append_to_buffer(out , resp.payload.data()  , resp.payload.size());
// }

static void start_serialise_response(vector<uint8_t> &out, size_t &header_pos){
    header_pos = out.size();
    // reserve space for total length - 4 bytes.
    append_to_buffer_u32(out, 0);
}

static size_t response_payload_size(vector<uint8_t> &out , size_t header_pos){
    return out.size()   - header_pos - 4;
}

static void end_serialise_response(vector<uint8_t> &out, size_t header_pos){
    size_t total_msg_size = response_payload_size(out , header_pos);
    if(total_msg_size > k_max_msg){
        out.resize(header_pos + 4); // discard the whole message.
        string err_msg = "response too big";
        out_err(out, ERR_TOO_BIG, err_msg.data(), err_msg.size());
        total_msg_size = response_payload_size(out , header_pos);
    }
    // fill in the total length of the message at header_pos.
    uint32_t total_size_u32 = (uint32_t) total_msg_size;
    memcpy(&out[header_pos], &total_size_u32, 4);
}

// actually parses the data accoring to our protocol.
static bool try_one_request(Conn *conn)
{
    if (conn->incoming.size() < 4)
    {
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);

    if (len > k_max_msg)
    {
        msg("append_to_buffer() too long");
        conn->want_close = true;
        return false;
    }

    if (4 + len > conn->incoming.size())
    {
        // more data is yet to arrive
        return false;
    }

    uint8_t *request = &conn->incoming[4];

    // ----- application level parsing ----
    vector<string> cmd;
    if(parse_request(request, len , cmd) < 0) {
        msg("bad request");
        conn->want_close = true;
        return false;
    }

    // ---- execute and serialise -----

    size_t header_pos = 0;
    start_serialise_response (conn->outgoing, header_pos);
    do_request(cmd, conn->outgoing);
    end_serialise_response(conn->outgoing , header_pos);

    // remove request from incoming buffer.
    buffer_consume(conn->incoming , 4 + len); 

    return true;
}

static void handle_write(Conn *conn) {
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());

    if(rv < 0 && errno == EAGAIN){
        // data not ready now, try again in the next iteration.
        return;
    }
    if(rv < 0){
        msg("handle_write() failed");
        conn->want_close = true;
        return;
    }
    // remove consumed data from buffer.
    buffer_consume(conn->outgoing , size_t(rv));

    // if all data written -> readiness intention.
    if(conn->outgoing.size() ==0){
        conn->want_read = true;
        conn->want_write = false;
    } // else continue write.
}

static void handle_read(Conn *conn)
{
    uint8_t buffer[64 * 1024]; // 64 KB buffer
    ssize_t rv = read(conn->fd, buffer, sizeof(buffer));

    if (rv < 0 && errno == EAGAIN)
    {
        // no data available right now.
        // eagain is a error code that means the operation cannot be completed immediately because it would block but may succeed later.
        return;
    }
    if (rv < 0)
    {
        // actual error
        msg("read() failed");
        conn->want_close = true;
        return;
    }

    // eof
    if (rv == 0)
    {
        if (conn->incoming.empty())
        {
            msg("client closed connection");
        }
        else
        {
            msg("client closed connection with pending data");
        }
        conn->want_close = true;
        return;
    }

    // append new data to incoming buffer
    append_to_buffer(conn->incoming, buffer, (size_t)rv);

    // pipeling.
    while (try_one_request(conn));

    if(conn->outgoing.size() > 0){
        conn->want_read= false;
        conn->want_write= true;
        return handle_write(conn);
    }
}



int main()
{
    printf("server up and running");
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

    // set the listening socket to non-blocking mode.
    fd_set_nonblocking(sockFd);

    rv = listen(sockFd, 10);
    if (rv < 0)
    {
        die("listen() failed");
    }
    vector<Conn *> fd2Conn;
    vector<struct pollfd> poll_args;

    while (true)
    {
        poll_args.clear();

        struct pollfd listen_pfd = {sockFd, POLLIN, 0};
        poll_args.push_back(listen_pfd);

        for (Conn *conn : fd2Conn)
        {
            if (conn == nullptr)
                continue;

            // always poll for error.

            struct pollfd pfd = {conn->fd, POLLERR, 0};
            if (conn->want_read)
            {
                pfd.events |= POLLIN;
            }
            if (conn->want_write)
            {
                pfd.events |= POLLOUT;
            }
            poll_args.push_back(pfd);
        }

        int rv = poll(poll_args.data(), nfds_t(poll_args.size()), -1);
        // interrupted by some signal
        if (rv < 0 && errno == EINTR)
        {
            continue;
        }
        // actual error.
        if (rv < 0)
        {
            die("poll() failed");
        }

        // handle new connections on the listening socket.
        if (poll_args[0].revents)
        {
            if (Conn *conn = handle_accept_new_client(sockFd))
            {
                // add to fd2Conn
                if (conn->fd >= (int)fd2Conn.size())
                {
                    // important point to note: the second argument will just initialize the new elements to nullptr not all of them.
                    fd2Conn.resize(conn->fd + 1, nullptr);
                }
                assert(fd2Conn[conn->fd] == nullptr);
                fd2Conn[conn->fd] = conn;
            }
        }

        // handling events on existing connections.
        for (size_t i = 1; i < poll_args.size(); i++)
        {
            uint32_t ready = poll_args[i].revents;

            if (ready == 0)
            {
                continue;
            }
            Conn *conn = fd2Conn[poll_args[i].fd];

            if (ready & POLLIN)
            {
                // ready to read from client;
                assert(conn->want_read);
                handle_read(conn);
            }
            if (ready & POLLOUT)
            {
                // ready to write to client
                assert(conn->want_write);
                handle_write(conn);
            }
            if ((ready & POLLERR) || conn->want_close)
            {
                // error or want to close the connection.
                close(conn->fd);
                fd2Conn[conn->fd] = nullptr;
                delete conn;
            }
        }
    }
    return 0;
}