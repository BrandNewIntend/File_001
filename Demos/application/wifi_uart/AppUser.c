
/********************
* ָ��Index
*  active  ����
*  login   ��¼
*********************/


/*

״̬����[ֻ��]:
  
  �ֶ�:  ��ֵ
  Code:   0- NoErro
  State:  0-������, 1-����
  Temp:   0--100 ��ǰ�¶�
  Appoint: 0-�ر�ԤԼ 1--����ԤԼ



���ò���[д]:
  
  �ֶ�:  ��ֵ
  SetTemp:  30--75 (Step:1)   �����¶�
  SetPower:  1,2,3      ���ù��� KW
  SetTimer:  00:00---24:00  �ַ���, Step:10min
  SetMode:   0--��ͨģʽ,  1--ҹ��ģʽ

*/

#include "MICO.h"
#include "MICOAppDefine.h"


#define appuser_log(M, ...) custom_log("appuser", M, ##__VA_ARGS__)
#define appuser_log_trace() custom_log_trace("appuser")


////�����ʽ�����ݵĺ궨��
#define  JS_GETDOMAIN   "{\"DeviceId\":\"%s\",\"Token\":\"%s\"}"
#define  JS_ACTIVE      "{\"UserId\":\"%s\",\"Token\":\"%s\",\"Mac\":\"%s\",\"ProductId\":\"%s\"}" 
#define  JS_LOGIN       "{\"Id\":\"%s\",\"Token\":\"%s\",%s}" 
#define  JS_UPDATA      "\"Status\":{\"Code\":\"%d\",\"OnOFF\":\"%d\",\"State\":\"%d\",\"TempCur\":\"%d\",\"Appoint\":\"%d\",\"TempSet\":\"%d\",\"Power\":\"%d\",\"Mode\":\"%d\",\"Timer\":\"%s\"}"


extern char g_appData[256];
uint8_t  g_comData[1024]={0};

char g_deviceId[37]={0};
char g_token[33]={0};
char* g_userId;
char* g_mac;
char* g_productId;
 

typedef struct {

  uint32_t len;
  uint8_t  *pt;
  
} MsgFrame;



typedef struct{
   uint8_t  code;
   uint8_t  onOFF;
   uint8_t  hot;
   uint8_t  tempCur;
   uint8_t  appoint;
   
   uint8_t  tempSet;
   uint8_t  power;
   uint8_t  mode;
   uint8_t  timer[6]; //00:00��β��
   
} DeviceObj;


DeviceObj g_device_cloudObj; ///����cloud��
DeviceObj g_device_mcuObj;  ////����mcu��

typedef enum {
  CMD_PING   =0, ////������
  CMD_ACTIVE =1, /// ����https����
  CMD_DOMAIN =2,  /// ����Domain 
  CMD_REGIST =0|(1<<13),
  CMD_LOGIN  =1|(1<<13),
  CMD_UPDATA =2|(1<<13), ///�豸�����ϱ�״̬����

} CmdIndexer ;


MsgFrame wifi2appCmd(DeviceObj obj);



void dumphex(char*str,int len)
{
  
  int i=0;
  printf("\n DumpHex: ");
  
  while(i<len){
     
     printf("%02X ",*(str+i));
     i++;
     
  }
   printf("[LEN]: %d \r\n",len); 
   fflush(stdout);
  
             
}



void Dg(int stepNum){
  printf("\n____Debug<%d> ## ",stepNum);
}



void SetDeviceIDPoint(char* did){
    ///Ϊ�˷�ֹ��ͬ�̼߳��ָ������,ͨ������������did
   int len=strlen(did);
   if(len<10 || len>38){
     printf("__device id data leng[%d] erro ... ...",len);
     return;
   }
   memcpy(g_deviceId,did,len);
   g_deviceId[len]=0;
}    

void SetDeviceTokenPoint(char* token){
  
   int len=strlen(token);
   if(len<10 || len>38){
     printf("__device token data leng[%d] erro ... ...",len);
     return;
   }
   memcpy(g_token,token,len);
   g_token[len]=0;
   
}


