#pragma comment( linker, "/defaultlib:ws2_32.lib" )

#include <winsock.h>
#include <iostream>
#include <windows.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <process.h>
#include <tchar.h>
#include <strsafe.h>
#include "Thread.h"
#include "server.h"
#include <vector>
using namespace std;
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
	Type type;
	char* buffer;
	long lSize;
	size_t fileSize;
	char fileName[FILENAME_LENGTH] = "";
	char fileList[MAX_LIST_LENGTH] = {0};	
	if(msg_recv(cs,&rmsg)!=rmsg.length)
		err_sys("Receive Req error,exit");

	//cast it to the request packet structure		
	reqp=(Req *)rmsg.buffer;	
	type = rmsg.type;
	//contruct the response and send it out
	smsg.type=RESP;
	smsg.length=sizeof(Resp);
	memset(resp.response,0,sizeof(resp));

	switch (type)
	{
	case REQ_GET:
		printf("User from %s requested file %s to be sent.", reqp->hostname, reqp->filename);

		if((result = _stat(reqp->filename,&stat_buf))!=0){
			sprintf_s(resp.response, RESP_LENGTH, "No such a file");
			memcpy(smsg.buffer,&resp,sizeof(resp));
			// Send the file size if cmd is GET, READY if cmd is PUT and the list of available files if cmd is LIST
			if(msg_send(cs,&smsg)!=smsg.length)
				err_sys("send Respose failed,exit");
		}
		else {	
			sprintf_s(resp.response, RESP_LENGTH,"File size:%ld",stat_buf.st_size );
			memcpy(smsg.buffer,&resp,sizeof(resp));
			// Send the file size if cmd is GET, READY if cmd is PUT and the list of available files if cmd is LIST
			if(msg_send(cs,&smsg)!=smsg.length)
				err_sys("send Respose failed,exit");

			if(msg_recv(cs,&rmsg)!=rmsg.length)
				err_sys("Receive Req error,exit");

			if(strcmp(rmsg.buffer, "READY") == 0) {			
				FILE * pFile;
				fopen_s(&pFile, reqp->filename,"rb");
				if (pFile!=NULL){

					fseek (pFile , 0 , SEEK_END);
					lSize = ftell (pFile);
					rewind (pFile);
					buffer = (char*) malloc (sizeof(char)*lSize);

					if (buffer != NULL){
						result = fread (buffer,1,lSize,pFile);
						if (result == lSize){
							sprintf_s(resp.response, RESP_LENGTH, buffer);
							memcpy(smsg.buffer, &resp,sizeof(resp));
							//printf("\nContent is: \n%s", smsg.buffer);
							if(msg_send(cs,&smsg)!=smsg.length)
								err_sys("send Respose failed,exit");

							if(msg_recv(cs,&rmsg)!=rmsg.length)
								err_sys("Receive Req error,exit");
							printf("File sent to the client %s", "successfully!");
						}
						else{
							printf("\nUnable to read from file: %s", reqp->filename);
						}
					}
					else{
						printf("\nUnable to allocate memory for file: %s", reqp->filename);
					}

				}
				else{
					printf("\ncannot open file %s", reqp->filename);
				}

				// start sending the file
				// TODO
			}
		}
		break;
	case REQ_PUT:
		strcpy_s(fileName, FILENAME_LENGTH, reqp->filename);
		// Deal with size / receive the file
		printf("User from %s will send file %s.\n", reqp->hostname, fileName);
		/*strcpy_s(smsg.buffer, BUFFER_LENGTH, "SERVER IS READY");
		smsg.length = sizeof("SERVER IS READY");
		if(msg_send(cs,&smsg)!=smsg.length)
		err_sys("send Respose failed,exit");*/

		/*if(msg_recv(cs,&rmsg)!=rmsg.length)
		err_sys("Receive Req error,exit");
		if(strcmp(rmsg.buffer, "START SENDING") != 0) 
		printf("\n%s\n", "Server is not ready.");*/

		smsg.type = RESP;
		strcpy_s(smsg.buffer, BUFFER_LENGTH, "OK");
		smsg.length = sizeof("OK");

		if (msg_send(cs,&smsg) != sizeof("OK"))
			err_sys("Sending req packet error.,exit");


		printf("Server start receving file %s\n", fileName);			
		FILE * pFile;
		fopen_s(&pFile, fileName,"wb");
		if (pFile!=NULL){

			if(msg_recv(cs,&rmsg)!=rmsg.length)
				err_sys("Receive Req error,exit");

			while(strcmp(rmsg.buffer, "END") != 0) {
				fwrite(rmsg.buffer, 1, sizeof(rmsg.buffer), pFile);
				
				if(msg_recv(cs,&rmsg)!=rmsg.length)
					err_sys("Receive Req error,exit");
			}

			fclose(pFile);
			smsg.type = RESP;
			strcpy_s(smsg.buffer, BUFFER_LENGTH, "Got the file");
			smsg.length = sizeof("Got the file");

			if (msg_send(cs,&smsg) != sizeof("Got the file"))
				err_sys("Sending req packet error.,exit");

			printf("Finish downloading the file: %s\n", fileName);

			break;
	case REQ_LIST:
		smsg.type = RESP;
		strcpy_s(smsg.buffer, BUFFER_LENGTH, "List files");
		smsg.length = sizeof("List files");

		if (msg_send(cs,&smsg) != sizeof("List files"))
			err_sys("Sending req packet error.,exit");
		if(msg_recv(cs,&rmsg)!=rmsg.length)
			err_sys("Receive Req error,exit");
		if(strcmp(rmsg.buffer, "READY") != 0) 
			printf("\n%s\n", "Server is not ready.");

		WIN32_FIND_DATA search_data;
		TCHAR currentDir[20];
		StringCchCopy(currentDir, 15, TEXT(".\\*"));

		memset(&search_data, 0, sizeof(WIN32_FIND_DATA));

		HANDLE handle = FindFirstFile(currentDir, &search_data);

		if(handle != INVALID_HANDLE_VALUE)
		{
			do {				
				size_t i;
				char *fName = (char*) malloc(FILENAME_LENGTH);
				wcstombs_s(&i, fName, (size_t) FILENAME_LENGTH, search_data.cFileName, (size_t) FILENAME_LENGTH);

				if(strstr(fName,"txt")){
					strcat_s(fileList, MAX_LIST_LENGTH, fName);
					strcat_s(fileList, MAX_LIST_LENGTH, ",");
				}
			}while (FindNextFile(handle, &search_data) != 0);
		}

		FindClose(handle);

		smsg.type = RESP;
		strcpy_s(smsg.buffer, BUFFER_LENGTH, fileList);
		smsg.length = sizeof(fileList);

		if (msg_send(cs,&smsg) != sizeof(fileList))
			err_sys("Sending req packet error.,exit");

		break;
		}

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


