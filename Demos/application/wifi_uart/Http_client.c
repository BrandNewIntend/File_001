/**
******************************************************************************
* @file    http_client.c 
* @author  William Xu
* @version V1.0.0
* @date    21-May-2015
* @brief   http client demo to download ota bin
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2014 MXCHIP Inc.
*
*  Permission is hereby granted, free of charge, to any person obtaining a 
copy 
*  of this software and associated documentation files (the "Software"), to 
deal
*  in the Software without restriction, including without limitation the 
rights 
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is 
furnished
*  to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in
*  all copies or substantial portions of the Software.
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY,
*  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
OR 
*  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.
******************************************************************************
*/



/*
*************************************************************************
  http_client线程
  功能:
   1.固件首先检测是否存有deviceId和token,无代表设备还未激活,等待激活
   2.已激活的设备,直接请求https有效服务器domain
   3.另一线程和有效服务器domain建立连接通讯
*************************************************************************
*/

#include "MICO.h"
#include "HTTPUtils.h"
#include "SocketUtils.h"
#include "StringUtils.h"


#include "mico_platform.h"

#include "appUserEx.h"
#include "MiCOAPPDefine.h"///testing...





#define http_client_log(M, ...) custom_log("HTTP", M, ##__VA_ARGS__)

static OSStatus onReceivedData(struct _HTTPHeader_t * httpHeader, 
                               uint32_t pos, 
                               uint8_t *data, 
                               size_t len,
                               void * userContext);
static void onClearData( struct _HTTPHeader_t * inHeader, void * inUserContext );
extern void timeSyncToMCU(uint8_t hour,uint8_t min); ///时间同步到设备端


static mico_semaphore_t wait_sem = NULL;

typedef struct _http_context_t{
  char *content;
  uint64_t content_length;
} http_context_t;


static app_context_t* appContext;
static mico_Context_t* micoContext;

extern  uint8_t     g_registTime; //=1第一次登陆,=2超过1次

void simple_http_req( char* host, char* query );
void simple_https_req( char* host, char* query );



extern  void SetDeviceIDPoint(char* did);                
extern  void SetDeviceTokenPoint(char* token);


#define OLED_DISPLAY_ROW_1    0    // yellow
#define OLED_DISPLAY_ROW_2    2    // blue
#define OLED_DISPLAY_ROW_3    4    // blue
#define OLED_DISPLAY_ROW_4    6    // blue

#define OLED_DISPLAY_COLUMN_START    0    // colloum from left pos 0

#define OLED_DISPLAY_MAX_CHAR_PER_ROW    16   // max 16 chars each row
extern void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *chr);


//万和host:120.76.42.86
//自己host:192.168.31.200
///8080-http, 1443-https
#define HTTP_REQ_PORT   8080
#define HTTPS_REQ_PORT  443

#define WEBSITE    "api.vanwardsmart.com"


void simple_http_get( char* host, char* query );
void simple_https_get( char* host, char* query );


//GET /api/errors/413 HTTP/1.1\r\n

#define SIMPLE_GET_REQUEST2 \
    "GET / HTTP/1.1\r\n" \
    "Host: vanwardsmart.com\r\n" \
    "Connection: close\r\n" \
    "\r\n"
      
#define SIMPLE_GET_REQUEST \
    "POST api/device/getDomain HTTP/1.1\r\n" \
    "Host: www.vanwardsmart.com\r\n" \
    "Connection: close\r\n" \
    "\r\n"      

static void micoNotify_WifiStatusHandler( WiFiEvent status, void* const inContext )
{
  UNUSED_PARAMETER(inContext);
  switch (status) {
  case NOTIFY_STATION_UP:
    mico_rtos_set_semaphore( &wait_sem );
    break;
  case NOTIFY_STATION_DOWN:
    break;
  }
}




