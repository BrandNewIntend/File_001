#include "MICO.h"

static char*   m_pBuffer;
static int     m_buffsize;
static int     m_writePos;
static int     m_readPos;
static int     m_dataLeng;//��ǰ�������ж����ֽ�


bool ringBuffer_init(int size){
    
	m_buffsize=size;
	m_pBuffer=(char*)malloc(size);
        
        if(m_pBuffer==NULL) 
          return false;
        else
          return true;
}

void ringBuffer_free(void){
   //�ͷ���Դ
  if(m_pBuffer!=NULL){
     free(m_pBuffer);
     m_pBuffer=NULL;
  }
}

int ringBuffer_getDataCount(void){
         
	 return m_dataLeng;
}


int ringBuffer_pushData(char *dat,int len){

	if(len<=0) return 0;

	if (m_dataLeng+len>m_buffsize)///���
	{
		len=m_buffsize-m_dataLeng;		
	}


	if(m_writePos+len<=m_buffsize){
		memcpy(m_pBuffer+m_writePos,dat,len);
		m_writePos+=len;
		if(m_writePos>=m_buffsize)
			m_writePos=0;//����
	}else{

		///����һȦ
		int wr1Leng=m_buffsize-m_writePos;
		int wr2Leng=m_writePos+len-m_buffsize-1;

		memcpy(m_pBuffer+m_writePos,dat,wr1Leng);
		memcpy(m_pBuffer,dat+wr1Leng,wr2Leng);

		m_writePos=wr2Leng;

	}

	m_dataLeng+=len;////��������

	return len;


}



int ring_buffer_popData(char* inBuf,int len){

	int readLen=len;//�����ȡ�˶����ֽ�
	if(m_dataLeng<1 || len<0 ) return 0; ///û�пɶ�ȡ������

	if(len>m_dataLeng){
		readLen=m_dataLeng;
	}


	if (m_readPos+readLen<m_buffsize)
	{
		memcpy(inBuf,m_pBuffer+m_readPos,readLen);
		m_readPos+=readLen;
		m_dataLeng-=readLen;
		return readLen;
	}


	int rd1Leng=m_buffsize-m_readPos;
	int rd2Leng=readLen-rd1Leng;

	memcpy(inBuf,m_pBuffer+m_readPos,rd1Leng);
	memcpy(inBuf+rd1Leng,m_pBuffer,rd2Leng);
        
	m_readPos=rd2Leng;
	m_dataLeng-=readLen;

	return readLen; 
}
