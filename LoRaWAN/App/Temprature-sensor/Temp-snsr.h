/*
 * light.h
 *
 *  Created on: Dec 2, 2022
 *      Author: KMG
 */

#ifndef APP_ED_LIGHT_LIGHT_H_
#define APP_ED_LIGHT_LIGHT_H_

/*
 * Include files here
 */
#include "main.h"
#include "platform.h"
#include "sys_app.h"
#include "lora_app.h"
#include "app_azure_rtos.h"
#include "tx_api.h"
#include "stm32_timer.h"
#include "utilities_def.h"
#include "app_version.h"
#include "lorawan_version.h"
#include "subghz_phy_version.h"
#include "lora_info.h"
#include "LmHandler.h"
#include "stm32_lpm.h"
#include "adc.h"
#include "adc_if.h"
#include "CayenneLpp.h"
#include "sys_sensors.h"
#include <stdlib.h>
#include <stdint.h>
#include "sys_app.h"
#include "flash_if.h"

#define PAYLOAD_HEADER_SIZE 12

/*
 * light on/off function
 */
float Vbat_Measurment(void);
void SensorDataRead(void);

#endif /* APP_ED_LIGHT_LIGHT_H_ */

