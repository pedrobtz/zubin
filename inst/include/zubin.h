/*
 * zubin.h -- umbrella header: includes every zubin area header.
 *
 * A consumer declares `LinkingTo: zubin, zufast` (both: LinkingTo is not
 * transitive, and these headers include zufast's) and nothing else, then
 *
 *     #include <zubin.h>            everything
 *     #include <zubin/cursor.h>     or one area at a time
 *
 * Every function is static inline: there is nothing to link and no
 * implementation macro to define. The R glue, <zubin-r.h>, is not included
 * here: it is the only header that includes R's. See design 4 and 5.
 *
 * Threads: every function here is pure, or reads and writes only the
 * buffer, cursor or arrays it is passed; no zubin header holds static or
 * global state (tools/check-headers refuses a static object under zubin/).
 * Any function may be called from any thread, as long as no two threads use
 * one buffer, cursor or output array at the same time (design 15).
 */
#ifndef ZUBIN_H
#define ZUBIN_H

#include "zubin/version.h"
#include "zubin/status.h"
#include "zubin/rw.h"
#include "zubin/buf.h"
#include "zubin/cursor.h"
#include "zubin/layout.h"
#include <zufast/utf8.h>

#endif /* ZUBIN_H */
