
#include "tinyos.h"
#include "kernel_pipe.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

int can_write(PIPE_CB* pipe) {
	/** checks if the writer has come full circle behind the reader and can't read anymore
		without corrupting unread data
	*/
	if (pipe->r_position == (pipe->w_position + 1) % PIPE_BUFFER_SIZE) return 0;
	return 1;
}

int can_read(PIPE_CB* pipe) {
	/** checks if the reader is in the same position as the writer
	 * which means there is no data to read
	 */
	if (pipe->r_position == pipe->w_position) return 0;
	return 1;
}

int pipe_write(void* pipecb, const char *buf, unsigned int n) {
	if (!pipecb) return -1;

	PIPE_CB* pipe = (PIPE_CB*) pipecb;
	if (pipe->reader == NULL || pipe->writer == NULL) return -1;

	while (!can_write(pipe) && pipe->reader != NULL) {
		kernel_broadcast(&pipe->has_data);
		kernel_wait(&pipe->has_space, SCHED_PIPE);
	}

	int chars_written;
	for (chars_written = 0; chars_written < n && can_write(pipe); chars_written++) {
		pipe->BUFFER[pipe->w_position] = buf[chars_written];
		pipe->w_position = (pipe->w_position + 1) % PIPE_BUFFER_SIZE;
	}
	
	kernel_broadcast(&pipe->has_data);
	return chars_written;
}

int pipe_read(void* pipecb, char *buf, unsigned int n) {
	if (!pipecb) return -1;
	PIPE_CB* pipe = (PIPE_CB*) pipecb;
	if (pipe->reader == NULL) return -1;

	while (!can_read(pipe) && pipe->writer != NULL) {
		kernel_broadcast(&pipe->has_space);
		kernel_wait(&pipe->has_data, SCHED_PIPE);
	}

	int chars_read;
	for (chars_read = 0; chars_read < n && can_read(pipe); chars_read++) {
		buf[chars_read] = pipe->BUFFER[pipe->r_position];
		pipe->r_position = (pipe->r_position + 1) % PIPE_BUFFER_SIZE;
	}

	kernel_broadcast(&pipe->has_space);
	return chars_read;
}

int pipe_writer_close(void* pipecb) {
	PIPE_CB* pipe = (PIPE_CB*) pipecb;

	if (!pipe) return -1;

	pipe->writer = NULL;
	(pipe->reader == NULL) ? free(pipe) : kernel_broadcast(&pipe->has_data);
	return 0;
}

int pipe_reader_close(void* pipecb) {
	PIPE_CB* pipe = (PIPE_CB*) pipecb;

	if (!pipe) return -1;

	pipe->reader = NULL;
	(pipe->writer == NULL) ? free(pipe) : kernel_broadcast(&pipe->has_space);
	return 0;
}

static file_ops reader_file_ops = {
	.Read = pipe_read,
	.Close = pipe_reader_close
};

static file_ops writer_file_ops = {
	.Write = pipe_write,
	.Close = pipe_writer_close
};

PIPE_CB* init_pipe_cb() {
	PIPE_CB* pipe_cb = (PIPE_CB*) xmalloc(sizeof(PIPE_CB));
	pipe_cb->reader = NULL;
	pipe_cb->writer = NULL;
	pipe_cb->has_space = COND_INIT;
	pipe_cb->has_data = COND_INIT;
	pipe_cb->w_position = 0;
	pipe_cb->r_position = 0;

	return pipe_cb;
}

int sys_Pipe(pipe_t* pipe)
{
	FCB* fcb[2];
	Fid_t fids[2];
	if(!FCB_reserve(2, fids, fcb)) return -1;

	pipe->read = fids[0];
	pipe->write = fids[1];
	
	PIPE_CB* new_pipe_cb = init_pipe_cb();

	new_pipe_cb->reader = fcb[0];
	new_pipe_cb->writer = fcb[1];

	fcb[0]->streamfunc = &reader_file_ops;
	fcb[1]->streamfunc = &writer_file_ops;

	fcb[0]->streamobj = new_pipe_cb;
	fcb[1]->streamobj = new_pipe_cb;

	return 0;
}

