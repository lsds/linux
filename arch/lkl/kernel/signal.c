#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/ptrace.h>
#include <asm-generic/ucontext.h>
#include <asm/cpu.h>
#include <asm/host_ops.h>

/* Disable temporary test prints - will be removed completely before the PR is merged but are of use now.*/
#define VERBOSE 0

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

/* MUST be called owning the lock */
static void handle_signal(struct ksignal *ksig, struct ucontext *uctx)
{
    lkl_thread_t self;
    /*
        The cpu lock must have been taken before we get here
    */
    int owned_by_self = lkl_check_cpu_owner(__func__);
    if (!owned_by_self)
        lkl_bug("%s expected the cpu to be locked\n", __func__);

    if (VERBOSE && ksig->sig != SIGUSR1 && ksig->sig != SIGUSR2)
        printk("signal %d\n", ksig->sig);

	/*
        Give up the cpu lock while we invoke the signal (David made me do it :))
    */
    
    lkl_cpu_put();

    /* In case it was recursively locked */
    owned_by_self = lkl_check_cpu_owner(__func__);
    if (owned_by_self)
        lkl_bug("%s expected the cpu to be unlocked or locked by another thread\n", __func__);

    /* Get the current thread before we invoke */
    self = lkl_ops->thread_self();

    ksig->ka.sa.sa_handler(ksig->sig, (void*)&ksig->info, (void*)uctx);

    /* 
        Check if the apparent current thread changes while processing the signal.
        Should not happen.
    */

	if (!lkl_ops->thread_equal(self, lkl_ops->thread_self())) {
        lkl_bug("confused about identity sig %u\n", ksig->sig);
    }

    /* take back the lock */
	lkl_cpu_get();
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

/* This is always called while owning the cpu */

void lkl_process_trap(int signr, struct ucontext *uctx)
{
    struct ksignal ksig;

    int ok = lkl_check_cpu_owner(__func__);
    if (!ok)
        lkl_bug("%s expected the cpu to be locked\n", __func__);

    LKL_TRACE("enter\n");

/* TODO - use a scheme like/refactored from the async signal case ie move/send */
    while (get_signal(&ksig)) {
        LKL_TRACE("ksig.sig=%d", ksig.sig);
        printk("trap ksig.sig=%d", ksig.sig);

        /* Handle required signal */
        if(signr == ksig.sig)
        {
            LKL_TRACE("trap task %p %d\n", current, signr);
            printk("trap task %p %d\n", current, signr);
            handle_signal(&ksig, uctx);
            break;
        }
    }
}

/*
   Get the end of the list and make it point at the new node,
   if there were no nodes before then bot head and tail point at this one node
*/

static void append_ksignal_node(struct thread_info *task, struct ksignal_list_node* node)
{
    struct ksignal_list_node *ptr;
    spin_lock(&task->signal_list_lock);
    ptr = task->signal_list_tail;
    if (ptr == NULL) { // list is empty
        task->signal_list_head = node;
        task->signal_list_tail = node;
    } else {
        ptr->next = node;
    }

    spin_unlock(&task->signal_list_lock);
}    

// needs to be called with the cpu lock held
void move_signals_to_task(void)
{
    struct ksignal ksig;    // place to hold retrieved signal
    // get the lkl local version of the current task, so we can store the signals
    // in a list hanging off it.
    struct thread_info *current_task;

    /* Lock the cpu while we rely on the task structures */
    lkl_cpu_get();
    current_task = task_thread_info(current); 
    while (get_signal(&ksig)) {
    	struct ksignal_list_node* node = kmalloc(sizeof(struct ksignal_list_node), GFP_KERNEL); // may sleep, is that ok?       
        if (node == NULL) {
            lkl_bug("Unable to allocate memory to hold signal state.");
        }
        LKL_TRACE("Appending task %p signal %d\n", current, ksig.sig);
        memcpy(&node->sig, &ksig, sizeof(ksig));
        node->next = NULL;
        append_ksignal_node(current_task, node);
    }
    /* give back the cpu */
    lkl_cpu_put();
}

static int get_next_ksignal(struct thread_info *task, struct ksignal* sig)
{
    struct ksignal_list_node *next;
    struct ksignal_list_node *node;

    spin_lock(&task->signal_list_lock);
    node = task->signal_list_head;

    if (!node) {
        spin_unlock(&task->signal_list_lock);
        return 0; // no pending signals
    }

    next = node->next;
    task->signal_list_head = next;      // drop the head
    if (next == NULL)                   // was that the last node?
        task->signal_list_tail = NULL;  // if so there is now no tail
        
    
    spin_unlock(&task->signal_list_lock);
    
    memcpy(sig, &node->sig, sizeof(*sig)); // copy the signal back to the caller
    LKL_TRACE("Fetching task %p signal %d\n", current, sig->sig);
    kfree(node);
    return 1;
}    

/*
    Must be called WITH the cpu lock held.
    The signal invoke code drops and retakes the lock.
*/

void send_current_signals(struct ucontext *uctx)
{
    struct ksignal ksig;
    struct ucontext uc;
    int sent_count = 0;
    
    if (uctx == NULL) {
        uctx = &uc;
        memset(uctx, 0, sizeof(uc));
        initialize_uctx(uctx, NULL);
    }

    LKL_TRACE("enter\n");
    
    if (VERBOSE)
        lkl_print_cpu_state("send_current_signals start");

    while (get_next_ksignal(task_thread_info(current), &ksig)) {
        LKL_TRACE("ksig.sig=%d", ksig.sig);
        
        /* Note to PR reviewers. This commit is essentially a backup and has a lot
           of debug code, as below, which knits with my tests */

        if (VERBOSE && ksig.sig != SIGUSR1 && ksig.sig != SIGUSR2)
            printk("sending %d\n", ksig.sig);

        if (VERBOSE && ksig.sig != SIGUSR1 && ksig.sig != SIGUSR2)
            lkl_print_cpu_state("send_current_signals before");
        
        /*
            Actually deliver the signal.
            Note that this might long jump away or otherwise never come back.
        */

        handle_signal(&ksig, uctx); 

        if (VERBOSE && ksig.sig != SIGUSR1 && ksig.sig != SIGUSR2)
            lkl_print_cpu_state("send_current_signals after");

        sent_count++;
    }

    if (VERBOSE && sent_count == 0)
        lkl_print_cpu_state("send_current_signals end none");
    if (VERBOSE && sent_count != 0)
        lkl_print_cpu_state("send_current_signals end some");
}
