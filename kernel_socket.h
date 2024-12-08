#ifndef KERNEL_SOCKET_H
#define KERNEL_SOCKET_H

#include "kernel_pipe.h"

int socket_read(void* fid, char *buf, unsigned int size);

int socket_write(void* fid, const char* buf, unsigned int size);

int socket_close(void* fid);

static file_ops socket_file_ops = {
    .Read = socket_read,
    .Write = socket_write,
    .Close = socket_close
};

enum socket_type {
    SOCKET_LISTENER,
    SOCKET_UNBOUND,
    SOCKET_PEER
};

struct socket_control_block; //forward declaration
struct listener_socket {
    rlnode queue;
    CondVar req_available;
};

struct unbound_socket {
    rlnode unbound_socket;
};

struct peer_socket {
    struct socket_control_block* peer;
    PIPE_CB* write_pipe;
    PIPE_CB* read_pipe;
};

typedef struct socket_control_block {
    uint refcount;
    FCB* fcb;
    enum socket_type type;
    port_t port;

    union {
        struct listener_socket listener_s;
        struct unbound_socket unbound_s;
        struct peer_socket peer_s;
    };
} SCB;

typedef struct connection_request {
    SCB* peer;
    int admitted;
    CondVar request_honored;
    rlnode request_node;
} request;

Fid_t sys_Socket(port_t port);

#endif