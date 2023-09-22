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
#include "Radio_Decoder.h"
#include "Radio_Encoder.h"
#include "Flashwork.h"
#include "led.h"
#include "key.h"
#include "pin_config.h"
#include "wifi-api.h"

#define DBG_TAG "RF_DE"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

void Device_Learn(Radio_Recv_Format buf)
{
    if(Flash_Get_Key_Valid(buf.From_ID) != RT_EOK)
    {
        if(Get_MainNums()!=RT_EOK)
        {
            learn_fail();
            return;
        }
    }
    if(buf.From_ID>=10000000 && buf.From_ID<20000000)
    {
        switch(buf.Data)
        {
        case 1:
            SlaveDataEnqueue(buf.From_ID,buf.Counter,3,2);
            break;
        case 2:
            LOG_I("Learn Success\r\n");
            Del_Device(buf.From_ID);
            Device_Add2Flash_Wifi(buf.From_ID,0);
            learn_success();
        }
    }
}
void NormalSolve(int rssi,uint8_t *rx_buffer,uint8_t rx_len)
{
    Radio_Recv_Format Rx_message;
    if(rx_buffer[rx_len]==0x0A&&rx_buffer[rx_len-1]==0x0D)
    {
        rt_sscanf((const char *)&rx_buffer[1],"{%d,%d,%d,%d,%d}",&Rx_message.Target_ID,&Rx_message.From_ID,&Rx_message.Counter,&Rx_message.Command,&Rx_message.Data);
        if(Rx_message.Target_ID == Get_RadioID())
        {
         if(Rx_message.From_ID == 98989898)
         {
             LOG_I("Factory Test verify ok,RSSI is %d\r\n",rssi);
             rf_refresh();
             if(rssi>-70)
             {
                 rf_led_factory(2);
             }
             else
             {
                 rf_led_factory(1);
            }
            return;
         }
         switch(Rx_message.Command)
         {
         case 3://学习
             Device_Learn(Rx_message);
             break;
         }
         rf_led(3);
         Main_Heart(Rx_message.From_ID,1);
        }
    }
}
void GatewaySyncSolve(int rssi,uint8_t *rx_buffer,uint8_t rx_len)
{
    Radio_Recv_Format Rx_message;
    if(rx_buffer[rx_len]=='A')
    {
        rt_sscanf((const char *)&rx_buffer[2],"{%d,%d,%d,%d,%d,%d,%d}",&Rx_message.ack,&Rx_message.type,&Rx_message.Target_ID,&Rx_message.From_ID,&Rx_message.Device_ID,&Rx_message.Rssi,&Rx_message.Data);
        if(Rx_message.Target_ID == Get_RadioID() && Flash_Get_Key_Valid(Rx_message.From_ID) == RT_EOK)
        {
            LOG_D("GatewaySyncSolve buf is %s,rssi is %d\r\n",rx_buffer,rssi);
            rf_led(3);
            if(Rx_message.ack)
            {
                GatewayDataEnqueue(Rx_message.From_ID,0,0,7,0);
            }
            Main_Rssi_Report(Rx_message.From_ID,rssi);
            Main_Heart(Rx_message.From_ID,1);
            switch(Rx_message.type)
            {
            case 1:
                Device_Heart(Rx_message.Device_ID,1);//子设备心跳
                Slave_Rssi_Report(Rx_message.Device_ID,Rx_message.Rssi);//rssi
                break;
            case 2:
                Local_Delete(Rx_message.Device_ID);
                Del_Device(Rx_message.Device_ID);//删除终端
                break;
            case 3://同步在线设备
                Sync_Refresh(Rx_message.From_ID);
                Device_Add2Flash_Wifi(Rx_message.Device_ID,Rx_message.From_ID);//增加终端
                Device_Heart(Rx_message.Device_ID,1);//子设备心跳
                Slave_Rssi_Report(Rx_message.Device_ID,Rx_message.Rssi);//rssi
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,2,Rx_message.Data);//终端电量
                break;
            case 4://删除全部
                Del_MainBind(Rx_message.From_ID);
                break;
            case 5://同步离线设备
                Sync_Refresh(Rx_message.From_ID);
                Device_Add2Flash_Wifi(Rx_message.Device_ID,Rx_message.From_ID);//增加终端
                Device_Heart(Rx_message.Device_ID,0);//子设备心跳
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,2,Rx_message.Data);//终端低电量
                break;
            case 6://添加设备
                Device_Add2Flash_Wifi(Rx_message.Device_ID,Rx_message.From_ID);//增加终端
                Device_Heart(Rx_message.Device_ID,1);
                Slave_Rssi_Report(Rx_message.Device_ID,Rx_message.Rssi);//心跳
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,2,Rx_message.Data);//终端低电量
                break;
            }
        }
    }
}
void GatewayWarningSolve(int rssi,uint8_t *rx_buffer,uint8_t rx_len)
{
    Radio_Recv_Format Rx_message;
    if(rx_buffer[rx_len]=='B')
    {
        rt_sscanf((const char *)&rx_buffer[2],"{%d,%d,%d,%d,%d,%d,%d}",&Rx_message.ack,&Rx_message.Target_ID,&Rx_message.From_ID,&Rx_message.Device_ID,&Rx_message.Rssi,&Rx_message.Command,&Rx_message.Data);
        if(Rx_message.Target_ID == Get_RadioID() && Flash_Get_Key_Valid(Rx_message.From_ID) == RT_EOK)
        {
            LOG_D("GatewayWarningSolve buf %s,rssi is %d\r\n",rx_buffer,rssi);
            rf_led(3);
            if(Rx_message.ack)
            {
                GatewayDataEnqueue(Rx_message.From_ID,0,0,7,0);
            }
            Main_Rssi_Report(Rx_message.From_ID,rssi);
            Main_Heart(Rx_message.From_ID,1);
            LOG_D("WariningUpload From ID is %ld,Device ID is %ld,type is %d,value is %d\r\n",Rx_message.From_ID,Rx_message.Device_ID,Rx_message.Command,Rx_message.Data);
            switch(Rx_message.Command)
            {
            case 1:
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,1,Rx_message.Data);//主控水警
                break;
            case 2:
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,0,Rx_message.Data);//主控阀门
                break;
            case 3:
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,2,Rx_message.Data);//主控测水线掉落
                break;
            case 4:
                Device_Heart(Rx_message.Device_ID,0);//子设备离线
                break;
            case 5:
                Device_Heart(Rx_message.Device_ID,1);//子设备心跳
                Slave_Rssi_Report(Rx_message.Device_ID,Rx_message.Rssi);//rssi
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,1,Rx_message.Data);//终端水警
                MotoStateUpload(Rx_message.From_ID,0);//主控开关阀
                break;
            case 6:
                Device_Heart(Rx_message.Device_ID,1);//子设备心跳
                Slave_Rssi_Report(Rx_message.Device_ID,Rx_message.Rssi);//rssi
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,2,Rx_message.Data);//终端低电量
                if(Rx_message.Data==2)//ultra low
                {
                    MotoStateUpload(Rx_message.From_ID,0);//主控开关阀
                }
                break;
            case 7:
                InitWarn_Main(Rx_message.From_ID);//报警状态
                break;
            case 8://NTC报警
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,3,Rx_message.Data);
                break;
            case 9://终端掉落
                WariningUpload(Rx_message.From_ID,Rx_message.Device_ID,3,Rx_message.Data);
                break;
            }
        }
    }
}
void GatewayControlSolve(int rssi,uint8_t *rx_buffer,uint8_t rx_len)
{
    Radio_Recv_Format Rx_message;
    if(rx_buffer[rx_len]=='C')
    {
        rt_sscanf((const char *)&rx_buffer[2],"{%d,%d,%d,%d,%d,%d,%d}",&Rx_message.ack,&Rx_message.Target_ID,&Rx_message.From_ID,&Rx_message.Device_ID,&Rx_message.Rssi,&Rx_message.Command,&Rx_message.Data);
        if(Rx_message.Target_ID == Get_RadioID() && Flash_Get_Key_Valid(Rx_message.From_ID) == RT_EOK)
        {
            LOG_D("GatewayControlSolve buf %s,rssi is %d\r\n",rx_buffer,rssi);
            rf_led(3);
            if(Rx_message.ack)
            {
                GatewayDataEnqueue(Rx_message.From_ID,0,0,7,0);
            }
            Main_Rssi_Report(Rx_message.From_ID,rssi);
            Main_Heart(Rx_message.From_ID,1);
            switch(Rx_message.Command)
            {
            case 1:
                MotoStateUpload(Rx_message.From_ID,Rx_message.Data);//主控开关阀
                if(Rx_message.Data == 0)
                {
                    CloseWarn_Main(Rx_message.From_ID);
                }
                break;
            case 2:
                Device_Heart(Rx_message.Device_ID,1);//子设备心跳
                Slave_Rssi_Report(Rx_message.Device_ID,Rx_message.Rssi);//rssi
                if(Rx_message.Data == 0 || Rx_message.Data == 1)
                {
                    MotoStateUpload(Rx_message.From_ID,Rx_message.Data);//主控开关阀
                    if(Rx_message.Data == 0)
                    {
                        CloseWarn_Slave(Rx_message.Device_ID);
                    }
                }
                else
                {
                    MotoStateUpload(Rx_message.From_ID,0);//主控开关阀
                }
                break;
            case 3:
                if(Rx_message.Device_ID)//Delay远程关闭
                {
                    Door_Delay_WiFi(Rx_message.From_ID,Rx_message.Device_ID,Rx_message.Data);
                    Device_Heart(Rx_message.Device_ID,1);//子设备心跳
                    Slave_Rssi_Report(Rx_message.Device_ID,Rx_message.Rssi);//rssi
                }
                break;
            case 4:
                MotoStateUpload(Rx_message.From_ID,Rx_message.Data);
                break;
            case 5:
                MotoStateUpload(Rx_message.From_ID,Rx_message.Data);//主控开关阀
                InitWarn_Main(Rx_message.From_ID);//报警状态
                if(Rx_message.ack)return;
                Ack_Report(Rx_message.From_ID);
                break;
            case 6:
                DoorControlUpload(Rx_message.Device_ID,Rx_message.Data);//主控开关阀
                Device_Heart(Rx_message.Device_ID,1);//子设备心跳
                Slave_Rssi_Report(Rx_message.Device_ID,Rx_message.Rssi);//rssi
                MotoStateUpload(Rx_message.From_ID,Rx_message.Data);//主控开关阀
                if(Rx_message.Data == 0)
                {
                    CloseWarn_Slave(Rx_message.Device_ID);
                }
                break;
            }
        }
    }
}
void rf433_rx_callback(int rssi,uint8_t *buffer,uint8_t len)
{
    switch(buffer[1])
    {
    case '{':
        NormalSolve(rssi,buffer,len);
        break;
    case 'A':
        GatewaySyncSolve(rssi,buffer,len);
        break;
    case 'B':
        GatewayWarningSolve(rssi,buffer,len);
        break;
    case 'C':
        GatewayControlSolve(rssi,buffer,len);
        break;
    }
}

