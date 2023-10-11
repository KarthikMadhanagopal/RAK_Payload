/*
 * Payload_ver.h
 *
 *  Created on: Sep 23, 2023
 *      Author: 91951
 */

#ifndef INC_PAYLOAD_VER_H_
#define INC_PAYLOAD_VER_H_

#include "main.h"

#define ORCH_CONFIG_LENGTH 6

typedef struct {
	uint8_t id;
	uint8_t orch_id;
	uint8_t step;
	uint8_t next_step;
	uint8_t local_id;
	uint8_t acc_type;
	uint8_t condition;
	uint8_t post_delay;
} orch_config_t;

#define VALVE 0x01
#define MOTOR 0x02
#define LIGHT 0x03
#define GATE  0x04
#define FENCE 0x05

void triggerOrch(orch_config_t orch[], uint8_t size);

#endif /* INC_PAYLOAD_VER_H_ */
