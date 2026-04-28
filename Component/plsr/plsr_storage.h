#ifndef __PLSR_STORAGE_H__
#define __PLSR_STORAGE_H__

#include "plsr_types.h"

void plsr_storage_set_defaults(PlsrConfig *config, PlsrRecord *record);
PlsrResult plsr_storage_load(PlsrConfig *config, PlsrRecord *record);
PlsrResult plsr_storage_save(const PlsrConfig *config, const PlsrRecord *record);
void plsr_storage_mark_dirty(void);
void plsr_storage_task_100ms(const PlsrConfig *config, const PlsrRecord *record, bool allow_write);

#endif
