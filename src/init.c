#include <stddef.h>

#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>

#include "zubin_r.h"

#define CALLDEF(name, n) {#name, (DL_FUNC) &name, n}

/* Every .Call entry point is listed here; nothing is registered with
   R_RegisterCCallable, because consumers include the headers instead
   (design 4). */
static const R_CallMethodDef call_methods[] = {
    CALLDEF(zubin_info, 0),
    CALLDEF(zubin_builder_new, 2),
    CALLDEF(zubin_builder_state, 1),
    CALLDEF(zubin_builder_put_raw, 2),
    CALLDEF(zubin_builder_put_str, 3),
    CALLDEF(zubin_builder_reserve, 2),
    CALLDEF(zubin_builder_reset, 1),
    CALLDEF(zubin_builder_take, 2),
    CALLDEF(zubin_layout_parse, 3),
    CALLDEF(zubin_unpack, 9),
    CALLDEF(zubin_pack, 6),
    CALLDEF(zubin_builder_put_typed, 4),
    CALLDEF(zubin_hexdump, 4),
    CALLDEF(zubin_diff, 3),
    CALLDEF(zubin_test_status_string, 1),
    CALLDEF(zubin_test_rw, 3),
    CALLDEF(zubin_test_rw_write, 3),
    CALLDEF(zubin_test_cursor, 2),
    CALLDEF(zubin_test_live_buffers, 0),
    CALLDEF(zubin_test_buf_growth, 2),
    CALLDEF(zubin_test_buf_cap, 3),
    CALLDEF(zubin_test_buf_borrow, 2),
    CALLDEF(zubin_test_buf_put, 4),
    CALLDEF(zubin_test_buf_misc, 0),
    CALLDEF(zubin_test_put_then_error, 0),
    CALLDEF(zubin_test_put_loop, 2),
    CALLDEF(zubin_test_layout, 4),
    CALLDEF(zubin_test_struct_offsets, 0),
    CALLDEF(zubin_test_unpack_kernel, 4),
    {NULL, NULL, 0}
};

/* The one exported symbol of zubin.so (design 15, 16.2). */
void attribute_visible R_init_zubin(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, call_methods, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
    R_forceSymbols(dll, TRUE);
}
