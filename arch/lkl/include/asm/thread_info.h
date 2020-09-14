#ifndef _ASM_LKL_THREAD_INFO_H
#define _ASM_LKL_THREAD_INFO_H

#define THREAD_SIZE	       (4096)

#ifndef __ASSEMBLY__
#include <asm/types.h>
#include <asm/processor.h>
#include <asm/host_ops.h>

#include <linux/signal_types.h>
#include <linux/spinlock_types.h>

/*
 * Used to track a thread's pending signals.
 * See signal_list_head/tail below.
 */

struct ksignal_list_node
{
	struct ksignal sig;
	struct ksignal_list_node *next;	/* consider using the kernel lists, but they are doubly linked and clumsy in this simple case */
};

typedef struct {
	unsigned long seg;
} mm_segment_t;

struct thread_info {
	struct task_struct *task;
	unsigned long flags;
	int preempt_count;
	mm_segment_t addr_limit;
	struct lkl_sem *sched_sem;
	struct lkl_jmp_buf sched_jb;
	bool dead;
	lkl_thread_t tid;
	struct task_struct *prev_sched;
	unsigned long stackend;
	/* The return address from the currently executing syscall. Invalid when
	 * the thread is not executing a syscall. */
	void *syscall_ret;
	/* The task for any child that was created during syscall execution.  Only
	 * valid on return from a clone-family syscall. */
	struct task_struct *cloned_child;
	
	/* lock for the list below, the init is in the thread_info creation fn */
 	spinlock_t signal_list_lock;	
 	/* a linked list of pending signals, pushed onto here as they are detected in move_signals_to_task */
 	struct ksignal_list_node* signal_list_head;
 	struct ksignal_list_node* signal_list_tail;
};

#define INIT_THREAD_INFO(tsk)				\
{							\
	.task		= &tsk,				\
	.preempt_count	= INIT_PREEMPT_COUNT,		\
	.flags		= 0,				\
	.addr_limit	= KERNEL_DS,			\
}

/* how to get the thread information struct from C */
extern struct thread_info *_current_thread_info;
static inline struct thread_info *current_thread_info(void)
{
	return _current_thread_info;
}

/* thread information allocation */
unsigned long *alloc_thread_stack_node(struct task_struct *, int node);
void free_thread_stack(struct task_struct *tsk);

void threads_init(void);
void threads_cleanup(void);

#define TIF_SYSCALL_TRACE		0
#define TIF_NOTIFY_RESUME		1
#define TIF_SIGPENDING			2
#define TIF_NEED_RESCHED		3
#define TIF_RESTORE_SIGMASK		4
#define TIF_MEMDIE			5
#define TIF_NOHZ			6
#define TIF_SCHED_JB			7
#define TIF_HOST_THREAD			8
#define TIF_NO_TERMINATION		9 // Do not terminate LKL on exit
#define TIF_CLONED_HOST_THREAD		10 // This is a host thread created via a clone-family call.

#define __HAVE_THREAD_FUNCTIONS

#define task_thread_info(task)	((struct thread_info *)(task)->stack)
#define task_stack_page(task)	((task)->stack)
void setup_thread_stack(struct task_struct *p, struct task_struct *org);
#define end_of_stack(p) (&task_thread_info(p)->stackend)

#endif /* __ASSEMBLY__ */

#endif
