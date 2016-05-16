/**
  ******************************************************************************
  * @file    SppProtocol.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   SPP protocol deliver any data received from UART to wlan and deliver
  * wlan data to UART.
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

#include "MICO.h"
#include "SppProtocol.h"
#include "SocketUtils.h"
#include "debug.h"
#include "appUserEx.h"



extern void sendToMCU(void);
extern int parseHex(uint8_t *inBuf,uint8_t len);
extern int g_uartDataOK; 
uint8_t  g_recvDeviceBuff[512]={0};///接收来自设备串口的缓冲数据


#define MAX_SOCK_MSG_LEN (10*1024)
int sockmsg_len = 0;
#define spp_log(M, ...) custom_log("SPP", M, ##__VA_ARGS__)
#define spp_log_trace() custom_log_trace("SPP")

void socket_msg_take(socket_msg_t*msg);
void socket_msg_free(socket_msg_t*msg);

 


OSStatus sppProtocolInit(app_context_t * const inContext)
{
  int i;
  
  spp_log_trace();
  (void)inContext;

  for(i=0; i < MAX_QUEUE_NUM; i++) {
    inContext->appStatus.socket_out_queue[i] = NULL;
  }
  mico_rtos_init_mutex(&inContext->appStatus.queue_mtx);
  return kNoErr;
}

OSStatus sppWlanCommandProcess(unsigned char *inBuf, int *inBufLen, int inSocketFd, app_context_t * const inContext)
{
  spp_log_trace();
  (void)inSocketFd;
  (void)inContext;
  OSStatus err = kUnknownErr;

  err = MicoUartSend(UART_FOR_APP, inBuf, *inBufLen);

  *inBufLen = 0;
  return err;
}


extern app_context_t* app_context;
////WIFI固件接收来自设备MCU的16进制指令,然后解析,发送JSON集合给云端服务器
////注意:若接收的数据过快,需要考虑Push进入消息队列另外线程Pop处理数据
OSStatus sppUartCommandProcess(uint8_t *inBuf, int inLen, app_context_t * const inContext)
{
  
     mico_Context_t *context;
    OSStatus err = kNoErr;
      MsgFrame msgfm;
      
      static uint32_t  tm32=0;
      
      uint32_t  lenTm=mico_get_time()-tm32;
         
               
      spp_log("Last__Timer:%d ms",lenTm);
      
      tm32=mico_get_time();
      
    //spp_log_trace();
      spp_log("[S:%d][Len:%d] RecvUART: ",g_cloudStep,inLen);
       dumphex(inBuf,inLen);
       return 0;//--->
    
      static bool gotFirstFrame=false;
      static int  frameOffset=0;
       
        
      if(inLen==32){///32字节才认为是完整字节
        g_uartDataOK=parseHex(inBuf,inLen); ///分析数据然后转成json发给云端
      }else{
      
      
            if(gotFirstFrame){
               ////合并成一个完整的数组
               gotFirstFrame=false;
               memcpy(g_recvDeviceBuff+frameOffset,inBuf,inLen);//小心溢出数组
               
               printf("\n___Fix [%d]:%d,%d__",frameOffset+inLen,frameOffset,inLen);//////??????
               
                g_uartDataOK=parseHex(g_recvDeviceBuff,frameOffset+inLen);
                
               
               
            
            }else{
               
                  memcpy(g_recvDeviceBuff,inBuf,inLen);
                  frameOffset=inLen;
                  gotFirstFrame=true;
            }
          
      }
                
        
        return 0;/////test

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  inLen=msgfm.len;///命令的长度参数转给变量inLen    

  int i;
  mico_queue_t* p_queue=NULL;
  socket_msg_t *real_msg;

  for(i=0; i < MAX_QUEUE_NUM; i++) {
    
    p_queue = inContext->appStatus.socket_out_queue[i];
    if(p_queue  != NULL ){
      break;
    }
  }
  if (p_queue == NULL)
    return kNoErr;
  
  if (MAX_SOCK_MSG_LEN < sockmsg_len)
    return kNoMemoryErr;
  real_msg = (socket_msg_t*)malloc(sizeof(socket_msg_t) - 1 + inLen);

  if (real_msg == NULL)
    return kNoMemoryErr;
  sockmsg_len += (sizeof(socket_msg_t) - 1 + inLen);

     
     //  memcpy(real_msg->data, inBuf, inLen);///将数据拷贝进real_msg
    memcpy(real_msg->data, msgfm.pt, msgfm.len);///将数据拷贝进real_msg
    real_msg->len = inLen; 
    real_msg->ref = 0; 
            
  
  mico_rtos_lock_mutex(&inContext->appStatus.queue_mtx);
  socket_msg_take(real_msg);
  for(i=0; i < MAX_QUEUE_NUM; i++) {
    p_queue = inContext->appStatus.socket_out_queue[i];
    if(p_queue  != NULL ){
      socket_msg_take(real_msg);
      if (kNoErr != mico_rtos_push_to_queue(p_queue, &real_msg, 0)) {
        socket_msg_free(real_msg);
    }
  }
  }        
  socket_msg_free(real_msg);
  mico_rtos_unlock_mutex(&inContext->appStatus.queue_mtx);
  return err;
  
}/////////////end of function sppUartCommandProcess////////////////////////


void socket_msg_take(socket_msg_t*msg)
{
    msg->ref++;
}

void socket_msg_free(socket_msg_t*msg)
{
    msg->ref--;
    if (msg->ref == 0) {
        sockmsg_len -= (sizeof(socket_msg_t) - 1 + msg->len);
        free(msg);
    
    }
}

int socket_queue_create(app_context_t * const inContext, mico_queue_t *queue)
{
    OSStatus err;
    int i;
    mico_queue_t *p_queue;
    
    err = mico_rtos_init_queue(queue, "sockqueue", sizeof(int), MAX_QUEUE_LENGTH);
    if (err != kNoErr)
        return -1;
    mico_rtos_lock_mutex(&inContext->appStatus.queue_mtx);
    for(i=0; i < MAX_QUEUE_NUM; i++) {
        p_queue = inContext->appStatus.socket_out_queue[i];
        if(p_queue == NULL ){
            inContext->appStatus.socket_out_queue[i] = queue;
            mico_rtos_unlock_mutex(&inContext->appStatus.queue_mtx);
            return 0;
        }
    }        
    mico_rtos_unlock_mutex(&inContext->appStatus.queue_mtx);
    mico_rtos_deinit_queue(queue);
    return -1;
}

int socket_queue_delete(app_context_t * const inContext, mico_queue_t *queue)
{
    int i;
    socket_msg_t *msg;
    int ret = -1;

    mico_rtos_lock_mutex(&inContext->appStatus.queue_mtx);
    // remove queue
    for(i=0; i < MAX_QUEUE_NUM; i++) {
        if (queue == inContext->appStatus.socket_out_queue[i]) {
            inContext->appStatus.socket_out_queue[i] = NULL;
            ret = 0;
        }
    }
    mico_rtos_unlock_mutex(&inContext->appStatus.queue_mtx);
    // free queue buffer
    while(kNoErr == mico_rtos_pop_from_queue( queue, &msg, 0)) {
        socket_msg_free(msg);
    }

    // deinit queue
    mico_rtos_deinit_queue(queue);
    
    return ret;
}

