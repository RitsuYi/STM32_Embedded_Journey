#ifndef SYSTEM_TEST_FREERTOS_H
#define SYSTEM_TEST_FREERTOS_H
#include <stddef.h>
#include <stdint.h>
#define configTOTAL_HEAP_SIZE (24U*1024U)
typedef uint32_t StackType_t;
typedef unsigned UBaseType_t;
typedef struct {size_t xAvailableHeapSpaceInBytes,xSizeOfLargestFreeBlockInBytes,
    xMinimumEverFreeBytesRemaining;} HeapStats_t;
void vPortGetHeapStats(HeapStats_t *stats);
#endif
