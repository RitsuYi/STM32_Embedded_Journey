#ifndef GUI_DEMO_H
#define GUI_DEMO_H
#include "instrument/instrument_ui.h"
void GUI_DemoInit(MeasurementPage *page, InstrumentModel *model);
bool GUI_DemoCalibration(void *context, uint32_t profile, InstrumentPoint points[4],
                         InstrumentPoint *verification, uint32_t *expected);
#endif
