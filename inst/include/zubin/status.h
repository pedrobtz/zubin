/*
 * zubin/status.h -- the result model (design 7).
 *
 * ZB_OK is 0 and nothing is negative, so `if (st)` means "not success".
 * Functions that cannot fail return void; functions that can return a
 * zb_status and write results through out-parameters. On any failure the
 * inputs are unchanged: a cursor that returns ZB_ERR_EOF has not advanced, a
 * buffer that returns ZB_ERR_LIMIT holds exactly what it held.
 *
 * Enumerator values are permanent (design 4.4).
 *
 * Threads: every function here is pure, or reads and writes only the
 * buffer, cursor or arrays it is passed; no zubin header holds static or
 * global state (tools/check-headers refuses a static object under zubin/).
 * Any function may be called from any thread, as long as no two threads use
 * one buffer, cursor or output array at the same time (design 15).
 */
#ifndef ZUBIN_STATUS_H
#define ZUBIN_STATUS_H

#include "detail/portability.h"

typedef enum {
    ZB_OK          = 0,
    ZB_ERR_INVALID = 1,   /* a precondition on arguments failed */
    ZB_ERR_EOF     = 2,   /* fewer bytes than the read or the layout needs */
    ZB_ERR_RANGE   = 3,   /* a value does not fit the target type */
    ZB_ERR_MEMORY  = 4,   /* allocation failed, or size arithmetic would overflow */
    ZB_ERR_LIMIT   = 5,   /* the buffer's hard cap was reached, or a field array is full */
    ZB_ERR_SPEC    = 6,   /* a layout specification is malformed */
    ZB_ERR_NA      = 7    /* a value has no representation in the field (NA into u8) */
} zb_status;

/* The enumerator's name, such as "ZB_ERR_EOF"; never NULL. A value outside
   the enumeration gives "ZB_ERR_UNKNOWN". */
ZB_INLINE const char *zb_status_string(zb_status s)
{
    switch (s) {
    case ZB_OK:          return "ZB_OK";
    case ZB_ERR_INVALID: return "ZB_ERR_INVALID";
    case ZB_ERR_EOF:     return "ZB_ERR_EOF";
    case ZB_ERR_RANGE:   return "ZB_ERR_RANGE";
    case ZB_ERR_MEMORY:  return "ZB_ERR_MEMORY";
    case ZB_ERR_LIMIT:   return "ZB_ERR_LIMIT";
    case ZB_ERR_SPEC:    return "ZB_ERR_SPEC";
    case ZB_ERR_NA:      return "ZB_ERR_NA";
    }
    return "ZB_ERR_UNKNOWN";
}

#endif /* ZUBIN_STATUS_H */
