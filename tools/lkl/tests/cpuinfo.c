#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <lkl.h>
#include <lkl_host.h>

#include "test.h"

LKL_TEST_CALL(start_kernel, lkl_start_kernel, 0, &lkl_host_ops,
	      "mem=16M loglevel=8");

static int lkl_test_cpuinfo(void)
{
	/* The buffer size must be a multiple of the read sizes */
#define BUF_SIZE (64 * 1024)
#define READ_SIZE_1 16
#define READ_SIZE_2 32

	int ret;
	int err;
	int f1, f2;
	static char buf1[BUF_SIZE], buf2[BUF_SIZE];
	long read1, read2;
	int done1, done2;
	long totalread1, totalread2;

	ret = TEST_FAILURE;

	err = lkl_mount_fs("proc");
	if (err < 0) {
		lkl_test_logf("failed to mount /proc\n");
		return TEST_FAILURE;
	}

	f1 = lkl_sys_open("/proc/cpuinfo", LKL_O_RDONLY, 0);
	if (f1 < 0) {
		lkl_test_logf("failed to open /proc/cpuinfo (f1): %s\n",
			      lkl_strerror(f1));
		return TEST_FAILURE;
	}

	f2 = lkl_sys_open("/proc/cpuinfo", LKL_O_RDONLY, 0);
	if (f2 < 0) {
		lkl_sys_close(f1);
		lkl_test_logf("failed to open /proc/cpuinfo (f2): %s\n",
			      lkl_strerror(f2));
		return TEST_FAILURE;
	}

	totalread1 = 0;
	totalread2 = 0;

	done1 = 0;
	done2 = 0;

	while (!done1 || !done2) {
		if (totalread1 >= (BUF_SIZE - READ_SIZE_1)) {
			lkl_test_logf("file is too big\n");
			totalread1 = 0;
			break;
		}

		if (totalread2 >= (BUF_SIZE - READ_SIZE_2)) {
			lkl_test_logf("file is too big\n");
			totalread2 = 0;
			break;
		}

		if (!done1) {
			read1 = lkl_sys_read(f1, &(buf1[totalread1]),
					     READ_SIZE_1);
			if (read1 <= 0)
				done1 = 1;
			totalread1 += read1;
		}

		if (!done2) {
			read2 = lkl_sys_read(f2, &(buf2[totalread2]),
					     READ_SIZE_2);
			if (read2 <= 0)
				done2 = 1;
			totalread2 += read2;
		}
	}

	lkl_sys_close(f1);
	lkl_sys_close(f2);

	if (totalread1 == 0) {
		lkl_test_logf("file is empty\n");
	} else if (totalread1 != totalread2) {
		lkl_test_logf("sizes don't match: %lu != %lu\n",
			      totalread1, totalread2);
	} else {
		if (memcmp(buf1, buf2, totalread1) == 0)
			ret = TEST_SUCCESS;
		else
			lkl_test_logf("read contents don't match");
	}

	return ret;
}

struct lkl_test tests[] = {
	LKL_TEST(start_kernel),
	LKL_TEST(cpuinfo),
};

int main(int argc, const char **argv)
{
	lkl_host_ops.print = lkl_test_log;

	return lkl_test_run(tests, sizeof(tests)/sizeof(struct lkl_test),
			    "cpuinfo");
}
