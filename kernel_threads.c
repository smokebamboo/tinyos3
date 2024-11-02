
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  /*Create a TCB and a PTCB and initialize them*/
  TCB* tcb = spawn_thread(CURPROC, start_thread);
  PTCB* ptcb = init_ptcb(task, argl, args);

  /*Connect ptcb and tcb together*/
  tcb->ptcb = ptcb;
  ptcb->tcb = tcb;

  /*
    Push the ptcb at the tail of the PTCB list of the current process
    and increment the counter of the amount of threads belonging to it
  */
  rlist_push_back(&CURPROC->ptcb_list, &ptcb->ptcb_list_node);
  CURPROC->thread_count++;
  /*START RUNNING*/
  wakeup(tcb);

  return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread();
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{ 
  /*Check to see thread exists*/
  rlnode* tempnode = rlist_find(&cur_thread()->owner_pcb->ptcb_list, (PTCB*) tid, NULL);
	if (tempnode == NULL) return -1;

  PTCB* joinedptcb = tempnode->ptcb;

  if (joinedptcb->detached || joinedptcb == CURPTCB) return -1;
  /*hawk tuah wait on that thang*/
  joinedptcb->refcount++;
  while(!(joinedptcb->exited || joinedptcb->detached)) {
    kernel_wait(&joinedptcb->exit_cv, SCHED_USER);
  }
  joinedptcb->refcount--;

  /*return the exit value*/
  if (joinedptcb->exitval) (*exitval = joinedptcb->exitval);

  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

}

