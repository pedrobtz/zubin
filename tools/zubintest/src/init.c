#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>
#include "zubintest.h"

static const R_CallMethodDef calls[] = {
    {"zt_unpack", (DL_FUNC) &zt_unpack, 2},
    {"zt_header", (DL_FUNC) &zt_header, 1},
    {"zt_build", (DL_FUNC) &zt_build, 2},
    {NULL, NULL, 0}
};

void attribute_visible R_init_zubintest(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, calls, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
    R_forceSymbols(dll, TRUE);
}
