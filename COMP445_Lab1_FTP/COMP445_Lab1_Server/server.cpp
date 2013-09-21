/*a small file Server
Usage: suppose Server is running on sd1.encs.concordia.ca and server is running on sd2.encs.concordia.ca
.Also suppose there is a file called test.txt on the server.
In the Server,issuse "Server sd2.encs.concordia.ca test.txt size" and you can get the size of the file.
In the Server,issuse "Server sd2.encs.concordia.ca test.txt time" and you can get creation time of the file
*/
#pragma comment( linker, "/defaultlib:ws2_32.lib" )

#include <winsock.h>
#include <iostream>
#include <windows.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <process.h>
#include "Thread.h"
#include "server.h"

TcpServer::TcpServer()
{
	WSADATA wsadata;
	if (WSAStartup(0x0202,&wsadata)!=0)
		TcpThread::err_sys("Starting WSAStartup() error\n");
	
	//Display name of local host
	if(gethostname(servername,HOSTNAME_LENGTH)!=0) //get the hostname
		TcpThread::err_sys("Get the host name error,exit");
	
	printf("ftpd_tcp starting at host: [%s]\n"
		   "waiting to be contacted for transferring files...\n",servername);
	
	
	//Create the server socket
	if ((serverSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		TcpThread::err_sys("Create socket error,exit");
	
	//Fill-in Server Port and Address info.
	ServerPort=REQUEST_PORT;
	memset(&ServerAddr, 0, sizeof(ServerAddr));      /* Zero out structure */
	ServerAddr.sin_family = AF_INET;                 /* Internet address family */
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);  /* Any incoming interface */
	ServerAddr.sin_port = htons(ServerPort);         /* Local port */
	
	//Bind the server socket
    if (bind(serverSock, (struct sockaddr *) &ServerAddr, sizeof(ServerAddr)) < 0)
		TcpThread::err_sys("Bind socket error,exit");
	
	//Successfull bind, now listen for Server requests.
	if (listen(serverSock, MAXPENDING) < 0)
		TcpThread::err_sys("Listen socket error,exit");
}

TcpServer::~TcpServer()
{
	WSACleanup();
}


void TcpServer::start()
{
	for (;;) /* Run forever */
	{
		/* Set the size of the result-value parameter */
		clientLen = sizeof(ServerAddr);
		
		/* Wait for a Server to connect */
		if ((clientSock = accept(serverSock, (struct sockaddr *) &ClientAddr, 
			&clientLen)) < 0)
			TcpThread::err_sys("Accept Failed ,exit");
		
        /* Create a Thread for this new connection and run*/
		TcpThread * pt=new TcpThread(clientSock);
		pt->start();
	}
}

//////////////////////////////TcpThread Class //////////////////////////////////////////
void TcpThread::err_sys(char * fmt,...)
{     
	perror(NULL);
	va_list args;
	va_start(args,fmt);
	fprintf(stderr,"error: ");
	vfprintf(stderr,fmt,args);
	fprintf(stderr,"\n");
	va_end(args);
	exit(1);
}
unsigned long TcpThread::ResolveName(char name[])
{
	struct hostent *host;            /* Structure containing host information */
	
	if ((host = gethostbyname(name)) == NULL)
		err_sys("gethostbyname() failed");
	
	/* Return the binary, network byte ordered address */
	return *((unsigned long *) host->h_addr_list[0]);
}

/*
msg_recv returns the length of bytes in the msg_ptr->buffer,which have been recevied successfully.
*/
int TcpThread::msg_recv(int sock,Msg * msg_ptr)
{
	int rbytes,n;
	
	for(rbytes=0;rbytes<MSGHDRSIZE;rbytes+=n)
		if((n=recv(sock,(char *)msg_ptr+rbytes,MSGHDRSIZE-rbytes,0))<=0)
			err_sys("Recv MSGHDR Error");
		
		for(rbytes=0;rbytes<msg_ptr->length;rbytes+=n)
			if((n=recv(sock,(char *)msg_ptr->buffer+rbytes,msg_ptr->length-rbytes,0))<=0)
				err_sys( "Recevier Buffer Error");
			
			return msg_ptr->length;
}

/* msg_send returns the length of bytes in msg_ptr->buffer,which have been sent out successfully
*/
int TcpThread::msg_send(int sock,Msg * msg_ptr)
{
	int n;
	if((n=send(sock,(char *)msg_ptr,MSGHDRSIZE+msg_ptr->length,0))!=(MSGHDRSIZE+msg_ptr->length))
		err_sys("Send MSGHDRSIZE+length Error");
	return (n-MSGHDRSIZE);
	
}

void TcpThread::run() //cs: Server socket
{
	Resp resp;//response
	Req * reqp; //a pointer to the Request Packet
	Msg smsg,rmsg; //send_message receive_message
	struct _stat stat_buf;
    int result;
	char buff[1024];
	char str[1024];
	Type type;
	

	if(msg_recv(cs,&rmsg)!=rmsg.length)
		err_sys("Receive Req error,exit");

	//cast it to the request packet structure		
	reqp=(Req *)rmsg.buffer;
	printf("User from %s requested file %s to be sent.", reqp->hostname, reqp->filename);
	
	//contruct the response and send it out
	smsg.type=RESP;
	smsg.length=sizeof(Resp);
	
	if((result = _stat(reqp->filename,&stat_buf))!=0)
		sprintf_s(resp.response, RESP_LENGTH, "No such a file");
	else {	
		memset(resp.response,0,sizeof(resp));
		type = rmsg.type;
		if(rmsg.type==REQ_GET) 
			sprintf_s(resp.response, RESP_LENGTH,"File size:%ld",stat_buf.st_size );
		else if(rmsg.type==REQ_PUT)
			sprintf_s(resp.response, RESP_LENGTH,"READY",stat_buf.st_size );
		else if(rmsg.type==REQ_LIST)
			sprintf_s(resp.response, RESP_LENGTH,"TODO: LIST",stat_buf.st_size );
	}		
	
	memcpy(smsg.buffer,&resp,sizeof(resp));
	// Send the file size if cmd is GET, READY if cmd is PUT and the list of available files if cmd is LIST
	if(msg_send(cs,&smsg)!=smsg.length)
		err_sys("send Respose failed,exit");
	
	switch (type)
	{
	case REQ_GET:
		if(msg_recv(cs,&rmsg)!=rmsg.length)
			err_sys("Receive Req error,exit");

		if(strcmp(rmsg.buffer, "READY") == 0) {			
			FILE * pFile;
			pFile = fopen(reqp->filename,"rb");
			if (pFile!=NULL){
				while(fgets(buff, 1024, pFile)!=NULL){
					
				}
					memcpy(smsg.buffer, buff,sizeof(*buff));
					
					if(msg_send(cs,&smsg)!=smsg.length)
						err_sys("send Respose failed,exit");				
			}
			else{
				printf("\ncannot open file %s", reqp->filename);
			}
  
			// start sending the file
			// TODO
		}
		break;
	case REQ_PUT:
			// Deal with size / receive the file
		break;
	default:
		break;
	}

	
	closesocket(cs);
}



////////////////////////////////////////////////////////////////////////////////////////

int main(void)
{
	
	TcpServer ts;
	ts.start();
	
	return 0;
}


