#include "common.h"
#include "buffer.h"
#include "connection.h"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

using namespace std;

/// in-memory key-value data storage
static map<string, string> g_data;

/// response structure
struct Response {
    uint32_t status = RES_OK;
    vector<uint8_t> data;
};

/// forward declaration for optimistic write in handle_read
static void handle_write(Conn *conn);

/// helper to parse a 32-bit integer from byte buffer
static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out) {
    if (cur + 4 > end) {
        return false;
    }
    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

/// helper to parse a string of length n from byte buffer
static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n, string &out) {
    if (cur + n > end) {
        return false;
    }
    out.assign(reinterpret_cast<const char *>(cur), n);
    cur += n;
    return true;
}

/// parse serialized command request into string vector
static int32_t parse_req(const uint8_t *data, size_t size, vector<string> &out) {
    const uint8_t *end = data + size;
    const uint8_t *cur = data;

    uint32_t nstr = 0;
    if (!read_u32(cur, end, nstr)) {
        return -1;
    }
    if (nstr > k_max_args) {
        return -1; /// safety limit
    }

    while (out.size() < nstr) {
        uint32_t len = 0;
        if (!read_u32(cur, end, len)) {
            return -1;
        }
        out.push_back(string());
        if (!read_str(cur, end, len, out.back())) {
            return -1;
        }
    }

    if (cur != end) {
        return -1; /// trailing garbage detected
    }
    return 0;
}

/// execute key-value command and populate response
static void do_request(vector<string> &cmd, Response &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        auto it = g_data.find(cmd[1]);
        if (it == g_data.end()) {
            out.status = RES_NX; /// key not found
            return;
        }
        const string &val = it->second;
        out.data.assign(val.begin(), val.end());
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        g_data[cmd[1]].swap(cmd[2]);
        out.status = RES_OK;
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        g_data.erase(cmd[1]);
        out.status = RES_OK;
    } else {
        out.status = RES_ERR; /// unrecognized command or invalid argument count
    }
}

/// serialize Response object into output buffer
static void make_response(const Response &resp, vector<uint8_t> &out) {
    uint32_t resp_len = 4 + static_cast<uint32_t>(resp.data.size());
    buf_append(out, reinterpret_cast<const uint8_t *>(&resp_len), 4);
    buf_append(out, reinterpret_cast<const uint8_t *>(&resp.status), 4);
    if (!resp.data.empty()) {
        buf_append(out, resp.data.data(), resp.data.size());
    }
}

/// accept a new connection and initialize its state
static Conn *handle_accept(int fd) {
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);
    int connfd = accept(fd, reinterpret_cast<struct sockaddr *>(&client_addr), &socklen);
    if (connfd < 0) {
        return nullptr;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    cout << "New client connected from " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;

    /// set connection socket to non-blocking mode
    fd_set_nonblock(connfd);

    /// allocate and initialize connection state
    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true;
    return conn;
}

/// process one request from incoming buffer if complete
static bool try_one_request(Conn *conn) {
    /// try to parse message length header (4 bytes)
    if (conn->incoming.size() < 4) {
        return false; /// need more data
    }

    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4); /// assume little-endian
    if (len > k_max_msg) {
        msg("too long");
        conn->want_close = true;
        return false;
    }

    /// check if full message body has arrived
    if (4 + len > conn->incoming.size()) {
        return false; /// need more data
    }

    const uint8_t *request = &conn->incoming[4];

    /// parse serialized command arguments
    vector<string> cmd;
    if (parse_req(request, len, cmd) < 0) {
        msg("bad request format");
        conn->want_close = true;
        return false;
    }

    /// execute command and serialize response
    Response resp;
    do_request(cmd, resp);
    make_response(resp, conn->outgoing);

    /// remove processed request from incoming buffer
    buf_consume(conn->incoming, 4 + len);
    return true;
}

