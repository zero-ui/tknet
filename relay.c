#include "headers.h"

struct FindRelayProcByRelayIDPa
{
	uint RelayID;
	struct RelayProc *pFound;
};

DEF_STRUCT_CONSTRUCTOR( RelayProc ,
		PeerCons(&out_cons->peer0);
		PeerCons(&out_cons->peer1);
		out_cons->pSock = NULL;
		ProcessCons(&out_cons->proc);
		)

uint g_BdgRelaysNow = 0;
static struct Iterator sta_IRelayProc;
static struct Process  sta_RelayProcessTemplate;

static __inline void
RelayTo(struct RelayProc *pa_pRelayProc,struct NetAddr *pa_pToAddr)
{
	DEF_AND_CAST(pMsg,struct TkNetMsg,pa_pRelayProc->pSock->RecvBuff);
	SockLocateTa(pa_pRelayProc->pSock,htonl(pa_pToAddr->IPv4),pa_pToAddr->port);
	SockWrite(pa_pRelayProc->pSock,BYS(*pMsg)); 
}

DEF_FREE_LIST_ELEMENT_SAFE_FUNCTION(FreeRelayProc,struct RelayProc,ln,;)

EXTERN_STEP( Relay )

void
RelayModuleInit()
{
	sta_IRelayProc = GetIterator(NULL);
	
	ProcessCons(&sta_RelayProcessTemplate);
	PROCESS_ADD_STEP( &sta_RelayProcessTemplate , Relay , 7000 , 0 );
}

void 
RelayMuduleDestruction()
{
	ForEach(&sta_IRelayProc,&FreeRelayProc,NULL);
	ProcessFree(&sta_RelayProcessTemplate);
}

STEP( Relay )
{
	struct RelayProc *pRelayProc = GET_STRUCT_ADDR(pa_pProc,struct RelayProc,proc);
	struct NetAddr   FromAddr = GetAddrFromSockAddr(&pRelayProc->pSock->AddrRecvfrom);
	DEF_AND_CAST(pMsg,struct TkNetMsg,pRelayProc->pSock->RecvBuff);

	if(pMsg->flag != TK_NET_BDG_MSG_FLAG &&
			pRelayProc->peer1.addr.port != 0)
	{
		if(ifNetAddrEqual(&FromAddr,&(pRelayProc->peer0.addr)))
		{
			RelayTo(pRelayProc,&(pRelayProc->peer1.addr));
			return PS_CALLBK_RET_REDO;
		}
		else if(ifNetAddrEqual(&FromAddr,&(pRelayProc->peer1.addr)))
		{
			RelayTo(pRelayProc,&(pRelayProc->peer0.addr));
			return PS_CALLBK_RET_REDO;
		}
	}

	if(pa_state == PS_STATE_LAST_TIME)
	{
		return PS_CALLBK_RET_ABORT;
	}

	return PS_CALLBK_RET_GO_ON;
}

BOOL
LIST_ITERATION_CALLBACK_FUNCTION(FindRelayProcByRelayID)
{
	struct RelayProc *pRelayProc
		= GET_STRUCT_ADDR_FROM_IT(pa_pINow,struct RelayProc,ln);
	DEF_AND_CAST(pFRPBRIPa,struct FindRelayProcByRelayIDPa ,pa_else);

	if(pFRPBRIPa->RelayID == pRelayProc->RelayID)
	{
		pFRPBRIPa->pFound = pRelayProc;
		return 1;
	}
	else
	{
		return pa_pINow->now == pa_pIHead->last;
	}
}

void
RelayProcNotify(struct Process *pa_)
{
	struct RelayProc *pRelayProc = GET_STRUCT_ADDR(pa_,struct RelayProc,proc);
	struct Iterator IForward,INow;

	INow = GetIterator(&pRelayProc->ln);
	IForward = GetIterator(INow.now->next);
	FreeRelayProc(&sta_IRelayProc,&INow,&IForward,NULL);

	tkfree(pRelayProc);
		
	g_BdgRelaysNow --;
	printf("relay proc end\n");
}

uchar 
RelayProcMerge(uint pa_RelayID,struct NetAddr pa_addr,struct ProcessingList *pa_pProcList,struct Iterator *pa_pINow, struct Iterator *pa_pIForward,struct Sock *pa_pSock)
{
	struct RelayProc *pRelayProc;
	struct FindRelayProcByRelayIDPa FRPBRIPa;
	
	FRPBRIPa.pFound = NULL;
	ForEach(&sta_IRelayProc,&FindRelayProcByRelayID,&FRPBRIPa);

	if(FRPBRIPa.pFound == NULL)
	{
		pRelayProc = tkmalloc(struct RelayProc);
		RelayProcCons(pRelayProc);

		pRelayProc->RelayID = pa_RelayID;
		pRelayProc->peer0.addr = pa_addr;
		pRelayProc->pSock = pa_pSock;
		pRelayProc->proc.NotifyCallbk = &RelayProcNotify;

		AddOneToListTail(&sta_IRelayProc,&pRelayProc->ln);
		ProcessSafeStart(&pRelayProc->proc,pa_pProcList,pa_pINow,pa_pIForward);

		return 0;
	}
	else if(FRPBRIPa.pFound->peer1.addr.port == 0)
	{
		FRPBRIPa.pFound->peer1.addr = pa_addr;
		g_BdgRelaysNow ++;

		return 1;
	}

	return 0;
}

BOOL
LIST_ITERATION_CALLBACK_FUNCTION(TraceRelayProc)
{
	struct RelayProc *pRelayProc
		= GET_STRUCT_ADDR_FROM_IT(pa_pINow,struct RelayProc,ln);
	char AddrText0[32];
	char AddrText1[32];
		
	GetAddrText(&(pRelayProc->peer0.addr),AddrText0);

	if(pRelayProc->peer1.addr.port == 0)
	{
		strcpy(AddrText1,"waiting");
	}
	else
	{
		GetAddrText(&(pRelayProc->peer1.addr),AddrText1);
	}
	
	printf("RelayID = %d ,(%s,%s)\n",pRelayProc->RelayID,AddrText0,AddrText1);

	return pa_pINow->now == pa_pIHead->last;
}

void 
RelayProcTrace()
{
	printf("relays(total = %d): \n",g_BdgRelaysNow);
	ForEach(&sta_IRelayProc,&FindRelayProcByRelayID,NULL);
}
