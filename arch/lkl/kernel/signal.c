#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/ptrace.h>
#include <linux/signal_types.h>
#include <linux/string.h>
#include <asm-generic/ucontext.h>
#include <asm/thread_info.h>

static void initialize_uctx(struct ucontext *uctx, const struct pt_regs *regs)
{
    if(regs)
    {
        uctx->uc_mcontext.rax = regs->rax;
        uctx->uc_mcontext.rbx = regs->rbx;
        uctx->uc_mcontext.rcx = regs->rcx;
        uctx->uc_mcontext.rdx = regs->rdx;
        uctx->uc_mcontext.rbp = regs->rbp;
        uctx->uc_mcontext.rsp = regs->rsp;
        uctx->uc_mcontext.rdi = regs->rdi;
        uctx->uc_mcontext.rsi = regs->rsi;
        uctx->uc_mcontext.r8  = regs->r8;
        uctx->uc_mcontext.r9  = regs->r9;
        uctx->uc_mcontext.r10 = regs->r10;
        uctx->uc_mcontext.r11 = regs->r11;
        uctx->uc_mcontext.r12 = regs->r12;
        uctx->uc_mcontext.r13 = regs->r13;
        uctx->uc_mcontext.r14 = regs->r14;
        uctx->uc_mcontext.r15 = regs->r15;
        uctx->uc_mcontext.rip = regs->rip;
    }
    return;
}

/* Function to invoke the signal handler of LKL application. This works for 
 * sig handler installed using sigaction or signal API. This will remove the
 * overhead of injecting the stack frame to pass the user context to user
 * space application (could lead to inclusion of ARCH specific code)
 */
static void handle_signal(struct ksignal *ksig, struct ucontext *uctx)
{
    
    lkl_printf("%s: line %d - signal %d\n", __func__, __LINE__, ksig->sig);
    ksig->ka.sa.sa_handler(ksig->sig, (void*)&ksig->info, (void*)uctx);
}



/* see thread_info.h for this:
struct ksignal_list_node
{
	ksignal sig;
	ksignal_list_node *next;
};

*/

/*
    Walk to the end of the list and make it point at the new node
    may replace with the proper linux lists.
*/

void append_ksignal_node(struct thread_info *task, struct ksignal_list_node* node)
{
    struct ksignal_list_node **node_ptr;
    spin_lock(&task->signal_list_lock);
    node_ptr = &task->signal_list;

    while (*node_ptr) {
        struct ksignal_list_node *next_node = *node_ptr;
        node_ptr = &(next_node->next);
    }
        
    *node_ptr = node;
    spin_unlock(&task->signal_list_lock);
}    

void move_signals_to_task(void)
{
	struct ksignal ksig;    // place to hold retrieved signal
    // get the lkl local version of the current task, so we can store the signals
    // in a list hanging off it.
    struct thread_info *current_task;
    current_task = task_thread_info(current);   // avoid annoying warning warning: ISO C90 forbids mixed declarations and code [-Wdeclaration-after-statement]  
	while (get_signal(&ksig)) {
		struct ksignal_list_node* node = kmalloc(sizeof(struct ksignal_list_node), GFP_KERNEL); // may sleep, is that ok?
        
        lkl_printf("%s: %d\n", __func__, __LINE__);
        if (node == NULL) {
            lkl_printf("kmalloc returned NULL");
        }
        LKL_TRACE("appending ksig.sig=", ksig.sig);
        memcpy(&node->sig, &ksig, sizeof(ksig));
        node->next = NULL;
        append_ksignal_node(current_task, node);
	}
}

int get_next_ksignal(struct thread_info *task, struct ksignal* sig)
{
    struct ksignal_list_node *next;
    struct ksignal_list_node *node;

    spin_lock(&task->signal_list_lock);
    node = task->signal_list;

    if (!node) {
        spin_unlock(&task->signal_list_lock);
        return 0; // no pending signals
    }

    next = node->next;
    task->signal_list = next; // drop the head
    
    spin_unlock(&task->signal_list_lock);

    memcpy(sig, &node->sig, sizeof(*sig)); // copy the signal back to the caller
    kfree(node);
    return 1;
}    

void send_current_signals(struct pt_regs *regs)
{
    struct thread_info *current_task = task_thread_info(current);
    struct ksignal ksig;

    LKL_TRACE("enter\n");
    
    //lkl_printf("%s: %d\n", __func__, __LINE__);
    while (get_next_ksignal(current_task, &ksig)) {
        // usually there is nothing to send so only set up the
        // registers if there is something to do.
        struct ucontext uc;
        memset(&uc, 0, sizeof(uc));
        initialize_uctx(&uc, regs);
        
        lkl_printf("%s: %d\n", __func__, __LINE__);
        LKL_TRACE("ksig.sig=", ksig.sig);

        /* Whee!  Actually deliver the signal.  */
        handle_signal(&ksig, &uc); 
    }
}



void lkl_process_trap(int signr, struct ucontext *uctx)
{
    struct ksignal ksig;

    LKL_TRACE("enter\n");

    while (get_signal(&ksig)) {
        LKL_TRACE("ksig.sig=", ksig.sig);

        /* Handle required signal */
        if(signr == ksig.sig)
        {
            handle_signal(&ksig, uctx);
            break;
        }
    }
}

void do_signal(struct pt_regs *regs)
{
    struct ksignal ksig;
    struct ucontext uc;

    LKL_TRACE("enter\n");
    move_signals_to_task();
  // turns out sending to the appropriate thread here may explode as we switch  send_current_signals(regs);
#if 0
    memset(&uc, 0, sizeof(uc));
    initialize_uctx(&uc, regs);
    while (get_signal(&ksig)) {
        LKL_TRACE("ksig.sig=", ksig.sig);

        /* Whee!  Actually deliver the signal.  */
        handle_signal(&ksig, &uc);
    }
#endif
}





