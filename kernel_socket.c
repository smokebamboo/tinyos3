
#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "unit_testing.h"

SCB* PORT_MAP[MAX_PORT + 1] = {NULL};

int fid_legal(Fid_t fid) {
	if (fid >= MAX_FILEID || fid <= NOFILE) return 0;
	return 1;
}

int port_legal(port_t port) {
	if (port > MAX_PORT || port <= NOPORT) return 0;
	return 1;
}

// SCB* get_scb (Fid_t fid) {
// 	return (fid_legal(fid)) ? CURPROC->FIDT[fid]->streamobj : NULL;
// }

SCB* get_scb (Fid_t fid) {
	FCB* fcb = get_fcb(fid);
	if (!fcb) return NULL;
	SCB* scb = fcb->streamobj;
	if (!scb) return NULL;
	return scb;
}

SCB* init_scb() {
	SCB* scb = xmalloc(sizeof(SCB));

	scb->refcount = 0;
	scb->fcb = NULL;
	scb->port = NOPORT;

	scb->type = SOCKET_UNBOUND;
	rlnode_init(&scb->unbound_s.unbound_socket, scb);

	return scb;
}

Fid_t sys_Socket(port_t port) {
	if (port < NOPORT || port > MAX_PORT) return -1;

	FCB* fcb[1];
	Fid_t fid[1];
	if(!FCB_reserve(1, fid, fcb)) return -1; //MAY NEED TO BE INITIALIZED AS AN ARRAY
	SCB* scb = init_scb();

	scb->fcb = fcb[0];

	fcb[0]->streamobj = scb;
	fcb[0]->streamfunc = &socket_file_ops;

	if (port != NOPORT) scb->port = port;

	return fid[0];
}

int socket_read(void* __scb, char *buf, unsigned int size) {
	// SCB* scb = get_scb((Fid_t) fid);
	SCB* scb = (SCB*) __scb;
	if (!scb) return -1;
	return pipe_read(scb->peer_s.read_pipe, buf, size);
}

int socket_write(void* __scb, const char* buf, unsigned int size) {
	// SCB* scb = get_scb((Fid_t) fid);
	SCB* scb = (SCB*) __scb;
	if (!scb) return -1;
	return pipe_write(scb->peer_s.write_pipe, buf, size);
}

int socket_close(void* __scb) {
	// SCB* scb = get_scb((Fid_t) fid);
	SCB* scb = (SCB*) __scb;
	if (!scb) return -1;
	
	switch(scb->type) {
		case SOCKET_LISTENER:
			PORT_MAP[scb->port] = NULL;

			kernel_signal(&scb->listener_s.req_available);
			while(!is_rlist_empty(&scb->listener_s.queue)) {
				rlnode* node = rlist_pop_front(&scb->listener_s.queue);
				kernel_signal(&node->req->request_honored);
			}
			break;
		case SOCKET_UNBOUND:
			break;
		case SOCKET_PEER:
			scb->peer_s.peer = NULL;
			pipe_reader_close(scb->peer_s.read_pipe);
			pipe_writer_close(scb->peer_s.write_pipe);
	}
	scb->port = NOPORT;
	scb->fcb = NULL;
	
	if(scb->refcount == 0) free(scb);

	return 0;
}

int sys_Listen(Fid_t sock) {
	SCB* scb = get_scb(sock);
	if (!scb
		|| scb->port == NOPORT
		|| scb->type != SOCKET_UNBOUND
		|| PORT_MAP[scb->port] != NULL)
			return -1;

	PORT_MAP[scb->port] = scb;
	scb->type = SOCKET_LISTENER;
	scb->listener_s.req_available = COND_INIT;
	rlnode_init(&scb->listener_s.queue, NULL);
	return 0;
}

void connect_peers(SCB* peer, SCB* client) {
	PIPE_CB* pipe1 = init_pipe_cb();
	PIPE_CB* pipe2 = init_pipe_cb();

	pipe1->reader = client->fcb;
	pipe1->writer = peer->fcb;

	pipe2->reader = peer->fcb;
	pipe2->writer = client->fcb;

	client->peer_s.write_pipe = pipe2;
	client->peer_s.read_pipe = pipe1;

	peer->peer_s.write_pipe = pipe1;
	peer->peer_s.read_pipe = pipe2;
	return;
}

Fid_t sys_Accept(Fid_t lsock) {
	SCB* listener = get_scb(lsock);
	if(!listener || listener->type != SOCKET_LISTENER) return NOFILE;

	listener->refcount++;

	if(is_rlist_empty(&listener->listener_s.queue)) {
		kernel_wait(&listener->listener_s.req_available, SCHED_PIPE);
	}
	if (!PORT_MAP[listener->port]) {
		listener->refcount--;
		return NOFILE;
	}

	request* req = rlist_pop_front(&listener->listener_s.queue)->req;
	if (!req) {
	listener->refcount--;
	return NOFILE;	
	}

	SCB* client = req->peer;
	Fid_t peer_fid = sys_Socket(NOPORT);
	if (peer_fid == NOFILE) return -1;
	SCB* peer = get_scb(peer_fid);

	peer->type = SOCKET_PEER;
	peer->peer_s.peer = client;

	//convert client to peer socket
	client->type = SOCKET_PEER;
	client->peer_s.peer = peer;

	connect_peers(peer, client);

	req->admitted = 1;
	kernel_signal(&req->request_honored);

	listener->refcount--;
	return peer_fid;
}

request* create_request(Fid_t sock) {
	request* newreq = xmalloc(sizeof(request));
	newreq->peer = get_scb(sock);
	newreq->admitted = 0;
	newreq->request_honored = COND_INIT;
	rlnode_init(&newreq->request_node, newreq);
	return newreq;
}

int sys_Connect(Fid_t sock, port_t port, timeout_t timeout) {
	if (!fid_legal(sock) || !port_legal(port) || !PORT_MAP[port]) return -1;
	SCB* listener = PORT_MAP[port];
	SCB* client = get_scb(sock);
	if (!client || !listener) return -1;

	client->refcount++;
	request* req = create_request(sock);
	rlist_push_back(&PORT_MAP[port]->listener_s.queue, &req->request_node);
	kernel_signal(&listener->listener_s.req_available);
	
	kernel_timedwait(&req->request_honored, SCHED_PIPE, 500);
	client->refcount--;

	int retval = (req->admitted) ? 0 : -1;
	rlist_remove(&req->request_node);
	free(req);

	return retval;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how) {
	SCB* scb = get_scb(sock);
	if (!scb) return -1;

	switch (how) {
		case SHUTDOWN_READ:
			pipe_reader_close(scb->peer_s.read_pipe);
			break;
		case SHUTDOWN_WRITE:
			pipe_writer_close(scb->peer_s.write_pipe);
			break;
		case SHUTDOWN_BOTH:
			pipe_reader_close(scb->peer_s.read_pipe);
			pipe_writer_close(scb->peer_s.write_pipe);
			break;
		default: 
			return -1;
	}
	return 0;
}