/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-07-17     Rick       the first version
 */
#include "wifi.h"
#include "wifi-api.h"
#include "flashwork.h"
#include "stdio.h"
#include "wifi-service.h"
#include "radio_encoder.h"

#define DBG_TAG "WIFI-API"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

char *door_pid = {"emnzq3qxwfplx7db"};
char *slave_pid = {"lnbkva6cip8dw7vy"};
char *main_pid = {"q3xnn9yqt55ifaxm"};

char *enterprise_door_pid = {"h2hotiru0zxtoyyg"};
char *enterprise_slave_pid = {"4zfihwwfbsvowhhk"};
char *enterprise_main_pid = {"kgywsygcm7zwdzgk"};

Remote_Info Remote_Device={0};
extern Device_Info Global_Device;

static uint8_t Sync_Start = 0;
static uint8_t Sync_Counter = 1;
static uint8_t Sync_Position = 1;
static rt_sem_t Sync_Once_Sem = RT_NULL;
static rt_thread_t Sync_t = RT_NULL;
static rt_timer_t Sync_Next_Timer = RT_NULL;
static rt_timer_t Sync_Timeout_Timer = RT_NULL;

void Secure_Sync(void)//C1 00 01
{
    if(DeviceType_Read())
    {
        unsigned short length = 0;
        length = set_wifi_uart_byte(length, ALARM_STATE_SET_SUBCMD); //写入子命令0x00
        length = set_wifi_uart_byte(length, 1);
        wifi_uart_write_frame(SECURITY_PROTECT_ALARM_CMD, MCU_TX_VER, length);
    }
}
void WariningUpload(uint32_t from_id,uint32_t device_id,uint8_t type,uint8_t value)
{
    unsigned char *device_id_buf = rt_malloc(16);
    unsigned char *from_id_buf = rt_malloc(16);
    rt_sprintf(device_id_buf,"%ld",device_id);
    rt_sprintf(from_id_buf,"%ld",from_id);
    if(device_id>0)
    {
        switch(type)
        {
            case 0://掉线
               mcu_dp_bool_update(103,value,device_id_buf,my_strlen(device_id_buf)); //BOOL型数据上报;
               break;
            case 1://漏水
                if(value)
                {
                    mcu_dp_bool_update(104,0,device_id_buf,my_strlen(device_id_buf)); //VALUE型数据上报;
                }
                mcu_dp_enum_update(1,value,device_id_buf,my_strlen(device_id_buf)); //BOOL型数据上报;
               break;
            case 2://电量
                mcu_dp_enum_update(102,value,device_id_buf,my_strlen(device_id_buf)); //BOOL型数据上报;
               break;
            case 3://掉落
                mcu_dp_bool_update(104,value,device_id_buf,my_strlen(device_id_buf)); //VALUE型数据上报;
               break;
        }
    }
    else
    {
        switch(type)
        {
            case 0://自检
                if(value == 0)
                {
                    mcu_dp_bool_update(DPID_VALVE1_CHECK_FAIL,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                    mcu_dp_bool_update(DPID_VALVE1_CHECK_SUCCESS,1,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                }
                else if(value == 1)
                {
                    mcu_dp_bool_update(DPID_VALVE2_CHECK_FAIL,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                    mcu_dp_bool_update(DPID_VALVE2_CHECK_SUCCESS,1,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                }
                else if(value == 2)
                {
                    mcu_dp_bool_update(DPID_TEMP_STATE,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                    mcu_dp_bool_update(DPID_LINE_STATE,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                    mcu_dp_bool_update(DPID_VALVE1_CHECK_FAIL,1,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                }
                else if(value == 3)
                {
                    mcu_dp_bool_update(DPID_TEMP_STATE,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                    mcu_dp_bool_update(DPID_LINE_STATE,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                    mcu_dp_bool_update(DPID_VALVE2_CHECK_FAIL,1,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                }
               break;
            case 1://漏水
                if(value)
                {
                    mcu_dp_bool_update(DPID_VALVE1_CHECK_FAIL,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                    mcu_dp_bool_update(DPID_VALVE2_CHECK_FAIL,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                    mcu_dp_bool_update(DPID_TEMP_STATE,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                    mcu_dp_bool_update(DPID_LINE_STATE,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                }
                mcu_dp_bool_update(DPID_DEVICE_ALARM,value,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
               break;
            case 2://掉落
                mcu_dp_bool_update(DPID_LINE_STATE,value,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
               break;
            case 3://NTC
                if(value)
                {
                    mcu_dp_bool_update(DPID_LINE_STATE,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
                }
                mcu_dp_bool_update(DPID_TEMP_STATE,value,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
               break;
        }
    }
	Secure_Sync();//C1 00 01
    rt_free(device_id_buf);
    rt_free(from_id_buf);
}
void CloseWarn_Main(uint32_t device_id)
{
    unsigned char *from_id_buf = rt_malloc(16);
    rt_sprintf(from_id_buf,"%ld",device_id);
    mcu_dp_bool_update(DPID_DEVICE_ALARM,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
    mcu_dp_bool_update(DPID_DELAY_STATE,0,from_id_buf,my_strlen(from_id_buf)); //VALUE型数据上报;
    LOG_I("CloseWarn_Main ID is %ld\r\n",device_id);
    rt_free(from_id_buf);
}
void InitWarn_Main(uint32_t device_id)
{
    unsigned char *from_id_buf = rt_malloc(16);
    rt_sprintf(from_id_buf,"%ld",device_id);
    mcu_dp_bool_update(DPID_SELF_ID,device_id,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
    mcu_dp_bool_update(DPID_DEVICE_ALARM,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
    mcu_dp_bool_update(DPID_VALVE1_CHECK_FAIL,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
    mcu_dp_bool_update(DPID_VALVE2_CHECK_FAIL,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
    mcu_dp_bool_update(DPID_TEMP_STATE,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
    mcu_dp_bool_update(DPID_LINE_STATE,0,from_id_buf,my_strlen(from_id_buf)); //BOOL型数据上报;
    LOG_I("InitWarn_Main ID is %ld\r\n",device_id);
    rt_free(from_id_buf);
}
void CloseWarn_Slave(uint32_t device_id)
{
    unsigned char *from_id_buf = rt_malloc(16);
    rt_sprintf(from_id_buf,"%ld",device_id);
    mcu_dp_enum_update(1,0,from_id_buf,my_strlen(from_id_buf));
    rt_free(from_id_buf);
}
void Remote_Delete(uint32_t device_id)
{
    unsigned char *id_buf = rt_malloc(16);
    rt_sprintf(id_buf,"%ld",GetBindID(device_id));
    GatewayDataEnqueue(GetBindID(device_id),device_id,0,6,0);
    LOG_I("Remote_Delete Success %ld\r\n",device_id);
    Del_Device(device_id);
    if(GetDoorValid(device_id) == RT_EOK)
    {
        mcu_dp_value_update(DPID_DOOR_ID,0,id_buf,my_strlen(id_buf));
    }
    rt_free(id_buf);
}
void Slave_Rssi_Report(uint32_t device_id,uint8_t rssi)
{
    Flash_Set_Rssi(device_id,rssi);
    char *Buf = rt_malloc(16);
    rt_sprintf(Buf,"%ld",device_id);
    mcu_dp_enum_update(101,rssi,Buf,my_strlen(Buf));
    LOG_I("Slave_Rssi_Report Device ID is %ld,rssi level is %d\r\n",device_id,rssi);
    rt_free(Buf);
}
void Gateway_ID_Upload(uint32_t gateway_id)
{
    uint8_t gw_id[]={"0000"};
    mcu_dp_value_update(101,gateway_id,gw_id,my_strlen(gw_id));
    LOG_I("Gateway_ID_Upload ID is %ld\r\n",gateway_id);
}
void MotoStateUpload(uint32_t device_id,uint8_t state)
{
    Remote_Delay_WiFi(device_id,0);
    Flash_Set_Moto(device_id,state);
    char *Buf = rt_malloc(16);
    LOG_I("MotoUpload State is %d,device_id is %ld\r\n",state,device_id);
    rt_sprintf(Buf,"%ld",device_id);
    mcu_dp_bool_update(DPID_DEVICE_STATE,state,Buf,my_strlen(Buf));
    rt_free(Buf);
}
void DoorControlUpload(uint32_t device_id,uint8_t state)
{
    char *id_str = rt_malloc(16);
    LOG_I("DoorUpload %ld upload %d\r\n",device_id,state);
    rt_sprintf(id_str,"%ld",device_id);
    if(GetDoorValid(device_id) == RT_EOK)
    {
        mcu_dp_bool_update(104,state,id_str,my_strlen(id_str));
    }
    rt_free(id_str);
}
void Device_Add2Flash_Wifi(uint32_t device_id,uint32_t from_id)
{
    if(device_id>=10000000 && device_id<20000000)
    {
        if(Flash_Get_Key_Valid(device_id) == RT_ERROR)
        {
            MainAdd_Flash(device_id);
        }
        if(Remote_Get_Key_Valid(device_id) == RT_ERROR)
        {
            Flash_Set_UploadFlag(device_id,0);
            Main_Add_WiFi(device_id);
        }
    }
    if(device_id>=20000000 && device_id<30000000)
    {
        if(Flash_Get_Key_Valid(device_id) == RT_ERROR)
        {
            SlaveAdd_Flash(device_id,from_id);
        }
        if(Remote_Get_Key_Valid(device_id) == RT_ERROR)
        {
            Flash_Set_UploadFlag(device_id,0);
            Slave_Add_WiFi(device_id);
        }
    }
    else if(device_id>=30000000 && device_id<40000000)
    {
        if(Flash_Get_Key_Valid(device_id) == RT_ERROR)
        {
            DoorAdd_Flash(device_id,from_id);
        }
        if(Remote_Get_Key_Valid(device_id) == RT_ERROR)
        {
            Flash_Set_UploadFlag(device_id,0);
            Door_Add_WiFi(device_id);
        }
    }
}
uint8_t Set_Slave_Heart(uint32_t Main_ID,uint8_t heart)//数据载入到内存中
{
    uint16_t num = Global_Device.Num;
    if(!num)
    {
        return 0;
    }
    while(num)
    {
        if(Global_Device.Bind_ID[num] == Main_ID)
        {
            if(heart)
            {
                if(Flash_Get_Heart(Global_Device.ID[num]))
                {
                    wifi_slave_online(Global_Device.ID[num]);
                }
            }
            else
            {
                wifi_slave_offline(Global_Device.ID[num]);
            }
        }
        num--;
    }
    return 0;
}
void Local_Delete(uint32_t device_id)
{
    char *Buf = rt_malloc(16);
    char *Mainbuf = rt_malloc(16);
    rt_sprintf(Buf,"%ld",device_id);
    rt_sprintf(Mainbuf,"%ld",GetBindID(device_id));
    local_subdev_del_cmd(Buf);
    if(device_id>=30000000)
    {
        mcu_dp_value_update(108,0,Mainbuf,my_strlen(Mainbuf)); //BOOL型数据上报;
    }
    rt_free(Buf);
    rt_free(Mainbuf);
}
void Main_Add_WiFi(uint32_t device_id)
{
    char *Buf = rt_malloc(16);
    rt_sprintf(Buf,"%ld",device_id);
    local_add_subdev_limit(1,0,0x01);
    if(DeviceType_Read())
    {
        gateway_subdevice_add("1.0",enterprise_main_pid,0,Buf,10,0);
    }
    else
    {
        gateway_subdevice_add("1.0",main_pid,0,Buf,10,0);
    }
    LOG_I("Main_Add_WiFi ID is %d\r\n",device_id);
    rt_free(Buf);
}
void Upload_Main_ID(uint32_t device_id)
{
    char *Buf = rt_malloc(16);
    rt_sprintf(Buf,"%ld",device_id);
    mcu_dp_value_update(DPID_SELF_ID,device_id,Buf,my_strlen(Buf)); //BOOL型数据上报;
    mcu_dp_bool_update(DPID_DEVICE_STATE,Flash_Get_Moto(device_id),Buf,my_strlen(Buf)); //VALUE型数据上报;
    mcu_dp_enum_update(DPID_SIGN_STATE,Flash_Get_Rssi(device_id),Buf,my_strlen(Buf)); //VALUE型数据上报;
    LOG_I("Upload_Main_ID is %d\r\n",device_id);
    rt_free(Buf);
}
void Reset_Main_Warn(uint32_t device_id)
{
    char *Buf = rt_malloc(16);
    rt_sprintf(Buf,"%ld",device_id);
    mcu_dp_bool_update(DPID_DEVICE_ALARM,0,Buf,my_strlen(Buf)); //BOOL型数据上报;
    mcu_dp_bool_update(DPID_LINE_STATE,0,Buf,my_strlen(Buf)); //BOOL型数据上报;
    mcu_dp_bool_update(DPID_TEMP_STATE,0,Buf,my_strlen(Buf)); //BOOL型数据上报;
    LOG_I("Reset_Main_Warn ID is %d\r\n",device_id);
    rt_free(Buf);
}
void Slave_Add_WiFi(uint32_t device_id)
{
    char *Buf = rt_malloc(16);
    rt_sprintf(Buf,"%ld",device_id);
    local_add_subdev_limit(1,0,0x01);
    if(DeviceType_Read())
    {
        gateway_subdevice_add("1.0",enterprise_slave_pid,0,Buf,10,0);
    }
    else
    {
        gateway_subdevice_add("1.0",slave_pid,0,Buf,10,0);
    }
    LOG_I("Slave_Add_by WiFi ID is %d\r\n",device_id);
    rt_free(Buf);
}
void Upload_Slave_ID(uint32_t device_id,uint32_t from_id)
{
    char *id_str = rt_malloc(16);
    rt_sprintf(id_str,"%ld",device_id);
    mcu_dp_value_update(107,from_id,id_str,my_strlen(id_str)); //BOOL型数据上报;
    mcu_dp_enum_update(101,Flash_Get_Rssi(device_id),id_str,my_strlen(id_str)); //VALUE型数据上报;
    LOG_I("Upload_Slave_ID is %d\r\n",device_id);
    rt_free(id_str);
}
void Reset_Slave_Warn(uint32_t device_id)
{
    char *Buf = rt_malloc(16);
    rt_sprintf(Buf,"%ld",device_id);
    mcu_dp_enum_update(1,0,Buf,my_strlen(Buf)); //BOOL型数据上报;
    mcu_dp_bool_update(104,0,Buf,my_strlen(Buf)); //VALUE型数据上报;
    LOG_I("Reset_Slave_Value ID is %d\r\n",device_id);
    rt_free(Buf);
}
void Door_Add_WiFi(uint32_t device_id)
{
    char *Doorbuf = rt_malloc(16);
    rt_sprintf(Doorbuf,"%ld",device_id);
    local_add_subdev_limit(1,0,0x01);
    if(DeviceType_Read())
    {
        gateway_subdevice_add("1.0",enterprise_door_pid,0,Doorbuf,10,0);
    }
    else
    {
        gateway_subdevice_add("1.0",door_pid,0,Doorbuf,10,0);
    }
    LOG_I("Door_Add_WiFi ID is %d\r\n",device_id);
    rt_free(Doorbuf);
}
void Upload_Door_ID(uint32_t device_id,uint32_t from_id)
{
    char *Mainbuf = rt_malloc(16);
    char *Doorbuf = rt_malloc(16);
    rt_sprintf(Mainbuf,"%ld",from_id);
    rt_sprintf(Doorbuf,"%ld",device_id);
    if(GetDoorValid(device_id) == RT_EOK)
    {
        mcu_dp_value_update(107,from_id,Doorbuf,my_strlen(Doorbuf)); //BOOL型数据上报;
        mcu_dp_value_update(108,device_id,Mainbuf,my_strlen(Mainbuf)); //BOOL型数据上报;
        mcu_dp_enum_update(101,Flash_Get_Rssi(device_id),Doorbuf,my_strlen(Doorbuf)); //VALUE型数据上报;
    }
    LOG_I("Upload_Door_ID is %d\r\n",device_id);
    rt_free(Mainbuf);
    rt_free(Doorbuf);
}
void Remote_Delay_WiFi(uint32_t device_id,uint8_t state)
{
    char *Buf = rt_malloc(16);
    LOG_D("Remote_Delay_WiFi %d from %ld is upload\r\n",state,device_id);
    rt_sprintf(Buf,"%ld",device_id);
    mcu_dp_bool_update(106,state,Buf,my_strlen(Buf)); //VALUE型数据上报;
    rt_free(Buf);
}
void Door_Delay_WiFi(uint32_t main_id,uint32_t device_id,uint8_t state)
{
    char *Main_Buf = rt_malloc(16);
    char *Device_Buf = rt_malloc(16);
    LOG_I("Door_Delay_WiFi %d from %ld is upload\r\n",state,device_id);
    rt_sprintf(Main_Buf,"%ld",main_id);
    rt_sprintf(Device_Buf,"%ld",device_id);
    if(GetDoorValid(device_id) == RT_EOK)
    {
        mcu_dp_bool_update(105,state,Device_Buf,my_strlen(Device_Buf)); //VALUE型数据上报;
        mcu_dp_bool_update(106,state,Main_Buf,my_strlen(Main_Buf)); //VALUE型数据上报;
    }
    rt_free(Device_Buf);
    rt_free(Main_Buf);
}
void Warning_WiFi(uint32_t device_id,uint8_t state)
{
    InitWarn_Main(device_id);
}
void Moto_CloseRemote(uint32_t device_id)
{
    LOG_I("Main %d Moto is Remote close\r\n",device_id);
    GatewayDataEnqueue(device_id,0,0,2,0);
}
void Moto_OpenRemote(uint32_t device_id)
{
    LOG_I("Main %d Moto is Remote Open\r\n",device_id);
    GatewayDataEnqueue(device_id,0,0,2,1);
}
void Delay_CloseRemote(uint32_t device_id)
{
    LOG_I("Main %d Delay is Remote close\r\n",device_id);
    GatewayDataEnqueue(device_id,0,0,1,0);
}
void Delay_OpenRemote(uint32_t device_id)
{
    LOG_I("Main %d Delay is Remote Open\r\n",device_id);
    GatewayDataEnqueue(device_id,0,0,1,1);
}
void Main_Rssi_Report(uint32_t device_id,int rssi)
{
    uint8_t level = 0;
    char *id_buf = rt_malloc(16);
    rt_sprintf(id_buf,"%ld",device_id);
    if(rssi<-94)
    {
        level = 0;
    }
    else if(rssi>=-94 && rssi<-78)
    {
        level = 1;
    }
    else if(rssi>=-78)
    {
        level = 2;
    }
    Flash_Set_Rssi(device_id,level);
    mcu_dp_enum_update(DPID_SIGN_STATE,level,id_buf,my_strlen(id_buf));
    rt_free(id_buf);
    LOG_I("Main_Rssi_Report %d is upload,rssi is %d,level is %d\r\n",device_id,rssi,level);
}
void Ack_Report(uint32_t device_id)
{
    GatewayDataEnqueue(device_id,0,0,5,1);
    LOG_I("Ack_Report %d is upload\r\n",device_id);
}
void Self_Bind_Upload(uint32_t device_id)
{
    if(Flash_Get_UploadFlag(device_id)==0)
    {
        Flash_Set_UploadFlag(device_id,1);
        if(device_id>=10000000 && device_id<20000000)
        {
            Upload_Main_ID(device_id);
            Upload_Door_ID(GetDoorID(device_id),device_id);
            Reset_Main_Warn(device_id);
        }
        else if(device_id>=20000000 && device_id<30000000)
        {
            Reset_Slave_Warn(device_id);
            Upload_Slave_ID(device_id,GetBindID(device_id));
        }
        else if(device_id>=30000000 && device_id<40000000)
        {
            Upload_Door_ID(device_id,GetBindID(device_id));
        }
    }
}
void wifi_heart_upload(uint32_t device_id)
{
    char *id = rt_malloc(16);
    rt_sprintf(id,"%ld",device_id);
    heart_beat_report(id,0);
    rt_free(id);
}
void wifi_slave_online(uint32_t device_id)
{
    char *id_str = rt_malloc(32);
    rt_sprintf(id_str,"%ld",device_id);
    heart_beat_report(id_str,0);
    user_updata_subden_online_state(0,id_str,1,1);
    rt_free(id_str);
}
void wifi_slave_offline(uint32_t device_id)
{
    char *id_str = rt_malloc(32);
    rt_sprintf(id_str,"%ld",device_id);
    user_updata_subden_online_state(0,id_str,1,0);
    rt_free(id_str);
}
void wifi_heart_reponse(char *id_buf)
{
    uint32_t id = 0;
    id = atol(id_buf);
    Self_Bind_Upload(id);
    if(id>=20000000 && id<40000000)//如果是子设备
    {
        if(Flash_Get_Heart(GetBindID(id)) == 1 && Flash_Get_Heart(id) == 1)//检测子设备所属主控以及自己本身是否在线
        {
            wifi_slave_online(id);

        }
        else
        {
            wifi_slave_offline(id);
        }
    }
    else
    {
        if(Flash_Get_Heart(id))
        {
            wifi_slave_online(id);
        }
        else
        {
            wifi_slave_offline(id);
        }
    }
}
void Remote_Device_Add(uint32_t device_id)
{
    Remote_Device.ID[++Remote_Device.Num]=device_id;
    LOG_I("Remote_Device_Add ID is %ld,Num is %d",device_id,Remote_Device.Num);
}
void Remote_Device_Clear(void)
{
    LOG_D("Remote_Device_Clear\r\n");
    rt_memset(&Remote_Device,0,sizeof(Remote_Device));
}
uint8_t Remote_Get_Key_Valid(uint32_t Device_ID)//查询内存中的ID
{
    uint16_t num = Remote_Device.Num;
    if(!num)return RT_ERROR;
    while(num)
    {
        if(Remote_Device.ID[num]==Device_ID)return RT_EOK;
        num--;
    }
    return RT_ERROR;
}
uint8_t Remote_Device_Delete(uint32_t Device_ID)//查询内存中的ID
{
    uint16_t num = Remote_Device.Num;
    if(!num)return RT_ERROR;
    while(num)
    {
        if(Remote_Device.ID[num]==Device_ID)
        {
            Remote_Device.ID[num] = 0;
            return RT_EOK;
        }
        num--;
    }
    return RT_ERROR;
}
uint8_t Get_Next_Main(void)
{
    while(Sync_Counter--)
    {
        if(Get_Main_Valid(Global_Device.ID[Sync_Counter]) == RT_EOK)
        {
            Sync_Position = Sync_Counter;
            Global_Device.SyncRetry[Sync_Counter] = 0;
            return RT_EOK;
        }
    }
    return RT_ERROR;
}
void Sync_Finish(void)
{
    Sync_Start = 0;
}
void Sync_Request_Next(void)
{
    if(Get_Next_Main()==RT_EOK)
    {
        LOG_I("Sync is Done,Go Next one\r\n");
        rt_sem_release(Sync_Once_Sem);
    }
    else
    {
        Sync_Finish();
        LOG_I("Sync is Finish\r\n");
    }
}
void Sync_Next_Callback(void *parameter)
{
    Sync_Request_Next();
}
void Sync_Timeout_Callback(void *parameter)
{
    if(Global_Device.SyncRecv[Sync_Position]==0)
    {
        Global_Device.SyncRetry[Sync_Position]++;
        LOG_W("Sync_Request %d is fail,Retry num is %d\r\n",Global_Device.ID[Sync_Position],Global_Device.SyncRetry[Sync_Position]);
        rt_sem_release(Sync_Once_Sem);
    }
}
void Sync_t_Callback(void *parameter)
{
    while(1)
    {
        rt_sem_take(Sync_Once_Sem,RT_WAITING_FOREVER);
        Device_Add2Flash_Wifi(Global_Device.ID[Sync_Position],0);//检查远端列表以及本地并进行
        if(Global_Device.SyncRetry[Sync_Position]<3)
        {
            Sync_Download(Global_Device.ID[Sync_Position]);
        }
        else
        {
            Sync_Request_Next();
        }
    }
}
void Sync_Init(void)
{
    Sync_Once_Sem = rt_sem_create("Sync_Once_Sem", 0, RT_IPC_FLAG_FIFO);
    Sync_Next_Timer = rt_timer_create("Sync_Next", Sync_Next_Callback, RT_NULL, 10000, RT_TIMER_FLAG_ONE_SHOT|RT_TIMER_FLAG_SOFT_TIMER);
    Sync_Timeout_Timer = rt_timer_create("Sync_Timeout", Sync_Timeout_Callback, RT_NULL, 5000, RT_TIMER_FLAG_ONE_SHOT|RT_TIMER_FLAG_SOFT_TIMER);
    Sync_t = rt_thread_create("device_sync", Sync_t_Callback, RT_NULL, 2048, 10, 10);
    rt_thread_startup(Sync_t);
}
void Sync_Restart(void)
{
    Sync_Start = 1;
    Sync_Counter = Global_Device.Num;
    Sync_Position = Global_Device.Num;
    rt_timer_stop(Sync_Timeout_Timer);
    rt_timer_stop(Sync_Next_Timer);
    if(Get_Next_Main() == RT_EOK)
    {
        LOG_I("Sync is restart,Go for next %ld\r\n",Global_Device.ID[Sync_Position]);
        rt_sem_release(Sync_Once_Sem);
    }
    else
    {
        Sync_Finish();
        LOG_I("Sync is restart without valid device \r\n");
    }
}
void Sync_Refresh(uint32_t device_id)
{
    if(Sync_Start)
    {
        if(device_id == Global_Device.ID[Sync_Position])//校对ID
        {
            Global_Device.SyncRecv[Sync_Position] = 1;
            Global_Device.SyncRetry[Sync_Position] = 0;
            rt_timer_stop(Sync_Timeout_Timer);
            rt_timer_start(Sync_Next_Timer);
        }
    }
}
void Sync_Download(uint32_t ID)
{
    Global_Device.SyncRecv[Sync_Position] = 0;
    GatewayDataEnqueue(ID,0,0,4,0);
    LOG_I("Sync_Request %d is download\r\n",ID);
    rt_timer_start(Sync_Timeout_Timer);
}
