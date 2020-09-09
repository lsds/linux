#ifndef _ASM_LKL_CPU_H
#define _ASM_LKL_CPU_H

int lkl_cpu_get(void);
void lkl_cpu_put(void);

#ifdef DEBUG
/* returns 1 if you have the cpu lock. */
int lkl_check_cpu_owner();
void lkl_print_cpu_state(const char *func_name);
#endif

int lkl_cpu_try_run_irq(int irq);
int lkl_cpu_init(void);
void lkl_cpu_shutdown(void);
void lkl_cpu_wait_shutdown(void);
void lkl_cpu_change_owner(lkl_thread_t owner);
void lkl_cpu_set_irqs_pending(void);

#endif /* _ASM_LKL_CPU_H */
