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
    /// create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        die("socket()");
    }

    /// specify the server address 
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    /// connect to the server
    int rv = connect(fd, reinterpret_cast<const struct sockaddr *>(&addr), sizeof(addr));
    if (rv < 0) {
        die("connect()");
    }

    /// send the message
    const char msg_text[] = "Hello :)";
    ssize_t bytes_written = write(fd, msg_text, strlen(msg_text));
    if (bytes_written < 0) {
        die("write()");
    }

    // read response
    char rbuf[64] = {};
    ssize_t n = read(fd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        die("read()");
    }
    cout << "server says: " << rbuf << endl;

    // close the connection
    close(fd);
    return 0;
}
