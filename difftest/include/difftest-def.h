#ifndef __DIFFTEST_DEF_H__
#define __DIFFTEST_DEF_H__

#include <stdint.h>

enum { DIFFTEST_TO_DUT, DIFFTEST_TO_REF };

# define DIFFTEST_REG_SIZE (sizeof(uint64_t) * (32 + 32 + 1 + 6 + 11 + 1))

#endif