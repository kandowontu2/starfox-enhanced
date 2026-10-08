#include "starfox/render/leia_sr_api.h"
#include <stddef.h>
/* The optional status symbol must not change the existing V1 ABI. */
_Static_assert(offsetof(StarfoxLeiaSrApiV1,create)==8,"V1 factory offset changed");
_Static_assert(offsetof(StarfoxLeiaSrApiV1,weave)==8+sizeof(void*),"V1 weave offset changed");
_Static_assert(offsetof(StarfoxLeiaSrApiV1,destroy)==8+2*sizeof(void*),"V1 teardown offset changed");
_Static_assert(sizeof(StarfoxLeiaSrApiV1)==8+3*sizeof(void*),"V1 size changed");
int main(void) {return 0;}
