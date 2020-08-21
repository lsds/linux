#pragma once
#include <asm-generic/signal.h>

struct ucontext;
void do_signal(struct pt_regs *regs);
void lkl_process_trap(int signr, struct ucontext *uctx);

struct thread_info;
struct ksignal_list_node;
struct ksignal;

extern void append_ksignal_node(struct thread_info *task, struct ksignal_list_node* node); 
extern void move_signals_to_task(void);
extern int get_next_ksignal(struct thread_info *task, struct ksignal* sig);
void send_current_signals(struct pt_regs *regs);