////���� json��������, �������ݳ���
uint16_t js_login(uint8_t offset,char* did,char* token,DeviceObj obj){
 
    uint16_t len=0;
    memset(g_appData,0,256); 
    
    char* pBuf=(char*)g_comData+offset;

        sprintf(g_appData,JS_UPDATA,
              obj.code,obj.onOFF,obj.hot,obj.tempCur,obj.appoint,obj.tempSet,obj.power,obj.mode,obj.timer);
       
     len=sprintf(pBuf,JS_LOGIN,did,token,g_appData);
     
     printf("__loginData:\n %s",pBuf);
       
     return len;
    
}



uint16_t js_updata(uint8_t offset,DeviceObj obj){
    
    uint16_t len=0;
    
   char* pBuf=(char*)g_comData+offset;
      
   *pBuf='{';
    pBuf++;
      
   len=sprintf(pBuf,JS_UPDATA,
            obj.code,obj.onOFF,obj.hot,obj.tempCur,obj.appoint,obj.tempSet,obj.power,obj.mode,obj.timer);
   
   *(pBuf+len)='}';
   *(pBuf+len+1)=0; //Ending
   
   len+=2;
    
   // printf("\n__Org:\n%s $$\n",pBuf);
   // dumphex(g_comData,len+12);
    
    
     
     return len;    

}


////���� json��������, �������ݳ���
uint32_t js_active(uint8_t offset,char* uid,char* token,char* mac,char* product_id){
 
    uint16_t len=0;
    
   char* pBuf=(char*)g_comData+offset;
    
    len=sprintf(pBuf,JS_ACTIVE,uid,token,mac,product_id);
     
     return len;
    

}


uint16_t js_getDomain(uint8_t offset,char* did, char* token){
  
    uint16_t len=0;
    
    char* pBuf=(char*)g_comData+offset;
  
    len=sprintf(pBuf,JS_GETDOMAIN,did,token);
     
    
    return len;

}




MsgFrame cloudCmder(uint16_t ver,uint32_t operation,uint32_t msgId){

   MsgFrame msgfm;
   uint32_t bodyLen=0;
   
   g_comData[0]=ver/0xff;
   g_comData[1]=ver%0xff;


   g_comData[2]=(uint8_t)(operation>>8);
   g_comData[3]=(uint8_t)operation;
   
   g_comData[4]=(uint8_t)(msgId>>24);
   g_comData[5]=(uint8_t)(msgId>>16);
   g_comData[6]=(uint8_t)(msgId>>8);
   g_comData[7]=(uint8_t)msgId;
   
   
   //int len=strlen((char*)g_deviceId);
   //printf("len=%d,did=%s",len,(char*)g_deviceId);
   //dumphex((char*)g_deviceId,len);
   
   switch(operation){
     
        case CMD_REGIST://first time register to cloud
           bodyLen=js_login(12,(char*)g_deviceId,(char*)g_token,g_device_cloudObj);
        break;
     
        case CMD_LOGIN://device login server
         bodyLen=js_login(12,(char*)g_deviceId,(char*)g_token,g_device_cloudObj);
       break;    
       
         case CMD_UPDATA:
          bodyLen=js_updata(12,g_device_cloudObj);
         break;
      

       default:
         break;
   
   }
   
   
   g_comData[8]=(uint8_t)(bodyLen>>24);
   g_comData[9]=(uint8_t)(bodyLen>>16);
   g_comData[10]=(uint8_t)(bodyLen>>8);
   g_comData[11]=(uint8_t)bodyLen; 
   
    msgfm.len=bodyLen+12;///�������ݵĳ���
    msgfm.pt=g_comData;
    g_comData[msgfm.len]=0; //��β��0
     
    return msgfm;
    
}



