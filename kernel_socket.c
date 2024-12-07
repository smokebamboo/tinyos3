
#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

SCB* PORT_MAP[MAX_PORT + 1]

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
	if (port < 1 || port > MAX_PORT) return -1;

	FCB* fcb;
	Fid_t fid;
	if(!FCB_reserve(1, &fid, &fcb)) return -1; //MAY NEED TO BE INITIALIZED AS AN ARRAY

	SCB* scb = init_scb();

	scb->fcb = fcb;

	fcb->streamobj = scb;
	fcb->streamfunc = &socket_file_ops;

	if (port != NOPORT) scb->port = port;

	return fid;
}

int socket_read(void* fid, char *buf, unsigned int size) {
	if (!fid) return -1;
	SCB* scb = CURPROC->FIDT[(Fid_t) fid]->streamobj;
	if (!scb) return -1;
	pipe_read(scb->peer_s.read_pipe, buf, size);
	return 0;
}

int socket_write(void* fid, const char* buf, unsigned int size) {
	if (!fid) return -1;
	SCB* scb = CURPROC->FIDT[(Fid_t) fid]->streamobj;
	if (!scb) return -1;
	pipe_write(scb->peer_s.write_pipe, buf, size);
	return 0;
}

int socket_close(void* fid) {
	if (!fid) return -1;
	SCB* scb = CURPROC->FIDT[(Fid_t) fid]->streamobj;
	if (!scb) return -1;
	pipe_reader_close(scb->peer_s.read_pipe);
	pipe_writer_close(scb->peer_s.write_pipe);
	return 0;
}

int sys_Listen(Fid_t sock) {
	if (sock == NULL) return -1;
	SCB* scb = CURPROC->FIDT[sock]->streamobj;
	if (!scb
		|| scb->port == NOPORT
		|| scb->type == SOCKET_LISTENER
		|| PORT_MAP[scb->port])
			return -1;

	PORT_MAP[scb->port] = scb;
	scb->type = SOCKET_LISTENER;
	scb->listener_s.req_available = COND_INIT;
	rlnode_init(&scb->listener_s.queue, NULL);
	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}

