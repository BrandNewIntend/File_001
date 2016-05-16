/**
  ******************************************************************************
  * @file    MICOAppDefine.h 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   This file create a TCP listener thread, accept every TCP client
  *          connection and create thread for them.
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

#pragma once

#include "MICO.h"
#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Demo C function call C++ function and C++ function call C function */
//#define MICO_C_CPP_MIXING_DEMO

/*User provided configurations*/
#define CONFIGURATION_VERSION               0x00000002 // if default configuration is changed, update this number
#define MAX_QUEUE_NUM                       6  // 1 remote client, 5 local server
#define MAX_QUEUE_LENGTH                    8  // each queue max 8 msg
#define LOCAL_PORT                          8080
#define DEAFULT_REMOTE_SERVER               "192.168.31.112" ///��Ҫ���ӵķ����ip
#define DEFAULT_REMOTE_SERVER_PORT          2300
#define UART_RECV_TIMEOUT                   20 //��ʱ��� org:500 [20-->]
#define UART_ONE_PACKAGE_LENGTH             1024
#define wlanBufferLen                       1024  
#define UART_BUFFER_LENGTH                  2048

#define LOCAL_TCP_SERVER_LOOPBACK_PORT      1000
#define REMOTE_TCP_CLIENT_LOOPBACK_PORT     1002
#define RECVED_UART_DATA_LOOPBACK_PORT      1003

#define BONJOUR_SERVICE                     "_easylink._tcp.local."

/* Define thread stack size */
#ifdef DEBUG
  #define STACK_SIZE_UART_RECV_THREAD           0x5A0 //0x2A0
  #define STACK_SIZE_POP_UART_DATA_THREAD       0x5A0  // Pop UART Data
  #define STACK_SIZE_LOCAL_TCP_SERVER_THREAD    0x300
  #define STACK_SIZE_LOCAL_TCP_CLIENT_THREAD    0x350
  #define STACK_SIZE_VANWARDSRV_THREAD          0x2500 //0x500 
  #define STACK_SIZE_HTTP_RECV_THREAD           0x2400//0x2500
  #define STACK_SIZE_GETAPPDATA_SRV_THREAD      0x500
  #define STACK_SIZE_TESTCMDSERV_THREAD         0x450 
#else
  #define STACK_SIZE_UART_RECV_THREAD           0x150
  #define STACK_SIZE_LOCAL_TCP_SERVER_THREAD    0x180
  #define STACK_SIZE_LOCAL_TCP_CLIENT_THREAD    0x200
  #define STACK_SIZE_REMOTE_TCP_CLIENT_THREAD   0x260  
  #define STACK_SIZE_HTTP_RECV_THREAD           0x500//0x2500  
  #define STACK_SIZE_TESTCMDSERV_THREAD         0x450   
#endif

typedef struct _socket_msg {
  int ref;
  int len;
  uint8_t data[1];
} socket_msg_t;

/*Application's configuration stores in flash*/
typedef struct
{
  uint32_t          configDataVer;
  uint32_t          localServerPort;

  /*local services*/
  bool              localServerEnable;
  bool              remoteServerEnable;
  char              remoteServerDomain[64];
  int               remoteServerPort;

  /*IO settings*/
  uint32_t          USART_BaudRate;
  
  /*LoginData for cloud*/
  char token[33];
  char deviceId[37];
  
} application_config_t;


/*Running status*/
typedef struct  {
  /*Local clients port list*/
  mico_queue_t*   socket_out_queue[MAX_QUEUE_NUM];
  mico_mutex_t    queue_mtx;
} current_app_status_t;

typedef struct _app_context_t
{
  /*Flash content*/
  application_config_t*     appConfig;

  /*Running status*/
  current_app_status_t      appStatus;
} app_context_t;


#ifdef __cplusplus
} /*extern "C" */
#endif