//////简单的检测一下json数据的格式是否合法
bool simplyJsonCheck(char* dataStr)
{
  uint8_t lDaKuo=0; //  {
  uint8_t rDaKuo=0;//  }
   uint8_t lZKuo=0;// [
   uint8_t rZKuo=0;// ]
   uint8_t yinHao=0; //引号
   uint8_t j;
   uint8_t len=strlen(dataStr);
   uint8_t temp;
   if(len<3) return false;
   if(*dataStr!='{') return false;
   
   for(j=0;j<len;j++){
   
     temp=*(dataStr+j);
     
       switch(temp){
       
       case '{': lDaKuo++; break;
       case '}': rDaKuo++; break;
       case '[': lZKuo++; break;
       case ']': rZKuo++; break;
       case '"': yinHao++; break;
       
       }
   
   }
     if(lDaKuo!=rDaKuo) return false; 
   
     if(lZKuo!=rZKuo) return false;
   
   if(yinHao%2==1) return false; //引号要成对出现!
   
    return true;
      
}



void httpsReq_thread( void *inContext ){
  
  MsgFrame msgFm;
  json_object* jsonObj=NULL;

  appContext=(app_context_t*)  inContext;
  micoContext=mico_system_context_get();
     
     //cleanCloudData(micoContext,appContext);/////testing.......

        while(!g_wifi_connected){
           mico_thread_msleep(80);
         }
      
        http_client_log( "Wifi connected take :%d ms", mico_get_time()-g_configTime  );
  
       g_configTime=mico_get_time();//重新Markdown 时间点

    if(0==isGotCloudData(micoContext,appContext) )
    {       
        
         http_client_log(" At the 1st step ...");
         g_cloudStep=1;
         
    }else
    {              
         http_client_log(" At the 2nd step ...");
         g_cloudStep=2;//已配网,直接请求
        goto secondTimeReq;
    }
       ////首次使用固件时候,需要初始化步骤
     
         while(!g_getAppData)  mico_thread_msleep(100); //等待app发送easylink数据
         
         http_client_log(" Get appEx Data ...");
         
        
          g_mac=micoContext->micoStatus.mac; //固件的MAC
          g_productId=PRODUCT_ID;
         
              
         jsonObj=json_tokener_parse(g_appData);
         
         json_object_object_foreach(jsonObj, key, val) {
         
             http_client_log("Key=%s,Value=%s",key,json_object_to_json_string(val));      
             
             if(!strcmp(key, "Token")){
                
                char *pChar;
                pChar=(char*)json_object_get_string(val);
                 SetDeviceTokenPoint(pChar);
                                 
             
             }else if(!strcmp(key, "Id")){
                  //user id
               g_userId=(char*)json_object_get_string(val);
                 
               
             }
             
         
         }///finish parse json   
             
     
firstTimeReq:////首次激活请求(用app发来的参数)
       g_registTime=1;
       http_client_log("___First_Time_Connect____");
       msgFm=httpReqJson(CMD_ACTIVE,"POST /api/user/activeDevice");    
       simple_https_get( WEBSITE,(char*) msgFm.pt ); 
       
       goto  exit_proc;
  
secondTimeReq:///再次domian请求(用保存在固件的参数)
         g_registTime=2;
         http_client_log("___Second_Time_Connect____");
    
         SetDeviceTokenPoint(appContext->appConfig->token);//设置token  
         SetDeviceIDPoint(appContext->appConfig->deviceId);//设置did
         msgFm=httpReqJson(CMD_DOMAIN,"POST /api/device/getDomain");
                   
         http_client_log("___Raw of request:\r\n%s",msgFm.pt);///show raw data
         
         simple_https_get( WEBSITE, (char*)msgFm.pt );
     
         
     
exit_proc:  
  if(jsonObj!=NULL){
     json_object_put(jsonObj);/*free memory*/   
     jsonObj=NULL;
    }
      http_client_log("https_req_ending... CloudStep=%d",g_cloudStep);
      mico_rtos_delete_thread(NULL);
      
      return; 

}



