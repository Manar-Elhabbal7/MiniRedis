#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
using namespace std;

static void msg(const char *msg) {
    cout << msg << endl;
}

static void die(const char *msg) {
    int err = errno;
    cerr << "[" << err << "] " << msg << ": " << strerror(err) << endl;
    abort();
}

static void do_something(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        msg("read() error");
        return;
    }
    cout << "client says: " << rbuf << endl;

    const char wbuf[] = "world";
    ssize_t bytes_written = write(connfd, wbuf, strlen(wbuf));
    if (bytes_written < 0) {
        msg("write() error");
        return;
    }
}

int main() {
    // 1. Obtain a socket file descriptor (IPv4, TCP stream)
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    // 2. Set SO_REUSEADDR to avoid "address already in use" errors on restart
    int val = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        die("setsockopt()");
    }

    // 3. Bind to 0.0.0.0:1234
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0); // 0.0.0.0

    int rv = bind(fd, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
    if (rv < 0) {
        die("bind()");
    }

    // 4. Start listening with SOMAXCONN backlog queue
    rv = listen(fd, SOMAXCONN);
    if (rv < 0) {
        die("listen()");
    }

    msg("Server is listening on port 1234...");

    // 5. Accept connections in a loop (one by one for step 1)
    while (true) {
        struct sockaddr_in client_addr = {};
        socklen_t socklen = sizeof(client_addr);
        int connfd = accept(fd, reinterpret_cast<struct sockaddr *>(&client_addr), &socklen);
        if (connfd < 0) {
            continue; // Skip failed accept and keep listening
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        cout << "New client connected from " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;

        do_something(connfd);
        close(connfd);
    }

    close(fd);
    return 0;
}
