#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h> 

#define BUF_SIZE 100
#define MAX_CLNT 256
#define MAX_ROOM 20

char lobbyMessage[] = "\n==========로비=========\n1. 방 목록 보기\n2. 방 만들기\n3. 종료\n=======================\n입력 : ";

int clntCnt=0;
SOCKET clntSocks[MAX_CLNT];
HANDLE hMutex;

int roomCnt = 0;
typedef struct Room{
	SOCKET clntSocks[5];
	char name[BUF_SIZE]; 
	int cnt;
}Room;
Room roomList[MAX_ROOM];

enum Position{
	lobby,
	room,
	makingRoom,
};

unsigned WINAPI HandleClnt(void * arg);
bool OnLobby(SOCKET clntSock, Position* pos);
void OnRoom(SOCKET clntSock, Position* pos);
void SelectRoom(SOCKET clntSock, Position* pos);
void AddRoom(SOCKET clntSock, Position* pos);
void Exit(SOCKET clntSock);
void ExitRoom(SOCKET clntSock, char* roomName);
char* GetRoomName(SOCKET clntSock);
void SendMsg(char *msg, SOCKET clntSock);
void SendMsgInRoom(char * msg, char * roomName);
void SendMsgAll(char * msg);
unsigned StartPosOfMsg(char * msg);
void ErrorHandling(char * msg);