void simple_http_get( char* host, char* query )
{
  OSStatus err;
  int client_fd = -1;
  fd_set readfds;
  char ipstr[16];
  struct sockaddr_t addr;
  HTTPHeader_t *httpHeader = NULL;
  http_context_t context = { NULL, 0 };
  
  

  err = gethostbyname(host, (uint8_t *)ipstr, 16);
  require_noerr(err, exit);
  http_client_log("HTTP server address: host:%s, ip: %s", host, ipstr);

  /*HTTPHeaderCreateWithCallback set some callback functions */
  //1024
  httpHeader = HTTPHeaderCreateWithCallback( 1024, onReceivedData, onClearData, &context );
  require_action( httpHeader, exit, err = kNoMemoryErr );
  
  client_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  addr.s_ip = inet_addr( ipstr );
  addr.s_port = HTTP_REQ_PORT;
  err = connect( client_fd, &addr, sizeof(addr) ); 
  require_noerr_string( err, exit, "connect http server failed" );

  /* Send HTTP Request */
  send( client_fd, query, strlen(query), 0 );

  FD_ZERO( &readfds );
  FD_SET( client_fd, &readfds );
  
  select( client_fd + 1, &readfds, NULL, NULL, NULL );
  if( FD_ISSET( client_fd, &readfds ) )
  {
    /*parse header*/
    err = SocketReadHTTPHeader( client_fd, httpHeader );
    switch ( err )
    {
      case kNoErr:
        PrintHTTPHeader( httpHeader );
        err = SocketReadHTTPBody( client_fd, httpHeader );/*get body data*/
        require_noerr( err, exit );
        /*get data and print*/
        http_client_log( "Content Data: %s", context.content );
        break;
      case EWOULDBLOCK:
      case kNoSpaceErr:
      case kConnectionErr:
      default:
        http_client_log("ERROR: HTTP Header parse error: %d", err);
        break;
    }
  }

exit:
  http_client_log( "Exit: Client exit with err = %d, fd:%d", err, client_fd );
  SocketClose( &client_fd );
  HTTPHeaderDestory( &httpHeader );
  
  
}



void simple_https_get( char* host, char* query )
{
   OSStatus err=0; 
  int client_fd = -1;
  int ssl_errno = 0;
  mico_ssl_t client_ssl = NULL;
  fd_set readfds;
  char ipstr[16];
  struct sockaddr_t addr;
  HTTPHeader_t *httpHeader = NULL;
  http_context_t context = { NULL, 0 };
  int retryTm=5;

  err = gethostbyname(host, (uint8_t *)ipstr, 16);
  
begin_req:
  
  require_noerr(err, exit);
  http_client_log("HTTPS server address: host:%s, ip: %s", host, ipstr);

  /*HTTPHeaderCreateWithCallback set some callback functions */
  
    
  httpHeader = HTTPHeaderCreateWithCallback( 1024, onReceivedData, onClearData, &context );  
    
  require_action( httpHeader, exit, err = kNoMemoryErr );
  
  client_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
     
  addr.s_ip = inet_addr( ipstr );
  addr.s_port = HTTPS_REQ_PORT;
  err = connect( client_fd, &addr, sizeof(addr) ); 
  
    
  require_noerr_string( err, exit, "connect http server failed" );
  
        
     ssl_version_set(TLS_V1_2_MODE);////客户端和服务端都要设置SSL Version
     client_ssl = ssl_connect( client_fd, 0, NULL , &ssl_errno );
      
  
  require_string( client_ssl != NULL, exit, "ERROR: ssl disconnect" );


  /* Send HTTP Request */
  ssl_send( client_ssl, query, strlen(query) );
  

  FD_ZERO( &readfds );
  FD_SET( client_fd, &readfds );
  
  select( client_fd + 1, &readfds, NULL, NULL, NULL );
  
  if( FD_ISSET( client_fd, &readfds ) )
  {
    /*parse header*/
    err = SocketReadHTTPSHeader( client_ssl, httpHeader );
    switch ( err )
    {
      case kNoErr:
        PrintHTTPHeader( httpHeader );
        err = SocketReadHTTPSBody( client_ssl, httpHeader );/*get body data*/
        require_noerr( err, exit );
        /*get data and print*/
        http_client_log( "Content Data: %s", context.content );
        break;
      case EWOULDBLOCK:
      case kNoSpaceErr:
      case kConnectionErr:
      default:
        http_client_log("ERROR: HTTP Header parse error: %d", err);
        break;
    }
  }

exit:
  http_client_log( "Exit: Client exit with err = %d, fd:%d", err, client_fd );
  if( client_ssl ) ssl_close( client_ssl );
  SocketClose( &client_fd );
  HTTPHeaderDestory( &httpHeader );
  
  retryTm--; //请求出错重试
  if(err!=kNoErr && retryTm>0){
     sleep(2); 
    http_client_log( " https req_retry left: %d",retryTm);
     goto begin_req;
  }
  
}

