/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    lora_app.c
 * @author  MCD Application Team
 * @brief   Application of the LRWAN Middleware
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
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
#include "adc_if.h"
#include "CayenneLpp.h"
#include "sys_sensors.h"
#include "flash_if.h"

/* USER CODE BEGIN Includes */
#include "sht2x_for_stm32_hal.h"
#include "Payload_ver.h"
#include "Temp-snsr.h"
#include "i2c.h"
#include "stdio.h"
#include "usart.h"
#include "usart_if.h"
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
extern  TX_THREAD App_MainThread;
extern  TX_BYTE_POOL *byte_pool;
extern  CHAR *pointer;
/* USER CODE BEGIN EV */
uint8_t Buffer[25] = {0};
//
//uint8_t uArt1_rxChar[1] = "0";
//char uArt3_rxbuffer[45] = "0";
uint8_t Space[]         = " \r\n";
uint8_t StartMSG[]      = "Starting I2C Scanning: \r\n";
uint8_t EndMSG[]        = "Done! \r\n\r\n";

extern float HummidityVal;
extern float TempratureVal;
extern orch_config_t orch_config[ORCH_CONFIG_LENGTH];

float Percentage     = 0.0;
/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/**
 * @brief LoRa State Machine states
 */
typedef enum TxEventType_e
{
	/**
	 * @brief Appdata Transmission issue based on timer every TxDutyCycleTime
	 */
	TX_ON_TIMER,
	/**
	 * @brief Appdata Transmission external event plugged on OnSendEvent( )
	 */
	TX_ON_EVENT
	/* USER CODE BEGIN TxEventType_t */

	/* USER CODE END TxEventType_t */
} TxEventType_t;

/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/**
 * LEDs period value of the timer in ms
 */
#define LED_PERIOD_TIME 500

/**
 * Join switch period value of the timer in ms
 */
#define JOIN_TIME 2000

/*---------------------------------------------------------------------------*/
/*                             LoRaWAN NVM configuration                     */
/*---------------------------------------------------------------------------*/
/**
 * @brief LoRaWAN NVM Flash address
 * @note last 2 sector of a 128kBytes device
 */
#define LORAWAN_NVM_BASE_ADDRESS                    ((void *)0x0803F000UL)

/* USER CODE BEGIN PD */
static const char *slotStrings[] = { "1", "2", "C", "C_MC", "P", "P_MC" };
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private function prototypes -----------------------------------------------*/
/**
 * @brief  LoRa End Node send request
 */
static void SendTxData(void);

/**
 * @brief  TX timer callback function
 * @param  context ptr of timer context
 */
static void OnTxTimerEvent(void *context);

/**
 * @brief  join event callback function
 * @param  joinParams status of join
 */
static void OnJoinRequest(LmHandlerJoinParams_t *joinParams);

/**
 * @brief callback when LoRaWAN application has sent a frame
 * @brief  tx event callback function
 * @param  params status of last Tx
 */
static void OnTxData(LmHandlerTxParams_t *params);

/**
 * @brief callback when LoRaWAN application has received a frame
 * @param appData data received in the last Rx
 * @param params status of last Rx
 */
static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params);

/**
 * @brief callback when LoRaWAN Beacon status is updated
 * @param params status of Last Beacon
 */
static void OnBeaconStatusChange(LmHandlerBeaconParams_t *params);

/**
 * @brief callback when system time has been updated
 */
static void OnSysTimeUpdate(void);

/**
 * @brief callback when LoRaWAN application Class is changed
 * @param deviceClass new class
 */
static void OnClassChange(DeviceClass_t deviceClass);

/**
 * @brief  LoRa store context in Non Volatile Memory
 */
static void StoreContext(void);

/**
 * @brief  stop current LoRa execution to switch into non default Activation mode
 */
static void StopJoin(void);

/**
 * @brief  Join switch timer callback function
 * @param  context ptr of Join switch context
 */
static void OnStopJoinTimerEvent(void *context);

/**
 * @brief  Notifies the upper layer that the NVM context has changed
 * @param  state Indicates if we are storing (true) or restoring (false) the NVM context
 */
static void OnNvmDataChange(LmHandlerNvmContextStates_t state);

/**
 * @brief  Store the NVM Data context to the Flash
 * @param  nvm ptr on nvm structure
 * @param  nvm_size number of data bytes which were stored
 */
static void OnStoreContextRequest(void *nvm, uint32_t nvm_size);

/**
 * @brief  Restore the NVM Data context from the Flash
 * @param  nvm ptr on nvm structure
 * @param  nvm_size number of data bytes which were restored
 */
static void OnRestoreContextRequest(void *nvm, uint32_t nvm_size);

/**
 * Will be called each time a Radio IRQ is handled by the MAC layer
 *
 */
static void OnMacProcessNotify(void);

/**
 * @brief Change the periodicity of the uplink frames
 * @param periodicity uplink frames period in ms
 * @note Compliance test protocol callbacks
 */
static void OnTxPeriodicityChanged(uint32_t periodicity);

/**
 * @brief Change the confirmation control of the uplink frames
 * @param isTxConfirmed Indicates if the uplink requires an acknowledgement
 * @note Compliance test protocol callbacks
 */
static void OnTxFrameCtrlChanged(LmHandlerMsgTypes_t isTxConfirmed);

/**
 * @brief Change the periodicity of the ping slot frames
 * @param pingSlotPeriodicity ping slot frames period in ms
 * @note Compliance test protocol callbacks
 */
static void OnPingSlotPeriodicityChanged(uint8_t pingSlotPeriodicity);

/**
 * @brief Will be called to reset the system
 * @note Compliance test protocol callbacks
 */
static void OnSystemReset(void);

/**
 * @brief  Entry point for the thread when receiving MailBox LmHandler Notification
 * @param  thread_input Not used
 */
static void Thd_LmHandlerProcess_Entry(unsigned long thread_input);

