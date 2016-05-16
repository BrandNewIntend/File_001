/**
  ******************************************************************************
  * @file    RemoteTcpClient.c 
  * @author  William Xu
  * @version V1.0.0
  * @date    05-May-2014
  * @brief   Create a TCP client thread, and connect to a remote server.
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
    

#include "appUserEx.h"    


//#define VANWARD_REMOTE_SERVER  "192.168.31.109"  //myPCtest srver 31.200
#define VANWARD_REMOTE_SERVER       "cloud.vanwardsmart.com" ///������������ip
#define VANWARD_REMOTE_SERVER_PORT  2300

#define client_log(M, ...) custom_log("TCP client", M, ##__VA_ARGS__)
#define client_log_trace() custom_log_trace("TCP client")


#define             MAX_RECV_SERV  1024
static char        g_recvServer[MAX_RECV_SERV];///���������ƶ˷�����������
static int         g_shortTimer=0;     ////����1����
uint32_t    g_configTime=0;///��������ʱ��
uint8_t      g_uartDataOK=0;///�յ���ȷ��uart���ݰ�=1 =2������
uint16_t     g_versoin=0;
uint16_t     g_operate=0;
uint32_t     g_msgId=0; /////MessageID
uint32_t     g_jsonLeng=0;///֡ͷ���⵽��������ݳ���
bool        g_headerRecv=false;///�Ѿ��յ����ݰ�ͷ
uint8_t      g_registTime=0;///��һ�ε�½������=1,=2�ٴ�
bool        g_flagRecvSock=false;///=1���յ��Ƶ�����


struct json_object*  g_recvJsonObj;

extern void sendToMCU(void);


void Trace(char*str){
  static int ix=0;
client_log("\nTrace:[%d]%s",ix++,str);

  
}


void pingCloud(char *ssl_active){
  
   char package[2];
   package[0]=0x80;
   ssl_send(ssl_active,(void*)package,1);

}


uint32_t timeCostCount(void){
   
  uint32_t costTm=mico_get_time()-g_configTime;
           g_configTime=mico_get_time();
       
       return costTm;

}

/***************************************************************
     ���˽����,��ʵ����߳�����Ϊ�ͻ���!!��������͵ķ�����
**************************************************************/
void vanwardSrv_thread(void *inContext)
{
 // mico_rtos_delete_thread(NULL); goto exit; ///testing
 
  
    OSStatus err=kNoErr;
    char ipstr[16];
    struct sockaddr_t addr;
    int tcp_fd = -1;
    int nfound;//���¼�����
    int len;
    fd_set rset; //�������
    fd_set set; //��ע����
    struct timeval_t timeout; ///��ȡɨ�賬ʱ
    static char *ssl_active = NULL;
      int errno = 0;
      uint8_t smartCloudLinkTime=0;
      uint8_t  firstTimeRegister=0;
     MsgFrame msgFm;
    
 
   client_log(" Start-tcp_ip_thread......%d ",g_registTime);
   
   
begin_connect:////��ʼ�����ƶ˳����ӷ�����
  
   
    while(!g_wifi_connected|| g_cloudStep!=3){  
         msleep(100);
   }
   
   firstTimeRegister= g_registTime;
   
  
   client_log("Vanward_Srv_Contrl starting...%d",g_cloudStep);
    

  
      err = gethostbyname(VANWARD_REMOTE_SERVER, (uint8_t *)ipstr, 16);
      require_noerr(err, exit);
      
      
       client_log("TCP_Server address: host:%s, ip: %s", VANWARD_REMOTE_SERVER, ipstr);
   
  
  tcp_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );  
  //maxfd=tcp_fd+1;
  addr.s_ip = inet_addr( ipstr );
  addr.s_port = VANWARD_REMOTE_SERVER_PORT;
  err = connect( tcp_fd, &addr, sizeof(addr) ); 
     require_noerr_string( err, exit, "Connect tcp_server failed" );
     
      
    
   ssl_version_set(TLS_V1_2_MODE);////�ͻ��˺ͷ���˶�Ҫ����
      
   ssl_active = ssl_connect(tcp_fd,0,NULL, &errno);
    require_noerr_action( NULL == ssl_active, exit, client_log("ERROR: ssl disconnect") );
           
    if(firstTimeRegister==2){
      msgFm=cloudCmder(1,CMD_LOGIN,0);
    }else{
      msgFm=cloudCmder(1,CMD_REGIST,0); //��һ��ע�ἤ��
    }
  
 //client_log("TCP_Data:\n%s",msgFm.pt);
  dumphex((char*)msgFm.pt,msgFm.len);

  client_log("__CloudConnect:%d___Take: %d ms\n",firstTimeRegister,mico_get_time()-g_configTime);
  
  
      ssl_send(ssl_active, (void*)msgFm.pt,msgFm.len);  
      g_cloudStep=4;///��������������������! 
     
     
   FD_ZERO(&set);
   FD_SET(tcp_fd,&set); //��ӵ�fd��ע����
 
   
   
    //int setsockopt(int sockfd, int level, int optname,const void *optval, socklen_t optlen);
   
    uint16_t nNetTimeout=1000; //ms
    setsockopt(tcp_fd,SOL_SOCKET,SO_RCVTIMEO,(void*)nNetTimeout,sizeof(int)); //set  socket_recv timeout 
                  
   while(true){
     
     if(!g_wifi_connected){ ////����wifi�Ƿ����
       
       static int loopTm=0;
        loopTm++;
        msleep(10); 
        if(loopTm<1500)
          continue;
        else
        {
           loopTm=0;
          goto exit; ///��;�����˳�����
        
        }
     } 
     
     rset=set; 
     timeout.tv_sec=0;
     timeout.tv_usec=50;
          nfound=select(1,&rset,NULL,NULL,&timeout);
       
    
          
          if(g_uartDataOK>=1){
            
            
                if(g_uartDataOK==2) ///����
                {
                     printf("\r\n__________Ping to cloud _______\r\n\r\n");
                     pingCloud(ssl_active);
                }
                else{
                   
                  printf("\r\n__________Updata status _______MsgID:%d__\r\n\r\n",g_msgId);            
                    msgFm=cloudCmder(0,CMD_UPDATA,g_msgId);
                      // dumphex(msgFm.pt,msgFm.len);
                    ssl_send(ssl_active,(void*)msgFm.pt,msgFm.len);
                }
            
                 client_log("____Take %d ms flush cloud status",timeCostCount());
                 g_uartDataOK=0;
                 g_shortTimer=0;

          }          
          
          if(nfound==0){
            
             g_shortTimer++;//����ʱ��
              //printf("\n@:%d",g_shortTimer); 
              
             continue;
          }
      
          if(FD_ISSET(tcp_fd,&rset)){///get data from server
            
              memset(g_recvServer,0,MAX_RECV_SERV);
              g_shortTimer=0;//�յ����ݶ�ʱ���ͼ��
              
             len=ssl_recv(ssl_active,g_recvServer,MAX_RECV_SERV);
             
            
              
               client_log("\n LEN [%d] DATA= %s",len,g_recvServer);
               dumphex(g_recvServer,len);
               

               
               if(len<=0) { client_log("\n_______ Register state____"); firstTimeRegister=2;  goto exit; } //�����Ѿ����ӹ�
               if(len<12) continue;//�յ�����������
               
               if(g_recvServer[0]==0x00 ) ///����ǰ�ͷ֡
               {
                     g_versoin=*(g_recvServer)*0x100+*(g_recvServer+1);
                      g_operate=*(g_recvServer+2)*0x100+*(g_recvServer+3);
                      g_msgId=*(g_recvServer+4)*0x1000000+*(g_recvServer+5)*0x10000+*(g_recvServer+6)*0x100+*(g_recvServer+7);
                     g_jsonLeng=*(g_recvServer+8)*0x1000000+*(g_recvServer+9)*0x10000+*(g_recvServer+10)*0x100+*(g_recvServer+11);
                     
                                       
                     if(len==12){
                        g_headerRecv=true;
                       
                     }else if(len>12){
                            ///��Ϊ������������֡
                          
                       timeCostCount();///��¼ʱ���;
                       
                         g_recvJsonObj= json_tokener_parse(g_recvServer+12);
                         
                         json_object_object_foreach(g_recvJsonObj,key, val) {
                         
                            client_log("Recv_Key=%s,Val=%s",key,json_object_to_json_string(val) ); 
                            
                            if(!strcmp(key,"OnOFF")){
                            
                              g_device_mcuObj.onOFF =json_object_get_int(val);
                               
                            }else if(!strcmp(key, "Appoint")){
                            
                               g_device_mcuObj.appoint =json_object_get_int(val);
                                 
                            }else if(!strcmp(key, "TempSet")){
                            
                               g_device_mcuObj.tempSet=json_object_get_int(val);
                               
                            }else if(!strcmp(key, "Power")){    
                              
                               g_device_mcuObj.power =json_object_get_int(val);
                               
                            }else if(!strcmp(key, "Mode")){
                            
                               g_device_mcuObj.mode =json_object_get_int(val);
                               
                            }else if(!strcmp(key, "Timer")){  
                                                            
                              // memset(g_device_mcuObj.timer,0,6);
                              sprintf((char*)g_device_mcuObj.timer,"%s",json_object_get_string(val));
                            
                            }
                         
                         }
                      
                         
                         sendToMCU();////send the data to wifi_mcu
                         g_flagRecvSock=true;
                         
                          json_object_put(g_recvJsonObj);/*free memory*/   
                          g_recvJsonObj=NULL;
                                                                                                                 
                     
                     }
                                  
               }else{
               
                     if(g_headerRecv){
                     
                        g_headerRecv=false; //�ϲ�ǰ֡����
                     
                     }
                  
                   
               }
               
             // write(tcp_fd,buf,len);
          }else{
              client_log("-Server exit-- ReConnect--");
              sleep(3);//��ʱһ��,��Ҫ̫Ƶ������
              goto begin_connect;////����������
          }          
   
   }  ////end of while(true) ///////////
   
    
    
exit:
  if(err!=kNoErr){
     smartCloudLinkTime++; 
  }
  
  g_cloudStep=3;//�ָ���3��
  Trace("---------------> VW_server Relinking ...... \r\n");
  close(tcp_fd);
  
        
  ssl_close(ssl_active);
  FD_CLR(tcp_fd,&set); //REmove Fd ��ע����
  
   sleep(4);
  
  g_configTime=mico_get_time();
            
  if(smartCloudLinkTime<5){
    
     goto begin_connect;///��������
  }
  
  Trace("-------->End Relink in a thread...\r\n"); 
  smartCloudLinkTime=0;
  //free(buf);
  mico_rtos_delete_thread(NULL);
  return;
  
  
  

}




