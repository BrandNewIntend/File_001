#include "MICO.h"
#include "appUserEx.h"

extern USED void PlatformEasyLinkButtonClickedCallback(void);
extern app_context_t* getAppContext(void);
extern void cleanCloudData(mico_Context_t* mico_context,app_context_t* app_context);


#define  WIFI2MCU_LEN  16
char     g_wifi2mcu[WIFI2MCU_LEN]={0}; ////WiFi固件-->阿里电热产品数据


///app-->cloud-->firmware--->device
/// Wifi固件 将数据 发送给 设备MCU
void sendToMCU(void){

   uint16_t sum=0;
   uint8_t  i;
   uint8_t hour;
   uint8_t min;
    g_wifi2mcu[0]=0x55;
    g_wifi2mcu[1]=0x55;
    g_wifi2mcu[2]=0x02; //1燃热 2电热 3采暖炉
    
     g_wifi2mcu[3]=0x03;//1主动刷新,3设置模式     
     g_wifi2mcu[4]=g_device_mcuObj.onOFF;
     g_wifi2mcu[5]=g_device_mcuObj.appoint;
     g_wifi2mcu[6]=g_device_mcuObj.mode;
     g_wifi2mcu[7]=g_device_mcuObj.power;
     g_wifi2mcu[8]=g_device_mcuObj.tempSet;
     
     
     //sscanf(g_device_mcuObj.timer,"%d:%d",&hour,&min);
     hour=(g_device_mcuObj.timer[0]-'0')*10+ (g_device_mcuObj.timer[1]-'0') ;
     min=(g_device_mcuObj.timer[3]-'0')*10+ (g_device_mcuObj.timer[4]-'0') ;  //标准格式: 02:03
     
     g_wifi2mcu[9]=hour; //appoint-hour
     g_wifi2mcu[10]=min;   //appoint-min 
    
    for(i=0;i<WIFI2MCU_LEN-2;i++){    
       sum+=g_wifi2mcu[i];
    }
    
    
    g_wifi2mcu[WIFI2MCU_LEN-2]=(char)(sum>>8);
    g_wifi2mcu[WIFI2MCU_LEN-1]=(char)(sum&0xFF);
    
    MicoUartSend(UART_FOR_APP, g_wifi2mcu , WIFI2MCU_LEN );
        
    
}


///服务器发送时间同步信息给MCU-?
void timeSyncToMCU(uint8_t hour,uint8_t min){

   uint8_t i;

    g_wifi2mcu[0]=0x55;
    g_wifi2mcu[1]=0x55;
    g_wifi2mcu[2]=0x02; //1燃热 2电热 3采暖炉
    g_wifi2mcu[3]=0x02;//2 时间同步
    
    g_wifi2mcu[4]=hour;
    g_wifi2mcu[5]=min;
    
    for(i=6;i<=13;i++){
      g_wifi2mcu[i]=0x00;
    }
    
    MicoUartSend(UART_FOR_APP, g_wifi2mcu , WIFI2MCU_LEN );    
    
    
}

                                 

void DebugFirmwareParam(uint8_t debugCmd){

  
    if(debugCmd==0xF0){
       ///查看DeviceId,Token
      custom_log("MsgDebug","%s DeviceID: %s\n   Dev_Token: %s","\r\n",g_deviceId,g_token);
      return ;
    }


}