/**
 * @brief  Entry point for the thread when receiving Store Context Notification
 * @param  thread_input Not used
 */
static void Thd_StoreContext_Entry(unsigned long thread_input);

/**
 * @brief  Entry point for the thread when receiving stop and change join mode request
 * @param  thread_input Not used
 */
static void Thd_StopJoin_Entry(unsigned long thread_input);

/* USER CODE BEGIN PFP */

/**
 * @brief  LED Tx timer callback function
 * @param  context ptr of LED context
 */
static void OnTxTimerLedEvent(void *context);

/**
 * @brief  LED Rx timer callback function
 * @param  context ptr of LED context
 */
static void OnRxTimerLedEvent(void *context);

/**
 * @brief  LED Join timer callback function
 * @param  context ptr of LED context
 */
static void OnJoinTimerLedEvent(void *context);

/**
 * @brief  Group-Orch Acknowldgent function
 * @param  context ptr of LED context
 */
void AcKnowledgementTxData(void);
/* USER CODE END PFP */

/* Private variables ---------------------------------------------------------*/
static __IO uint8_t Thd_LmHandlerProcess_RescheduleFlag = 0;
TX_THREAD Thd_LmHandlerProcessId;
TX_THREAD Thd_LoraStoreContextId;
TX_THREAD Thd_LoraStopJoinId;

/**
 * @brief LoRaWAN default activation type
 */
static ActivationType_t ActivationType = LORAWAN_DEFAULT_ACTIVATION_TYPE;

/**
 * @brief LoRaWAN force rejoin even if the NVM context is restored
 */
static bool ForceRejoin = LORAWAN_FORCE_REJOIN_AT_BOOT;

/**
 * @brief LoRaWAN handler Callbacks
 */
static LmHandlerCallbacks_t LmHandlerCallbacks =
{
		.GetBatteryLevel =              GetBatteryLevel,
		.GetTemperature =               GetTemperatureLevel,
		.GetUniqueId =                  GetUniqueId,
		.GetDevAddr =                   GetDevAddr,
		.OnRestoreContextRequest =      OnRestoreContextRequest,
		.OnStoreContextRequest =        OnStoreContextRequest,
		.OnMacProcess =                 OnMacProcessNotify,
		.OnNvmDataChange =              OnNvmDataChange,
		.OnJoinRequest =                OnJoinRequest,
		.OnTxData =                     OnTxData,
		.OnRxData =                     OnRxData,
		.OnBeaconStatusChange =         OnBeaconStatusChange,
		.OnSysTimeUpdate =              OnSysTimeUpdate,
		.OnClassChange =                OnClassChange,
		.OnTxPeriodicityChanged =       OnTxPeriodicityChanged,
		.OnTxFrameCtrlChanged =         OnTxFrameCtrlChanged,
		.OnPingSlotPeriodicityChanged = OnPingSlotPeriodicityChanged,
		.OnSystemReset =                OnSystemReset,
};

/**
 * @brief LoRaWAN handler parameters
 */
static LmHandlerParams_t LmHandlerParams =
{
		.ActiveRegion =             ACTIVE_REGION,
		.DefaultClass =             LORAWAN_DEFAULT_CLASS,
		.AdrEnable =                LORAWAN_ADR_STATE,
		.IsTxConfirmed =            LORAWAN_DEFAULT_CONFIRMED_MSG_STATE,
		.TxDatarate =               LORAWAN_DEFAULT_DATA_RATE,
		.TxPower =                  LORAWAN_DEFAULT_TX_POWER,
		.PingSlotPeriodicity =      LORAWAN_DEFAULT_PING_SLOT_PERIODICITY,
		.RxBCTimeout =              LORAWAN_DEFAULT_CLASS_B_C_RESP_TIMEOUT
};

/**
 * @brief Type of Event to generate application Tx
 */
static TxEventType_t EventType = TX_ON_TIMER;

/**
 * @brief Timer to handle the application Tx
 */
static UTIL_TIMER_Object_t TxTimer;

/**
 * @brief Tx Timer period
 */
static UTIL_TIMER_Time_t TxPeriodicity = APP_TX_DUTYCYCLE;

/**
 * @brief Join Timer period
 */
static UTIL_TIMER_Object_t StopJoinTimer;

/* USER CODE BEGIN PV */
/**
 * @brief User application buffer
 */
static uint8_t AppDataBuffer[LORAWAN_APP_DATA_BUFFER_MAX_SIZE];

/**
 * @brief User application data structure
 */
static LmHandlerAppData_t AppData = { 0, 0, AppDataBuffer };

/**
 * @brief Specifies the state of the application LED
 */
uint8_t AppLedStateOn = RESET;

/**
 * @brief Timer to handle the application Tx Led to toggle
 */
static UTIL_TIMER_Object_t TxLedTimer;

/**
 * @brief Timer to handle the application Rx Led to toggle
 */
static UTIL_TIMER_Object_t RxLedTimer;

/**
 * @brief Timer to handle the application Join Led to toggle
 */
static UTIL_TIMER_Object_t JoinLedTimer;

/* USER CODE END PV */

/* Exported functions ---------------------------------------------------------*/
/* USER CODE BEGIN EF */

/* USER CODE END EF */

