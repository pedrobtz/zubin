#ifndef ZUBINTEST_H
#define ZUBINTEST_H
#define R_NO_REMAP
#include <R.h>
#include <Rinternals.h>
SEXP zt_unpack(SEXP spec, SEXP x);    /* layout.c */
SEXP zt_header(SEXP x);               /* cursor.c */
SEXP zt_build(SEXP values, SEXP tail); /* buffer.c */
SEXP zt_serial(SEXP x, SEXP cut);      /* serial.c */
SEXP zt_rdz(SEXP x, SEXP corrupt);     /* rdz.c */
#endif
