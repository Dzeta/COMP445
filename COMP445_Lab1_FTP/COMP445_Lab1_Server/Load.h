#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <windows.h>


#define HOSTNAME_LENGTH 20
#define RESP_LENGTH 40
#define FILENAME_LENGTH 128
#define BUFFER_LENGTH 1024 
#define MSGHDRSIZE 8

typedef enum{
	REQ_GET=1,REQ_PUT,REQ_LIST,REQ_CANCEL,RESP  //Message type
} Type;

typedef struct 
{
	Type type;
	int  length; //length of effective bytes in the buffer
	char buffer[BUFFER_LENGTH];
} Msg;

class Load
{
	Msg smsg,rmsg;

public:
	void sendFile(FILE* , char* , char* , int);
	void receiveFile(char*, char*, int);
	int msg_recv(int ,Msg * );
	int msg_send(int ,Msg * );
	void err_sys(char * fmt,...);
};