/**
  ******************************************************************************
  * @file    UartRecv.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   This file create a UART recv thread.
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


extern uint32_t timeCostCount(void);    
extern bool  compareLastState(uint8_t *inBuf,uint8_t len);    
extern uint8_t  parseHex(uint8_t *inBuf,uint8_t len);
extern void    dumphex(char*str,int len);
extern  bool ringBuffer_init(int size);
extern  void ringBuffer_free(void);
extern  int  ringBuffer_getDataCount(void);
extern  int  ringBuffer_pushData(char *dat,int len);
extern  int  ring_buffer_popData(char* inBuf,int len);

#define uart_recv_log(M, ...) custom_log("UART RECV", M, ##__VA_ARGS__)
#define uart_recv_log_trace() custom_log_trace("UART RECV")

static size_t _uart_get_one_packet(uint8_t* buf, int maxlen);

#define LENG_BUFF  1024
char     g_uartBuffer[LENG_BUFF];
char     g_secondBuffer[LENG_BUFF];
extern   uint8_t  g_uartDataOK;///收到正确的uart数据包=1
extern   bool g_flagRecvSock;///=1接收到云端数据



bool compareData(char* str1,int len1, char *str2, int len2){

   int i;
   if(len1!=len2) return false;
   
   for(i=0;i<len1;i++){
   
    if( *(str1+i)!=*(str2+i))  return false;
   
   }
          
     return true;
   
}



void popUartData_thread(void *inContext){
  
  int       datNum,datCount,len;
  uint32_t   temp;
  bool      flag_dataChange=true;///=true立刻发送一帧

  uint32_t  overTm=0;//超时累计
  uint32_t  ping_interval=0;///30秒内无数据变化则发送ping!
  extern   uint32_t   g_msgId; ///消息ID,当app发信号时增1
  
  uart_recv_log("---pop uart data thread begin----");

  
  while(true){
  
    
    datCount=ringBuffer_getDataCount();//循环获取缓冲区的数据
         
    if(overTm>15){
                 
         overTm=0;
        
         len=ring_buffer_popData(g_uartBuffer,LENG_BUFF);
         g_uartBuffer[len]=0;
         
         if(g_flagRecvSock){
            g_flagRecvSock=false;///app发操作到设备,设备马上返回状态
            temp=timeCostCount();
            g_msgId++;
            uart_recv_log("_____Take %d ms flush uart status  MsgID:%d",temp,g_msgId);
         
         }
         
         flag_dataChange=compareLastState((uint8_t*)g_uartBuffer,len);
         
         if(flag_dataChange){
             ///有数据改变
              uart_recv_log("  ~~~~~~~~~~ Updata uart data ~~~~~~~~~~  ");
            g_uartDataOK=1;
             ping_interval=mico_get_time(); 
         
         }else{
           
             uart_recv_log(" count ping interval=%d ms",mico_get_time()-ping_interval ); ///test//////
         
           if( mico_get_time()-ping_interval>45000){
               ping_interval=mico_get_time(); 
               uart_recv_log("  ~~~~~~~~~~ Ping the cloud ~~~~~~~~~~  ");              
               g_uartDataOK=2;///心跳帧
              
           }
         }
    
                                    
          
        
    } ////if(overTm>15)
    
    msleep(2);
   
          if(datCount>0){   
             overTm++;
             if(datNum!=datCount){
                     overTm=0;
                     datNum=datCount;
             }    
          }
  
  }//////end while(1) //////////
  
exit:
   mico_rtos_delete_thread(NULL);
   return;
  
}


void uartRecv_thread(void *inContext)
{
  uart_recv_log_trace();

  int recvlen;
  uint8_t *inDataBuffer;
  
  inDataBuffer = malloc(UART_ONE_PACKAGE_LENGTH);
  require(inDataBuffer, exit);
  
  recvlen=ringBuffer_init(LENG_BUFF);
  
  if(recvlen==0) 
  {
    uart_recv_log("--- no enough memory to initial ---");
    goto exit;
  }
  
  while(true) {
    recvlen = _uart_get_one_packet(inDataBuffer, UART_ONE_PACKAGE_LENGTH);
    if (recvlen <= 0)
      continue; 
    //sppUartCommandProcess(inDataBuffer, recvlen, Context);
      ringBuffer_pushData((char*)inDataBuffer,recvlen);
  }
  
exit:
  if(inDataBuffer) free(inDataBuffer);
  ringBuffer_free();
  mico_rtos_delete_thread(NULL);
  
}

/* Packet format: BB 00 CMD(2B) Status(2B) datalen(2B) data(x) checksum(2B)
* copy to buf, return len = datalen+10
*/
size_t _uart_get_one_packet(uint8_t* inBuf, int inBufLen)
{
  uart_recv_log_trace();

  int datalen;
  
  while(true) {
    if( MicoUartRecv( UART_FOR_APP, inBuf, inBufLen, UART_RECV_TIMEOUT) == kNoErr){
      return inBufLen;
    }
   else{
     datalen = MicoUartGetLengthInBuffer( UART_FOR_APP );
     if(datalen){
       MicoUartRecv(UART_FOR_APP, inBuf, datalen, UART_RECV_TIMEOUT);
       return datalen;
     }
   }
  }
  
}