int main(int argc, char *argv[])
{
	WSADATA wsaData;
	SOCKET hServSock, hClntSock;
	SOCKADDR_IN servAdr, clntAdr;
	int clntAdrSz;
	HANDLE  hThread;
	
	if(argc!=2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	if(WSAStartup(MAKEWORD(2, 2), &wsaData)!=0)
		ErrorHandling("WSAStartup() error!"); 
  
	hMutex=CreateMutex(NULL, FALSE, NULL);
	hServSock=socket(PF_INET, SOCK_STREAM, 0);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family=AF_INET; 
	servAdr.sin_addr.s_addr=htonl(INADDR_ANY);
	servAdr.sin_port=htons(atoi(argv[1]));
	
	if(bind(hServSock, (SOCKADDR*) &servAdr, sizeof(servAdr))==SOCKET_ERROR)
		ErrorHandling("bind() error");
	if(listen(hServSock, 5)==SOCKET_ERROR)
		ErrorHandling("listen() error");
	
	while(1)
	{
		clntAdrSz=sizeof(clntAdr);
		hClntSock=accept(hServSock, (SOCKADDR*)&clntAdr,&clntAdrSz);
		
		WaitForSingleObject(hMutex, INFINITE);
		clntSocks[clntCnt++]=hClntSock;
		ReleaseMutex(hMutex);
	
		hThread= (HANDLE)_beginthreadex(NULL, 0, HandleClnt, (void*)&hClntSock, 0, NULL);
		printf("Connected client IP: %s \n", inet_ntoa(clntAdr.sin_addr));
	}
	closesocket(hServSock);
	WSACleanup();
	
	return 0;
}
	
unsigned WINAPI HandleClnt(void * arg)
{
	SOCKET hClntSock=*((SOCKET*)arg);
	enum Position pos = lobby;
	bool exitFlag = false;
	
	while(true){
		switch(pos){
			case lobby:
				exitFlag = OnLobby(hClntSock, &pos);
				break;
			case room:
				OnRoom(hClntSock, &pos);
				break;
			default:
				break;
		}
		if(exitFlag)
			return 0;
	}
	Exit(hClntSock);
	
	return 0;
}

bool OnLobby(SOCKET clntSock, Position* pos){
	int strLen=0, orgPos;
	char msg[BUF_SIZE];
	
	SendMsg(lobbyMessage, clntSock);
	strLen = recv(clntSock, msg, sizeof(msg), 0);
	msg[strLen]=0;
	orgPos = StartPosOfMsg(msg);
	
	if(!strncmp(msg+orgPos, "1", 1))
		SelectRoom(clntSock, pos);
	else if(!strncmp(msg+orgPos, "2", 1))
		AddRoom(clntSock, pos);
	else if(!strncmp(msg+orgPos, "3", 1)){
		Exit(clntSock);
		return true;
	}
	return false;
}

void SelectRoom(SOCKET clntSock, Position* pos){
	int i, orgPos, strLen=0;
	char msg[BUF_SIZE];
	char str[BUF_SIZE];
	bool isEntered = false;
	
	WaitForSingleObject(hMutex, INFINITE);
	SendMsg("\n방 개수 : ", clntSock);
	itoa(roomCnt, str, 10);
	SendMsg(str, clntSock); 
	
	SendMsg("\n=====방 목록=====\n", clntSock);
	for(i=0; i<roomCnt; i++){
		Room room = roomList[i];
		SendMsg(room.name, clntSock);
	}
	SendMsg("=================\n", clntSock);
	ReleaseMutex(hMutex);
	
	while(true){
		SendMsg("접속할 방 이름 입력하세요!(q누르면 로비로 이동)\n입력: ", clntSock);
		strLen = recv(clntSock, msg, sizeof(msg), 0);	
		msg[strLen] = 0;
		orgPos = StartPosOfMsg(msg);
		if(!strcmp(msg+orgPos, "나가기")){
			ReleaseMutex(hMutex);
			break;
		}
		WaitForSingleObject(hMutex, INFINITE);
		//printf("Selection: %s %s\n", msg+orgPos, roomList[0].name);
		for(i=0; i<roomCnt; i++){
			if(!strcmp(msg+orgPos, roomList[i].name)) {
				roomList[i].clntSocks[roomList[i].cnt++] = clntSock;
				isEntered = true;
			}	
		}
		if(isEntered){
			*pos = room;
			ReleaseMutex(hMutex);
			break;
		}
		else SendMsg("\n방을 찾지 못했습니다, 다시 입력하십시오!\n", clntSock);
		ReleaseMutex(hMutex);
	}
	
}

void AddRoom(SOCKET clntSock, Position* pos){
	int strLen=0, orgPos;
	char msg[BUF_SIZE];
	
	SendMsg("방 이름 입력하세요...\n", clntSock);
	strLen = recv(clntSock, msg, sizeof(msg), 0);
	msg[strLen]=0;
	orgPos = StartPosOfMsg(msg);
	
	WaitForSingleObject(hMutex, INFINITE);
	Room room;
	strcpy(room.name, msg+orgPos);
	roomList[roomCnt++] = room; 
	ReleaseMutex(hMutex);
}


void OnRoom(SOCKET clntSock, Position* pos){
	int strLen=0, i, orgPos;
	char msg[BUF_SIZE];
	char* roomName = GetRoomName(clntSock);
	
	SendMsg("@@@방입장@@@   ('나가기'를 입력하면 방 나감..)\n\n", clntSock);
	while((strLen=recv(clntSock, msg, sizeof(msg), 0))>0){
		msg[strLen]=0;
		orgPos = StartPosOfMsg(msg);
		if(!strncmp(msg+orgPos, "나가기", 3)) {
			*pos = lobby;
			ExitRoom(clntSock, roomName);
			break;
		}	
		SendMsgInRoom(msg, roomName);
		printf("%s", msg);
	}
}

char* GetRoomName(SOCKET clntSock){
	int i, j;
	
	char *roomName = (char *)malloc(sizeof(char) * BUF_SIZE);
	WaitForSingleObject(hMutex, INFINITE);
	for(i=0; i<roomCnt; i++){
		Room room = roomList[i];
		for(j=0; j<room.cnt; j++){
			if(room.clntSocks[j] == clntSock){
				strcpy(roomName, room.name);
				break;
			}
		}
	}
	ReleaseMutex(hMutex);
	return roomName;
}

void Exit(SOCKET clntSock){
	int i;
	
	WaitForSingleObject(hMutex, INFINITE);
	for(i=0; i<clntCnt; i++)   // remove disconnected client
	{
		if(clntSock==clntSocks[i])
		{
			while(i++<clntCnt-1)
				clntSocks[i]=clntSocks[i+1];
			break;
		}
	}
	clntCnt--;
	ReleaseMutex(hMutex);
	closesocket(clntSock);
}

void ExitRoom(SOCKET clntSock, char* roomName){
	int i, j, k;
	
	for(i=0; i<roomCnt; i++){
		if(!strcmp(roomList[i].name, roomName)){
			for(j=0; j<roomList[i].cnt; j++){
				if(roomList[i].clntSocks[j] == clntSock){
					roomList[i].clntSocks[j] == NULL;
					k = j;
					break;
				}
			}
			if(k==4) break;
			for(j=k+1; j<5; j++){
				roomList[i].clntSocks[j-1] = roomList[i].clntSocks[j];
				roomList[i].clntSocks[j] = NULL;
			}
			roomList[i].cnt--;
			break;
		}
	}
} 


void SendMsg(char * msg, SOCKET clntSock){     // send to one
	send(clntSock, msg, strlen(msg), 0);
}

void SendMsgInRoom(char * msg, char * roomName){  // send to room members
	int i, j;
	
	WaitForSingleObject(hMutex, INFINITE);
	for(i=0; i<roomCnt; i++){
		if(!strcmp(roomList[i].name, roomName)){
			for(j=0; j<roomList[i].cnt; j++)
				SendMsg(msg, roomList[i].clntSocks[j]);
		}
	}
	ReleaseMutex(hMutex);
}

void SendMsgAll(char * msg){   // send to all
	int i;
	
	WaitForSingleObject(hMutex, INFINITE);
	for(i=0; i<clntCnt; i++)
		send(clntSocks[i], msg, strlen(msg), 0);
	ReleaseMutex(hMutex);
}


unsigned StartPosOfMsg(char * msg){
	int i, pos;
	
	for(i=0; i<strlen(msg); i++){
		if(msg[i] == ']')
			return i+2;	
	}
	return 0;
}

void ErrorHandling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

