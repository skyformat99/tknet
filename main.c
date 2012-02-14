#include "headers.h"

BOOL             g_MainLoopFlag = 1;
uchar            g_NATtype = NAT_T_UNKNOWN;
struct NetAddr   g_BdgPeerAddr;
char             g_TargetName[PEER_NAME_ID_LEN];
char             g_MyName[PEER_NAME_ID_LEN];

extern void ON_CONNECT();//demo

void tkNetInit()
{
	g_ConnectionNotify = &OnConnect;//demo
	tkInitRandom();
	tkLogInit();
	SockInit();

	g_TargetName[0]='\0';
	g_MyName[0]='\0';
}

void tkNetUninit()
{
	SockDestory();
	tkLogClose();
	printf("unfree memory:%d \n",g_allocs);
}

int main(int pa_argn,char **in_args)
{
	struct KeyInfoCache   KeyInfoCache;
	struct ProcessingList ProcList;
	struct BackGroundArgs BkgdArgs;
	struct PeerData       PeerDataRoot;
	struct Iterator       ISeedPeer;
	struct Sock           MainSock;
	struct BridgeProc     BdgServerProc;
	struct BridgeProc     BdgClientProc;
	struct BridgeProc     *pBdgClientProc = NULL;
	BOOL                  ifBdgClientProcMade = 0;
	char                  BdgPeerAddrStr[32];
	char                  *pTargetName = NULL;
	int                   TestPurposeNatType;

	tkNetInit();
	MutexInit(&g_BkgdMutex);

	ISeedPeer = GetIterator(NULL);

	PeerDataCons(&PeerDataRoot);
	PeerDataRoot.tpnd.RanPriority = 0;
	PeerDataRoot.addr.port = 0;
	PeerDataRoot.addr.IPv4 = 0;

	ProcessingListCons( &ProcList );

	RelayModuleInit();

	KeyInfoCacheCons(&KeyInfoCache);
	if(!KeyInfoReadFile(&KeyInfoCache,"tknet.info"))
	{
		printf("config file lost.\n");
		goto exit;
	}

	if(!KeyInfoTry(&KeyInfoCache,KEY_INFO_TYPE_CONFIG,&MainSock))
	{
		printf("please config port & name\n");
		goto exit;
	}

	if(pa_argn == 2)
	{
		//strcpy(g_TargetName,in_args[1]);
		{
			sscanf(in_args[1],"%d",&TestPurposeNatType);
			g_NATtype = (uchar)TestPurposeNatType;
		}
	}
	
	if( g_TargetName[0] != '\0' )
	{
		printf("Target Name: %s \n", g_TargetName);
		pTargetName = g_TargetName; 
	}
	else
	{
		printf("Target Name unset. \n");
	}

//	while(!KeyInfoTry(&KeyInfoCache,KEY_INFO_TYPE_STUNSERVER,&MainSock))
//	{
//		if(!KeyInfoTry(&KeyInfoCache,KEY_INFO_TYPE_MAILSERVER,&MainSock))
//		{
//			printf("No way to get NAT type :( \n");
//			goto exit;
//		}
//	}

	printf("NAT type: %d\n",g_NATtype);

	while(!KeyInfoTry(&KeyInfoCache,KEY_INFO_TYPE_BRIDGEPEER,&MainSock))
	{
		if(!KeyInfoTry(&KeyInfoCache,KEY_INFO_TYPE_MAILSERVER,&MainSock))
		{
			printf("no avalible Bridge peer :( \n");
			goto only_server;
		}
	}

	GetAddrText(&g_BdgPeerAddr,BdgPeerAddrStr);
	printf("using Bridge peer: %s\n",BdgPeerAddrStr);

	BridgeMakeClientProc(&BdgClientProc,&MainSock,&ProcList,&g_BdgPeerAddr,g_MyName,g_NATtype,pTargetName);
	//TaName can be NULL
	ProcessStart(&BdgClientProc.proc,&ProcList);
	ifBdgClientProcMade = 1;
	pBdgClientProc = &BdgClientProc;

only_server:

	BkgdArgs.pPeerDataRoot = &PeerDataRoot;
	BkgdArgs.pInfoCache = &KeyInfoCache;
	BkgdArgs.pProcList = &ProcList;
	BkgdArgs.pBdgClientProc = pBdgClientProc;
	tkBeginThread( &BackGround , &BkgdArgs );

	ConsAndStartBridgeServer(&BdgServerProc,&PeerDataRoot,&ProcList,&MainSock,&ISeedPeer);

	while( g_MainLoopFlag )
	{
		MutexLock(&g_BkgdMutex);
		SockRead(&MainSock);
		DoProcessing( &ProcList );
		MainSock.RecvLen = 0;
		MutexUnlock(&g_BkgdMutex);

		tkMsSleep(100);
	}

	SockClose(&MainSock);

	if(ifBdgClientProcMade)
		FreeBdgClientProc(&BdgClientProc);
	FreeBridgeServer(&BdgServerProc);

exit:

	PeerDataDestroy(&PeerDataRoot,&ISeedPeer);
	KeyInfoUpdate( &KeyInfoCache );
	KeyInfoWriteFile(&KeyInfoCache,"tknet.updateinfo");
	KeyInfoFree(&KeyInfoCache);
	RelayMuduleDestruction();
	MutexDelete(&g_BkgdMutex);
	tkNetUninit();

	return 0;
}
