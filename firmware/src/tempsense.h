#pragma once
#include "config.h"

// ============================================================
//  Pomiar temperatury grota - wspólny interfejs dla czujników
//  (wybór: TEMP_SENSOR w config.h)
//    MAX6675  - termopara typu K, własna kompensacja zimnego złącza
//    ADS1115  - pomiar napięcia termopary C245 (różnicowo A0-A1)
//               + kompensacja zimnego złącza (czujnik Pico albo NTC na A2)
// ============================================================

void  tsens_init(void);
// Start pomiaru (wywoływane po wyłączeniu grzałki i odczekaniu TC_SETTLE_MS)
void  tsens_start(int ch);
// Wynik po TSENS_CONV_MS [°C]; *err = 1 gdy brak grota / czujnika / zły odczyt
float tsens_read(int ch, uint8_t *err);
// Diagnostyka do konsoli (komenda "tc")
void  tsens_debug_print(void);
// Test linii I2C i skan adresów (komenda "i2c")
void  tsens_i2c_scan(void);
