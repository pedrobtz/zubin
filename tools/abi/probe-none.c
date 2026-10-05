/* Includes everything and uses nothing: must compile without a single
   unused-function warning (design 4.5, 16.1). */
#include <zubin.h>

int zb_probe_none(void);
int zb_probe_none(void) { return 0; }
