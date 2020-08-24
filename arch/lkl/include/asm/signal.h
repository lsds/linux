#pragma once
#include <asm-generic/signal.h>

struct ucontext;
struct thread_info;
struct ksignal_list_node;
struct ksignal;

void do_signal(struct pt_regs *regs);
void lkl_process_trap(int signr, struct ucontext *uctx);

extern void append_ksignal_node(struct thread_info *task, struct ksignal_list_node* node); 
extern void move_signals_to_task(void);
extern int get_next_ksignal(struct thread_info *task, struct ksignal* sig);

void initialize_uctx(struct ucontext *uctx, const struct pt_regs *regs);
void send_current_signals(struct ucontext *uctx);




