#include "mico.h"
#include "SocketUtils.h"
#include "appUserEx.h"



typedef struct {
    char     filename[32];
    uint32_t filelen;
    uint32_t flashaddr; // the flash address of this file
    int      flashtype; // SPI flash or Internal flash.
} tftp_file_info_t;


#define OTA_PORT  9443
#define ota_thread_log(M, ...) custom_log("OTA_THR", M, ##__VA_ARGS__)

//////作为一个Server服务器来烧录升级OTA

void ota_thread( void *inContext ){
  
  char m_buffer[256]={0};      
    ota_thread_log("__Start OTA Thread...");
 
  OSStatus err = kNoErr;
  struct sockaddr_t server_addr,client_addr;
  socklen_t sockaddr_t_size = sizeof( client_addr );
  char  client_ip_str[16];
  int tcp_listen_fd = -1, client_fd = -1;
  fd_set readfds;
  
  tcp_listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  require_action(IsValidSocket( tcp_listen_fd ), exit, err = kNoResourcesErr );
  
  server_addr.s_ip =  INADDR_ANY;  /* Accept conenction request on all network interface */
  server_addr.s_port = OTA_PORT;   /* Server listen on port: 20000 */
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
        ota_thread_log( "OTA Client %s:%d connected, fd: %d", client_ip_str, client_addr.s_port, client_fd );
       // if ( kNoErr !=  mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "TCP Clients", tcp_client_thread, 0x800, (void *)client_fd ) )
        //  SocketClose( &client_fd );  
        SocketClose( &tcp_listen_fd );
        break;
      }
    }
  }////end while(true) ///
  
  
  
  
  extern int tsend (tftp_file_info_t *fileinfo, uint32_t ipaddr);
    tftp_file_info_t fileinfo;
   uint32_t ipaddr = inet_addr("192.168.1.1");
  mico_logic_partition_t* ota_partition = MicoFlashGetInfo( MICO_PARTITION_OTA_TEMP );
  
      fileinfo.filelen = 16;//ota_partition->partition_length;
      fileinfo.flashaddr = 0;
      fileinfo.flashtype = MICO_PARTITION_OTA_TEMP;
    strcpy(fileinfo.filename, "vanward_ota.bin");
    
    tsend(&fileinfo,ipaddr);
    
/////>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
  int fd = client_fd;
  int len = 0; 
  struct timeval_t t;
  
   len=sizeof(m_buffer);
   memset(m_buffer,0,len);
   
  t.tv_sec = 2;
  t.tv_usec = 0;
  
  int sizer=sizeof(m_buffer);
 while(true)
  {
    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    
    require_action( select(1, &readfds, NULL, NULL, &t) >= 0, exit, err = kConnectionErr );
    
    if( FD_ISSET( fd, &readfds ) ) /*one client has data*/
    {
      len = recv( fd, m_buffer, sizer , 0 );
      require_action( len >= 0, exit, err = kConnectionErr );
      
      if( len <= 0){
        ota_thread_log( "OTA Client is disconnected, fd: %d", fd );
        goto exit;
      }  
      
      m_buffer[len]=0;
      //ota_thread_log("fd: %d, recv data %d from OTAClient", fd, len);
      ota_thread_log("OTA data recv=%s __end [%d]___",m_buffer,len);
         
      
    }
  }
 
exit: 
  if( err != kNoErr ) ota_thread_log( "OTA Thread exit with err: %d", err );
 
      SocketClose( &fd );
      mico_rtos_delete_thread(NULL);      
      return; 
      
      
}