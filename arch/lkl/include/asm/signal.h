#pragma once
#include <asm-generic/signal.h>

struct ucontext;

struct thread_info;
struct ksignal_list_node;
struct ksignal;

void lkl_process_trap(int signr, struct ucontext *uctx);
 
/* capture pending signals and move them to a task specific list */
/* cpu lock must be held */
void move_signals_to_task(void);

/* send any signals targeting this task */
/* cpu lock must not be held */
void send_current_signals(struct ucontext *uctx);
