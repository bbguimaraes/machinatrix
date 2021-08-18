#include "socket.h"

#include <assert.h>
#include <string.h>

#include <netdb.h>
#include <sys/un.h>

#include "log.h"
#include "utils.h"

static_assert(
    sizeof(((struct sockaddr_un*)0)->sun_path)
    <= MTRIX_MAX_UNIX_PATH);

struct addrinfo *mtrix_socket_addr(const char *addr) {
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    };
    char *port = NULL;
    char *const colon = strchr(addr, ':');
    if(colon)
        *colon = 0, port = colon + 1;
    struct addrinfo *ret = NULL;
    const int err = getaddrinfo(addr, port, &hints, &ret);
    if(err) {
        log_err("%s: getaddrinfo: %s\n", __func__, gai_strerror(err));
        return NULL;
    }
    return ret;
}