//Content-Type: application/json
MsgFrame httpReqJson(uint8_t httpCmd,uint8_t* headerUrl){

   MsgFrame msgfm;
     uint16_t offset=0;
     uint16_t datLen=0;
     
     ////ע��:��Content-Length ����000 ���������󸲸�,Ϊ�˹̶�����
     offset=sprintf((char*)g_comData,"%s HTTP/1.1\r\nHost: %s\r\nContent-type: application/json\r\nContent-Length: 000\r\n\r\n",headerUrl,"www.vanwardsmart.com");
   
   switch(httpCmd){
       
      case CMD_ACTIVE: ///active-http
        datLen=js_active(offset,(char*)g_userId,(char*)g_token,(char*)g_mac,(char*)g_productId);
       break;

      case CMD_DOMAIN:///get-domain
        datLen=js_getDomain(offset,(char*)g_deviceId,(char*)g_token);
       break;
       
       
     default:
       break;
       
     
   }
   
   sprintf((char*)g_comData+offset-7,"%03d",datLen);
   g_comData[offset-4]='\r'; //����0��
   
    msgfm.len=offset+datLen;
    msgfm.pt=g_comData;
   
    g_comData[msgfm.len]=0; //��β��0
    
   return msgfm;
   
   
   
}


MsgFrame wifi2appCmd(DeviceObj obj){

   MsgFrame msgfm;
   memset(g_appData,0,256);
   
   sprintf((char*)g_appData,"{\"Code\":\"%d\",\"State\":\"%d\",\"TempCur\":\"%d\",\"Appoint\":\"%d\",\"TempSet\":\"%d\",\"Power\":\"%d\",\"Mode\":\"%d\",\"Timer\":\"%s\"}" ,
              obj.code,obj.hot,obj.tempCur,obj.appoint,obj.tempSet,obj.power,obj.mode,obj.timer);
  

  msgfm.len=strlen((char const*)g_appData);
  msgfm.pt=(uint8_t*)g_appData;
  
  printf("\n_Format: ");
  printf(g_appData);
  
    printf("\n[Len]:%d ",msgfm.len);
    
      fflush(stdout);
      
  return msgfm;
}





void setUserToken(mico_Context_t* mico_context,app_context_t* app_context,uint8_t *strToken){

 
  app_context->appConfig = mico_system_context_get_user_data( mico_context );
   sprintf(app_context->appConfig->token,"%s",strToken);
     
     
    mico_system_context_update(mico_context);
     return;
  
  
}


void setDeviceId(mico_Context_t* mico_context,app_context_t* app_context,uint8_t *did){

  
  app_context->appConfig = mico_system_context_get_user_data( mico_context );
   sprintf(app_context->appConfig->deviceId,"%s",did);
          
    mico_system_context_update(mico_context);
}




char*  getUserToken(mico_Context_t* mico_context,app_context_t* app_context){
  
  app_context->appConfig = mico_system_context_get_user_data( mico_context );

    return app_context->appConfig->token;
  
}


char*  getDeviceId(mico_Context_t* mico_context,app_context_t* app_context){
  
    
  app_context->appConfig = mico_system_context_get_user_data( mico_context );

    return app_context->appConfig->deviceId;
  
}

/////�ж��Ƿ��һ�μ����豸 =0 δ����, =1�Ѽ���
uint8_t isGotCloudData(mico_Context_t* mico_context,app_context_t* app_context){
  
   ///������token���ַ���,δʹ�ù���flash��0xFF
   char ch;
   char *tempToken=getUserToken(mico_context,app_context);
   
   uint8_t i=0;
   uint8_t cnt=0;
   
   
   char *did=getDeviceId(mico_context,app_context);
   
  
   printf("\n__LOG_DID:%s__",did);
   printf("\n__LOG_Token:%s___",tempToken);
   
   for(i=0;i<36;i++){
     
     ch=*(tempToken+i);
     if(cnt>=3) return 0;
     if(ch==0xff) cnt++;
     
   }
   
   return 1;

}


////����ƶ˷�����������,һ���ڲ��Խ׶�ʹ��.
void cleanCloudData(mico_Context_t* mico_context,app_context_t* app_context){
  
    app_context->appConfig = mico_system_context_get_user_data( mico_context );
    
    if(app_context->appConfig->token){
     sprintf(app_context->appConfig->token,"%c%c%c%c",0xff,0xff,0xff,0xff); //��Ҫ�������ж�����һ��        
     mico_system_context_update(mico_context);
      printf("\n_____Cloud data is clean______");
    }else{
    
      printf("\n____Null pointer____");
    }
     
    
    
}






