
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"
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
	return (Tid_t) CURPTCB;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{ 
  /*Check to see thread exists*/
  rlnode* tempnode = rlist_find(&CURPROC->ptcb_list, (PTCB*) tid, NULL);
	if (tempnode == NULL) return -1;

  PTCB* joinedptcb = (PTCB*) tid;

  if (joinedptcb->detached || joinedptcb == CURPTCB) return -1;
  /*hawk tuah wait on that thang*/
  joinedptcb->refcount++;
  while(!(joinedptcb->exited || joinedptcb->detached)) {
    kernel_wait(&joinedptcb->exit_cv, SCHED_USER);
  }
  joinedptcb->refcount--;

  /*We can't go on if the thread is not exited*/
  if (joinedptcb->detached == 1){
    // kernel_broadcast(&joinedptcb->exit_cv);
    return -1;
  }
  /*return the exit value*/
  if (joinedptcb->exitval && exitval) (*exitval = joinedptcb->exitval);
  
  if(joinedptcb->refcount == 0) {
    rlist_remove(&joinedptcb->ptcb_list_node);
    // kernel_broadcast(&joinedptcb->exit_cv);
    free(joinedptcb);
  }
  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  /*Check if tid exists*/
  rlnode* node = rlist_find(&CURPROC->ptcb_list, (PTCB*) tid, NULL);
  if (node == NULL) return -1;

  PTCB* ptcb = node->ptcb;
  if (ptcb->exited == 1) return -1;
  /*clear its waitset*/
  kernel_broadcast(&ptcb->exit_cv);
  ptcb->refcount = 0;

  /*detach the thread*/
  ptcb->detached = 1;
  return 0;
}



/* Terminates the currently running thread*/
void kill_curr_thread(int exitval) {
  /*Steal his exitval and kill it (batman lore ptcb)*/
  PTCB* curptcb = CURPTCB;
  if (exitval) (curptcb->exitval = exitval);

  curptcb->exited = 1;

  /*Wake up the ones waiting*/
  kernel_broadcast(&curptcb->exit_cv);
}

/* Clean up the currently running process if it has no more threads*/
void clean_process() {
  PCB* curproc = CURPROC;

  if (get_pid(curproc) != 1) {
    /* Reparent any children of the exiting process to the 
       initial task */
    PCB* initpcb = get_pcb(1);
    while(!is_rlist_empty(& curproc->children_list)) {
      rlnode* child = rlist_pop_front(& curproc->children_list);
      child->pcb->parent = initpcb;
      rlist_push_front(& initpcb->children_list, child);
    }

    /* Add exited children to the initial task's exited list 
       and signal the initial task */
    if(!is_rlist_empty(& curproc->exited_list)) {
      rlist_append(& initpcb->exited_list, &curproc->exited_list);
      kernel_broadcast(& initpcb->child_exit);
    }

    /* Put me into my parent's exited list */
    rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
    kernel_broadcast(& curproc->parent->child_exit);
  }

  assert(is_rlist_empty(& curproc->children_list));
  assert(is_rlist_empty(& curproc->exited_list));

  /* 
    Do all the other cleanup we want here, close files etc. 
   */

  /* Release the args data */
  if(curproc->args) {
    free(curproc->args);
    curproc->args = NULL;
  }

  /* Clean up FIDT */
  for(int i=0;i<MAX_FILEID;i++) {
    if(curproc->FIDT[i] != NULL) {
      FCB_decref(curproc->FIDT[i]);
      curproc->FIDT[i] = NULL;
    }
  }

  while(!is_rlist_empty(&curproc->ptcb_list)) {
    PTCB* temp_ptcb = rlist_pop_front(&curproc->ptcb_list)->ptcb;
    if (temp_ptcb) free(temp_ptcb);
  }

  /* Disconnect my main_thread */
  curproc->main_thread = NULL;

  /* Now, mark the process as exited. */
  curproc->pstate = ZOMBIE;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  kill_curr_thread(exitval);
  CURPROC->thread_count--;

  if (CURPROC->thread_count == 0) {
    clean_process();
  }

  kernel_sleep(EXITED, SCHED_USER);
}