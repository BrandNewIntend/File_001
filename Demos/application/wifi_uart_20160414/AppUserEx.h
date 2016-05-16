#ifndef  _APP_USEREX_H__ 
#define  _APP_USEREX_H__

typedef struct {

  uint32_t len;
  uint8_t  *pt;
  
} MsgFrame;

#include "MICOAPPDefine.h"

typedef struct{
   uint8_t  code;
   uint8_t  onOFF;
   uint8_t  hot;
   uint8_t  tempCur;
   uint8_t  appoint;
   
   uint8_t  tempSet;
   uint8_t  power;
   uint8_t  mode;
   uint8_t  timer[6]; //00:00结尾留
   
} DeviceObj;



typedef enum {
  CMD_PING   =0, ////心跳包
  CMD_ACTIVE =1, /// 请求https激活
  CMD_DOMAIN =2,  /// 请求纯Domain 
  CMD_REGIST =0|(1<<13),
  CMD_LOGIN  =1|(1<<13),
  CMD_UPDATA =2|(1<<13), ///设备主动上报状态数据

} CmdIndexer ;


extern DeviceObj g_device_cloudObj;///专用与接收云端数据形成的obj;
extern DeviceObj g_device_mcuObj;/////专用于接收设备数据形成的obj;
extern char* g_userId;

extern uint32_t  g_msgId;

extern char g_deviceId[37];
extern char g_token[33];
extern char* g_userId;
extern char* g_mac;
extern char* g_productId; 


///define in system_misc.c
extern int g_cloudStep; //join to cloud step=0 ini ,=1 eassyLink_getData,=2 https_req, =3 linking cloud =4 linked cloud
extern bool g_getAppData; //the flag app send data to firmware
extern char g_appData[256];
extern bool g_wifi_connected;///the flag for if wifi connected
extern uint32_t    g_configTime;///count how long it take frome easylink to cloud

extern app_context_t* getAppContext(void);///MICOAppEntrance.c
extern mico_Context_t* getMicoContext(void);
extern void dumphex(char*str,int len);
extern MsgFrame cloudCmder(uint16_t ver,uint32_t operation,uint32_t msgId);
extern MsgFrame httpReqJson(uint8_t httpCmd,uint8_t* headerUrl);
extern MsgFrame wifi2appCmd(DeviceObj obj);

extern void setUserToken(mico_Context_t* mico_context,app_context_t* app_context,uint8_t *strToken);
extern void setDeviceId(mico_Context_t* mico_context,app_context_t* app_context,uint8_t *did);
extern char*  getUserToken(mico_Context_t* mico_context,app_context_t* app_context);
extern char*  getDeviceId(mico_Context_t* mico_context,app_context_t* app_context);
extern uint8_t isGotCloudData(mico_Context_t* mico_context,app_context_t* app_context);
 
#ifndef _PRODUCT_ID_
  #define _PRODUCT_ID_
  #define PRODUCT_ID   "2e952421-9a3f-470e-a98b-7a8882da15ee" ///万和-电热 产品ID
  #define FIRM_VERSION  "V0.01"
#endif

#endif  // _APP_USEREX_H__