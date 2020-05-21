/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_LKL_FUTEX_H
#define _ASM_LKL_FUTEX_H

#ifdef __KERNEL__

#include <linux/futex.h>
#include <asm/errno.h>

static inline int arch_futex_atomic_op_inuser(int op, int oparg, int *oval,
		u32 __user *uaddr)
{
	int oldval = 0, ret = 0;

	switch (op) {
	case FUTEX_OP_SET:
		__atomic_exchange(uaddr, &oparg, oval, __ATOMIC_SEQ_CST);
		break;
	case FUTEX_OP_ADD:
		__atomic_fetch_add(uaddr, oparg, __ATOMIC_SEQ_CST);
		break;
	case FUTEX_OP_OR:
		__atomic_fetch_or(uaddr, oparg, __ATOMIC_SEQ_CST);
		break;
	case FUTEX_OP_ANDN:
		__atomic_fetch_and(uaddr, ~oparg, __ATOMIC_SEQ_CST);
		break;
	case FUTEX_OP_XOR:
		__atomic_fetch_xor(uaddr, oparg, __ATOMIC_SEQ_CST);
		break;
	default:
		ret = -ENOSYS;
	}

	if (!ret)
		*oval = oldval;

	return ret;
}

static inline int futex_atomic_cmpxchg_inatomic(u32 *uval, u32 __user *uaddr,
						u32 oldval, u32 newval)
{
	if (uaddr == NULL) {
		return -EFAULT;
	} else {
		u32 v = oldval;
		int ret = __atomic_compare_exchange_n(uaddr, &v, newval, 0 /*weak*/, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);

		*uval = v;

		return ret;
	}
}

#endif
#endif
