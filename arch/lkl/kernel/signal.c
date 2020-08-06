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

void lkl_process_trap(int signr, struct ucontext *uctx)
{
    struct ksignal ksig;

    LKL_TRACE("enter\n");

    while (get_signal(&ksig)) {
        LKL_TRACE("ksig.sig=%d", ksig.sig);

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

    memset(&uc, 0, sizeof(uc));
    initialize_uctx(&uc, regs);
    while (get_signal(&ksig)) {
        LKL_TRACE("ksig.sig=%d", ksig.sig);

        /* Whee!  Actually deliver the signal.  */
        handle_signal(&ksig, &uc);
    }
}
