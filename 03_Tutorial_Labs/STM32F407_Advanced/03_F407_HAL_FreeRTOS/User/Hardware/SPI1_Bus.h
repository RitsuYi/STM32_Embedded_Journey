#ifndef SPI1_BUS_H
#define SPI1_BUS_H
#include <stdbool.h>

/* All future SPI1 clients, including Flash, must hold this lock from CS low
 * through transfer completion and CS high. Call Init from the GUI task. */
bool SPI1_Bus_Init(void);
bool SPI1_Bus_Lock(void);
void SPI1_Bus_Unlock(void);
#endif
