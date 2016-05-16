#include "mico.h"
#include "SocketUtils.h"

#include "MICOAPPDefine.h"


#define tcp_server_log(M, ...) custom_log("APP_Data", M, ##__VA_ARGS__)

#define SERVER_PORT 9000 /*set up a tcp server,port at 20000*/

extern bool    g_wifi_connected;
extern bool  g_bSmartCloudReLink; ///是否需要重启开启线程
int     g_cloudStep; //join to cloud step=0 ini ,=1 eassyLink_getData,=2 https_req, =3 tcpip
bool    g_getAppData; //the flag app send data to firmware
char    g_appData[256];

extern void vanwardSrv_thread( void *inContext );///连接到万和服务器



void getAppExData_thread( void *inContext ){
  
  int fd;
  fd_set readfds;
  struct sockaddr_t server_addr,client_addr;
  socklen_t sockaddr_t_size = sizeof( client_addr );
  char  client_ip_str[16];
  int tcp_listen_fd = -1, client_fd = -1; 
  
 
  int len = 0; 
  struct timeval_t t;
  
  OSStatus err = kNoErr;
  

    while(g_cloudStep==0)  mico_thread_msleep(80);
     
    if(g_cloudStep>1) goto exit;//不用获取app数据直接退出线程
     
     tcp_server_log("____Ready to listen app data...[%d]",g_cloudStep);
 
  
  tcp_listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  require_action(IsValidSocket( tcp_listen_fd ), exit, err = kNoResourcesErr );
  
  server_addr.s_ip =  INADDR_ANY;  /* Accept conenction request on all network interface */
  server_addr.s_port = SERVER_PORT;/* Server listen on port: 20000 */
  err = bind( tcp_listen_fd, &server_addr, sizeof( server_addr ) );
  require_noerr( err, exit );
  
  err = listen( tcp_listen_fd, 0);
  require_noerr( err, exit );
  
  while(true)
  {
    FD_ZERO( &readfds );
    FD_SET( tcp_listen_fd, &readfds );
    
    require( select(1, &readfds, NULL, NULL, NULL) >= 0, exit );
    
    if(FD_ISSET(tcp_listen_fd, &readfds)){
      client_fd = accept( tcp_listen_fd, &client_addr, &sockaddr_t_size );
      if( IsValidSocket( client_fd ) ) {
        inet_ntoa( client_ip_str, client_addr.s_ip );
        tcp_server_log( "TCP Client %s:%d connected, fd: %d", client_ip_str, client_addr.s_port, client_fd );
       // if ( kNoErr !=  mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "TCP Clients", tcp_client_thread, 0x800, (void *)client_fd ) )
        //  SocketClose( &client_fd );  
        SocketClose( &tcp_listen_fd );
        break;
      }
    }
  }//////////  end  while(true) //////////
  
  
   fd = client_fd; 
   len=sizeof(g_appData);
   memset(g_appData,0,len);
   
  t.tv_sec = 2;
  t.tv_usec = 0;
  
 while(true)
  {
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    require_action( select(1, &readfds, NULL, NULL, &t) >= 0, exit, err = kConnectionErr );
    
    if( FD_ISSET( fd, &readfds ) ) /*one client has data*/
    {
      len = recv( fd, g_appData, len , 0 );
      require_action( len >= 0, exit, err = kConnectionErr );
      
      if( len <= 0){
        tcp_server_log( "TCP Client is disconnected, fd: %d", fd );
        goto exit;
      }  
      
      tcp_server_log("fd: %d, recv data %d from client", fd, len);
      tcp_server_log("APP_Data=%s __end [%d]___",g_appData,len);
      
      if( g_appData[0]=='{' && g_appData[len-1]=='}')
      { 
        g_getAppData=true; ///通知https请求线程
      }else
      {
           tcp_server_log( "APP format is erro !" );///app发过来的json格式有误
      }
      break;//只接受一次数据
    }
  }
 
exit: 
  if( err != kNoErr ) tcp_server_log( "TCP Thread exit with err: %d", err );
 
      SocketClose( &fd );
              
  
      mico_rtos_delete_thread(NULL);      
      return; 

}