
#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_sched.h"
#include "kernel_proc.h"


SCB* init_scb() {
	SCB* scb = xmalloc(sizeof(SCB));

	scb->refcount = 0;
	scb->fcb = NULL;
	scb->port = NOPORT;

	scb->type = SOCKET_UNBOUND;
	rlnode_init(&scb->unbound_s.unbound_socket, scb);

	return scb;
}

Fid_t sys_Socket(port_t port)
{
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

	SCB* scb = CURPROC->FIDT[(Fid_t) fid]->streamobj;
	pipe_read(scb->peer_s.read_pipe, buf, size);
}

int socket_write(void* fid, const char* buf, unsigned int size) {
		SCB* scb = CURPROC->FIDT[(Fid_t) fid]->streamobj;
		pipe_write(scb->peer_s.write_pipe, buf, size);
}

int socket_close(void* fid) {
	SCB* scb = CURPROC->FIDT[(Fid_t) fid]->streamobj;
	pipe_reader_close(scb->peer_s.read_pipe);
	pipe_writer_close(scb->peer_s.write_pipe);
}

int sys_Listen(Fid_t sock)
{
	return -1;
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

