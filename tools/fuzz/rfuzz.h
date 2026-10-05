/*
 * Shared by the fuzz targets that need R (fuzz_serialize, fuzz_sink): R is
 * embedded once, in LLVMFuzzerInitialize, and each input builds an R object
 * from its bytes. R's own signal handlers and C stack check are turned off,
 * since libFuzzer and ASan own both. R_HOME must be set in the environment.
 */
#ifndef ZB_RFUZZ_H
#define ZB_RFUZZ_H

#define R_NO_REMAP
#include <Rinternals.h>
#include <Rembedded.h>
#define R_INTERFACE_PTRS
#define CSTACK_DEFNS
#include <Rinterface.h>

#include "fuzz.h"
#include <zubin-r.h>

/* A cursor over the fuzz input that yields 0 once it is spent. */
typedef struct {
    const uint8_t *p;
    size_t n, i;
} zb_rfuzz_in;

int zb_rfuzz_init(void);
uint8_t zb_rfuzz_u8(zb_rfuzz_in *in);
/* An R object of depth at most `depth`, built from the input; protected
   once by the caller's PROTECT. */
SEXP zb_rfuzz_object(zb_rfuzz_in *in, int depth);

#endif
