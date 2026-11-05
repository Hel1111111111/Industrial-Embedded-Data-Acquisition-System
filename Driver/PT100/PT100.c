#include "PT100.h"
#include <math.h>


#define PT100_R0_OHM              100.0f
#define PT100_LINEAR_COEF         0.385f
#define PT100_FLOAT_EPS           0.000001f
#define PT100_NEWTON_ITERATIONS   8U
#define PT100_STABLE_WRITE_COUNT  3U
#define PT100_CONFIG_DELAY_MS     25U
#define PT100_CONVERT_DELAY_MS    50U
#define PT100_READBACK_DELAY_MS   5U
#define PT100_INIT_DONE_DELAY_MS  50U

#define PT100_CVD_A               3.9083e-3f
#define PT100_CVD_B              -5.775e-7f
#define PT100_CVD_C              -4.183e-12f


typedef struct
{
    float voltage;
    float resistance;
} PT100_Point_t;


typedef struct
{
    float raw_voltage;
    float resistance;
    float temperature;
    uint16_t raw_code;
} PT100_Result_t;


static const PT100_Point_t s_pt100_points[PT100_TABLE_SIZE] =
{
    {0.901f, 100.00f},
    {0.968f, 107.79f},
    {1.034f, 115.54f},
    {1.100f, 123.24f},
    {1.162f, 130.90f},
    {1.223f, 138.51f},
    {0.731f,  80.60f},
    {0.909f,  82.50f},
    {1.013f, 113.00f},
    {1.030f, 115.00f},
    {1.302f, 150.00f},
    {1.322f, 154.00f}
};


static float PT100_AbsFloat(float value)
{
    return (value >= 0.0f) ? value : -value;
}


static float PT100_LinearMap(float input,
                             float in_min,
                             float in_max,
                             float out_min,
                             float out_max)
{
    float span;

    span = in_max - in_min;

    if(PT100_AbsFloat(span) < PT100_FLOAT_EPS)
    {
        return out_min;
    }

    return out_min + ((input - in_min) * (out_max - out_min)) / span;
}


static void PT100_LoadPointPair(uint8_t left_index,
                                float *v_left,
                                float *v_right,
                                float *r_left,
                                float *r_right)
{
    *v_left  = s_pt100_points[left_index].voltage;
    *v_right = s_pt100_points[left_index + 1U].voltage;
    *r_left  = s_pt100_points[left_index].resistance;
    *r_right = s_pt100_points[left_index + 1U].resistance;
}


static uint8_t PT100_FindVoltageSection(float voltage, uint8_t *section_index)
{
    uint8_t i;
    float left_v;
    float right_v;

    for(i = 0U; i < (PT100_TABLE_SIZE - 1U); i++)
    {
        left_v = s_pt100_points[i].voltage;
        right_v = s_pt100_points[i + 1U].voltage;

        if((voltage >= left_v) && (voltage <= right_v))
        {
            *section_index = i;
            return 1U;
        }
    }

    return 0U;
}


static float PT100_ConvertRawToVoltage(uint16_t raw)
{
    int16_t signed_code;

    signed_code = (int16_t)raw;

    return ((float)signed_code * PT100_ADC_PGA_RANGE_V) / PT100_ADC_FULL_SCALE;
}


static float PT100_ConvertVoltageToResistance(float voltage)
{
    uint8_t section;
    float v0;
    float v1;
    float r0;
    float r1;

    if(voltage <= s_pt100_points[0].voltage)
    {
        PT100_LoadPointPair(0U, &v0, &v1, &r0, &r1);
        return PT100_LinearMap(voltage, v0, v1, r0, r1);
    }

    if(voltage >= s_pt100_points[PT100_TABLE_SIZE - 1U].voltage)
    {
        PT100_LoadPointPair((uint8_t)(PT100_TABLE_SIZE - 2U),
                            &v0,
                            &v1,
                            &r0,
                            &r1);

        return PT100_LinearMap(voltage, v0, v1, r0, r1);
    }

    if(PT100_FindVoltageSection(voltage, &section) != 0U)
    {
        PT100_LoadPointPair(section, &v0, &v1, &r0, &r1);
        return PT100_LinearMap(voltage, v0, v1, r0, r1);
    }

    return 0.0f;
}


static float PT100_CalcResistanceFromTemp(float temp)
{
    float temp2;
    float temp3;

    temp2 = temp * temp;
    temp3 = temp2 * temp;

    return PT100_R0_OHM *
           (1.0f
            + PT100_CVD_A * temp
            + PT100_CVD_B * temp2
            + PT100_CVD_C * (temp - 100.0f) * temp3);
}


static float PT100_CalcResistanceDerivative(float temp)
{
    float temp2;
    float temp3;

    temp2 = temp * temp;
    temp3 = temp2 * temp;

    return PT100_R0_OHM *
           (PT100_CVD_A
            + 2.0f * PT100_CVD_B * temp
            + PT100_CVD_C * (4.0f * temp3 - 300.0f * temp2));
}


