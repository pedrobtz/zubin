/*
 * zubin/version.h -- the version of the zubin headers a unit is compiled against.
 *
 * zubin is header-only: a consumer carries its own copy of every function it
 * uses, so these macros describe the code compiled into the consumer, not a
 * library loaded at run time. Within major version 1 the promise is source
 * compatibility (design 4.4); there is no ABI.
 */
#ifndef ZUBIN_VERSION_H
#define ZUBIN_VERSION_H

#define ZUBIN_VERSION_MAJOR 0
#define ZUBIN_VERSION_MINOR 0
#define ZUBIN_VERSION_PATCH 0

#define ZUBIN_VERSION "0.0.0"

/* A single comparable number: 10000 * major + 100 * minor + patch. */
#define ZUBIN_VERSION_NUMBER \
    (ZUBIN_VERSION_MAJOR * 10000 + ZUBIN_VERSION_MINOR * 100 + ZUBIN_VERSION_PATCH)

#endif /* ZUBIN_VERSION_H */
