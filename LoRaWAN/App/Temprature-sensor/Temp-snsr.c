/*
 * light.c
 *
 *  Created on: Dec 2, 2022
 *      Author: KMG
 */
#include "Temp-snsr.h"
#include "stdio.h"
#include "sht2x_for_stm32_hal.h"

/* USER CODE BEGIN Macros */
#define REF_ADC_VAL     1430.0 //Reference voltage for battery percentage read
#define HIGHL_TRACE  //Command this line on release code
/* USER CODE BEGIN Macros */

/* USER CODE BEGIN PV */
uint8_t     lightAck = 0 ;
uint64_t    TxDTT    = 0 ;

float TempratureVal  = 0.0;
float HummidityVal   = 0.0;

char printBuff[80]     = {0};

/* USER CODE END PV */

float Floatreturn(float integer, uint8_t decimal);

void SensorDataRead(void)
{
	float Temp         = 0;
	float rh		   = 0;

	Temp = SHT2x_GetTemperature(SHT2x_READ_TEMP_HOLD);
	rh = SHT2x_GetRelativeHumidity(SHT2x_READ_RH_HOLD);
	TempratureVal = Floatreturn(SHT2x_GetInteger(Temp),SHT2x_GetDecimal(Temp,2));
	HummidityVal = Floatreturn(SHT2x_GetInteger(rh),SHT2x_GetDecimal(rh,2));

#ifdef HIGHL_TRACE
	sprintf(printBuff,"====>Temprature : %.2f C \r\n====>Humidity : %.2f rh \r\n",TempratureVal,HummidityVal);
	APP_LOG(TS_OFF, VLEVEL_H, "\r\n%s\r\n",printBuff);
#else
	APP_LOG(TS_OFF, VLEVEL_H, "\r\n----Device in boot mode---- \r\n");
#endif

}

float Floatreturn(float integer, uint8_t decimal)
{
	return integer += decimal/100.0;
}

float Vbat_Measurment(void)
{
	uint32_t adcValue = 0;

	adcValue = ADC_ReadChannels(ADC_CHANNEL_2);

#ifdef HIGHL_TRACE
	APP_LOG(TS_OFF, VLEVEL_H, "\r\n==========> ADC value : %d\n",adcValue);
#else
	APP_LOG(TS_OFF, VLEVEL_H, "\r\n----Device in boot mode---- \r\n");
#endif

	float voltage = ((float)adcValue / REF_ADC_VAL) * 1.15;

	return ((voltage * 100) / 1.15);
}

