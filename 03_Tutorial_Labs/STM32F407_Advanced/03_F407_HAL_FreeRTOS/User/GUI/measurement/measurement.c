#include "measurement.h"
#include <stdio.h>

bool measurement_result_check(const MeasurementResult * r)
{
    if(!r || (r->geometry != MEASUREMENT_RING && r->geometry != MEASUREMENT_RECTANGLE)) return false;
    /* Some embedded compilers use an unsigned underlying type for this enum.
     * One unsigned range check rejects both negative and oversized values. */
    if((unsigned)r->status > (unsigned)MEASUREMENT_PENDING) return false;
    /* Bound arithmetic and reject nonsensical geometry before calculating pixels. */
    if(r->primary_cmm <= 0 || r->primary_cmm > 1000000 ||
       r->secondary_cmm <= 0 || r->secondary_cmm > 1000000 || r->residual_cmm > 1000000) return false;
    return r->geometry != MEASUREMENT_RING || r->secondary_cmm < r->primary_cmm;
}

void measurement_format_cmm(char * buffer, size_t capacity, uint32_t value)
{
    if(buffer && capacity) snprintf(buffer, capacity, "%lu.%02lu",
                                   (unsigned long)(value / 100), (unsigned long)(value % 100));
}
