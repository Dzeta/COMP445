/**
 *		Lab Assignment 1 - COMP 445
 *
 *		Zuo Xiang ZHOU	- 9279148 
 *		FLEURY Gaetan	- 6380565
 *
**/


#ifndef SER_TCP_H
#define SER_TCP_H

#define HOSTNAME_LENGTH 20
#define RESP_LENGTH 40
#define FILENAME_LENGTH 128
#define MAX_LIST_LENGTH 1024
#define REQUEST_PORT 5001
#define BUFFER_LENGTH 1024 
#define MAXPENDING 10
#define MSGHDRSIZE 8 //Message Header Size

#include "../Load/Load.h"

typedef struct  
{
	char hostname[HOSTNAME_LENGTH];
	char filename[FILENAME_LENGTH];
} Req;  //request

typedef struct  
{
	char response[RESP_LENGTH];
} Resp; //response


class TcpServer
{
	int serverSock,clientSock;     /* Socket descriptor for server and client*/
	struct sockaddr_in ClientAddr; /* Client address */
	struct sockaddr_in ServerAddr; /* Server address */
	unsigned short ServerPort;     /* Server port */
	int clientLen;            /* Length of Server address data structure */
	char servername[HOSTNAME_LENGTH];	

public:
	TcpServer();
	~TcpServer();
	void TcpServer::start();
};

class TcpThread :public Thread
{
	Load* loader;
	Msg smsg,rmsg; //send_message receive_message7
	int cs;
public: 
	TcpThread(int clientsocket):cs(clientsocket)
	{}
	virtual void run();
	int msg_recv(int ,Msg * );
	int msg_send(int ,Msg * );
	void sendList();
	unsigned long ResolveName(char name[]);
	static void err_sys(char * fmt,...);
};

#endif