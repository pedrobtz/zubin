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
 */
#ifndef ZUBIN_H
#define ZUBIN_H

#include "zubin/version.h"
#include "zubin/status.h"
#include "zubin/rw.h"
#include "zubin/cursor.h"
#include <zufast/utf8.h>

#endif /* ZUBIN_H */
