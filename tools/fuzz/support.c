/* The non-harness source each fuzz target links. */
#include "fuzz.h"

int zb_fuzz_support(void) { return ZUBIN_VERSION_NUMBER; }
