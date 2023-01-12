/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-07-13     RT-Thread    first version
 */

#include <rtthread.h>
#include "flashwork.h"
#include "device.h"
#include "key.h"
#include "led.h"
#include "wifi-uart.h"
#include "heart.h"
#include "board.h"
#include "protocol.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

int main(void)
{
    RadioID_Init();
    Flash_Init();
    Led_Init();
    Factory_Detect();
    Key_Reponse_Init();
    Button_Init();
    Sync_Init();
    RF_Init();
    Heart_Init();
    WiFi_Init();
    while (1)
    {
        rt_thread_mdelay(1000);
    }
}