/////对比上次数据的状态, 不对比当前温度!!!
/// =true 状态有变化,
bool  compareLastState(uint8_t *inBuf,uint8_t len){

    static uint8_t flTm=0; ///错误次数    
    uint8_t i;
    uint8_t errType=0;//校验失败的原因码
    uint16_t sum=0;
    uint16_t sumRes=0;

    uint8_t  dataType=0;
    uint8_t  moduleFunc=0;
    bool    flag_res=false;
    
    
   
    custom_log("toDevice","\n[S%d] UartRecv[%d]",g_cloudStep,len);
    
    trace("\r\n<i>[S%d]__RecvUart[%d]:",g_cloudStep,len);
    trace_hex(inBuf,len);   
    
    if(g_cloudStep>=3){
      dumphex(inBuf,len); ///设备未连接云,不用显示HEX
    }
    
    
     if(len!=32) 
     {
        return false ;   
     }
     
     if(*inBuf!=0xAA  || *(inBuf+1)!=0xAA) {
        
       return false;
     }
     
     for(i=0;i<len-2;i++){
     
         sum+=(uint8_t)inBuf[i];
     }
     
         sumRes=inBuf[len-2]*0x100+inBuf[len-1];
    
       
       if(sumRes==sum) 
         goto right_check;//校验正常
       else
          errType=3;
      
         
erro_check:       
          return false;        
right_check:
  
      dataType=inBuf[3];
      moduleFunc=inBuf[4];
      
      if(dataType==0x02){
      
        if(moduleFunc==0x01) //进入配网EasyLink模式
        {
           mico_Context_t* micoContext=mico_system_context_get();
           app_context_t* app_context=getAppContext();
           
            custom_log("REBOOT","\n----Enter config mode-------");
            cleanCloudData(micoContext,app_context);
            PlatformEasyLinkButtonClickedCallback ();
        }
        
        DebugFirmwareParam(moduleFunc);
        
          return false;
      }
      
      
      static uint8_t   hour,min;
      static uint32_t  timePoint=0;
      static uint8_t tempChangTm=0;///由于温度飘逸,记录改变的次数
      
            ///准备发往云端的对象结构
      if(g_device_cloudObj.onOFF!=inBuf[4])     { flag_res=true; goto updata_obj;} ////有状态变化      
      if(g_device_cloudObj.hot!=inBuf[5])       { flag_res=true; goto updata_obj;} 
      if(g_device_cloudObj.appoint!=inBuf[6])   { flag_res=true; goto updata_obj;} 
      if(g_device_cloudObj.mode!=inBuf[7])      { flag_res=true; goto updata_obj;} 
      if(g_device_cloudObj.power!=inBuf[8])     { flag_res=true; goto updata_obj;} 
      if(g_device_cloudObj.tempSet!=inBuf[9])   { flag_res=true; goto updata_obj;}       
      if(g_device_cloudObj.code!=inBuf[15])     { flag_res=true; goto updata_obj;} 
         
     
     if(hour!=inBuf[13] || min!=inBuf[14]) {
       
        hour=inBuf[13];
        min=inBuf[14];
       flag_res=true;
       goto updata_obj;
     }
     
     ///计算和上一帧的间隔时间,太密的话认为未变化
     if(g_device_cloudObj.tempCur!=inBuf[10]) { 
       
         if(mico_get_time()-timePoint<2000){
           
            tempChangTm++; //温度变化速度2秒才检测一次!!
            
          
            timePoint=mico_get_time();
            
            if(tempChangTm>10)  
            {
              flag_res=true;
              tempChangTm=0;
            }
            
            
         }else{
           
             tempChangTm=0;
             flag_res=true;
         }
            
         timePoint=mico_get_time();
     }
     
     
updata_obj:
     
       ///准备发往云端的对象结构
      g_device_cloudObj.onOFF=inBuf[4];
      g_device_cloudObj.hot=inBuf[5];
      g_device_cloudObj.appoint=inBuf[6];
      g_device_cloudObj.mode=inBuf[7];
      g_device_cloudObj.power=inBuf[8];
      g_device_cloudObj.tempSet=inBuf[9];
      g_device_cloudObj.tempCur=inBuf[10];
      
      sprintf(g_device_cloudObj.timer,"%02d:%02d",inBuf[13],inBuf[14]);  //为预约时间        
       
      g_device_cloudObj.code=inBuf[15]; 
          
      
      return flag_res; ///返回flag结果
            

}

////// 解析 设备mcu上报发送过来的数据, len=32
uint8_t  parseHex(uint8_t *inBuf,uint8_t len){
  
    
    static uint8_t flTm=0; ///错误次数
    uint8_t i;
    uint8_t errType=0;//校验失败的原因码
    uint16_t sum=0;
    uint16_t sumRes=0;

    uint8_t  dataType=0;
    uint8_t  moduleFunc=0;
    
    custom_log("UART_Get","\n---Parse Data Len:[%d]----",len);
    
 
    
     if(len!=32) 
     {
        errType=1;
       goto erro_check;     
     }
     
     if(*inBuf!=0xAA  || *(inBuf+1)!=0xAA) {
         errType=2;
       goto erro_check;
     }
     
     for(i=0;i<len-2;i++){
     
         sum+=(uint8_t)inBuf[i];
     }
     
         sumRes=inBuf[len-2]*0x100+inBuf[len-1];
    
       
       if(sumRes==sum) 
         goto right_check;//校验正常
       else
          errType=3;
      
         
erro_check:
           
          printf("\n[Device Data]: ErrTime:%d  ErrType:%d___.",flTm++,errType);
          //fflush(stdout);
          return 0;

        
right_check:
  
      dataType=inBuf[3];
      moduleFunc=inBuf[4];
      
      if(dataType==0x02){
      
        if(moduleFunc==0x01) //进入配网EasyLink模式
        {
           mico_Context_t* micoContext=mico_system_context_get();
           app_context_t* app_context=getAppContext();
           
            custom_log("REBOOT","\n----Enter config mode-------");
            cleanCloudData(micoContext,app_context);
            PlatformEasyLinkButtonClickedCallback ();
        }
        
        DebugFirmwareParam(moduleFunc);
        
        return 0;
      }
      
      
 
      ///准备发往云端的对象结构
      g_device_cloudObj.onOFF=inBuf[4];
      g_device_cloudObj.hot=inBuf[5];
      g_device_cloudObj.appoint=inBuf[6];
      g_device_cloudObj.mode=inBuf[7];
      g_device_cloudObj.power=inBuf[8];
      g_device_cloudObj.tempSet=inBuf[9];
      g_device_cloudObj.tempCur=inBuf[10];
      
      sprintf(g_device_cloudObj.timer,"%02d:%02d",inBuf[13],inBuf[14]);  //为预约时间        
       
      g_device_cloudObj.code=inBuf[15]; 
      
      printf("\r\n---Finish  parseInt Code=[%d]----",g_device_cloudObj.code);
      fflush(stdout);
      return 1;
             

}