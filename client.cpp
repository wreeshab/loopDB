#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>

using namespace std;

static void write_u32(vector<uint8_t>& buf, uint32_t x) {
    uint8_t* p = (uint8_t*)&x;
    buf.insert(buf.end(), p, p + 4);
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

    cout << "kv-cli connected. Type commands:\n";

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

        uint32_t len = 0;
        if (!read_exact(sock, &len, 4)) break;

        uint32_t status = 0;
        read_exact(sock, &status, 4);

        vector<uint8_t> payload(len - 4);
        if (!payload.empty()) {
            read_exact(sock, payload.data(), payload.size());
        }

        if (status == 0) {
            if (!payload.empty())
                cout << string(payload.begin(), payload.end()) << "\n";
            else
                cout << "OK\n";
        } else if (status == 2) {
            cout << "(nil)\n";
        } else {
            cout << "(error)\n";
        }
    }

    close(sock);
    return 0;
}
