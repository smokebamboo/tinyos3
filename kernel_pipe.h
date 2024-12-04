#ifndef KERNEL_PIPE_H
#define KERNEL_PIPE_H

#include "kernel_streams.h"


/** 
 * 	@brief The amount of bytes the buffer can hold
 */
#define PIPE_BUFFER_SIZE 4096


/** @brief The pipe control block.
 * 
 * 	The pipe control block serves as a means for processes
 * 	to communicate with each other. It utilizes two file control blocks
 * 	one for reading and one for writing data.
 */
typedef struct pipe_control_block {
	FCB *reader, *writer;
	CondVar has_space;    				/* For blocking writer if no space is available */
	CondVar has_data;     				/* For blocking reader until data are available */
	int w_position, r_position;  		/* write, read position in buffer */
	char BUFFER[PIPE_BUFFER_SIZE];   	/* bounded (cyclic) byte buffer */
} PIPE_CB;

int pipe_write(void* pipecb, const char *buf, unsigned int n);

int pipe_read(void* pipecb, char *buf, unsigned int n);

int pipe_writer_close(void* _pipecb);

int pipe_reader_close(void* _pipecb);

PIPE_CB* init_pipe_cb();

#endif