/*one request may receive multi reply*/
static OSStatus onReceivedData( struct _HTTPHeader_t * inHeader, uint32_t inPos, uint8_t * inData, size_t inLen, void * inUserContext )
{
  
    char *pChar=NULL;
       
  OSStatus err = kNoErr;
  http_context_t *context = inUserContext;
  if( inHeader->chunkedData == false){ //Extra data with a content length value
    if( inPos == 0 ){
      context->content = calloc( inHeader->contentLength + 1, sizeof(uint8_t) );
      require_action( context->content, exit, err = kNoMemoryErr );
      context->content_length = inHeader->contentLength;
      
    }
    memcpy( context->content + inPos, inData, inLen );
  }else{ //extra data use a chunked data protocol
    http_client_log("This is a chunked data, %d", inLen);
    if( inPos == 0 ){
      context->content = calloc( inHeader->contentLength + 1, sizeof(uint8_t) );
      require_action( context->content, exit, err = kNoMemoryErr );
      context->content_length = inHeader->contentLength;
    }else{
      context->content_length += inLen;
      context->content = realloc(context->content, context->content_length + 1);
      require_action( context->content, exit, err = kNoMemoryErr );
    }
    memcpy( context->content + inPos, inData, inLen );
  }
  
  ////--------向万和https服务器请求激活返回--------------
    http_client_log("Https_Response:%s",inData);////////test 
           
  if(*inData!='{' || *(inData+inLen-1)!='}') ///非JSON格式退出
  {
    
     http_client_log("--Can not parse this data ---!");
     goto exit;
     
  }
         ///将api请求返回的数据分析,并储存到本地flash
    
          json_object* httpJson=json_tokener_parse(inData);
                             
           json_object_object_foreach(httpJson, key, val) {
              
             http_client_log("Resp_KEY:%s,VAL=%s",key,val);
             
             if(!strcmp(key, "DeviceId")){
               
                pChar=(char*)json_object_get_string(val);
                setDeviceId(micoContext,appContext,pChar);
                
                http_client_log("_____Feed_Did:%s",pChar);
                
               SetDeviceIDPoint(pChar);
                
               
                
             }else if(!strcmp(key, "Token")){
             
                pChar=(char*)json_object_get_string(val);
                setUserToken(micoContext,appContext,pChar);
                
                SetDeviceTokenPoint(pChar); 
                
                 http_client_log("_Feed_Token:%s",pChar);
             }else if(!strcmp(key,"NetTime")){
             
               int len=0; 
               int hour=0,min=0;
               pChar=(char*)json_object_get_string(val);
               len=strlen(pChar);
               if(len==19)//数据长度OK
               {
                 //格式 2016-04-21 11:22:33
                   hour=(*(pChar+11)-'0')*10+*(pChar+12)-'0';
                   min=(*(pChar+14)-'0')*10+*(pChar+15)-'0';
                   timeSyncToMCU(hour,min);///将时间同步到设备
               }
               http_client_log("_Feed_Time:%d:%d___[Len:%d]",hour,min,len);
               
             }
                                       
             if(!strcmp(key,"Domain")){
                  
                  http_client_log("___switch to cloud_step 3____");
                  g_cloudStep=3;/////却换到登录控制服务器!!
                  
                  
                  
             } 
             
         }
         if(httpJson!=NULL){
               json_object_put(httpJson);/*free memory*/   
               httpJson=NULL;
         }
exit:
  return err;
}

/* Called when HTTPHeaderClear is called */
static void onClearData( struct _HTTPHeader_t * inHeader, void * inUserContext )
{
  UNUSED_PARAMETER( inHeader );
  http_context_t *context = inUserContext;
  if( context->content ) {
    free( context->content );
    context->content = NULL;
  }
}



