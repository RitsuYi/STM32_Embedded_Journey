#include "SPI1_Bus.h"
#include "FreeRTOS.h"
#include "semphr.h"

static StaticSemaphore_t mutex_storage;
static SemaphoreHandle_t mutex;

bool SPI1_Bus_Init(void)
{
    if(!mutex) mutex = xSemaphoreCreateRecursiveMutexStatic(&mutex_storage);
    return mutex != NULL;
}
bool SPI1_Bus_Lock(void)
{
    return mutex && xSemaphoreTakeRecursive(mutex, portMAX_DELAY) == pdTRUE;
}
void SPI1_Bus_Unlock(void)
{
    if(mutex) (void)xSemaphoreGiveRecursive(mutex);
}
