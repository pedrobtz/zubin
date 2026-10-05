#include <stddef.h>

#include <R_ext/Rdynload.h>
#include <R_ext/Visibility.h>

/* Every .Call entry point is listed here; nothing is registered with
   R_RegisterCCallable, because consumers include the headers instead
   (design 4). The table grows with each stage. */
static const R_CallMethodDef call_methods[] = {
    {NULL, NULL, 0}
};

/* The one exported symbol of zubin.so (design 15, 16.2). */
void attribute_visible R_init_zubin(DllInfo *dll)
{
    R_registerRoutines(dll, NULL, call_methods, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
    R_forceSymbols(dll, TRUE);
}
