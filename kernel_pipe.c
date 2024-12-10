
#include "tinyos.h"
#include "kernel_pipe.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

/*Checks if the pipe is able to write*/
int can_write(PIPE_CB* pipe) {
	/** if writer has come full circle behind the reader ->
	 *  can't read anymore without corrupting unread data
	 */
	if (pipe->r_position == (pipe->w_position + 1) % PIPE_BUFFER_SIZE) return 0;
	return 1;
}

/*Checks if the pipe is able to read*/
int can_read(PIPE_CB* pipe) {
	/** if reader is in same position as writer
	 *  means there is no data to read
	 */
	if (pipe->r_position == pipe->w_position) return 0;
	return 1;
}

void* false_open_pipe (uint minor) {
	return NULL;
}

int false_read (void* pipecb, char *buf, unsigned int n) {
	return -1;
}

int false_write (void* pipecb, const char *buf, unsigned int n) {
	return -1;
}

int pipe_write(void* pipecb, const char *buf, unsigned int n) {
	if (!pipecb) return -1;

	PIPE_CB* pipe = (PIPE_CB*) pipecb;
	if (pipe->reader == NULL || pipe->writer == NULL) return -1;

	//Wait for space to write
	while (!can_write(pipe) && pipe->reader != NULL) {
		//Broadcast we are full
		kernel_broadcast(&pipe->has_data);
		kernel_wait(&pipe->has_space, SCHED_PIPE);
	}

	//copy data to BUFFER
	int chars_written;
	for (chars_written = 0; chars_written < n && can_write(pipe); chars_written++) {
		pipe->BUFFER[pipe->w_position] = buf[chars_written];
		pipe->w_position = (pipe->w_position + 1) % PIPE_BUFFER_SIZE;
	}
	
	//GET MY DATA
	kernel_broadcast(&pipe->has_data);
	return chars_written;
}

int pipe_read(void* pipecb, char *buf, unsigned int n) {
	if (!pipecb) return -1;
	PIPE_CB* pipe = (PIPE_CB*) pipecb;
	//We don't really need the writer to read
	if (pipe->reader == NULL) return -1;

	//Wait for data to read
	while (!can_read(pipe) && pipe->writer != NULL) {
		//Broadcast we are empty
		kernel_broadcast(&pipe->has_space);
		kernel_wait(&pipe->has_data, SCHED_PIPE);
	}

	//Get data from buf
	int chars_read;
	for (chars_read = 0; chars_read < n && can_read(pipe); chars_read++) {
		buf[chars_read] = pipe->BUFFER[pipe->r_position];
		pipe->r_position = (pipe->r_position + 1) % PIPE_BUFFER_SIZE;
	}

	//GIVE ME MORE DATA
	kernel_broadcast(&pipe->has_space);
	return chars_read;
}

int pipe_writer_close(void* pipecb) {
	PIPE_CB* pipe = (PIPE_CB*) pipecb;

	if (!pipe) return -1;

	pipe->writer = NULL;
	//If reader is also closed, we dont need the pipe
	//Else we need the current data to leave the pipe
	if (pipe->reader == NULL) {
		free(pipe);
		// pipe = NULL;
	} else {
		kernel_broadcast(&pipe->has_space);
	}
	return 0;
}

int pipe_reader_close(void* pipecb) {
	PIPE_CB* pipe = (PIPE_CB*) pipecb;

	if (!pipe) return -1;

	pipe->reader = NULL;
	//If writer is also closed, we dont need the pipe
	//Else we can still write
	if (pipe->writer == NULL) {
		free(pipe);
		// pipe = NULL;
	} else {
		kernel_broadcast(&pipe->has_space);
	}
	return 0;
}

/*The calls a reader can make*/
static file_ops reader_file_ops = {
	.Open = false_open_pipe,
	.Read = pipe_read,
	.Write = false_write,
	.Close = pipe_reader_close
};

/*The calls a writer can make*/
static file_ops writer_file_ops = {
	.Open = false_open_pipe,
	.Read = false_read,
	.Write = pipe_write,
	.Close = pipe_writer_close
};

/*Initialize and return a new pipe_cb*/
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

