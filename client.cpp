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

static void msg(const char *themsg) { fprintf(stderr, "%s\n", themsg); }

static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
}

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

/*
static int32_t query(int fd, const char *text) {
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg) {
        return -1;
    }

    char wbuf[4 + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    if (int32_t err = write_all(fd, wbuf, 4 + len)) {
        return err;
    }

    // 4 byte header
    char rbuf[4 + k_max_msg + 1];
    errno = 0;
    int32_t err = read_full(fd, rbuf, 4);
    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    memcpy(&len, rbuf, 4);
    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    // reply body
    err = read_full(fd, &rbuf[4], len);
    if (err) {
        msg("read() error");
        return err;
    }

    rbuf[4 + len] = '\0';
    printf("Server says :  %s\n", &rbuf[4]);
    return 0;
}
*/

static int32_t send_req(int fd, const char *text) {
    uint32_t len = (uint32_t)strlen(text);
    if (len > k_max_msg) {
        return -1;
    }

    char wbuf[4 + len + k_max_msg];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    return write_all(fd, wbuf, 4 + len);
}

static int32_t read_res(int fd) {
    char rbuf[4 + k_max_msg + 1];

    errno = 0;

    int32_t err = read_full(fd, rbuf, 4);

    if (err) {
        if (errno == 0) {
            msg("EOF");
        } else {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;

    memcpy(&len, rbuf, 4);

    if (len > k_max_msg) {
        msg("too long");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);

    if (err) {
        msg("Read error");
        return err;
    }

    rbuf[4 + len] = '\0';
    printf("Server says : %s\n", &rbuf[4]);
    return 0;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        die("Socket()");
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

    int rv = connect(fd, (const struct sockaddr *)&addr, sizeof(addr));

    if (rv) {
        die("Connect");
    }

    const char *query_list[3] = {"hello1", "hello2", "hello3"};

    for (size_t i = 0; i < 3; i++) {
        int32_t err = send_req(fd, query_list[i]);

        if (err) {
            goto L_DONE;
        }
    }

    for (size_t i = 0; i < 3; i++) {
        int32_t err = read_res(fd);

        if (err) {
            goto L_DONE;
        }
    }

L_DONE:
    close(fd);

    return 0;
}

