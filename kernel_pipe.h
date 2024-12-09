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
 * 	to communicate with each other. It utilizes two file control blocks:
 * 	one for reading and one for writing data.
 */
typedef struct pipe_control_block {
	FCB *reader, *writer;				/**< @brief The FCBs used to read or write to the pipe. */
	CondVar has_space;    				/**< @brief CondVar used to block writer if no space is available. */
	CondVar has_data;     				/**< @brief CondVar used to block reader until data are available. */
	int w_position, r_position;  		/**< @brief Write and read positions in buffer. */
	char BUFFER[PIPE_BUFFER_SIZE];   	/**< @brief A bounded (cyclic) byte buffer */
} PIPE_CB;

/**
 * @brief Commands @c pipecb to write as many as possible of the data it holds to @c buf
 */
int pipe_write(void* pipecb, const char *buf, unsigned int n);

/**
 * @brief Commands @c pipecb to read from buf as many data as possible
 */
int pipe_read(void* pipecb, char *buf, unsigned int n);

/**
 * @brief Close the write end of a pipe
 */
int pipe_writer_close(void* _pipecb);

/**
 * @brief Close the read end of a pipe
 */
int pipe_reader_close(void* _pipecb);

/**
 * @brief Initialize and return a new PIPE_CB
 */
PIPE_CB* init_pipe_cb();

#endif