/*
  this thread is for test some command for data
*/
#include "mico.h"
#include "SocketUtils.h"

#include "MICOAPPDefine.h"

#define test_cmdServ_log(M, ...) custom_log("Test_Cmd", M, ##__VA_ARGS__)

extern USED void PlatformEasyLinkButtonClickedCallback(void);
extern app_context_t* getAppContext(void);
extern void cleanCloudData(mico_Context_t* mico_context,app_context_t* app_context);


#define SERVER_PORT 9005 /*set up a tcp server,port at 20000*/

extern bool g_wifi_connected;

char  g_testData[128]={0};

void testCmdServ_thread( void *inContext ){

     while(!g_wifi_connected)  mico_thread_msleep(80);
          
    test_cmdServ_log("__Ready to listen APP Test...");
 
  OSStatus err = kNoErr;
  struct sockaddr_t server_addr,client_addr;
  socklen_t sockaddr_t_size = sizeof( client_addr );
  char  client_ip_str[16];
  int tcp_listen_fd = -1, client_fd = -1;
  fd_set readfds;
  
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
        test_cmdServ_log( "CmdTCP Client %s:%d connected, fd: %d", client_ip_str, client_addr.s_port, client_fd );
       // if ( kNoErr !=  mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "TCP Clients", tcp_client_thread, 0x800, (void *)client_fd ) )
        //  SocketClose( &client_fd );  
        SocketClose( &tcp_listen_fd );
        break;
      }
    }
  }////end while(1) ///
  
  

  int fd = client_fd;
  int len = 0; 
  struct timeval_t t;
  
   len=sizeof(g_testData);
   memset(g_testData,0,len);
   
  t.tv_sec = 2;
  t.tv_usec = 0;
  
  int sizer=sizeof(g_testData);
 while(true)
  {
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    require_action( select(1, &readfds, NULL, NULL, &t) >= 0, exit, err = kConnectionErr );
    
    if( FD_ISSET( fd, &readfds ) ) /*one client has data*/
    {
      len = recv( fd, g_testData, sizer , 0 );
      require_action( len >= 0, exit, err = kConnectionErr );
      
      if( len <= 0){
        test_cmdServ_log( "CmdTCP Client is disconnected, fd: %d", fd );
        goto exit;
      }  
      
      g_testData[len]=0;
      test_cmdServ_log("fd: %d, recv data %d from CmdClient", fd, len);
      test_cmdServ_log("CmdSrv_Data=%s __end [%d]___",g_testData,len);
      
      if(g_testData[0]!='-') continue;
      
      if(strcmp("-config",g_testData)==0){
           mico_Context_t* micoContext=mico_system_context_get();
           app_context_t* app_context=getAppContext();
           
           test_cmdServ_log("\n----APPCmd: Config mode-------");
            cleanCloudData(micoContext,app_context);
            PlatformEasyLinkButtonClickedCallback ();
            test_cmdServ_log("\n----End config thread-------");
             mico_rtos_delete_thread(NULL);              
             return;
            
      }
    
      
    }
  }
 
exit: 
  if( err != kNoErr ) test_cmdServ_log( "CmdTCP Thread exit with err: %d", err );
 
      SocketClose( &fd );
      mico_rtos_delete_thread(NULL);      
      return; 

}