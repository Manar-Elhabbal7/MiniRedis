#include "common.h"

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

/// serialize and send a command vector to the server
static int32_t send_req(int fd, const vector<string> &cmd) {
    uint32_t len = 4;
    for (const string &s : cmd) {
        len += 4 + static_cast<uint32_t>(s.size());
    }
    if (len > k_max_msg) {
        return -1;
    }

    /// prepare payload
    vector<uint8_t> wbuf;
    wbuf.resize(4 + len);

    /// total message length header
    memcpy(wbuf.data(), &len, 4);

    /// number of strings
    uint32_t nstr = static_cast<uint32_t>(cmd.size());
    memcpy(&wbuf[4], &nstr, 4);

    size_t cur = 8;
    for (const string &s : cmd) {
        uint32_t p_len = static_cast<uint32_t>(s.size());
        memcpy(&wbuf[cur], &p_len, 4);
        memcpy(&wbuf[cur + 4], s.data(), s.size());
        cur += 4 + s.size();
    }

    return write_all(fd, reinterpret_cast<const char *>(wbuf.data()), wbuf.size());
}

/// read and parse a serialized response from the server
static int32_t read_res(int fd) {
    /// read 4-byte length header
    char rbuf[4];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf, 4);
    if (len < 4 || len > k_max_msg) {
        msg("bad response length");
        return -1;
    }

    /// read status code (4 bytes)
    uint32_t status = 0;
    err = read_full(fd, reinterpret_cast<char *>(&status), 4);
    if (err) {
        msg("read() status error");
        return err;
    }

    /// read response data payload (if any)
    size_t data_len = len - 4;
    vector<char> data(data_len + 1, 0);
    if (data_len > 0) {
        err = read_full(fd, data.data(), data_len);
        if (err) {
            msg("read() data error");
            return err;
        }
    }

    /// display formatted response
    if (status == RES_OK) {
        if (data_len > 0) {
            cout << "(string) \"" << data.data() << "\"" << endl;
        } else {
            cout << "(status) OK" << endl;
        }
    } else if (status == RES_NX) {
        cout << "(nil)" << endl;
    } else if (status == RES_ERR) {
        cout << "(error) unknown command or invalid arguments" << endl;
    } else {
        cout << "(unknown status: " << status << ")" << endl;
    }

    return 0;
}

int main() {
    /// create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    /// specify server address (127.0.0.1:1234)
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /// connect to server
    int rv = connect(fd, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
    if (rv < 0) {
        die("connect()");
    }

    /// test suite of Redis KV commands
    vector<vector<string>> test_commands = {
        {"set", "mykey", "hello_redis"},
        {"get", "mykey"},
        {"get", "non_existing_key"},
        {"set", "mykey", "updated_value"},
        {"get", "mykey"},
        {"del", "mykey"},
        {"get", "mykey"},
        {"bad_command", "foo", "bar"}
    };

    for (const auto &cmd : test_commands) {
        cout << "> ";
        for (const auto &arg : cmd) {
            cout << arg << " ";
        }
        cout << endl;

        if (send_req(fd, cmd) != 0) {
            die("send_req()");
        }
        if (read_res(fd) != 0) {
            die("read_res()");
        }
    }

    /// close connection
    close(fd);
    return 0;
}
