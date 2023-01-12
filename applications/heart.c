/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-07-27     Rick       the first version
 */
#include "rtthread.h"
#include "heart.h"
#include "flashwork.h"
#include "radio_encoder.h"
#include "wifi-api.h"

#define DBG_TAG "heart"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

rt_thread_t heart_t = RT_NULL;
extern Device_Info Global_Device;

uint8_t Main_Heart(uint32_t From_ID,uint8_t Heart)
{
    uint32_t index = GetIndex(From_ID);
    if(index == 0)return RT_ERROR;
    if(Global_Device.Heart[index] != Heart)//去重
    {
        Set_Slave_Heart(From_ID,Heart);//子设备全部操作
        Flash_Set_Heart(From_ID,Heart);//写入Flash
    }
    if(Heart)
    {
        Global_Device.HeartRecv[index] = 1;
        wifi_heart_upload(From_ID);
    }
}
uint8_t Device_Heart(uint32_t Device_ID,uint8_t Heart)
{
    uint32_t index = GetIndex(Device_ID);
    if(index == 0)return RT_ERROR;
    if(Global_Device.Heart[index] != Heart)//去重
    {
        Flash_Set_Heart(Device_ID,Heart);
    }
    if(Heart)
    {
        wifi_heart_upload(Device_ID);
    }
}
void heart_callback(void *parameter)
{
    while(1)
    {
        rt_thread_mdelay(15*60000);//检测周期
        uint8_t num = 1;
        while(num <= Global_Device.Num)
        {
            if(Get_Main_Valid(Global_Device.ID[num]) == RT_EOK)
            {
                LOG_D("Start Heart With %d,Retry num is %d\r\n",Global_Device.ID[num],Global_Device.HeartRetry[num]);
                Global_Device.HeartRecv[num] = 0;
                GatewayDataUrgentEnqueue(Global_Device.ID[num],0,0,3,0);//Send
                rt_thread_mdelay(2000);//心跳后等待周期
                if(Global_Device.HeartRecv[num])//RecvFlag
                {
                    Global_Device.HeartCount[num] = 0;
                    Global_Device.HeartRetry[num] = 0;
                    Main_Heart(Global_Device.ID[num],1);//主控心跳
                    rt_thread_mdelay(2000);//设备与设备之间的间隔
                    num++;
                }
                else
                {
                    if(Global_Device.HeartRetry[num] < 6)
                    {
                        LOG_D("Rerty Again times %d\r\n",Global_Device.HeartRetry[num]++);
                    }
                    else
                    {
                        if(Global_Device.HeartCount[num] < 5)
                        {
                            Global_Device.HeartCount[num]++;
                            LOG_D("%d Rerty Stop,HeartCount is %d\r\n",Global_Device.ID[num],Global_Device.HeartCount[num]);
                        }
                        else
                        {
                            Global_Device.HeartCount[num] = 0;
                            Main_Heart(Global_Device.ID[num],0);//主控离线
                            LOG_D("%d Offline\r\n",Global_Device.ID[num]);
                        }
                        Global_Device.HeartRetry[num] = 0;
                        num++;
                    }
                    rt_thread_mdelay(3000);//设备与设备之间的间隔
                }
            }
            else
            {
                num++;
            }
        }
    }
}
void Heart_Init(void)
{
    heart_t = rt_thread_create("heart", heart_callback, RT_NULL, 2048, 10, 10);
    if(heart_t!=RT_NULL)
    {
        rt_thread_startup(heart_t);
    }
}
