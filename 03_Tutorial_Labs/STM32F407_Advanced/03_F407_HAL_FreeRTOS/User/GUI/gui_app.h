#ifndef GUI_APP_H
#define GUI_APP_H
#include "instrument/instrument_ui.h"

/* Persistence is optional. A future Flash implementation must report durable
 * success only after write/verification; NULL ops means volatile RAM only. */
typedef struct {
    bool (*load)(void *context, InstrumentModel *model);
    bool (*save)(void *context, const InstrumentModel *model);
    void *context;
    const char *name;
} GUI_StorageOps;
void GUI_AppInit(const GUI_StorageOps *storage);
void GUI_AppKey(unsigned key);
void GUI_AppProcess(void);
#endif
