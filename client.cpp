#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

static void die(const char *msg) {
    int err = errno;
    cerr << "[" << err << "] " << msg << ": " << strerror(err) << endl;
    abort();
}

int main() {
    // 1. Create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    // 2. Specify server address (127.0.0.1:1234)
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 3. Connect to server
    int rv = connect(fd, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
    if (rv < 0) {
        die("connect()");
    }

    // 4. Send message
    const char msg_text[] = "hello";
    ssize_t bytes_written = write(fd, msg_text, strlen(msg_text));
    if (bytes_written < 0) {
        die("write()");
    }

    // 5. Read response
    char rbuf[64] = {};
    ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        die("read()");
    }
    cout << "server says: " << rbuf << endl;

    // 6. Close connection
    close(fd);
    return 0;
}
