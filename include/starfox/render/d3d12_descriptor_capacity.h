#ifndef STARFOX_D3D12_DESCRIPTOR_CAPACITY_H
#define STARFOX_D3D12_DESCRIPTOR_CAPACITY_H
#include <stdint.h>
/* Reserve whole tables, not their first slot. Subtraction cannot wrap. */
static inline int Starfox_D3D12DescriptorReservationNeeded(uint32_t cursor,
                                                         uint32_t capacity,
                                                         uint32_t requested)
{
    return cursor > capacity || requested > capacity - cursor;
}
#endif
