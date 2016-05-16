/**
  ******************************************************************************
  * @file    MICOAppEntrance.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   Mico application entrance, addd user application functons and threads.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, MXCHIP Inc. SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2014 MXCHIP Inc.</center></h2>
  ******************************************************************************
  */ 

#include "mico.h"

#include "StringUtils.h"
#include "SppProtocol.h"
#include "cfunctions.h"
#include "cppfunctions.h"
#include "MICOAPPDefine.h"


#include "appUserEx.h"

#define app_log(M, ...) custom_log("APP", M, ##__VA_ARGS__)
#define app_log_trace() custom_log_trace("APP")

volatile ring_buffer_t  rx_buffer;
volatile uint8_t        rx_data[UART_BUFFER_LENGTH];

static app_context_t*  app_context;///globle value
static mico_Context_t* mico_context; ///globle value

extern void localTcpServer_thread(void *inContext);

extern void uartRecv_thread(void *inContext); ///收发串口数据

extern void popUartData_thread(void *inContext); //环形缓冲接受

extern void httpsReq_thread( void *inContext ); ///https_api请求

extern void vanwardSrv_thread( void *inContext );///连接到万和服务器

extern void getAppExData_thread(void *inContext); ///与app交互数据

extern void testCmdServ_thread(void *inContext);///测试与app通讯的接口

extern void cleanCloudData(mico_Context_t* mico_context,app_context_t* app_context);///清空token_did缓冲数据

extern void ota_thread( void *inContext ); ///用于OTA远程升级

extern void traceInit_thread(void *inContext); ///trace 输出调试追踪


extern OSStatus MICOStartBonjourService ( WiFi_Interface interface, app_context_t * const inContext );



app_context_t* getAppContext(void){
  return  app_context;
}


mico_Context_t* getMicoContext(void) {  
  return mico_context;
}


/* MICO system callback: Restore default configuration provided by application */
void appRestoreDefault_callback(void * const user_config_data, uint32_t size)
{
  
  UNUSED_PARAMETER(size);
  application_config_t* appConfig = user_config_data;
  appConfig->configDataVer = CONFIGURATION_VERSION;
  appConfig->localServerPort = LOCAL_PORT;
  appConfig->localServerEnable = true;
  appConfig->USART_BaudRate = 9600; // 115200;
  appConfig->remoteServerEnable = true;
  sprintf(appConfig->remoteServerDomain, DEAFULT_REMOTE_SERVER);
  appConfig->remoteServerPort = DEFAULT_REMOTE_SERVER_PORT;
  
  app_log("Default_setting and clean data...\r\n");

  cleanCloudData(getMicoContext(),getAppContext());
 
  
}

  

int application_start(void)
{
  app_log_trace(); 
  OSStatus err = kNoErr;
  mico_uart_config_t uart_config;
       
  printf("\r\n|---------Vanward_Firmware: %s -------------|\r\n",FIRM_VERSION);
  //join to cloud step=0 ini ,=1 eassyLink_getData,=2 https_req, =3 tcpip
  g_cloudStep=0;///初始化云连入的步骤序列
  g_getAppData=false;//清空,等待app的easylink数据
  
 //Create application context 
  app_context = ( app_context_t *)calloc(1, sizeof(app_context_t) );
  require_action( app_context, exit, err = kNoMemoryErr );

// Create mico system context and read application's config data from flash 
  mico_context = mico_system_context_init( sizeof( application_config_t) );
  app_context->appConfig = mico_system_context_get_user_data( mico_context );


  
  // mico system initialize 
  err = mico_system_init( mico_context );
  require_noerr( err, exit );
   
  
  /* Initialize service dsicovery */
  err = MICOStartBonjourService( Station, app_context );
  require_noerr( err, exit );
  
  /* Protocol initialize */
  sppProtocolInit( app_context );

  /*UART receive thread*/
  uart_config.baud_rate    = 9600;//app_context->appConfig->USART_BaudRate;
  uart_config.data_width   = DATA_WIDTH_8BIT;
  uart_config.parity       = NO_PARITY;
  uart_config.stop_bits    = STOP_BITS_1;
  uart_config.flow_control = FLOW_CONTROL_DISABLED;
  
  if(mico_context->flashContentInRam.micoSystemConfig.mcuPowerSaveEnable == true)
    uart_config.flags = UART_WAKEUP_ENABLE;
  else
    uart_config.flags = UART_WAKEUP_DISABLE;
  
  ring_buffer_init  ( (ring_buffer_t *)&rx_buffer, (uint8_t *)rx_data, UART_BUFFER_LENGTH );
  MicoUartInitialize( UART_FOR_APP, &uart_config, (ring_buffer_t *)&rx_buffer );
 err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "UART Recv", uartRecv_thread, STACK_SIZE_UART_RECV_THREAD, (void*)app_context );
  require_noerr_action( err, exit, app_log("ERROR: Unable to start the uart recv thread.") );

  
  /*
   ///>>>>>>>>>>----test----->>>>>>>>>>
   err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "OTA Thread", ota_thread, 0x800, (void*)app_context );
   require_noerr_action( err, exit, app_log("ERROR: Unable to start the op OTA thread.") );
   ///>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
   */
  
    // Pop data form uart thread 
   err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Pop UART Data", popUartData_thread, STACK_SIZE_LOCAL_TCP_SERVER_THREAD, (void*)app_context );
   require_noerr_action( err, exit, app_log("ERROR: Unable to start the op UART Data thread.") );
      
    
   // debug trace thread 
   err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Trace Init Data", traceInit_thread, STACK_SIZE_DEBUGTRACE_THREAD, (void*)app_context );
   require_noerr_action( err, exit, app_log("ERROR: Unable to start the op Trace Init thread.") );     
     
  // Local TCP server thread 
 if(app_context->appConfig->localServerEnable == true){
   err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Local Server", localTcpServer_thread, STACK_SIZE_LOCAL_TCP_SERVER_THREAD, (void*)app_context );
   require_noerr_action( err, exit, app_log("ERROR: Unable to start the local server thread.") );
 }

  // Remote TCP client thread   //tcpThread  //remoteTcpClient_thread
 if(app_context->appConfig->remoteServerEnable == true){
   err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "VanwardSrv", vanwardSrv_thread , STACK_SIZE_VANWARDSRV_THREAD, (void*)app_context );
   require_noerr_action( err, exit, app_log("ERROR: Unable to start the vanwardSrv thread.") );
 }
 
 
 /* http_client thread  */
   err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "Https Req",  httpsReq_thread, STACK_SIZE_HTTP_RECV_THREAD, (void*)app_context );
  require_noerr_action( err, exit, app_log("ERROR: Unable to start the http client thread.") );

/* get app data for parameter  */
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "GetAppExData", getAppExData_thread , STACK_SIZE_GETAPPDATA_SRV_THREAD, (void*)app_context );
  require_noerr_action( err, exit, app_log("ERROR: Unable to start the getAppExData thread.") );
   
//testCmdServ_thread  
  /* test cmd serv */
  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "TestCmdServ", testCmdServ_thread , STACK_SIZE_TESTCMDSERV_THREAD, (void*)app_context );
  require_noerr_action( err, exit, app_log("ERROR: Unable to start the testCmdServ thread.") );
exit:
  mico_rtos_delete_thread(NULL);
  return err;
}
