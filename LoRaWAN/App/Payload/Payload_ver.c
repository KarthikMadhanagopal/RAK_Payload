/*
 * Payload_ver.c
 *
 *  Created on: Sep 23, 2023
 *      Author: 91951
 */


#include "Payload_ver.h"

#include <gpio.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys_app.h>

void triggerOrch(orch_config_t[], uint8_t);
void opsAccessory(uint8_t, uint8_t, uint8_t, uint8_t);

uint8_t static_data[48] = {2,2,0,1,'\0','\0',1,3,3,3,0,1,'\0','\0',1,3,3,3,0,1,'\0','\0',1,3,
		4,4,0,1,'\0','\0',1,3,5,5,0,1,'\0','\0',1,3,6,6,0,1,'\0','\0',1,3,};

orch_config_t orch_config[ORCH_CONFIG_LENGTH] = {
		// id or_id step next lid type cont po_delay
		{ 1, 1, 0, 1, '\0', '\0', 0, 0 },
		{ 2, 1, 1, 2, 3, 0x04, 1, 2 },
		{ 3, 1, 1, 2, 4, 0x04, 1, 2 },
		{ 4, 1, 1, 2, 5, 0x04, 1, 2 },
		{ 5, 1, 1, 2, 6, 0x04, 1, 2 },
		{ 6, 1, 2, 0, 1, 0x01, 1, 5 },

		// { 7, 2, 0, 1, '\0', '\0', 0, 0 },
		// { 8, 2, 1, 2, 1, 0x01, 0, 1 },
		// { 9, 2, 2, 0, 3, 0x04, 0, 1 },
		// { 10, 2, 2, 0, 4, 0x04, 0, 1 },
		// { 11, 2, 2, 0, 5, 0x04, 0, 1 },
		// { 12, 2, 2, 0, 6, 0x04, 0, 5 }
};

uint8_t AckBit = 0;

void swap(orch_config_t *xp, orch_config_t *yp) {
	orch_config_t temp = *xp;
	*xp = *yp;
	*yp = temp;
}

void bubbleSort(orch_config_t arr[], uint8_t n) {
	for (uint8_t i = 0; i < n - 1; i++)
		for (uint8_t j = 0; j < n - i - 1; j++)
			if (arr[j].id > arr[j + 1].id)
				swap(&arr[j], &arr[j + 1]);
}

uint8_t nextStep = 0;
uint8_t nextStepCurrent = 0;
void triggerOrch(orch_config_t orch[], uint8_t size) {

	for(uint8_t loop = 0; loop < ORCH_CONFIG_LENGTH; loop++)
	{
		APP_LOG(TS_ON, VLEVEL_M,"\t\tId: %d\t orch_id: %d\tstep: %d\tnext_step : %d\tLocal ID: %d\tAcc. Type: %x\tcondition: %d\tpost_delay : %d\t\n",
				orch_config[loop].id,orch_config[loop].orch_id,orch_config[loop].step,orch_config[loop].next_step,
				orch_config[loop].local_id,orch_config[loop].acc_type,orch_config[loop].condition,orch_config[loop].post_delay);
	}

	for (uint8_t i = 0; i < size; i++) {

		APP_LOG(TS_ON, VLEVEL_M,"===============>INSIDE ORCH TRIGGER<=================\r\n");

		if(orch[i].step == 0) {
			APP_LOG(TS_ON, VLEVEL_M,"Started orch\n");
			nextStep = orch[i].next_step;
		} else {
			if(orch[i].step == nextStep) {
				APP_LOG(TS_ON, VLEVEL_M,"Step: %d\n", orch[i].step);

				opsAccessory(
						orch[i].local_id,
						orch[i].acc_type,
						orch[i].condition,
						orch[i].post_delay
				);
				if(AckBit == 1)
				{
					if(orch[i].condition == 0x01){
						orch[i].condition = 0x11;}
					else if(orch[i].condition == 0x00){
						orch[i].condition = 0x10;
					}
					AckBit = 0 ;
				}

				if (orch[i].next_step == 0) {
					APP_LOG(TS_ON, VLEVEL_M,"Ended orch\n");
					break;
				} else {
					nextStepCurrent = orch[i].next_step;
				}
			} else {
				APP_LOG(TS_ON, VLEVEL_M,"Next step\n");

				nextStep = nextStepCurrent;
				i--;
			}
		}
	}
}

void opsAccessory(uint8_t local_id, uint8_t acc_type, uint8_t condition, uint8_t post_delay) {
	APP_LOG(TS_ON, VLEVEL_M,"\t\tLocal ID: %d\tAcc. Type: %x\n", local_id, acc_type);
	if(acc_type == MOTOR)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, SET);
		HAL_Delay(2000);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, RESET);
	}else if(acc_type == VALVE)
	{
		char valveTransmit[10] = "\0";
		if(condition == 0x01)
		{
			APP_LOG(TS_ON, VLEVEL_M,"===============>INSIDE VALVE TRIGGER<=================\r\n");
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, SET);
			sprintf(valveTransmit,"V%d_on",local_id);
			APP_LOG(TS_ON, VLEVEL_M,"------------->VALVE : %s\n",valveTransmit);
			AckBit = 1;
		}
		else if(condition == 0x00)
		{
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, RESET);
			sprintf(valveTransmit,"V%d_off",local_id);
			APP_LOG(TS_ON, VLEVEL_M,"------------->VALVE : %s\n",valveTransmit);
			AckBit = 1;
		}

	}
	HAL_Delay(post_delay);
}

void payloadToOrch(void)
{
	uint8_t arrayTraves = 0;

	for(uint8_t loop = 0; loop < ORCH_CONFIG_LENGTH; loop++)
	{
		orch_config[loop].id = static_data[arrayTraves++];
		orch_config[loop].orch_id = static_data[arrayTraves++];
		orch_config[loop].step = static_data[arrayTraves++];
		orch_config[loop].next_step = static_data[arrayTraves++];
		orch_config[loop].local_id = static_data[arrayTraves++];
		orch_config[loop].acc_type = static_data[arrayTraves++];
		orch_config[loop].condition = static_data[arrayTraves++];
		orch_config[loop].post_delay = static_data[arrayTraves++];
	}
	for(uint8_t loop = 0; loop < ORCH_CONFIG_LENGTH; loop++)
	{
		APP_LOG(TS_ON, VLEVEL_M,"\t\tId: %d\t orch_id: %d\tstep: %d\tnext_step : %d\tLocal ID: %d\tAcc. Type: %x\tcondition: %d\tpost_delay : %d\t\n",
				orch_config[loop].id,orch_config[loop].orch_id,orch_config[loop].step,orch_config[loop].next_step,
				orch_config[loop].local_id,orch_config[loop].acc_type,orch_config[loop].condition,orch_config[loop].post_delay);
	}
}
//int main(int argc, const char * argv[]) {
// int n = sizeof(orch_config) / sizeof(orch_config[0]);
//
// bubbleSort(orch_config, n);
//
// triggerOrch(orch_config, n);
//
// return 0;
//}