/// handle read readiness on connection
static void handle_read(Conn *conn) {
    /// 1. perform non-blocking read
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv <= 0) {
        if (rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return; /// not ready yet
        }
        if (rv < 0 && errno == EINTR) {
            return; /// interrupted by signal
        }
        /// EOF (rv == 0) or unrecoverable error
        conn->want_close = true;
        return;
    }

    /// 2. append received bytes to incoming buffer
    buf_append(conn->incoming, buf, static_cast<size_t>(rv));

    /// 3. process all complete requests available in pipeline
    while (try_one_request(conn)) {}

    /// 4. update readiness intent and try optimistic non-blocking write
    if (!conn->outgoing.empty()) {
        conn->want_read = false;
        conn->want_write = true;
        return handle_write(conn); /// optimistic write to save 1 poll() iteration
    }
}

/// handle write readiness on connection
static void handle_write(Conn *conn) {
    assert(!conn->outgoing.empty());
    ssize_t rv = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    if (rv < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return; /// socket write buffer full, wait for POLLOUT
        }
        if (errno == EINTR) {
            return; /// interrupted by signal, retry on next readiness
        }
        conn->want_close = true;
        return;
    }

    /// remove written bytes from outgoing buffer
    buf_consume(conn->outgoing, static_cast<size_t>(rv));

    /// update readiness intent when all data is written
    if (conn->outgoing.empty()) {
        conn->want_read = true;
        conn->want_write = false;
    }
}

int main() {
    /// create listening socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    /// set SO_REUSEADDR to avoid address in use errors
    int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        die("setsockopt()");
    }

    /// bind to 0.0.0.0:1234
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0); /// 0.0.0.0

    int rv = bind(fd, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
    if (rv < 0) {
        die("bind()");
    }

    /// start listening for incoming connections
    rv = listen(fd, SOMAXCONN);
    if (rv < 0) {
        die("listen()");
    }

    /// set listening socket to non-blocking mode
    fd_set_nonblock(fd);

    msg("Server is listening on port 1234 (KV store)...");

    /// flat map of all client connections keyed by file descriptor
    vector<Conn *> fd2conn;

    /// event loop arguments for poll()
    vector<struct pollfd> poll_args;

    /// main event loop
    while (true) {
        poll_args.clear();

        /// put listening socket in first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        /// add all active connections
        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }
            struct pollfd conn_pfd = {conn->fd, POLLERR, 0};
            if (conn->want_read) {
                conn_pfd.events |= POLLIN;
            }
            if (conn->want_write) {
                conn_pfd.events |= POLLOUT;
            }
            poll_args.push_back(conn_pfd);
        }

        /// wait for readiness notifications
        int poll_rv = poll(poll_args.data(), static_cast<nfds_t>(poll_args.size()), -1);
        if (poll_rv < 0) {
            if (errno == EINTR) {
                continue; /// interrupted by signal, retry
            }
            die("poll()");
        }

        /// handle listening socket readiness
        if (poll_args[0].revents & POLLIN) {
            if (Conn *conn = handle_accept(fd)) {
                if (fd2conn.size() <= static_cast<size_t>(conn->fd)) {
                    fd2conn.resize(conn->fd + 1, nullptr);
                }
                fd2conn[conn->fd] = conn;
            }
        }

        /// handle connection socket events
        for (size_t i = 1; i < poll_args.size(); ++i) {
            uint32_t ready = poll_args[i].revents;
            Conn *conn = fd2conn[poll_args[i].fd];
            if (!conn) {
                continue;
            }

            if (ready & POLLIN) {
                handle_read(conn);
            }
            if (ready & POLLOUT) {
                handle_write(conn);
            }

            /// destroy connection on error or if application requested close
            if ((ready & POLLERR) || conn->want_close) {
                close(conn->fd);
                fd2conn[conn->fd] = nullptr;
                delete conn;
            }
        }
    }

    /// close listening socket
    close(fd);
    return 0;
}
