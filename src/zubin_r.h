/* Declarations of the .Call entry points registered in init.c. */
#ifndef ZUBIN_R_H
#define ZUBIN_R_H

#define R_NO_REMAP
#include <R.h>
#include <Rinternals.h>

/* zubin_r.c */
SEXP zubin_info(void);

/* zubin_test.c */
SEXP zubin_test_status_string(SEXP codes);
SEXP zubin_test_rw(SEXP type, SEXP endian, SEXP bytes);
SEXP zubin_test_rw_write(SEXP type, SEXP endian, SEXP values);
SEXP zubin_test_cursor(SEXP bytes, SEXP plan);

#endif /* ZUBIN_R_H */
