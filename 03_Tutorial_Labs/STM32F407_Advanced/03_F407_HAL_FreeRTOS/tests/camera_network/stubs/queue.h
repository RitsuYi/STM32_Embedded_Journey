#include "FreeRTOS.h"
QueueHandle_t xQueueCreateStatic(unsigned length,unsigned size,uint8_t *data,StaticQueue_t *control);
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t wait);
BaseType_t xQueueOverwrite(QueueHandle_t q,const void *item);
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t wait);
