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

#define DBG_TAG "RADIO_ENCODER"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static rt_mq_t rf_en_mq;
extern struct ax5043 rf_433;

rt_thread_t rf_encode_t = RT_NULL;
static struct rt_completion rf_txdone_sem;

uint32_t RadioID = 0;
uint32_t Self_Default_Id = 40000000;
uint8_t Self_Type = 0;

char radio_send_buf[255];

uint32_t RadioID_Read(void)
{
    return RadioID;
}

uint8_t DeviceType_Read(void)
{
    return Self_Type;
}

void SlaveDataEnqueue(uint32_t Taget_Id, uint8_t Counter, uint8_t Command, uint8_t Data)
{
    Radio_Send_Format Send_Buf = {0};

    Send_Buf.Type = 0;
	Send_Buf.Ack = 0;
    Send_Buf.Taget_ID = Taget_Id;
    Send_Buf.Counter = Counter;
    Send_Buf.Command = Command;
    Send_Buf.Data = Data;

    rt_mq_send(rf_en_mq, &Send_Buf, sizeof(Radio_Send_Format));
}

void SlaveDataUrgentEnqueue(uint32_t Taget_Id, uint8_t Counter, uint8_t Command, uint8_t Data)
{
    Radio_Send_Format Send_Buf = {0};

    Send_Buf.Type = 0;
    Send_Buf.Taget_ID = Taget_Id;
    Send_Buf.Counter = Counter;
    Send_Buf.Command = Command;
    Send_Buf.Data = Data;

    rt_mq_urgent(rf_en_mq, &Send_Buf, sizeof(Radio_Send_Format));
}

void GatewayDataEnqueue(uint32_t Taget_Id, uint8_t Payload_ID, uint8_t Rssi, uint8_t Command, uint8_t Data)
{
    Radio_Send_Format Send_Buf = {0};

    Send_Buf.Type = 1;
    Send_Buf.Taget_ID = Taget_Id;
    Send_Buf.Payload_ID = Payload_ID;
    Send_Buf.Rssi = Rssi;
    Send_Buf.Command = Command;
    Send_Buf.Data = Data;

    rt_mq_send(rf_en_mq, &Send_Buf, sizeof(Radio_Send_Format));
}

void GatewayDataUrgentEnqueue(uint32_t Taget_Id, uint8_t Payload_ID, uint8_t Rssi, uint8_t Command, uint8_t Data)
{
    Radio_Send_Format Send_Buf = {0};

    Send_Buf.Type = 1;
    Send_Buf.Taget_ID = Taget_Id;
    Send_Buf.Payload_ID = Payload_ID;
    Send_Buf.Rssi = Rssi;
    Send_Buf.Command = Command;
    Send_Buf.Data = Data;

    rt_mq_urgent(rf_en_mq, &Send_Buf, sizeof(Radio_Send_Format));
}

void SendPrepare(Radio_Send_Format Send)
{
    rt_memset(radio_send_buf, 0, sizeof(radio_send_buf));
    uint8_t check = 0;
    switch(Send.Type)
    {
    case 0://Slave
        Send.Counter++ <= 255 ? Send.Counter : 0;
        rt_sprintf(radio_send_buf, "{%08ld,%08ld,%03d,%02d,%d}", Send.Taget_ID, RadioID, Send.Counter, Send.Command, Send.Data);
        for (uint8_t i = 0; i < 28; i++)
        {
            check += radio_send_buf[i];
        }
        radio_send_buf[28] = ((check >> 4) < 10) ? (check >> 4) + '0' : (check >> 4) - 10 + 'A';
        radio_send_buf[29] = ((check & 0xf) < 10) ? (check & 0xf) + '0' : (check & 0xf) - 10 + 'A';
        radio_send_buf[30] = '\r';
        radio_send_buf[31] = '\n';
        break;
    case 1://GW
        rt_sprintf(radio_send_buf,"G{%08ld,%08ld,%08ld,%03d,%03d,%02d}G",Send.Taget_ID,RadioID,Send.Payload_ID,Send.Rssi,Send.Command,Send.Data);
        break;
    }
}

void send_done_callback(void)
{
    rt_completion_done(&rf_txdone_sem);
}

void rf_encode_entry(void *paramaeter)
{
    Radio_Send_Format Send_Data;
    while (1)
    {
        if (rt_mq_recv(rf_en_mq,&Send_Data, sizeof(Radio_Send_Format), RT_WAITING_FOREVER) == RT_EOK)
        {
            /*
             * Clear RF Flag
             */
            rt_completion_init(&rf_txdone_sem);
            /*
             * Start RF Send
             */
            SendPrepare(Send_Data);
            RF_Send(&rf_433,radio_send_buf, rt_strlen(radio_send_buf));
            /*
             * Wait RF TxDone
             */
            rt_completion_wait(&rf_txdone_sem,200);

        }
    }
}
uint32_t Get_RadioID(void)
{
    return RadioID;
}
void RadioID_Init(void)
{
    int *p;
    p=(int *)(0x0800FFF0);
    RadioID = *p;
    if(RadioID==0xFFFFFFFF || RadioID==0)
    {
        RadioID = Self_Default_Id;
    }
    if(RadioID >= 46000001 && RadioID <= 49999999)
    {
        Self_Type = 1;
    }
    LOG_I("System Version:%s,Radio ID:%ld,Device Type:%d\r\n",MCU_VER,RadioID_Read(),DeviceType_Read());
}
void RadioQueue_Init(void)
{
    rf_led(1);
    beep_power(1);
    rf_en_mq = rt_mq_create("rf_en_mq", sizeof(Radio_Send_Format), 10, RT_IPC_FLAG_PRIO);
    rf_encode_t = rt_thread_create("radio_send", rf_encode_entry, RT_NULL, 1024, 9, 10);
    if (rf_encode_t)rt_thread_startup(rf_encode_t);
}
