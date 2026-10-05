/* The .Call wrappers behind R/ (design 15). They include <zubin.h> like any
   consumer, so CRAN's instrumented builds exercise the headers through the
   package's own tests. */
#include <stdio.h>

#include <zubin.h>
#include "zubin_r.h"

SEXP zubin_info(void)
{
    const char *names[] = {"version", "version_major", "version_minor", "version_patch",
                           "zufast", "endian", "compiler", "build", ""};
    const char *build_names[] = {"c_standard", "optimized", "ndebug", "fortify_source", ""};
    char v[32];
    SEXP build;
    SEXP out = PROTECT(Rf_mkNamed(VECSXP, names));
    SET_VECTOR_ELT(out, 0, Rf_mkString(ZUBIN_VERSION));
    SET_VECTOR_ELT(out, 1, Rf_ScalarInteger(ZUBIN_VERSION_MAJOR));
    SET_VECTOR_ELT(out, 2, Rf_ScalarInteger(ZUBIN_VERSION_MINOR));
    SET_VECTOR_ELT(out, 3, Rf_ScalarInteger(ZUBIN_VERSION_PATCH));
    SET_VECTOR_ELT(out, 4, Rf_mkString(ZUFAST_VERSION));
    SET_VECTOR_ELT(out, 5, Rf_mkString(zb_host_big_endian() ? "big" : "little"));
#if defined(__clang__)
    SET_VECTOR_ELT(out, 6, Rf_mkString("clang " __clang_version__));
#elif defined(__GNUC__)
    SET_VECTOR_ELT(out, 6, Rf_mkString("gcc " __VERSION__));
#else
    SET_VECTOR_ELT(out, 6, Rf_mkString("unknown"));
#endif
    /* The flags that change the generated code, as the preprocessor saw them
       when this file was compiled. */
    build = Rf_mkNamed(STRSXP, build_names);
    SET_VECTOR_ELT(out, 7, build);
#if defined(__STDC_VERSION__)
    snprintf(v, sizeof v, "%ld", (long)__STDC_VERSION__);
#else
    snprintf(v, sizeof v, "C89");
#endif
    SET_STRING_ELT(build, 0, Rf_mkChar(v));
#if defined(__OPTIMIZE__)
    SET_STRING_ELT(build, 1, Rf_mkChar("true"));
#else
    SET_STRING_ELT(build, 1, Rf_mkChar("false"));
#endif
#if defined(NDEBUG)
    SET_STRING_ELT(build, 2, Rf_mkChar("true"));
#else
    SET_STRING_ELT(build, 2, Rf_mkChar("false"));
#endif
#if defined(_FORTIFY_SOURCE)
    snprintf(v, sizeof v, "%d", (int)_FORTIFY_SOURCE);
#else
    snprintf(v, sizeof v, "0");
#endif
    SET_STRING_ELT(build, 3, Rf_mkChar(v));
    UNPROTECT(1);
    return out;
}
