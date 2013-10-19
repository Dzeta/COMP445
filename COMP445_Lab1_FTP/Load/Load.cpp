/**
 *		Lab Assignment 1 - COMP 445
 *
 *		Zuo Xiang ZHOU	- 9279148 
 *		FLEURY Gaetan	- 6380565
 *
**/

#include "Load.h"

void Load::sendFile(FILE* pFile, char* fileName, char* destName, int sock) {
	long lSize = 0;
	long sizeSent = 0;
	int result = 0;

	char buffer[BUFFER_LENGTH] = "";

	fseek (pFile , 0 , SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);

	smsg.type = RESP;
	sprintf_s(smsg.buffer, BUFFER_LENGTH, "%ld", lSize);
	smsg.length = sizeof(buffer);

	if (msg_send(sock,&smsg) != sizeof(buffer))
		err_sys("Sending req packet error.,exit");

	printf("Start sending the file %s to %s\n", fileName,destName);
	while(sizeSent < lSize)
	{
		int toRead = (lSize - sizeSent < BUFFER_LENGTH) ? lSize - sizeSent : BUFFER_LENGTH;

		fread (buffer,1,toRead,pFile);

		smsg.type = RESP;
		memcpy(smsg.buffer, buffer, BUFFER_LENGTH);
		smsg.length = sizeof(buffer);

		sizeSent += toRead;

		if (msg_send(sock,&smsg) != sizeof(buffer))
			err_sys("Sending req packet error.,exit");
	}

	fclose(pFile);
	pFile = NULL;

	smsg.type = RESP;
	strcpy_s(smsg.buffer, BUFFER_LENGTH, "END");
	smsg.length = sizeof("END");

	if (msg_send(sock,&smsg) != sizeof("END"))
		err_sys("Sending req packet error.,exit");

	if(msg_recv(sock,&rmsg)!=rmsg.length)
		err_sys("Receive Req error,exit");

	if(strcmp(rmsg.buffer, "Got the file") == 0) 
		printf("File successfully sent to %s", destName);
}

void Load::receiveFile(char* fileName, char* sourceName, int sock) {
	printf("Retrieving file %s from %s.\n", fileName, sourceName);

	smsg.type = RESP;
	strcpy_s(smsg.buffer, BUFFER_LENGTH, "OK");
	smsg.length = sizeof("OK");

	if (msg_send(sock,&smsg) != sizeof("OK"))
		err_sys("Sending req packet error.,exit");

	long lSize = 0;
	long receiveSize = 0;

	if(msg_recv(sock,&rmsg)!=rmsg.length)
		err_sys("Receive Req error,exit");

	lSize = atol(rmsg.buffer);

	FILE * pFile;
	fopen_s(&pFile, fileName,"wb");
	if (pFile!=NULL){

		if(msg_recv(sock,&rmsg)!=rmsg.length)
			err_sys("Receive Req error,exit");

		printf("First bytes of %s received\n", fileName);

		while(receiveSize < lSize) {

			int toWrite = (lSize - receiveSize < BUFFER_LENGTH) ? lSize - receiveSize : BUFFER_LENGTH;

			fwrite(rmsg.buffer, 1, toWrite, pFile);

			receiveSize += toWrite;

			if(msg_recv(sock,&rmsg)!=rmsg.length)
				err_sys("Receive Req error,exit");
		}

		fclose(pFile);
		pFile = NULL;

		if(strcmp(rmsg.buffer, "END") == 0)
		{
			smsg.type = RESP;
			strcpy_s(smsg.buffer, BUFFER_LENGTH, "Got the file");
			smsg.length = sizeof("Got the file");

			if (msg_send(sock,&smsg) != sizeof("Got the file"))
				err_sys("Sending req packet error.,exit");

			printf("Finish downloading the file: %s\n", fileName);
		}
		else
		{
			printf("An error occured while receiving the file");
		}
	}

}


int Load::msg_recv(int sock,Msg * msg_ptr)
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
int Load::msg_send(int sock,Msg * msg_ptr)
{
	int n;
	if((n=send(sock,(char *)msg_ptr,MSGHDRSIZE+msg_ptr->length,0))!=(MSGHDRSIZE+msg_ptr->length))
		err_sys("Send MSGHDRSIZE+length Error");
	return (n-MSGHDRSIZE);

}

void Load::err_sys(char * fmt,...) //from Richard Stevens's source code
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