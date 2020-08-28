#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/ptrace.h>
#include <asm-generic/ucontext.h>

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
    ksig->ka.sa.sa_handler(ksig->sig, (void*)&ksig->info, (void*)uctx);
}

/*
    While you might think this should call move_signals_to_task, send_current_signals
    it is different here in that only a specific signal is being requested.
    This is the sync signal case and that signal will have just been forced into the
    regular task queue of signals so the pre-existing code will work. See lkl_do_trap

    Up for debate though is whether it would be better to either directly invoke
    the handler in lkl_do_trap or here to have versions of move_signals_to_task, send_current_signals
    that operate by scanning the list for signr, the given signal.
*/

void lkl_process_trap(int signr, struct ucontext *uctx)
{
    struct ksignal ksig;

    LKL_TRACE("enter\n");

    while (get_signal(&ksig)) {
        LKL_TRACE("ksig.sig=%d", ksig.sig);

        /* Handle required signal */
        if(signr == ksig.sig)
        {
            LKL_TRACE("trap task %p %d\n", current, signr);
            handle_signal(&ksig, uctx);
            break;
        }
    }
}

/*
    Walk to the end of the list and make it point at the new node
*/


void append_ksignal_node(struct thread_info *task, struct ksignal_list_node* node)
{
    volatile struct ksignal_list_node **node_ptr;
    spin_lock(&task->signal_list_lock);
    node_ptr = &task->signal_list;

    while (*node_ptr) {
        volatile struct ksignal_list_node *next_node = *node_ptr;
        node_ptr = &(next_node->next);
    }
        
    *node_ptr = node;
    spin_unlock(&task->signal_list_lock);
}    

// probably needs to be called with the cpu lock
void move_signals_to_task(void)
{
    struct ksignal ksig;    // place to hold retrieved signal
    // get the lkl local version of the current task, so we can store the signals
    // in a list hanging off it.
    struct thread_info *current_task;
    current_task = task_thread_info(current); 
    while (get_signal(&ksig)) {
<<<<<<< HEAD
    	struct ksignal_list_node* node = kmalloc(sizeof(struct ksignal_list_node), GFP_KERNEL); // may sleep, is that ok?       
        if (node == NULL) {
            lkl_bug("kmalloc returned NULL");
        }
        LKL_TRACE("Appending task %p signal %d\n", current, ksig.sig);
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
    LKL_TRACE("Fetching task %p signal %d\n", current, sig->sig);
    kfree(node);
    return 1;
}    

// Must be called without the cpu lock
void send_current_signals(struct ucontext *uctx)
{
    struct thread_info *current_task = task_thread_info(current);
    struct ksignal ksig;
    
    struct ucontext uc;
    
    if (uctx == NULL) {
        uctx = &uc;
        memset(uctx, 0, sizeof(uc));
        initialize_uctx(uctx, NULL);
    }

    LKL_TRACE("enter\n");
    
    while (get_next_ksignal(current_task, &ksig)) {
    	LKL_TRACE("ksig.sig=%d", ksig.sig);
        /* Actually deliver the signal.  */
        handle_signal(&ksig, uctx); 
    }
}



