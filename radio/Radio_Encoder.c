/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-11-22     Rick       the first version
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>
#include "drv_spi.h"
#include <string.h>
#include "AX5043.h"
#include "Radio_Encoder.h"
#include "Radio_Common.h"
#include "led.h"
#include "protocol.h"

#define DBG_TAG "RF_EN"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

uint32_t Self_Id = 0;
uint32_t Self_Default_Id = 40000000;
uint8_t Self_Type = 0;

rt_mq_t rf_en_mq;
rt_thread_t Radio_Queue433 = RT_NULL;

extern struct ax5043 rf_433;

uint32_t RadioID_Read(void)
{
    return Self_Id;
}

uint8_t DeviceType_Read(void)
{
    return Self_Type;
}

void SlaveDataEnqueue(uint32_t target_id,uint8_t counter,uint8_t command,uint8_t data)//终端类型的数据入队列
{
    RF_Msg_t Msg_433 = {0};
    Msg_433.Type = 0;
    Msg_433.Taget_Id = target_id;
    Msg_433.Device_Id = 0;
    Msg_433.Counter = counter;
    Msg_433.Command = command;
    Msg_433.Data = data;
    rt_mq_send(rf_en_mq, &Msg_433, sizeof(RF_Msg_t));
}
void SlaveDataUrgentEnqueue(uint32_t target_id,uint8_t counter,uint8_t command,uint8_t data)//终端类型的数据入队列
{
    RF_Msg_t Msg_433 = {0};
    Msg_433.Type = 0;
    Msg_433.Taget_Id = target_id;
    Msg_433.Device_Id = 0;
    Msg_433.Counter = counter;
    Msg_433.Command = command;
    Msg_433.Data = data;
    rt_mq_urgent(rf_en_mq, &Msg_433, sizeof(RF_Msg_t));
}

void GatewayDataEnqueue(uint32_t target_id,uint32_t device_id,uint8_t rssi,uint8_t control,uint8_t value)//网关类型的数据入队列
{
    RF_Msg_t Msg_433 = {0};
    Msg_433.Type = 1;
    Msg_433.Taget_Id = target_id;
    Msg_433.Device_Id = device_id;
    Msg_433.Counter = rssi;
    Msg_433.Command = control;
    Msg_433.Data = value;
    rt_mq_send(rf_en_mq, &Msg_433, sizeof(RF_Msg_t));
}

void GatewayDataUrgentEnqueue(uint32_t target_id,uint32_t device_id,uint8_t rssi,uint8_t control,uint8_t value)//网关类型的数据入队列
{
    RF_Msg_t Msg_433 = {0};
    Msg_433.Type = 1;
    Msg_433.Taget_Id = target_id;
    Msg_433.Device_Id = device_id;
    Msg_433.Counter = rssi;
    Msg_433.Command = control;
    Msg_433.Data = value;
    rt_mq_urgent(rf_en_mq, &Msg_433, sizeof(RF_Msg_t));
}

void SlaveDataDequeue(RF_Msg_t Msg_433)//终端类型的数据出队列
{
    char send_buf[32] = {0};
    uint8_t check = 0;
    if(Msg_433.Counter<255)
    {
        Msg_433.Counter++;
    }
    else
    {
        Msg_433.Counter=0;
    }
    rt_sprintf(send_buf,"{%08ld,%08ld,%03d,%02d,%d}",Msg_433.Taget_Id,Self_Id,Msg_433.Counter,Msg_433.Command,Msg_433.Data);
    for(uint8_t i = 0 ; i < 28 ; i ++)
    {
        check += send_buf[i];
    }
    send_buf[28] = ((check>>4) < 10)?  (check>>4) + '0' : (check>>4) - 10 + 'A';
    send_buf[29] = ((check&0xf) < 10)?  (check&0xf) + '0' : (check&0xf) - 10 + 'A';
    send_buf[30] = '\r';
    send_buf[31] = '\n';
    RF_Send(&rf_433,send_buf,32);
    LOG_D("SlaveDataDequeue is %s\r\n",send_buf);
}

void GatewayDataDequeue(RF_Msg_t Msg_433)//网关类型的数据出队列
{
    char send_buf[48] = {0};
    rt_sprintf(send_buf,"G{%08ld,%08ld,%08ld,%03d,%03d,%02d}G",Msg_433.Taget_Id,Self_Id,Msg_433.Device_Id,Msg_433.Counter,Msg_433.Command,Msg_433.Data);
    RF_Send(&rf_433,send_buf,41);
    LOG_D("GatewayDataDequeue is %s\r\n",send_buf);
}
void RadioDequeue(void *paramaeter)
{
    LOG_D("Queue Init Success\r\n");
    while(1)
    {
        RF_Msg_t Msg_433 = {0};
        rt_mq_recv(rf_en_mq, &Msg_433, sizeof(RF_Msg_t), RT_WAITING_FOREVER);
        rt_thread_mdelay(50);
        switch(Msg_433.Type)
        {
        case 0:
            SlaveDataDequeue(Msg_433);
            break;
        case 1:
            GatewayDataDequeue(Msg_433);
            break;
        }
        rt_thread_mdelay(100);
    }
}
uint32_t Get_Self_ID(void)
{
    return Self_Id;
}
void RadioID_Init(void)
{
    int *p;
    p=(int *)(0x0800FFF0);
    Self_Id = *p;
    if(Self_Id==0xFFFFFFFF || Self_Id==0)
    {
        Self_Id = Self_Default_Id;
    }
    if(Self_Id >= 46000001 && Self_Id <= 49999999)
    {
        Self_Type = 1;
    }
    LOG_I("System Version:%s,Radio ID:%ld,Device Type:%d\r\n",MCU_VER,RadioID_Read(),DeviceType_Read());
}
void RadioDequeueTaskInit(void)
{
    rf_en_mq = rt_mq_create("rf_en_mq", sizeof(RF_Msg_t), 20, RT_IPC_FLAG_PRIO);
    Radio_Queue433 = rt_thread_create("Radio433_Dequeue", RadioDequeue, RT_NULL, 1024, 9, 10);
    if(Radio_Queue433)rt_thread_startup(Radio_Queue433);
}
