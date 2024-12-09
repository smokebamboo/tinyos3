#ifndef KERNEL_SOCKET_H
#define KERNEL_SOCKET_H

#include "kernel_pipe.h"

/**
 * @brief Reads from to socket and returns data to buf
 * 
 * Uses the read pipe in the given socket to read the data in the pipe and put them in the socket.
 * @returns Number of chars read
 */
int socket_read(void* __scb, char *buf, unsigned int size);

/**
 * @brief Writes socket data to buf
 * 
 * Uses the write pipe in the given socket to write the data from buf to the socket.
 * @returns Number of chars written
 */
int socket_write(void* __scb, const char* buf, unsigned int size);

/**
 * @brief Terminates the given socket
 * 
 * The function handles each socket according to its type:
 * Listener Sockets -> Stop listening to their port, send a signal to stop the accept process and notify all the requests they have been handled
 * Peer Sockets -> Disconnect from their peer and close their read and write pipes
 * Then, the sockets disconnect from their port and their FCB and if it no longer needed the socket is freed
 */
int socket_close(void* __scb);

/**
 * @brief Just a dummy function to use in socket_file_ops.
 * 
 * Returns NULL.
 */
void* false_open_sock (uint minor);

/**
 * @brief Functions peer sockets can use
 */
static file_ops socket_file_ops = {
    .Open = false_open_sock,
    .Read = socket_read,
    .Write = socket_write,
    .Close = socket_close
};

/**
 * @brief The type of the socket
 * 
 * Each socket is described by a type.
 * Listeners are responsible to listen to a port and try to accept incoming connections
 * Unbound Sockets are sockets that may or may not be bound to a port, without any special role whatsoever
 * Peer Sockets are connected with another peer socket and they can communicate using pipes
 */
enum socket_type {
    SOCKET_LISTENER,
    SOCKET_UNBOUND,
    SOCKET_PEER
};

/*See further down*/
struct socket_control_block; //forward declaration

/**
 * @brief Struct that contains the necessary info for a listener socket
 */
struct listener_socket {
    rlnode queue;                   /**< @brief A queue containing the request sent to the server. They are handled in a FIFO fashion. */
    CondVar req_available;          /**< @brief A CondVar sent when there is a new request to be processed. */
};

/**
 * @brief Struct that contains the necessary info for an unbound socket
 */
struct unbound_socket {
    rlnode unbound_socket;
};

/**
 * @brief Struct that contains the necessary info for a peer socket
 */
struct peer_socket {
    struct socket_control_block* peer;
    PIPE_CB* write_pipe;            /**< @brief A PIPE_CB used to write data to. */
    PIPE_CB* read_pipe;             /**< @brief A PIPE_CB used to read data from. */
};

/**
 * @brief A block containing the info for a socket
 * 
 * Each socket is described by a SCB. The SCB contains the neccessary info of the socket and also contains one instance of a socket type struct
 */
typedef struct socket_control_block {
    uint refcount;                      /**< @brief The amount of processes currently using the socket. */
    FCB* fcb;                           /**< @brief The FCB connected to the socket. */
    enum socket_type type;              /**< @brief The type of the socket (listening, unbound, peer) */
    port_t port;                        /**< @brief A port the socket is bound to. If it becomes either a listening or peer socket, the port will be used to listen or connect to. */

    union {
        struct listener_socket listener_s;
        struct unbound_socket unbound_s;
        struct peer_socket peer_s;
    };
} SCB;

/**
 * @brief A block containing the info for a connection request sent by a socket
 * 
 * A request is sent by an unbound socket when it tries to connect to the server. It is received by the port listener socket, which handles it accordingly.
 */
typedef struct connection_request {
    SCB* peer;                      /**< @brief The socket that sent the request. */
    int admitted;                   /**< @brief Whether the request has been handled. */
    CondVar request_honored;        /**< @brief CondVar sent when the request has been handled. */
    rlnode request_node;            /**< @brief A node to register the request in the queue of the port it wants to connect. */
} request;

// Fid_t sys_Socket(port_t port);
#define SERVER_TIMEOUT 500          /**< @brief How long to wait (msec) before a connection request times out. */

#endif