/*a small file client
Usage: suppose client is running on sd1.encs.concordia.ca and server is running on sd2.encs.concordia.ca
.Also suppose there is a file called test.txt on the server.
In the client,issuse "client sd2.encs.concordia.ca test.txt size" and you can get the size of the file.
In the client,issuse "client sd2.encs.concordia.ca test.txt time" and you can get creation time of the file
*/

#pragma comment( linker, "/defaultlib:ws2_32.lib" )

#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <windows.h>

#define HOSTNAME_LENGTH 20
#define RESP_LENGTH 40
#define FILENAME_LENGTH 20
#define REQUEST_PORT 5001
#define BUFFER_LENGTH 1024 
#define CMD_LENGTH 20
#define TRACE 0
#define MSGHDRSIZE 8 //Message Header Size

using namespace std;

typedef enum{
	REQ_GET=1,REQ_PUT,REQ_LIST,RESP //Message type
} Type;

typedef struct  
{
	char hostname[HOSTNAME_LENGTH];
	char filename[FILENAME_LENGTH];
} Req;  //request

typedef struct  
{
	char response[RESP_LENGTH];
} Resp; //response


typedef struct 
{
	Type type;
	int  length; //length of effective bytes in the buffer
	char buffer[BUFFER_LENGTH];
} Msg; //message format used for sending and receiving


class TcpClient
{
    int sock;                    /* Socket descriptor */
	struct sockaddr_in ServAddr; /* server socket address */
	unsigned short ServPort;     /* server port */	
	Req req;               /* request */
	Resp * respp;          /* pointer to response*/
	Msg smsg,rmsg;               /* receive_message and send_message */
	WSADATA wsadata;
public:
	TcpClient(){}
	void run();	
	~TcpClient();
    int msg_recv(int ,Msg * );
	int msg_send(int ,Msg * );
	unsigned long ResolveName(char name[]);
    void err_sys(char * fmt,...);

};

void TcpClient::run()
{
	char serverName[HOSTNAME_LENGTH];
	char fileName[FILENAME_LENGTH];
	char cmd[CMD_LENGTH];
	
	//initilize winsocket
	if (WSAStartup(0x0202,&wsadata)!=0)
	{  
		WSACleanup();  
	    err_sys("Error in starting WSAStartup()\n");
	}
	
	
	//Display name of local host and copy it to the req
	if(gethostname(req.hostname,HOSTNAME_LENGTH)!=0) //get the hostname
		err_sys("can not get the host name,program exit");
	printf("ftp_tcp starting on host: %s\n",req.hostname);

	printf("Type name of ftp server: ");
	cin >> serverName;

	printf("\nType name of file to be transferred:  ");
	cin >> fileName;

	printf("\nType direction of transfer: ");
	cin >> cmd;
		
	strcpy_s(req.filename, FILENAME_LENGTH, fileName);

	if(strcmp(cmd,"get")==0)
		smsg.type=REQ_GET;
	else if (strcmp(cmd,"put")==0)
		smsg.type=REQ_PUT;
	else if (strcmp(cmd,"list")==0)
		smsg.type=REQ_LIST;
	else err_sys("Wrong request type\n");
		//Create the socket
	if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) //create the socket 
			err_sys("Socket Creating Error");
	
	//connect to the server
	ServPort=REQUEST_PORT;
	memset(&ServAddr, 0, sizeof(ServAddr));     /* Zero out structure */
	ServAddr.sin_family      = AF_INET;             /* Internet address family */
	ServAddr.sin_addr.s_addr = ResolveName(serverName);   /* Server IP address */
	ServAddr.sin_port        = htons(ServPort); /* Server port */
	if (connect(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
			err_sys("Socket Creating Error");

    //send out the message
	memcpy(smsg.buffer,&req,sizeof(req)); //copy the request to the msg's buffer
	smsg.length=sizeof(req);
    if (msg_send(sock,&smsg) != sizeof(req))
			err_sys("Sending req packet error.,exit");

	printf("\nSent request to %s, waiting...", serverName);

	//receive the response
	if(msg_recv(sock,&rmsg)!=rmsg.length)
		err_sys("recv response error,exit");

	//cast it to the response structure
	respp=(Resp *)rmsg.buffer;
	if(strcmp(respp->response, "No such a file") == 0) 
	{
		// File does not exist
		printf("%s\n", respp->response);
	}
	else if(strcmp(respp->response, "READY") == 0) 
	{
		// Send the file size / file
	}
	else 
	{
		// Get the file
		int fileSize = atoi(respp->response);

		smsg.type = RESP;
		strcpy_s(smsg.buffer, BUFFER_LENGTH, "READY");
		smsg.length = sizeof("READY");
		
		if (msg_send(sock,&smsg) != sizeof("READY"))
			err_sys("Sending req packet error.,exit");

		// Start receiving the file
		// TODO
	}
		

	//close the client socket
	closesocket(sock);

}

TcpClient::~TcpClient()
{
	/* When done uninstall winsock.dll (WSACleanup()) and exit */ 
	WSACleanup();  
}


void TcpClient::err_sys(char * fmt,...) //from Richard Stevens's source code
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

unsigned long TcpClient::ResolveName(char name[])
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
int TcpClient::msg_recv(int sock,Msg * msg_ptr)
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
int TcpClient::msg_send(int sock,Msg * msg_ptr)
{
	int n;
	if((n=send(sock,(char *)msg_ptr,MSGHDRSIZE+msg_ptr->length,0))!=(MSGHDRSIZE+msg_ptr->length))
		err_sys("Send MSGHDRSIZE+length Error");
	return (n-MSGHDRSIZE);
	
}

int main(int argc, char *argv[]) //argv[1]=servername argv[2]=filename argv[3]=time/size
{
	
	TcpClient * tc=new TcpClient();
	tc->run();
	while(1);
	return 0;
}
