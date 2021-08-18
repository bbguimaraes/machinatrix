#include "numeraria.h"

#include <string.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "log.h"
#include "socket.h"
#include "utils.h"

int mtrix_numeraria_init_socket(const char *addr) {
    struct addrinfo *const addr_info = mtrix_socket_addr(addr);
    if(!addr_info)
        return log_err("%s: failed to determine address\n", __func__), -1;
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1) {
        log_errno("%s: failed to create socket", __func__);
        goto err;
    }
    if(connect(fd, addr_info->ai_addr, addr_info->ai_addrlen) == -1) {
        log_errno("%s: failed to connect", __func__);
        goto err;
    }
    freeaddrinfo(addr_info);
    return fd;
err:
    freeaddrinfo(addr_info);
    if(fd != -1 && close(fd) == -1)
        log_errno("%s: failed to close socket", __func__);
    return -1;
}

int mtrix_numeraria_init_unix(const char *path) {
    const size_t len = strlen(path), max_len = MTRIX_MAX_UNIX_PATH;
    if(len >= max_len) {
        log_err("%s: path too long (%zu >= %zu)\n", __func__, len, max_len);
        return -1;
    }
    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(fd == -1)
        return log_errno("%s: socket"), -1;
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    addr.sun_path[len] = 0;
    if(connect(fd, (const struct sockaddr*)&addr, sizeof(addr)) == -1) {
        log_errno("%s: connect", __func__);
        if(close(fd) == -1)
            log_errno("%s: close", __func__);
        return -1;
    }
    return fd;
}
