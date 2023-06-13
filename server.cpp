#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>

const size_t k_max_msg = 4096;

static void msg_error(const char *msg) { fprintf(stderr, "%s\n", msg); }

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0) {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        msg_error("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;

    (void)fcntl(fd, F_SETFL, flags);

    if (errno) {
        msg_error("fcntl set flags");
    }
}

enum {
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2,  // mark connection for deletion
};

struct Conn {
    int fd = -1;
    uint32_t state = 0;    // either state req or res
    size_t rbuf_size = 0;  // buffer for reading
    uint8_t rbuf[4 + k_max_msg];
    size_t wbuf_size = 0;  // buffer for writing
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];
};

static void conn_put(std::vector<Conn *> &fd2conn, struct Conn *Conn) {
    if (fd2conn.size() < = (size_t)conn->fd) {
        fd2conn.resize(conn->fd + 1);
    }
    fd2conn[conn->fd] = conn;
}

static int32_t one_request(int connfd) {
    // 4 byte header
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            msg_error("EOF");
        } else {
            msg_error("read() error");
        }
        return err;
    }

    uint32_t len = 0;

    memcpy(&len, rbuf, 4);

    if (len > k_max_msg) {
        msg_error("too long");
        return -1;
    }

    // request body
    err = read_full(connfd, &rbuf[4], len);
    if (err) {
        msg_error("read() error");
        return err;
    }

    // do something
    rbuf[4 + len] = '\0';
    printf("Client says : %s\n", &rbuf[4]);

    // reply using the same protocol
    const char reply[] = "wassup";
    char wbuf[4 + sizeof(reply)];

    len = (uint32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_all(connfd, wbuf, 4 + len);
}

static void do_something(int connfd) {
    char rbuf[64] = {};
    ssize_t n = read(connfd, rbuf, sizeof(rbuf) - 1);
    if (n < 0) {
        msg_error("read() error");
        return;
    }

    printf("Client says : %s\n", rbuf);

    const char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

void connection_io(Conn *conn) {
    if (conn->state == STATE_REQ) {
        state_req(conn);
    } else if (conn->state == STATE_RES) {
        state_res(conn);
    } else {
        assert(0);  // not expected
    }
}

int main() {
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
    addr.sin_addr.s_addr = ntohl(0);

    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));

    if (rv) {
        die("bind");
    }

    rv = listen(fd, SOMAXCONN);

    if (rv) {
        die("listen()");
    }

    // a map of all client connections keyed by fd
    std::vector<Conn *> fd2conn;

    // set listening fd to nonblocking
    fd_set_nb(fd);

    // the event loop
    std::vector<struct pollfd> poll_args;

    while (true) {
        // prepare the arguments of poll()
        poll_args.clear();

        // for convince the listening fd is put in the first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }
            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
            pfd.evnets = pfd.events | POLLERR;
            poll_args.push_back(pfd);
        }

        // poll for active fds
        int rv = poll(poll_args.data(), (ndfs_t)poll_args.size(), 1000);
        if (rv < 0) {
            die("poll");
        }

        // process active connections
        for (size_t i = 1; i < poll_args.size(); ++i) {
            if (poll_args[i].revents) {
                Conn *conn = fd2conn[poll_args[i].fd];
                connection_io(conn);
                if (conn->state == STATE_END) {
                    // client closed normally or something happened
                    // destroy this connection
                    fd2conn[conn->fd] = NULL;
                    (void)close(conn->fd);
                    free(conn);
                }
            }
        }

        if (poll_args[0].revents) {
            (void)accept_new_connection(fd2conn, fd);
        }
        return 0;
    }
