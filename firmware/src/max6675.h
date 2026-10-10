#pragma once
#include "config.h"

void  max6675_init(void);
// Przerywa bieżącą konwersję i startuje nową (CS low -> high).
// Wynik gotowy po MAX6675_CONV_MS.
void  max6675_restart(int ch);
// Odczyt temperatury [°C]; *error_flag = 1 gdy termopara otwarta / zły odczyt
float max6675_read(int ch, uint8_t *error_flag);
