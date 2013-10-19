/**
 *		Lab Assignment 1 - COMP 445
 *
 *		Zuo Xiang ZHOU	- 9279148 
 *		FLEURY Gaetan	- 6380565
 *
**/

#pragma comment( linker, "/defaultlib:ws2_32.lib" )

#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <windows.h>
#include "../Load/Load.h"

#define HOSTNAME_LENGTH 20
#define RESP_LENGTH 40
#define FILENAME_LENGTH 128
#define MAX_LIST_LENGTH 1024
#define REQUEST_PORT 5001
#define BUFFER_LENGTH 1024 
#define CMD_LENGTH 20
#define TRACE 0
#define MSGHDRSIZE 8 //Message Header Size

using namespace std;

typedef struct  
{
	char hostname[HOSTNAME_LENGTH];
	char filename[FILENAME_LENGTH];
} Req;  //request

typedef struct  
{
	char response[RESP_LENGTH];
} Resp; //response


class TcpClient
{
	int sock;                    /* Socket descriptor */
	struct sockaddr_in ServAddr; /* server socket address */
	unsigned short ServPort;     /* server port */	
	Req req;               /* request */
	Resp * respp;          /* pointer to response*/
	Msg smsg,rmsg;               /* receive_message and send_message */
	WSADATA wsadata;
	Load* loader;

public:
	TcpClient(){}
	void run();	
	~TcpClient();
	int msg_recv(int ,Msg * );
	int msg_send(int ,Msg * );
	void getList(char* );
	long ResolveName(char name[]);
	void err_sys(char * fmt,...);

};

void TcpClient::run()
{
	char serverName[HOSTNAME_LENGTH];
	char fileName[FILENAME_LENGTH];
	char cmd[CMD_LENGTH];
	FILE * pFile = NULL;

	loader = new Load(); 

	//initilize winsocket
	if (WSAStartup(0x0202,&wsadata)!=0)
	{  
		WSACleanup();  
		err_sys("Error in starting WSAStartup()\n");
	}


	//Display name of local host and copy it to the req
	if(gethostname(req.hostname,HOSTNAME_LENGTH)!=0) //get the hostname
		err_sys("can not get the host name,program exit");
	printf("ftp_tcp starting on host: %s",req.hostname);

	do {

		printf("\n\nType name of ftp server: ");
		cin >> serverName;

		if(strcmp(serverName, "quit") != 0) {

			//Create the socket
			if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) //create the socket 
				err_sys("Socket Creating Error");

			long ipAdr = ResolveName(serverName);
			if(ipAdr == -1) {
				printf("Unable to resolve server name.\n");
			}
			else {
				//connect to the server
				ServPort=REQUEST_PORT;
				memset(&ServAddr, 0, sizeof(ServAddr));     /* Zero out structure */
				ServAddr.sin_family      = AF_INET;             /* Internet address family */
				ServAddr.sin_addr.s_addr = ipAdr;   /* Server IP address */
				ServAddr.sin_port        = htons(ServPort); /* Server port */
				if (connect(sock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0) {
					printf("Failed to connect to server %s\n", serverName);
				}
				else {

					printf("\nType name of file to be transferred:  ");
					cin >> fileName;

					boolean correctCmd = false;
					do {
						printf("\nType direction of transfer: ");
						cin >> cmd;

						strcpy_s(req.filename, FILENAME_LENGTH, fileName);

						if(strcmp(cmd,"get")==0) {
							smsg.type=REQ_GET;
							correctCmd = true;
						}
						else if (strcmp(cmd,"put")==0)
						{
							smsg.type=REQ_PUT;
							correctCmd = true;

							fopen_s(&pFile, fileName,"rb");
							if(pFile == NULL) {
								printf("\nCannot open file %s : the file might not exist, or you don't have the permissions to open it.", fileName);
								smsg.type=REQ_CANCEL;
							}
						}
						else if (strcmp(cmd,"list")==0) {
							smsg.type=REQ_LIST;
							correctCmd = true;
						}
						else {
							printf("Wrong request command.\nAvailable:\n \t\tget\n\t\tput\n\t\tlist");
							correctCmd = false;
						}
					}while(!correctCmd);

					//send out the message
					memcpy(smsg.buffer,&req,sizeof(req)); //copy the request to the msg's buffer
					smsg.length=sizeof(req);
					if (msg_send(sock,&smsg) != sizeof(req))
						err_sys("Sending req packet error.,exit");

					if(smsg.type != REQ_CANCEL)
						printf("\nSent request to %s, waiting...\n", serverName);

					//receive the response
					if(msg_recv(sock,&rmsg)!=rmsg.length)
						err_sys("recv response error,exit");

					//cast it to the response structure
					respp=(Resp *)rmsg.buffer;

					// File not found
					if(strcmp(respp->response, "No such a file") == 0) 
					{
						// File does not exist
						printf("\n%s\n", respp->response);
					}
					// PUT
					else if(strcmp(respp->response, "OK") == 0) 
					{
						if (pFile!=NULL){
							loader->sendFile(pFile, fileName, serverName, sock);
						}

					} // END PUT

					// GET
					else if(strcmp(respp->response, "READY") == 0) 
					{
						loader->receiveFile(fileName, serverName, sock);
					} // END GET

					// LIST
					else if(strcmp(respp->response, "List files") == 0){
						getList(serverName);
					} // END LIST

					//close the client socket
					closesocket(sock);
				}
			}
		} // END if severname = quit
		else {
			printf("\nClient closing...");
			printf("\nPress any key to quit...");
			fflush(stdin);
			getchar();
		}

	}
	while(strcmp(serverName, "quit") != 0);

}

void TcpClient::getList(char* serverName)
{
	char resultSet[MAX_LIST_LENGTH];
	char* pch;
	char* next_tok = NULL;

	smsg.type = RESP;
	strcpy_s(smsg.buffer, BUFFER_LENGTH, "READY");
	smsg.length = sizeof("READY");

	if (msg_send(sock,&smsg) != sizeof("READY"))
		err_sys("Sending req packet error.,exit");

	// Start receiving the file
	if(msg_recv(sock,&rmsg)!=rmsg.length)
		err_sys("recv response error,exit");
	printf("\nList all the files from server %s\n", serverName);
	strcpy_s(resultSet, MAX_LIST_LENGTH, rmsg.buffer);
	pch =strtok_s(resultSet, ",", &next_tok);

	while (pch != NULL)
	{
		printf ("\n%s",pch);
		pch = strtok_s(NULL, ",", &next_tok);
	}
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

long TcpClient::ResolveName(char name[])
{
	struct hostent *host;            /* Structure containing host information */

	if ((host = gethostbyname(name)) == NULL)
		return -1;

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

int main(int argc, char *argv[]) 
{
	TcpClient * tc=new TcpClient();
	tc->run();
	return 0;
}
