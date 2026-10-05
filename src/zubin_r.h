/* Declarations of the .Call entry points registered in init.c. */
#ifndef ZUBIN_R_H
#define ZUBIN_R_H

#define R_NO_REMAP
#include <R.h>
#include <Rinternals.h>

/* Every buffer the glue creates is counted, so test-lifetime.R can prove
   that an error or an interrupt leaks none (design 16.3). */
extern long zubin_int_live_buffers;
#define ZB_INT_R_ON_NEW()  (zubin_int_live_buffers++)
#define ZB_INT_R_ON_FREE() (zubin_int_live_buffers--)
#include <zubin.h>
#include <zubin/layout.h>
#include <zubin-r.h>

/* zubin_r.c: shared with the harness */
SEXP zubin_int_status(zb_status st, R_xlen_t index);
SEXP zubin_int_failure(const char *name, R_xlen_t index, int field);
int zubin_int_size(SEXP x, size_t *out);
zb_status zubin_int_parse(SEXP spec, int big, int align, uint32_t max_fields,
                          zb_layout *out, size_t *err_pos);

/* zubin_r.c */
SEXP zubin_info(void);
SEXP zubin_builder_new(SEXP reserve, SEXP max);
SEXP zubin_builder_state(SEXP ptr);
SEXP zubin_builder_put_raw(SEXP ptr, SEXP x);
SEXP zubin_builder_put_str(SEXP ptr, SEXP x, SEXP width);
SEXP zubin_builder_reserve(SEXP ptr, SEXP n);
SEXP zubin_builder_reset(SEXP ptr);
SEXP zubin_builder_take(SEXP ptr, SEXP reset);
SEXP zubin_layout_parse(SEXP spec, SEXP big, SEXP align);
SEXP zubin_unpack(SEXP x, SEXP spec, SEXP align, SEXP offset, SEXP n, SEXP stride,
                  SEXP int64, SEXP allow_na, SEXP encoding);

/* zubin_test.c */
SEXP zubin_test_status_string(SEXP codes);
SEXP zubin_test_rw(SEXP type, SEXP endian, SEXP bytes);
SEXP zubin_test_rw_write(SEXP type, SEXP endian, SEXP values);
SEXP zubin_test_cursor(SEXP bytes, SEXP plan);
SEXP zubin_test_live_buffers(void);
SEXP zubin_test_buf_growth(SEXP chunk, SEXP total);
SEXP zubin_test_buf_cap(SEXP reserve, SEXP max, SEXP puts);
SEXP zubin_test_buf_borrow(SEXP bytes, SEXP puts);
SEXP zubin_test_buf_put(SEXP type, SEXP endian, SEXP values, SEXP vectorised);
SEXP zubin_test_buf_misc(void);
SEXP zubin_test_put_then_error(void);
SEXP zubin_test_put_loop(SEXP chunk, SEXP times);
SEXP zubin_test_layout(SEXP spec, SEXP big, SEXP align, SEXP max_fields);
SEXP zubin_test_struct_offsets(void);
SEXP zubin_test_unpack_kernel(SEXP bytes, SEXP spec, SEXP n, SEXP stride);

#endif /* ZUBIN_R_H */