static float PT100_SolvePositiveTemp(float resistance)
{
    float delta;

    delta = PT100_CVD_A * PT100_CVD_A
          - 4.0f * PT100_CVD_B * (1.0f - resistance / PT100_R0_OHM);

    if(delta < 0.0f)
    {
        delta = 0.0f;
    }

    return (-PT100_CVD_A + sqrtf(delta)) / (2.0f * PT100_CVD_B);
}


static float PT100_SolveNegativeTemp(float resistance)
{
    uint8_t i;
    float temp;
    float estimate_r;
    float slope;

    temp = (resistance - PT100_R0_OHM) / PT100_LINEAR_COEF;

    for(i = 0U; i < PT100_NEWTON_ITERATIONS; i++)
    {
        estimate_r = PT100_CalcResistanceFromTemp(temp);
        slope = PT100_CalcResistanceDerivative(temp);

        if(PT100_AbsFloat(slope) < PT100_FLOAT_EPS)
        {
            break;
        }

        temp = temp - (estimate_r - resistance) / slope;
    }

    return temp;
}


static float PT100_ConvertResistanceToTemp(float resistance)
{
    if(resistance >= PT100_R0_OHM)
    {
        return PT100_SolvePositiveTemp(resistance);
    }

    return PT100_SolveNegativeTemp(resistance);
}


static void PT100_ApplyAdcConfigSeveralTimes(void)
{
    uint8_t index;

    for(index = 0U; index < PT100_STABLE_WRITE_COUNT; index++)
    {
        ad3344_init(AD3344_CONFIG);
        delay_1ms(PT100_CONFIG_DELAY_MS);
    }
}


static PT100_Result_t PT100_ReadSingleResult(void)
{
    PT100_Result_t result;

    result.raw_code = PT100_ReadRaw();
    result.raw_voltage = PT100_ConvertRawToVoltage(result.raw_code);
    result.resistance = PT100_ConvertVoltageToResistance(result.raw_voltage);
    result.temperature = PT100_ConvertResistanceToTemp(result.resistance);

    return result;
}


static float PT100_ReadAverageVoltage(void)
{
    uint8_t sample_index;
    uint16_t raw_code;
    float voltage_sum;

    voltage_sum = 0.0f;

    for(sample_index = 0U; sample_index < PT100_SAMPLE_COUNT; sample_index++)
    {
        raw_code = PT100_ReadRaw();
        voltage_sum += PT100_ConvertRawToVoltage(raw_code);
    }

    return voltage_sum / (float)PT100_SAMPLE_COUNT;
}


/*
 * Initialize PT100 measurement path and GD30AD3344 configuration.
 */
void PT100_Init(void)
{
    uint16_t readback_config;
    uint16_t readback_data;

    readback_config = 0U;
    readback_data = 0U;

    AD3344_CONFIG = AD3344_PT100_CONFIG;

    printf("AD3344_CONFIG=0x%04X\r\n", AD3344_CONFIG);

#if AD3344_USE_AIN3_EXT_REF
    printf("AD3344 reference mode: AIN3 external reference enabled\r\n");

    ad3344_process();
    ad3344_ExtRef();

    delay_1ms(10U);
#else
    printf("AD3344 reference mode: internal reference\r\n");
#endif

    PT100_ApplyAdcConfigSeveralTimes();

    readback_data = ad3344_read_data32(AD3344_CONFIG, &readback_config);
    delay_1ms(PT100_READBACK_DELAY_MS);

    readback_data = ad3344_read_data32(AD3344_CONFIG, &readback_config);

    printf("AD3344 readback data=0x%04X, config_readback=0x%04X\r\n",
           readback_data,
           readback_config);

    delay_1ms(PT100_INIT_DONE_DELAY_MS);
}


/*
 * Start one conversion and read one raw ADC code.
 */
uint16_t PT100_ReadRaw(void)
{
    uint16_t raw_code;

    AD3344_CONFIG = AD3344_PT100_CONFIG;

    ad3344_init(AD3344_CONFIG);

    delay_1ms(PT100_CONVERT_DELAY_MS);

    raw_code = ad3344_read_data16(AD3344_CONFIG);

    return raw_code;
}


/*
 * Read averaged ADC input voltage.
 */
float PT100_ReadAdcVoltage(void)
{
    return PT100_ReadAverageVoltage();
}


/*
 * Read PT100 resistance calculated from calibration table.
 */
float PT100_ReadResistance(void)
{
    float voltage;

    voltage = PT100_ReadAverageVoltage();

    return PT100_ConvertVoltageToResistance(voltage);
}


/*
 * Read PT100 temperature in Celsius.
 */
float PT100_ReadTemperature(void)
{
    float resistance;

    resistance = PT100_ReadResistance();

    return PT100_ConvertResistanceToTemp(resistance);
}


/*
 * Read raw code, voltage, resistance and temperature once.
 */
void PT100_ReadAll(uint16_t *raw_out,
                   float *vadc_out,
                   float *r_out,
                   float *temp_out)
{
    PT100_Result_t result;

    result = PT100_ReadSingleResult();

    if(raw_out != 0)
    {
        *raw_out = result.raw_code;
    }

    if(vadc_out != 0)
    {
        *vadc_out = result.raw_voltage;
    }

    if(r_out != 0)
    {
        *r_out = result.resistance;
    }

    if(temp_out != 0)
    {
        *temp_out = result.temperature;
    }
}