#ifndef PT100_H
#define PT100_H

#include "HeaderFiles.h"


#define PT100_TABLE_SIZE         12


#define PT100_SAMPLE_COUNT       8

void PT100_Init(void);

uint16_t PT100_ReadRaw(void);
float PT100_ReadAdcVoltage(void);
float PT100_ReadResistance(void);
float PT100_ReadTemperature(void);

void PT100_ReadAll(uint16_t *raw_out,
                   float *vadc_out,
                   float *r_out,
                   float *temp_out);

#endif