void LoRaWAN_Init(void)
{
	/* USER CODE BEGIN LoRaWAN_Init_LV */
	uint32_t feature_version = 0UL;
	/* USER CODE END LoRaWAN_Init_LV */

	/* USER CODE BEGIN LoRaWAN_Init_1 */
	/* Get LoRaWAN APP version*/
	APP_LOG(TS_OFF, VLEVEL_M, "APPLICATION_VERSION: V%X.%X.%X\r\n",
			(uint8_t)(APP_VERSION_MAIN),
			(uint8_t)(APP_VERSION_SUB1),
			(uint8_t)(APP_VERSION_SUB2));

	/* Get MW LoRaWAN info */
	APP_LOG(TS_OFF, VLEVEL_M, "MW_LORAWAN_VERSION:  V%X.%X.%X\r\n",
			(uint8_t)(LORAWAN_VERSION_MAIN),
			(uint8_t)(LORAWAN_VERSION_SUB1),
			(uint8_t)(LORAWAN_VERSION_SUB2));

	/* Get MW SubGhz_Phy info */
	APP_LOG(TS_OFF, VLEVEL_M, "MW_RADIO_VERSION:    V%X.%X.%X\r\n",
			(uint8_t)(SUBGHZ_PHY_VERSION_MAIN),
			(uint8_t)(SUBGHZ_PHY_VERSION_SUB1),
			(uint8_t)(SUBGHZ_PHY_VERSION_SUB2));

	/* Get LoRaWAN Link Layer info */
	LmHandlerGetVersion(LORAMAC_HANDLER_L2_VERSION, &feature_version);
	APP_LOG(TS_OFF, VLEVEL_M, "L2_SPEC_VERSION:     V%X.%X.%X\r\n",
			(uint8_t)(feature_version >> 24),
			(uint8_t)(feature_version >> 16),
			(uint8_t)(feature_version >> 8));

	/* Get LoRaWAN Regional Parameters info */
	LmHandlerGetVersion(LORAMAC_HANDLER_REGION_VERSION, &feature_version);
	APP_LOG(TS_OFF, VLEVEL_M, "RP_SPEC_VERSION:     V%X-%X.%X.%X\r\n",
			(uint8_t)(feature_version >> 24),
			(uint8_t)(feature_version >> 16),
			(uint8_t)(feature_version >> 8),
			(uint8_t)(feature_version));

	UTIL_TIMER_Create(&TxLedTimer, LED_PERIOD_TIME, UTIL_TIMER_ONESHOT, OnTxTimerLedEvent, NULL);
	UTIL_TIMER_Create(&RxLedTimer, LED_PERIOD_TIME, UTIL_TIMER_ONESHOT, OnRxTimerLedEvent, NULL);
	UTIL_TIMER_Create(&JoinLedTimer, LED_PERIOD_TIME, UTIL_TIMER_PERIODIC, OnJoinTimerLedEvent, NULL);

	if (FLASH_IF_Init(NULL) != FLASH_IF_OK)
	{
		Error_Handler();
	}

	/* USER CODE END LoRaWAN_Init_1 */

	UTIL_TIMER_Create(&StopJoinTimer, JOIN_TIME, UTIL_TIMER_ONESHOT, OnStopJoinTimerEvent, NULL);

	/* Allocate the stack and create thread for LmHandlerProcess.  */
	if (tx_byte_allocate(byte_pool, (VOID **) &pointer, CFG_LM_HANDLER_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
		Error_Handler();
	}
	if (tx_thread_create(&Thd_LmHandlerProcessId, "Thread LmHandlerProcess", Thd_LmHandlerProcess_Entry, 0,
			pointer, CFG_LM_HANDLER_THREAD_STACK_SIZE,
			CFG_LM_HANDLER_THREAD_PRIO, CFG_LM_HANDLER_THREAD_PREEMPTION_THRESHOLD,
			TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
	{
		Error_Handler();
	}

	/* No need to allocate the stack and create thread for Thd_LoraSendProcess. */
	/* App_MainThread is used for it (see app_lorawan.c)  */

	if (tx_byte_allocate(byte_pool, (VOID **) &pointer, CFG_APP_LORA_STORE_CONTEXT_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
		Error_Handler();
	}
	if (tx_thread_create(&Thd_LoraStoreContextId, "Thread StoreContext", Thd_StoreContext_Entry, 0,
			pointer, CFG_APP_LORA_STORE_CONTEXT_STACK_SIZE,
			CFG_APP_LORA_STORE_CONTEXT_PRIO, CFG_APP_LORA_STORE_CONTEXT_PREEMPTION_THRESHOLD,
			TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
	{
		Error_Handler();
	}

	if (tx_byte_allocate(byte_pool, (VOID **) &pointer, CFG_APP_LORA_STOP_JOIN_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
		Error_Handler();
	}
	if (tx_thread_create(&Thd_LoraStopJoinId, "Thread StopJoin", Thd_StopJoin_Entry, 0,
			pointer, CFG_APP_LORA_STOP_JOIN_STACK_SIZE,
			CFG_APP_LORA_STOP_JOIN_PRIO, CFG_APP_LORA_STOP_JOIN_PREEMPTION_THRESHOLD,
			TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
	{
		Error_Handler();
	}

	/* Init Info table used by LmHandler*/
	LoraInfo_Init();

	/* Init the Lora Stack*/
	LmHandlerInit(&LmHandlerCallbacks, APP_VERSION);

	LmHandlerConfigure(&LmHandlerParams);

	/* USER CODE BEGIN LoRaWAN_Init_2 */
	UTIL_TIMER_Start(&JoinLedTimer);

	/* USER CODE END LoRaWAN_Init_2 */

	LmHandlerJoin(ActivationType, ForceRejoin);

	if (EventType == TX_ON_TIMER)
	{
		/* send every time timer elapses */
		UTIL_TIMER_Create(&TxTimer, TxPeriodicity, UTIL_TIMER_ONESHOT, OnTxTimerEvent, NULL);
		UTIL_TIMER_Start(&TxTimer);
	}
	else
	{
		/* USER CODE BEGIN LoRaWAN_Init_3 */

		/* USER CODE END LoRaWAN_Init_3 */
	}

	/* USER CODE BEGIN LoRaWAN_Init_Last */

	/* USER CODE END LoRaWAN_Init_Last */
}

/* USER CODE BEGIN PB_Callbacks */

#if 0 /* User should remove the #if 0 statement and adapt the below code according with his needs*/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	switch (GPIO_Pin)
	{
	case BUT1_Pin:
		/* Note: when "EventType == TX_ON_TIMER" this GPIO is not initialized */
		if (EventType == TX_ON_EVENT)
		{
			tx_thread_resume(&App_MainThread);
		}
		break;
	case BUT2_Pin:
		tx_thread_resume(&Thd_LoraStopJoinId);
		break;
	case BUT3_Pin:
		tx_thread_resume(&Thd_LoraStoreContextId);
		break;
	default:
		break;
	}
}
#endif


/* USER CODE END PB_Callbacks */

/* Private functions ---------------------------------------------------------*/
/* USER CODE BEGIN PrFD */
void myRxCallback(uint8_t *rxChar, uint16_t size) {
	APP_LOG(TS_OFF, VLEVEL_M, "Received data of size %u: ", size);
	for (uint16_t i = 0; i < size; i++) {
		APP_LOG(TS_OFF, VLEVEL_M, "%c", rxChar[i]); // Assuming the data is ASCII characters
	}
	APP_LOG(TS_OFF, VLEVEL_M, "\n");
}

/* USER CODE END PrFD */

void App_Main_Thread_Entry(unsigned long thread_input)
{
	(void) thread_input;

	/* USER CODE BEGIN App_Main_Thread_Entry_1 */

	/* USER CODE END App_Main_Thread_Entry_1 */
	SystemApp_Init();
	LoRaWAN_Init();
	/* USER CODE BEGIN App_Main_Thread_Entry_2 */
	vcom_ReceiveInit((void*)myRxCallback);
	/* USER CODE END App_Main_Thread_Entry_2 */

	/* Infinite loop */
	while (1)
	{
		tx_thread_suspend(&App_MainThread);
		SendTxData();
		/*do what you want*/
		/* USER CODE BEGIN App_Main_Thread_Entry_Loop */
		HAL_UART_Transmit(&huart1, (uint8_t*)"Hello in main \n", 16,10);
		/* USER CODE END App_Main_Thread_Entry_Loop */
	}
	/* Following USER CODE SECTION will be never reached */
	/* it can be use for compilation flag like #else or #endif */
	/* USER CODE BEGIN App_Main_Thread_Entry_Last */

	/* USER CODE END App_Main_Thread_Entry_Last */
}

static void Thd_LmHandlerProcess_Entry(unsigned long thread_input)
{
	/* USER CODE BEGIN Thd_LmHandlerProcess_Entry_1 */

	/* USER CODE END Thd_LmHandlerProcess_Entry_1 */
	(void) thread_input;
	/* Infinite loop */
	for (;;)
	{
		if (Thd_LmHandlerProcess_RescheduleFlag > 0)
		{
			APP_LOG(TS_ON, VLEVEL_H, "Thd_LmHandlerProcess_RescheduleFlag: %d \n", Thd_LmHandlerProcess_RescheduleFlag);
		}
		else
		{
			tx_thread_suspend(&Thd_LmHandlerProcessId);
			APP_LOG(TS_ON, VLEVEL_H, "Thd_LmHandlerProcess resumed from suspend \n");
		}
		Thd_LmHandlerProcess_RescheduleFlag = 0;
		LmHandlerProcess();
		/* USER CODE BEGIN Thd_LmHandlerProcess_Entry_2 */

		/* USER CODE END Thd_LmHandlerProcess_Entry_2 */
	}
	/* Following USER CODE SECTION will be never reached */
	/* it can be use for compilation flag like #else or #endif */
	/* USER CODE BEGIN Thd_LmHandlerProcess_Entry_Last */

	/* USER CODE END Thd_LmHandlerProcess_Entry_Last */
}

static void Thd_StoreContext_Entry(unsigned long thread_input)
{
	/* USER CODE BEGIN Thd_StoreContext_Entry_1 */

	/* USER CODE END Thd_StoreContext_Entry_1 */
	(void) thread_input;
	/* Infinite loop */
	for (;;)
	{
		tx_thread_suspend(&Thd_LoraStoreContextId);
		StoreContext();
		/* USER CODE BEGIN Thd_StoreContext_Entry_2 */

		/* USER CODE END Thd_StoreContext_Entry_2 */
	}
	/* Following USER CODE SECTION will be never reached */
	/* it can be use for compilation flag like #else or #endif */
	/* USER CODE BEGIN Thd_StoreContext_Entry_Last */

	/* USER CODE END Thd_StoreContext_Entry_Last */
}

static void Thd_StopJoin_Entry(unsigned long thread_input)
{
	/* USER CODE BEGIN Thd_StopJoin_Entry_1 */

	/* USER CODE END Thd_StopJoin_Entry_1 */
	(void) thread_input;
	/* Infinite loop */
	for (;;)
	{
		tx_thread_suspend(&Thd_LoraStopJoinId);
		StopJoin();
		/* USER CODE BEGIN Thd_StopJoin_Entry_2 */

		/* USER CODE END Thd_StopJoin_Entry_2 */
	}
	/* Following USER CODE SECTION will be never reached */
	/* it can be use for compilation flag like #else or #endif */
	/* USER CODE BEGIN Thd_StopJoin_Entry_Last */

	/* USER CODE END Thd_StopJoin_Entry_Last */
}

static void OnRxData(LmHandlerAppData_t *appData, LmHandlerRxParams_t *params)
{
	/* USER CODE BEGIN OnRxData_1 */
	uint8_t RxPort = 0;

	uint8_t i = 0,k = appData->BufferSize;
	APP_LOG(TS_OFF, VLEVEL_M, "===========================>INSIDE RX DATA : %d",appData->BufferSize);
	do
	{
		APP_LOG(TS_OFF, VLEVEL_M, "%02x ",appData->Buffer[i]);
		k--;
		i++;
	}while(k!=0);
	APP_LOG(TS_OFF, VLEVEL_M, "\n==========PORT : %d\r\n",appData->Port);

	if (params != NULL)
	{
		// HAL_GPIO_WritePin(SNSR_PWR_EN_1_GPIO_Port, SNSR_PWR_EN_1_Pin, GPIO_PIN_SET); /* LED_BLUE */

		UTIL_TIMER_Start(&RxLedTimer);

		if (params->IsMcpsIndication)
		{
			if (appData != NULL)
			{
				RxPort = appData->Port;
				if (appData->Buffer != NULL)
				{
					switch (appData->Port)
					{
					case LORAWAN_SWITCH_CLASS_PORT:
						/*this port switches the class*/
						if (appData->BufferSize == 1)
						{
							switch (appData->Buffer[0])
							{
							case 0:
							{
								LmHandlerRequestClass(CLASS_A);
								break;
							}
							case 1:
							{
								LmHandlerRequestClass(CLASS_B);
								break;
							}
							case 2:
							{
								LmHandlerRequestClass(CLASS_C);
								break;
							}
							default:
								break;
							}
						}
						break;

					case LORAWAN_USER_APP_PORT:
						if (appData->BufferSize > 0)
						{
							APP_LOG(TS_OFF, VLEVEL_M, "------------------------INSIDE ACTION FUNCTION ---------------------\r\n");
							uint8_t arrayTraves = 0;

							for(uint8_t loop = 0; loop < ORCH_CONFIG_LENGTH; loop++)
							{
								orch_config[loop].id = appData->Buffer[arrayTraves++];
								orch_config[loop].orch_id = appData->Buffer[arrayTraves++];
								orch_config[loop].step = appData->Buffer[arrayTraves++];
								orch_config[loop].next_step = appData->Buffer[arrayTraves++];
								orch_config[loop].local_id = appData->Buffer[arrayTraves++];
								orch_config[loop].acc_type = appData->Buffer[arrayTraves++];
								orch_config[loop].condition = appData->Buffer[arrayTraves++];
								orch_config[loop].post_delay = appData->Buffer[arrayTraves++];
							}

							triggerOrch(orch_config,ORCH_CONFIG_LENGTH);//ACTION Trigger Function
							AcKnowledgementTxData();
						}
						break;

					default:

						break;
					}
				}
			}
		}
		if (params->RxSlot < RX_SLOT_NONE)
		{
			APP_LOG(TS_OFF, VLEVEL_H, "###### D/L FRAME:%04d | PORT:%d | DR:%d | SLOT:%s | RSSI:%d | SNR:%d\r\n",
					params->DownlinkCounter, RxPort, params->Datarate, slotStrings[params->RxSlot],
					params->Rssi, params->Snr);
		}
	}
	/* USER CODE END OnRxData_1 */
}

static void SendTxData(void)
{
	/* USER CODE BEGIN SendTxData_1 */
	HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_SET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(STATUS_LED_GPIO_Port, STATUS_LED_Pin, GPIO_PIN_RESET);

	APP_LOG(TS_ON, VLEVEL_M, "======================== ADC Start...\r\n");
	HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin, GPIO_PIN_SET);
	HAL_Delay(10);
	Percentage = Vbat_Measurment();
	HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin, GPIO_PIN_RESET);
	APP_LOG(TS_ON, VLEVEL_M, "======================== ADC End\r\n");

	APP_LOG(TS_OFF, VLEVEL_M, "======================== Temperature Starts\r\n");
	HAL_GPIO_WritePin(GPIO_1_GPIO_Port, GPIO_1_Pin, GPIO_PIN_SET);
	HAL_Delay(10);

	SHT2x_Init(&hi2c2);
	SHT2x_SetResolution(RES_14_12);

	LmHandlerErrorStatus_t status = LORAMAC_HANDLER_ERROR;
	UTIL_TIMER_Time_t nextTxIn = 0;

	if (LmHandlerIsBusy() == false)
	{
#ifdef CAYENNE_LPP
		uint8_t channel = 0;
#else
		uint8_t Txcopy     = 0;
		uint8_t byte1      = 0;
		uint32_t hex       = 0;
#endif /* CAYENNE_LPP */

		//		  Sensing the data from SHt20
		SensorDataRead();

#ifdef CAYENNE_LPP
		CayenneLppReset();
		CayenneLppAddBarometricPressure(channel++, sensor_data.pressure);
		CayenneLppAddTemperature(channel++, sensor_data.temperature);
		CayenneLppAddRelativeHumidity(channel++, (uint16_t)(sensor_data.humidity));

		if ((LmHandlerParams.ActiveRegion != LORAMAC_REGION_US915) && (LmHandlerParams.ActiveRegion != LORAMAC_REGION_AU915)
				&& (LmHandlerParams.ActiveRegion != LORAMAC_REGION_AS923))
		{
			CayenneLppAddDigitalInput(channel++, GetBatteryLevel());
			CayenneLppAddDigitalOutput(channel++, AppLedStateOn);
		}

		CayenneLppCopy(AppData.Buffer);
		AppData.BufferSize = CayenneLppGetSize();
#else  /* not CAYENNE_LPP */

		AppData.Port = LORAWAN_USER_APP_PORT;
		AppData.BufferSize = sizeof(uint64_t) + sizeof(uint32_t);

		hex  = *(uint32_t*)&TempratureVal;
		for(uint8_t loop = 0;loop < sizeof(uint32_t);loop++)
		{
			AppData.Buffer[Txcopy++] = (uint8_t)(hex >> (24 - byte1))& 0xff; //copying the every single byte to buffer
			byte1 += 8;
		}
		byte1= 0;
		hex  = 0;

		hex  = *(uint32_t*)&HummidityVal;
		for(uint8_t loop = 0;loop < sizeof(uint32_t);loop++)
		{
			AppData.Buffer[Txcopy++] = (uint8_t)(hex >> (24 - byte1))& 0xff; //copying the every single byte to buffer
			byte1 += 8;
		}
		byte1= 0;
		hex  = 0;

		hex  = *(uint32_t*)&Percentage;
		for(uint8_t loop = 0;loop < sizeof(uint32_t);loop++)
		{
			AppData.Buffer[Txcopy++] = (uint8_t)(hex >> (24 - byte1))& 0xff; //copying the every single byte to buffer
			byte1 += 8;
		}

		HAL_GPIO_WritePin(GPIO_1_GPIO_Port, GPIO_1_Pin, GPIO_PIN_RESET);

		APP_LOG(TS_OFF, VLEVEL_M, "\r\n==========> TEMP value : %x-%x-%x-%x\r\n",AppData.Buffer[0],AppData.Buffer[1],AppData.Buffer[2],AppData.Buffer[3]);
		APP_LOG(TS_OFF, VLEVEL_M, "\r\n==========> HUMIDITY value : %x-%x-%x-%x\r\n",AppData.Buffer[4],AppData.Buffer[5],AppData.Buffer[6],AppData.Buffer[7]);
		APP_LOG(TS_OFF, VLEVEL_M, "\r\n==========> Battery percentage : %x-%x-%x-%x\r\n",AppData.Buffer[8],AppData.Buffer[9],AppData.Buffer[10],AppData.Buffer[11]);
		APP_LOG(TS_OFF, VLEVEL_M, "======================== Temperature End\r\n");

#endif /* CAYENNE_LPP */

		if ((JoinLedTimer.IsRunning) && (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET))
		{
			UTIL_TIMER_Stop(&JoinLedTimer);
			//      HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET); /* LED_RED */
		}

		status = LmHandlerSend(&AppData, LmHandlerParams.IsTxConfirmed, false);
		if (LORAMAC_HANDLER_SUCCESS == status)
		{
			APP_LOG(TS_ON, VLEVEL_L, "SEND REQUEST\r\n");
		}
		else if (LORAMAC_HANDLER_DUTYCYCLE_RESTRICTED == status)
		{
			nextTxIn = LmHandlerGetDutyCycleWaitTime();
			if (nextTxIn > 0)
			{
				APP_LOG(TS_ON, VLEVEL_L, "Next Tx in  : ~%d second(s)\r\n", (nextTxIn / 1000));
			}
		}
	}

	if (EventType == TX_ON_TIMER)
	{
		UTIL_TIMER_Stop(&TxTimer);
		UTIL_TIMER_SetPeriod(&TxTimer, MAX(nextTxIn, TxPeriodicity));
		UTIL_TIMER_Start(&TxTimer);
	}

	/* USER CODE END SendTxData_1 */
}

static void OnTxTimerEvent(void *context)
{
	/* USER CODE BEGIN OnTxTimerEvent_1 */

	/* USER CODE END OnTxTimerEvent_1 */
	/* Reactivate App_MainThread */
	tx_thread_resume(&App_MainThread);

	/*Wait for next tx slot*/
	UTIL_TIMER_Start(&TxTimer);
	/* USER CODE BEGIN OnTxTimerEvent_2 */

	/* USER CODE END OnTxTimerEvent_2 */
}

/* USER CODE BEGIN PrFD_LedEvents */
static void OnTxTimerLedEvent(void *context)
{
	//  HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin, GPIO_PIN_RESET); /* LED_GREEN */
}

static void OnRxTimerLedEvent(void *context)
{
	//  HAL_GPIO_WritePin(SNSR_PWR_EN_1_GPIO_Port, SNSR_PWR_EN_1_Pin, GPIO_PIN_RESET); /* LED_BLUE */
}

static void OnJoinTimerLedEvent(void *context)
{
	//  HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin); /* LED_RED */
}

void AcKnowledgementTxData(void)
{
	uint8_t arrayTraves = 0;
	AppData.Port = LORAWAN_USER_APP_PORT;

	AppData.Buffer[arrayTraves++] = 0x01;
	for(uint8_t loop = 0; loop < ORCH_CONFIG_LENGTH; loop++)
	{
		AppData.Buffer[arrayTraves++] = orch_config[loop].id;
		AppData.Buffer[arrayTraves++] = orch_config[loop].orch_id;
		AppData.Buffer[arrayTraves++] = orch_config[loop].step;
		AppData.Buffer[arrayTraves++] = orch_config[loop].next_step;
		AppData.Buffer[arrayTraves++] = orch_config[loop].local_id;
		AppData.Buffer[arrayTraves++] = orch_config[loop].acc_type;
		AppData.Buffer[arrayTraves++] = orch_config[loop].condition;
		AppData.Buffer[arrayTraves++] = orch_config[loop].post_delay;
	}

	for(uint8_t loop = 0; loop < ORCH_CONFIG_LENGTH; loop++)
	{
		APP_LOG(TS_OFF, VLEVEL_M, "\t\tId: %d\t orch_id: %d\tstep: %d\tnext_step : %d\tLocal ID: %d\tAcc. Type: %x\tcondition: %d\tpost_delay : %d\t\n",
				orch_config[loop].id,orch_config[loop].orch_id,orch_config[loop].step,orch_config[loop].next_step,
				orch_config[loop].local_id,orch_config[loop].acc_type,orch_config[loop].condition,orch_config[loop].post_delay);
	}
	HAL_Delay(1000);
	AppData.BufferSize = arrayTraves;
	LmHandlerSend(&AppData, LmHandlerParams.IsTxConfirmed, false);
}
/* USER CODE END PrFD_LedEvents */

static void OnTxData(LmHandlerTxParams_t *params)
{
	/* USER CODE BEGIN OnTxData_1 */
	if ((params != NULL))
	{
		/* Process Tx event only if its a mcps response to prevent some internal events (mlme) */
		if (params->IsMcpsConfirm != 0)
		{
			//      HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin, GPIO_PIN_SET); /* LED_GREEN */
			UTIL_TIMER_Start(&TxLedTimer);

			APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### ========== MCPS-Confirm =============\r\n");
			APP_LOG(TS_OFF, VLEVEL_H, "###### U/L FRAME:%04d | PORT:%d | DR:%d | PWR:%d", params->UplinkCounter,
					params->AppData.Port, params->Datarate, params->TxPower);

			APP_LOG(TS_OFF, VLEVEL_H, " | MSG TYPE:");
			if (params->MsgType == LORAMAC_HANDLER_CONFIRMED_MSG)
			{
				APP_LOG(TS_OFF, VLEVEL_H, "CONFIRMED [%s]\r\n", (params->AckReceived != 0) ? "ACK" : "NACK");
			}
			else
			{
				APP_LOG(TS_OFF, VLEVEL_H, "UNCONFIRMED\r\n");
			}
		}
	}
	/* USER CODE END OnTxData_1 */
}

static void OnJoinRequest(LmHandlerJoinParams_t *joinParams)
{
	/* USER CODE BEGIN OnJoinRequest_1 */
	if (joinParams != NULL)
	{
		if (joinParams->Status == LORAMAC_HANDLER_SUCCESS)
		{
			UTIL_TIMER_Stop(&JoinLedTimer);
			//      HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET); /* LED_RED */

			APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOINED = ");
			if (joinParams->Mode == ACTIVATION_TYPE_ABP)
			{
				APP_LOG(TS_OFF, VLEVEL_M, "ABP ======================\r\n");
			}
			else
			{
				APP_LOG(TS_OFF, VLEVEL_M, "OTAA =====================\r\n");
			}
		}
		else
		{
			APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### = JOIN FAILED\r\n");
		}

		APP_LOG(TS_OFF, VLEVEL_H, "###### U/L FRAME:JOIN | DR:%d | PWR:%d\r\n", joinParams->Datarate, joinParams->TxPower);
	}
	/* USER CODE END OnJoinRequest_1 */
}

static void OnBeaconStatusChange(LmHandlerBeaconParams_t *params)
{
	/* USER CODE BEGIN OnBeaconStatusChange_1 */
	if (params != NULL)
	{
		switch (params->State)
		{
		default:
		case LORAMAC_HANDLER_BEACON_LOST:
		{
			APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### BEACON LOST\r\n");
			break;
		}
		case LORAMAC_HANDLER_BEACON_RX:
		{
			APP_LOG(TS_OFF, VLEVEL_M,
					"\r\n###### BEACON RECEIVED | DR:%d | RSSI:%d | SNR:%d | FQ:%d | TIME:%d | DESC:%d | "
					"INFO:02X%02X%02X %02X%02X%02X\r\n",
					params->Info.Datarate, params->Info.Rssi, params->Info.Snr, params->Info.Frequency,
					params->Info.Time.Seconds, params->Info.GwSpecific.InfoDesc,
					params->Info.GwSpecific.Info[0], params->Info.GwSpecific.Info[1],
					params->Info.GwSpecific.Info[2], params->Info.GwSpecific.Info[3],
					params->Info.GwSpecific.Info[4], params->Info.GwSpecific.Info[5]);
			break;
		}
		case LORAMAC_HANDLER_BEACON_NRX:
		{
			APP_LOG(TS_OFF, VLEVEL_M, "\r\n###### BEACON NOT RECEIVED\r\n");
			break;
		}
		}
	}
	/* USER CODE END OnBeaconStatusChange_1 */
}

static void OnSysTimeUpdate(void)
{
	/* USER CODE BEGIN OnSysTimeUpdate_1 */

	/* USER CODE END OnSysTimeUpdate_1 */
}

static void OnClassChange(DeviceClass_t deviceClass)
{
	/* USER CODE BEGIN OnClassChange_1 */
	APP_LOG(TS_OFF, VLEVEL_M, "Switch to Class %c done\r\n", "ABC"[deviceClass]);
	/* USER CODE END OnClassChange_1 */
}

static void OnMacProcessNotify(void)
{
	/* USER CODE BEGIN OnMacProcessNotify_1 */

	/* USER CODE END OnMacProcessNotify_1 */
	if (Thd_LmHandlerProcess_RescheduleFlag < 255)
	{
		Thd_LmHandlerProcess_RescheduleFlag++;
	}
	tx_thread_resume(&Thd_LmHandlerProcessId);

	/* USER CODE BEGIN OnMacProcessNotify_2 */

	/* USER CODE END OnMacProcessNotify_2 */
}

static void OnTxPeriodicityChanged(uint32_t periodicity)
{
	/* USER CODE BEGIN OnTxPeriodicityChanged_1 */

	/* USER CODE END OnTxPeriodicityChanged_1 */
	TxPeriodicity = periodicity;

	if (TxPeriodicity == 0)
	{
		/* Revert to application default periodicity */
		TxPeriodicity = APP_TX_DUTYCYCLE;
	}

	/* Update timer periodicity */
	UTIL_TIMER_Stop(&TxTimer);
	UTIL_TIMER_SetPeriod(&TxTimer, TxPeriodicity);
	UTIL_TIMER_Start(&TxTimer);
	/* USER CODE BEGIN OnTxPeriodicityChanged_2 */

	/* USER CODE END OnTxPeriodicityChanged_2 */
}

static void OnTxFrameCtrlChanged(LmHandlerMsgTypes_t isTxConfirmed)
{
	/* USER CODE BEGIN OnTxFrameCtrlChanged_1 */

	/* USER CODE END OnTxFrameCtrlChanged_1 */
	LmHandlerParams.IsTxConfirmed = isTxConfirmed;
	/* USER CODE BEGIN OnTxFrameCtrlChanged_2 */

	/* USER CODE END OnTxFrameCtrlChanged_2 */
}

static void OnPingSlotPeriodicityChanged(uint8_t pingSlotPeriodicity)
{
	/* USER CODE BEGIN OnPingSlotPeriodicityChanged_1 */

	/* USER CODE END OnPingSlotPeriodicityChanged_1 */
	LmHandlerParams.PingSlotPeriodicity = pingSlotPeriodicity;
	/* USER CODE BEGIN OnPingSlotPeriodicityChanged_2 */

	/* USER CODE END OnPingSlotPeriodicityChanged_2 */
}

static void OnSystemReset(void)
{
	/* USER CODE BEGIN OnSystemReset_1 */

	/* USER CODE END OnSystemReset_1 */
	if ((LORAMAC_HANDLER_SUCCESS == LmHandlerHalt()) && (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET))
	{
		NVIC_SystemReset();
	}
	/* USER CODE BEGIN OnSystemReset_Last */

	/* USER CODE END OnSystemReset_Last */
}

static void StopJoin(void)
{
	/* USER CODE BEGIN StopJoin_1 */
	//  HAL_GPIO_WritePin(SNSR_PWR_EN_1_GPIO_Port, SNSR_PWR_EN_1_Pin, GPIO_PIN_SET); /* LED_BLUE */
	//  HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin, GPIO_PIN_SET); /* LED_GREEN */
	//  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET); /* LED_RED */
	/* USER CODE END StopJoin_1 */

	UTIL_TIMER_Stop(&TxTimer);

	if (LORAMAC_HANDLER_SUCCESS != LmHandlerStop())
	{
		APP_LOG(TS_OFF, VLEVEL_M, "LmHandler Stop on going ...\r\n");
	}
	else
	{
		APP_LOG(TS_OFF, VLEVEL_M, "LmHandler Stopped\r\n");
		if (LORAWAN_DEFAULT_ACTIVATION_TYPE == ACTIVATION_TYPE_ABP)
		{
			ActivationType = ACTIVATION_TYPE_OTAA;
			APP_LOG(TS_OFF, VLEVEL_M, "LmHandler switch to OTAA mode\r\n");
		}
		else
		{
			ActivationType = ACTIVATION_TYPE_ABP;
			APP_LOG(TS_OFF, VLEVEL_M, "LmHandler switch to ABP mode\r\n");
		}
		LmHandlerConfigure(&LmHandlerParams);
		LmHandlerJoin(ActivationType, true);
		UTIL_TIMER_Start(&TxTimer);
	}
	UTIL_TIMER_Start(&StopJoinTimer);
	/* USER CODE BEGIN StopJoin_Last */

	/* USER CODE END StopJoin_Last */
}

static void OnStopJoinTimerEvent(void *context)
{
	/* USER CODE BEGIN OnStopJoinTimerEvent_1 */

	/* USER CODE END OnStopJoinTimerEvent_1 */
	if (ActivationType == LORAWAN_DEFAULT_ACTIVATION_TYPE)
	{
		tx_thread_resume(&Thd_LoraStopJoinId);
	}
	/* USER CODE BEGIN OnStopJoinTimerEvent_Last */
	//  HAL_GPIO_WritePin(SNSR_PWR_EN_1_GPIO_Port, SNSR_PWR_EN_1_Pin, GPIO_PIN_RESET); /* LED_BLUE */
	//  HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin, GPIO_PIN_RESET); /* LED_GREEN */
	//  HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET); /* LED_RED */
	/* USER CODE END OnStopJoinTimerEvent_Last */
}

static void StoreContext(void)
{
	LmHandlerErrorStatus_t status = LORAMAC_HANDLER_ERROR;

	/* USER CODE BEGIN StoreContext_1 */

	/* USER CODE END StoreContext_1 */
	status = LmHandlerNvmDataStore();

	if (status == LORAMAC_HANDLER_NVM_DATA_UP_TO_DATE)
	{
		APP_LOG(TS_OFF, VLEVEL_M, "NVM DATA UP TO DATE\r\n");
	}
	else if (status == LORAMAC_HANDLER_ERROR)
	{
		APP_LOG(TS_OFF, VLEVEL_M, "NVM DATA STORE FAILED\r\n");
	}
	/* USER CODE BEGIN StoreContext_Last */

	/* USER CODE END StoreContext_Last */
}

static void OnNvmDataChange(LmHandlerNvmContextStates_t state)
{
	/* USER CODE BEGIN OnNvmDataChange_1 */

	/* USER CODE END OnNvmDataChange_1 */
	if (state == LORAMAC_HANDLER_NVM_STORE)
	{
		APP_LOG(TS_OFF, VLEVEL_M, "NVM DATA STORED\r\n");
	}
	else
	{
		APP_LOG(TS_OFF, VLEVEL_M, "NVM DATA RESTORED\r\n");
	}
	/* USER CODE BEGIN OnNvmDataChange_Last */

	/* USER CODE END OnNvmDataChange_Last */
}

static void OnStoreContextRequest(void *nvm, uint32_t nvm_size)
{
	/* USER CODE BEGIN OnStoreContextRequest_1 */

	/* USER CODE END OnStoreContextRequest_1 */
	/* store nvm in flash */
	if (FLASH_IF_Erase(LORAWAN_NVM_BASE_ADDRESS, FLASH_PAGE_SIZE) == FLASH_IF_OK)
	{
		FLASH_IF_Write(LORAWAN_NVM_BASE_ADDRESS, (const void *)nvm, nvm_size);
	}
	/* USER CODE BEGIN OnStoreContextRequest_Last */

	/* USER CODE END OnStoreContextRequest_Last */
}

static void OnRestoreContextRequest(void *nvm, uint32_t nvm_size)
{
	/* USER CODE BEGIN OnRestoreContextRequest_1 */

	/* USER CODE END OnRestoreContextRequest_1 */
	FLASH_IF_Read(nvm, LORAWAN_NVM_BASE_ADDRESS, nvm_size);
	/* USER CODE BEGIN OnRestoreContextRequest_Last */

	/* USER CODE END OnRestoreContextRequest_Last */
}

