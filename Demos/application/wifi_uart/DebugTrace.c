#include "mico.h"
#include "SocketUtils.h"
#include "appUserEx.h"
#include <stdarg.h>

/************************************
*   用于无线调试输出的函数+线程
*************************************/
#define TRACE_PORT      58000

#define __ENABLE_TRACE   1

static int g_client_fd = -1;
static bool g_traceAble=false;

#define trace_thread_log(M, ...) custom_log("TRACE_THR", M, ##__VA_ARGS__)

void traceInit_thread(void *inContext){
 
   
#ifndef  __ENABLE_TRACE
    mico_rtos_delete_thread(NULL); ///直接退出
#endif  

  char m_buffer[128]={0};         
 
  OSStatus err = kNoErr;
  struct sockaddr_t server_addr,client_addr;
  socklen_t sockaddr_t_size = sizeof( client_addr );
  char  client_ip_str[16]={0};
  int tcp_listen_fd = -1;
  fd_set readfds;
  
  
  int fd = -1;
  int len = 0; 
  struct timeval_t t;
  int sizer;
  
  
start:  
   trace_thread_log("__Start DebugTrace Thread...");
  tcp_listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  require_action(IsValidSocket( tcp_listen_fd ), exit, err = kNoResourcesErr );
  
  server_addr.s_ip =  INADDR_ANY;  /* Accept conenction request on all network interface */
  server_addr.s_port = TRACE_PORT;/* Server listen on port: 20000 */
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
      g_client_fd = accept( tcp_listen_fd, &client_addr, &sockaddr_t_size );
      if( IsValidSocket( g_client_fd ) ) {
        inet_ntoa( client_ip_str, client_addr.s_ip );
        trace_thread_log( "Debug Trace Client %s:%d connected, fd: %d", client_ip_str, client_addr.s_port, g_client_fd );
        SocketClose( &tcp_listen_fd );
        break;
      }
    }
  }////end while(true) ///
  
  
/////>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
   fd = g_client_fd;
  g_traceAble=true;
  
   len=sizeof(m_buffer);
   sizer=sizeof(m_buffer);
   memset(m_buffer,0,len);
   
  t.tv_sec = 2;
  t.tv_usec = 0;
  
 
  
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
        trace_thread_log( "Debug_Trace Client is disconnected, fd: %d", fd );
        goto exit;
      }  
      
      m_buffer[len]=0;
      //trace_thread_log("fd: %d, recv data %d from OTAClient", fd, len);
      trace_thread_log("Debug_Trace recv=%s __end [%d]___",m_buffer,len);
         
      
    }
  }
 
exit: 
  if( err != kNoErr ) trace_thread_log( "Debug Trace Thread exit with err: %d", err );
      SocketClose( &fd );
      g_traceAble=false;
      g_client_fd=-1;
      tcp_listen_fd=-1;
      goto start;///重新连接
     
      mico_rtos_delete_thread(NULL);      
      
      return; 
      
      
}



char hex2char(int ch){
	
	if(ch>0 && ch<10){
	  return ch+'0';
	}

	if(ch>=10 && ch<=15){
	  return ch-10+'A';
	}

     return '0';
}


void trace_str(char *str,int len){
 
#ifndef __ENABLE_TRACE
    return;
#endif 
    
    if(len==0){
       len=strlen(str);
    } 
     
    
  if(!g_traceAble || len<=0 ) return;
  
  send(g_client_fd,str,len,0);
  
}



void trace_int(int i){
 
  char buf[10]={0};
  int len;
  len=sprintf(buf,"%d",i);
  send(g_client_fd,buf,len,0);
  
}

void trace_char(char ch){
 
  char buf[2]={0};
  buf[0]=ch;
  send(g_client_fd,buf,1,0);
  
}



void trace_hex(char *str, int len){
  
#ifndef __ENABLE_TRACE
    return;
#endif 
    

   if(!g_traceAble || len<0 ) return;
   
  int i=0;
  char ch;
  char buf[20]={0};
  

  
  i=sprintf(buf,"\r\n DumpHex[%d]: ",len);
  send(g_client_fd,buf,i,0);
  
  buf[2]=' ';
  buf[3]=0;
  buf[4]=0;
  i=0;
  while(i<len){
     
    ch=*(str+i);
    buf[0]=hex2char(ch/0x10);  
    buf[1]=hex2char(ch%0x10);
    
     send(g_client_fd,buf,3,0);
     i++;
     
  }
   
   
}


void putcharx(char ch){
  
  char buf[2]={0};
  buf[0]=ch;
  printf("%c",ch);
  if(!g_traceAble) return;
  
   send(g_client_fd,buf,1,0);
  
}


void trace(const char* fmt, ... )
{
    va_list ap;
    va_start(ap,fmt); /* 用最后一个具有参数的类型的参数去初始化ap */
    for (;*fmt;++fmt)
    {
        /* 如果不是控制字符 */
        if (*fmt!='%')
        {
            putcharx(*fmt); /* 直接输出 */
            continue;
        }
        /* 如果是控制字符，查看下一字符 */
        ++fmt;
        if ('\0'==*fmt) /* 如果是结束符 */
        {
           // assert(0);  /* 这是一个错误 */
            break;
        }
        switch (*fmt)
        {
        case '%': /* 连续2个'%'输出1个'%' */
            putcharx('%');
            break;
        case 'd': /* 按照int输出 */
            {
                /* 下一个参数是int，取出 */
                int i = va_arg(ap,int);
                printf("%d",i);
                trace_int(i);
            }
            break;
        case 'c': /* 按照字符输出 */
            {
                char c = va_arg(ap,char);
                printf("%c",c);
                trace_char(c);
            }
            break;
        case 's':
            {
                 char* s = va_arg(ap,char*);
                    int len=printf("%s",s);
                     trace_str(s,len);
            }
            break;
            
        default:
          break;
            
        }////end switch
    }
    va_end(ap);  /* 释放ap—— 必须！ 见相关链接*/
    
}

