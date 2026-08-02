#pragma once

void storage_save(void);      // Settings + Hot-Datensatz
void storage_save_hot(void);  // nur Trip/Odo/Wh-pro-km (eine Page)
void storage_load(void);
void storage_handler(void);

#if STORAGE_TYPE == STORAGE_FRAM
  void storage_test(void);
#endif
