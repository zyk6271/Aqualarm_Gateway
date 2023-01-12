/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-22     Rick       the first version
 */
#ifndef RADIO_RADIO_ENCODER_H_
#define RADIO_RADIO_ENCODER_H_

typedef struct
{
    uint8_t Type;
    uint32_t Taget_Id;
    uint32_t Device_Id;
    uint8_t Counter;
    uint8_t Command;
    uint8_t Data;
}RF_Msg_t;

uint8_t DeviceType_Read(void);
uint32_t RadioID_Read(void);
void RadioID_Init(void);
void SlaveDataEnqueue(uint32_t target_id,uint8_t counter,uint8_t command,uint8_t data);//发送给终端的数据入队列
void SlaveDataUrgentEnqueue(uint32_t target_id,uint8_t counter,uint8_t command,uint8_t data);//发送给终端的数据入队列
void GatewayDataEnqueue(uint32_t target_id,uint32_t device_id,uint8_t rssi,uint8_t control,uint8_t value);//发送给网关的数据入队列
void GatewayDataUrgentEnqueue(uint32_t target_id,uint32_t device_id,uint8_t rssi,uint8_t control,uint8_t value);//发送给网关的数据入队列
void SlaveDataDequeue(RF_Msg_t Msg_433);//发送给终端的数据出队列
void GatewayDataDequeue(RF_Msg_t Msg_433);//发送给网关的数据出队列
void RadioDequeueTaskInit(void);

#endif /* RADIO_RADIO_ENCODER_H_ */
