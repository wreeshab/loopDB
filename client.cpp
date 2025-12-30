#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdint>

using namespace std;

enum {
    TAG_NIL = 0,
    TAG_ERR = 1,
    TAG_STR = 2,
    TAG_INT = 3,
    TAG_DBL = 4,
    TAG_ARR = 5,
};

enum {
    ERR_UNKNOWN = 1,
    ERR_TOO_BIG = 2,
};

static void write_u32(vector<uint8_t>& buf, uint32_t x) {
    uint8_t* p = (uint8_t*)&x;
    buf.insert(buf.end(), p, p + 4);
}

static void write_u8(vector<uint8_t>& buf, uint8_t x) {
    buf.push_back(x);
}

static void write_str(vector<uint8_t>& buf, const string& s) {
    write_u32(buf, s.size());
    buf.insert(buf.end(), s.begin(), s.end());
}

static vector<uint8_t> make_request(const vector<string>& cmd) {
    vector<uint8_t> body;
    write_u32(body, cmd.size());
    for (auto& s : cmd) {
        write_str(body, s);
    }

    vector<uint8_t> msg;
    write_u32(msg, body.size());
    msg.insert(msg.end(), body.begin(), body.end());
    return msg;
}

static bool read_exact(int fd, void* buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t r = read(fd, (char*)buf + off, n - off);
        if (r <= 0) return false;
        off += r;
    }
    return true;
}

static void print_response(const vector<uint8_t>& resp_data, size_t& pos);

static void print_array(const vector<uint8_t>& resp_data, size_t& pos) {
    if (pos + 4 > resp_data.size()) {
        cout << "(error: invalid array format)\n";
        return;
    }
    uint32_t count;
    memcpy(&count, &resp_data[pos], 4);
    pos += 4;

    cout << "[\n";
    for (uint32_t i = 0; i < count; i++) {
        cout << "  " << (i + 1) << ") ";
        print_response(resp_data, pos);
    }
    cout << "]\n";
}

static void print_response(const vector<uint8_t>& resp_data, size_t& pos) {
    if (pos >= resp_data.size()) {
        cout << "(empty response)\n";
        return;
    }

    uint8_t tag = resp_data[pos++];

    switch (tag) {
        case TAG_NIL:
            cout << "(nil)\n";
            break;
        case TAG_ERR: {
            if (pos + 4 > resp_data.size()) {
                cout << "(error: invalid error format)\n";
                break;
            }
            uint32_t err_code;
            memcpy(&err_code, &resp_data[pos], 4);
            pos += 4;
            
            if (pos + 4 > resp_data.size()) {
                cout << "(error: invalid error format)\n";
                break;
            }
            uint32_t msg_len;
            memcpy(&msg_len, &resp_data[pos], 4);
            pos += 4;

            if (pos + msg_len > resp_data.size()) {
                cout << "(error: invalid error format)\n";
                break;
            }
            string err_msg(resp_data.begin() + pos, resp_data.begin() + pos + msg_len);
            cout << "(error code " << err_code << "): " << err_msg << "\n";
            pos += msg_len;
            break;
        }
        case TAG_STR: {
            if (pos + 4 > resp_data.size()) {
                cout << "(error: invalid string format)\n";
                break;
            }
            uint32_t str_len;
            memcpy(&str_len, &resp_data[pos], 4);
            pos += 4;

            if (pos + str_len > resp_data.size()) {
                cout << "(error: invalid string format)\n";
                break;
            }
            string result(resp_data.begin() + pos, resp_data.begin() + pos + str_len);
            cout << "\"" << result << "\"\n";
            pos += str_len;
            break;
        }
        case TAG_INT: {
            if (pos + 8 > resp_data.size()) {
                cout << "(error: invalid int format)\n";
                break;
            }
            int64_t val;
            memcpy(&val, &resp_data[pos], 8);
            cout << val << "\n";
            pos += 8;
            break;
        }
        case TAG_DBL: {
            if (pos + 8 > resp_data.size()) {
                cout << "(error: invalid double format)\n";
                break;
            }
            double val;
            memcpy(&val, &resp_data[pos], 8);
            cout << val << "\n";
            pos += 8;
            break;
        }
        case TAG_ARR: {
            print_array(resp_data, pos);
            break;
        }
        default:
            cout << "(unknown tag: " << (int)tag << ")\n";
            break;
    }
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    cout << "╔══════════════════════════════════════════╗\n";
    cout << "║     LoopDB Key-Value Store Client        ║\n";
    cout << "╚══════════════════════════════════════════╝\n\n";
    cout << "Available commands:\n";
    cout << "  HashMap commands:\n";
    cout << "    • set <key> <value>   - Store a key-value pair\n";
    cout << "    • get <key>           - Retrieve value by key\n";
    cout << "    • del <key>           - Delete a key\n";
    cout << "    • keys                - List all keys\n\n";
    cout << "  AVL Tree commands:\n";
    cout << "    • avl_set <key> <value>   - Store in AVL tree\n";
    cout << "    • avl_get <key>           - Retrieve from AVL tree\n";
    cout << "    • avl_del <key>           - Delete from AVL tree\n";
    cout << "    • avl_keys                - List all keys (sorted)\n\n";
    cout << "  • quit / exit         - Disconnect\n\n";

    string line;
    while (true) {
        cout << "> ";
        if (!getline(cin, line)) break;

        istringstream iss(line);
        vector<string> cmd;
        string s;
        while (iss >> s) cmd.push_back(s);

        if (cmd.empty()) continue;
        if (cmd[0] == "quit" || cmd[0] == "exit") break;

        auto req = make_request(cmd);
        write(sock, req.data(), req.size());

        // Read message length
        uint32_t msg_len = 0;
        if (!read_exact(sock, &msg_len, 4)) break;

        // Read message payload
        vector<uint8_t> payload(msg_len);
        if (msg_len > 0) {
            if (!read_exact(sock, payload.data(), msg_len)) break;
        }

        size_t pos = 0;
        print_response(payload, pos);
    }

    close(sock);
    return 0;
}
