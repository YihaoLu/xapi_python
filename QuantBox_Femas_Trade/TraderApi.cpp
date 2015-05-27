#include "stdafx.h"
#include "TraderApi.h"

#include "../include/QueueEnum.h"
#include "../include/QueueHeader.h"

#include "../include/ApiHeader.h"
#include "../include/ApiStruct.h"

#include "../include/toolkit.h"

#include "../QuantBox_Queue/MsgQueue.h"

#include "TypeConvert.h"

#include <cfloat>
#include <cstring>
#include <assert.h>

void* __stdcall Query(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	// ���ڲ����ã����ü���Ƿ�Ϊ��
	CTraderApi* pApi = (CTraderApi*)pApi2;
	pApi->QueryInThread(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
	return nullptr;
}

void CTraderApi::QueryInThread(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	int iRet = 0;
	switch (type)
	{
	case E_Init:
		iRet = _Init();
		break;
	case E_ReqUserLoginField:
		iRet = _ReqUserLogin(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryUserInvestorField:
		iRet = _ReqQryUserInvestor(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryInvestorAccountField:
		iRet = _ReqQryInvestorAccount(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryInvestorPositionField:
		iRet = _ReqQryInvestorPosition(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryInstrumentField:
		iRet = _ReqQryInstrument(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryOrderField:
		iRet = _ReqQryOrder(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryTradeField:
		iRet = _ReqQryTrade(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	default:
		break;
	}

	if (0 == iRet)
	{
		//���سɹ�����ӵ��ѷ��ͳ�
		m_nSleep = 1;
	}
	else
	{
		m_msgQueue_Query->Input_Copy(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		//ʧ�ܣ���4���ݽ�����ʱ����������1s
		m_nSleep *= 4;
		m_nSleep %= 1023;
	}
	this_thread::sleep_for(chrono::milliseconds(m_nSleep));
}

CTraderApi::CTraderApi(void)
{
	m_pApi = nullptr;
	m_lRequestID = 0;
	m_nSleep = 1;

	// �Լ�ά��������Ϣ����
	m_msgQueue = new CMsgQueue();
	m_msgQueue_Query = new CMsgQueue();

	m_msgQueue_Query->Register((void*)Query,this);
	m_msgQueue_Query->StartThread();
}


CTraderApi::~CTraderApi(void)
{
	Disconnect();
}

void CTraderApi::Register(void* pCallback, void* pClass)
{
	m_pClass = pClass;
	if (m_msgQueue == nullptr)
		return;

	m_msgQueue_Query->Register((void*)Query,this);
	m_msgQueue->Register(pCallback,this);
	if (pCallback)
	{
		m_msgQueue_Query->StartThread();
		m_msgQueue->StartThread();
	}
	else
	{
		m_msgQueue_Query->StopThread();
		m_msgQueue->StopThread();
	}
}

bool CTraderApi::IsErrorRspInfo(CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	bool bRet = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	if (bRet)
	{
		ErrorField* pField = (ErrorField*)m_msgQueue->new_block(sizeof(ErrorField));

		pField->ErrorID = pRspInfo->ErrorID;
		strcpy(pField->ErrorMsg, pRspInfo->ErrorMsg);

		m_msgQueue->Input_NoCopy(ResponeType::OnRtnError, m_msgQueue, m_pClass, bIsLast, 0, pField, sizeof(ErrorField), nullptr, 0, nullptr, 0);
	}
	return bRet;
}

bool CTraderApi::IsErrorRspInfo(CUstpFtdcRspInfoField *pRspInfo)
{
	bool bRet = ((pRspInfo) && (pRspInfo->ErrorID != 0));

	return bRet;
}

void CTraderApi::Connect(const string& szPath,
	ServerInfoField* pServerInfo,
	UserInfoField* pUserInfo)
{
	m_szPath = szPath;
	memcpy(&m_ServerInfo, pServerInfo, sizeof(ServerInfoField));
	memcpy(&m_UserInfo, pUserInfo, sizeof(UserInfoField));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_Init, m_msgQueue_Query, this, 0, 0,
		nullptr, 0, nullptr, 0, nullptr, 0);
}

int CTraderApi::_Init()
{
	char *pszPath = new char[m_szPath.length() + 1024];
	srand((unsigned int)time(nullptr));
	sprintf(pszPath, "%s/%s/%s/Td/%d/", m_szPath.c_str(), m_ServerInfo.BrokerID, m_UserInfo.UserID, rand());
	makedirs(pszPath);

	m_pApi = CUstpFtdcTraderApi::CreateFtdcTraderApi(pszPath);
	delete[] pszPath;

	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Initialized, 0, nullptr, 0, nullptr, 0, nullptr, 0);

	if (m_pApi)
	{
		m_pApi->RegisterSpi(this);

		//��ӵ�ַ
		size_t len = strlen(m_ServerInfo.Address) + 1;
		char* buf = new char[len];
		strncpy(buf, m_ServerInfo.Address, len);

		char* token = strtok(buf, _QUANTBOX_SEPS_);
		while (token)
		{
			if (strlen(token)>0)
			{
				m_pApi->RegisterFront(token);
			}
			token = strtok(nullptr, _QUANTBOX_SEPS_);
		}
		delete[] buf;

		if (m_ServerInfo.PublicTopicResumeType<ResumeType::Undefined)
			m_pApi->SubscribePublicTopic((USTP_TE_RESUME_TYPE)m_ServerInfo.PublicTopicResumeType);
		if (m_ServerInfo.PrivateTopicResumeType<ResumeType::Undefined)
			m_pApi->SubscribePrivateTopic((USTP_TE_RESUME_TYPE)m_ServerInfo.PrivateTopicResumeType);
		// ����������ɽ��ر���������
		//if (m_ServerInfo.UserTopicResumeType<ResumeType::Undefined)
		//	m_pApi->SubscribeUserTopic((USTP_TE_RESUME_TYPE)m_ServerInfo.UserTopicResumeType);
		//m_pApi->SubscribeForQuote(USTP_TERT_RESTART); //����ѯ��

		//��ʼ������
		m_pApi->Init();
		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Connecting, 0, nullptr, 0, nullptr, 0, nullptr, 0);
	}

	return 0;
}

void CTraderApi::Disconnect()
{
	if (m_msgQueue_Query)
	{
		m_msgQueue_Query->StopThread();
		m_msgQueue_Query->Register(nullptr,nullptr);
		m_msgQueue_Query->Clear();
		delete m_msgQueue_Query;
		m_msgQueue_Query = nullptr;
	}

	if (m_pApi)
	{
		m_pApi->RegisterSpi(nullptr);
		m_pApi->Release();
		m_pApi = nullptr;

		// ȫ����ֻ�����һ��
		m_msgQueue->Clear();
		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		// ��������
		m_msgQueue->Process();
	}

	if (m_msgQueue)
	{
		m_msgQueue->StopThread();
		m_msgQueue->Register(nullptr,nullptr);
		m_msgQueue->Clear();
		delete m_msgQueue;
		m_msgQueue = nullptr;
	}

	m_lRequestID = 0;

	Clear();
}

void CTraderApi::Clear()
{
	for (unordered_map<string, OrderField*>::iterator it = m_id_platform_order.begin(); it != m_id_platform_order.end(); ++it)
		delete it->second;
	m_id_platform_order.clear();

	for (unordered_map<string, CUstpFtdcOrderField*>::iterator it = m_id_api_order.begin(); it != m_id_api_order.end(); ++it)
		delete it->second;
	m_id_api_order.clear();

	for (unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.begin(); it != m_id_platform_quote.end(); ++it)
		delete it->second;
	m_id_platform_quote.clear();

	//for (unordered_map<string, CUstpFtdcRtnQuoteField*>::iterator it = m_id_api_quote.begin(); it != m_id_api_quote.end(); ++it)
	//	delete it->second;
	//m_id_api_quote.clear();

	//for (unordered_map<string, PositionField*>::iterator it = m_id_platform_position.begin(); it != m_id_platform_position.end(); ++it)
	//	delete it->second;
	//m_id_platform_position.clear();
}


void CTraderApi::OnFrontConnected()
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Connected, 0, nullptr, 0, nullptr, 0, nullptr, 0);

	ReqUserLogin();
}

void CTraderApi::OnFrontDisconnected(int nReason)
{
	RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

	//����ʧ�ܷ��ص���Ϣ��ƴ�Ӷ��ɣ���Ҫ��Ϊ��ͳһ���
	pField->ErrorID = nReason;
	GetOnFrontDisconnectedMsg(nReason, pField->ErrorMsg);

	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
}

void CTraderApi::ReqUserLogin()
{
	CUstpFtdcReqUserLoginField* pBody = (CUstpFtdcReqUserLoginField*)m_msgQueue_Query->new_block(sizeof(CUstpFtdcReqUserLoginField));

	strncpy(pBody->UserID, m_UserInfo.UserID, sizeof(TUstpFtdcInvestorIDType));
	strncpy(pBody->BrokerID, m_ServerInfo.BrokerID, sizeof(TUstpFtdcBrokerIDType));
	strncpy(pBody->Password, m_UserInfo.Password, sizeof(TUstpFtdcPasswordType));
	strncpy(pBody->UserProductInfo, m_ServerInfo.UserProductInfo, sizeof(TUstpFtdcProductInfoType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_ReqUserLoginField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CUstpFtdcReqUserLoginField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqUserLogin(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Logining, 0, nullptr, 0, nullptr, 0, nullptr, 0);
	return m_pApi->ReqUserLogin((CUstpFtdcReqUserLoginField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspUserLogin(CUstpFtdcRspUserLoginField *pRspUserLogin, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

	if (!IsErrorRspInfo(pRspInfo)
		&&pRspUserLogin)
	{
		pField->TradingDay = GetDate(pRspUserLogin->TradingDay);
		pField->LoginTime = GetTime(pRspUserLogin->LoginTime);

		//sprintf(pField->SessionID, "%d:%d", pRspUserLogin->FrontID, pRspUserLogin->SessionID);

		memcpy(&m_RspUserLogin__, pField, sizeof(RspUserLoginField));

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Logined, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);

		// ���µ�¼��Ϣ�����ܻ��õ�
		memcpy(&m_RspUserLogin,pRspUserLogin,sizeof(CUstpFtdcRspUserLoginField));
		long long x = 0;
		for (int i = 0; i<12; ++i)//��ط�Ϊ��һ��Ҫ��12
		{
			if (m_RspUserLogin.MaxOrderLocalID[i] == 0)
			{
				m_RspUserLogin.MaxOrderLocalID[i] = '0';
			}
			long long b = (m_RspUserLogin.MaxOrderLocalID[i] - '0');
			x = x * 10L + b;
		}

		m_nMaxOrderRef = x + 1;
		ReqQryUserInvestor();
	}
	else
	{
		pField->ErrorID = pRspInfo->ErrorID;
		strcpy(pField->ErrorMsg, pRspInfo->ErrorMsg);

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::ReqQryUserInvestor()
{
	CUstpFtdcQryUserInvestorField* pBody = (CUstpFtdcQryUserInvestorField*)m_msgQueue_Query->new_block(sizeof(CUstpFtdcQryUserInvestorField));

	strncpy(pBody->BrokerID, m_ServerInfo.BrokerID, sizeof(TUstpFtdcBrokerIDType));
	strncpy(pBody->UserID, m_UserInfo.UserID, sizeof(TUstpFtdcUserIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryUserInvestorField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CUstpFtdcQryUserInvestorField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryUserInvestor(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Doing, 0, nullptr, 0, nullptr, 0, nullptr, 0);
	return m_pApi->ReqQryUserInvestor((CUstpFtdcQryUserInvestorField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryUserInvestor(CUstpFtdcRspUserInvestorField *pRspUserInvestor, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

	if (!IsErrorRspInfo(pRspInfo)
		&& pRspUserInvestor)
	{
		memcpy(&m_RspUserInvestor, pRspUserInvestor, sizeof(CUstpFtdcRspUserInvestorField));

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Done, 0, nullptr, 0, nullptr, 0, nullptr, 0);

		if (m_ServerInfo.PrivateTopicResumeType > ResumeType::Restart
			&& (m_ServerInfo.PrivateTopicResumeType<ResumeType::Undefined))
		{
			ReqQryOrder();
			//ReqQryTrade();
			//ReqQryQuote();
		}
	}
	else
	{
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->ErrorMsg, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
	}
}

int CTraderApi::ReqOrderInsert(
	OrderField* pOrder,
	int count,
	OrderIDType* pInOut)
{
	int OrderRef = -1;
	if (nullptr == m_pApi)
		return 0;

	CUstpFtdcInputOrderField body = {0};

	strcpy(body.BrokerID, m_RspUserInvestor.BrokerID);
	strcpy(body.InvestorID, m_RspUserInvestor.InvestorID);
	strcpy(body.UserID, m_RspUserInvestor.UserID);

	strcpy(body.ExchangeID, "CFFEX");

	body.MinVolume = 1;
	body.ForceCloseReason = USTP_FTDC_FCR_NotForceClose;
	body.IsAutoSuspend = 0;
	//body.UserForceClose = 0;
	//body.IsSwapOrder = 0;

	//��Լ
	strncpy(body.InstrumentID, pOrder->InstrumentID, sizeof(TUstpFtdcInstrumentIDType));
	//����
	body.Direction = OrderSide_2_TUstpFtdcDirectionType(pOrder->Side);
	//��ƽ
	body.OffsetFlag = OpenCloseType_2_TUstpFtdcOffsetFlagType(pOrder->OpenClose);
	//Ͷ��
	body.HedgeFlag = HedgeFlagType_2_TUstpFtdcHedgeFlagType(pOrder->HedgeFlag);
	//����
	body.Volume = (int)pOrder->Qty;

	// ���������������õ�һ�������ļ۸񣬻��������������ļ۸���أ�
	body.LimitPrice = pOrder->Price;
	body.StopPrice = pOrder->StopPx;


	// �м����޼�
	switch (pOrder->Type)
	{
	case Market:
	case Stop:
	case MarketOnClose:
	case TrailingStop:
		body.OrderPriceType = USTP_FTDC_OPT_AnyPrice;
		body.TimeCondition = USTP_FTDC_TC_IOC;
		break;
	case Limit:
	case StopLimit:
	case TrailingStopLimit:
	default:
		body.OrderPriceType = USTP_FTDC_OPT_LimitPrice;
		body.TimeCondition = USTP_FTDC_TC_GFD;
		break;
	}

	// IOC��FOK
	switch (pOrder->TimeInForce)
	{
	case IOC:
		body.TimeCondition = USTP_FTDC_TC_IOC;
		body.VolumeCondition = USTP_FTDC_VC_AV;
		break;
	case FOK:
		body.TimeCondition = USTP_FTDC_TC_IOC;
		body.VolumeCondition = USTP_FTDC_VC_CV;
		//body.MinVolume = body.VolumeTotalOriginal; // ����ط��������
		break;
	default:
		body.VolumeCondition = USTP_FTDC_VC_AV;
		break;
	}

	long long nRet = 0;
	{
		//���ܱ���̫�죬m_nMaxOrderRef��û�иı���ύ��
		lock_guard<mutex> cl(m_csOrderRef);

		if (OrderRef < 0)
		{
			nRet = m_nMaxOrderRef;
			++m_nMaxOrderRef;
		}
		else
		{
			nRet = OrderRef;
		}
		sprintf(body.UserOrderLocalID, "%012lld", nRet);

		//�����浽���У�����ֱ�ӷ���
		int n = m_pApi->ReqOrderInsert(&body, ++m_lRequestID);
		if (n < 0)
		{
			nRet = n;
			sprintf(m_orderInsert_Id, "%d", nRet);
		}
		else
		{
			// ���ڸ���������ҵ�ԭ���������ڽ�����Ӧ��֪ͨ
			sprintf(m_orderInsert_Id, "%s:%012lld", m_RspUserLogin__.SessionID, nRet);

			OrderField* pField = (OrderField*)m_msgQueue->new_block(sizeof(OrderField));
			memcpy(pField, pOrder, sizeof(OrderField));
			strcpy(pField->ID, m_orderInsert_Id);
			m_id_platform_order.insert(pair<string, OrderField*>(m_orderInsert_Id, pField));
		}
		strncpy((char*)pInOut, m_orderInsert_Id, sizeof(OrderIDType));
	}

	return nRet;
}

void CTraderApi::OnRspOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	// ֻҪ�µ���͵�һ�����⣬���������̨�ܾ�
	OrderIDType orderId = { 0 };

	if (pInputOrder)
	{
		sprintf(orderId, "%s:%s", m_RspUserLogin__.SessionID, pInputOrder->UserOrderLocalID);
	}
	else
	{
		IsErrorRspInfo(pRspInfo, nRequestID, bIsLast);
	}

	unordered_map<string, OrderField*>::iterator it = m_id_platform_order.find(orderId);
	if (it == m_id_platform_order.end())
	{
		// û�ҵ�����Ӧ�������ʾ������
		//assert(false);
	}
	else
	{
		if (pRspInfo->ErrorID == 0)
		{
			OrderField* pField = it->second;
			pField->ExecType = ExecType::ExecNew;
			pField->Status = OrderStatus::New;
			pField->ErrorID = pRspInfo->ErrorID;
			strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
			strcat(pField->Text, "OnRspOrderInsert");
			m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
		}
		else
		{
			OrderField* pField = it->second;
			pField->ExecType = ExecType::ExecRejected;
			pField->Status = OrderStatus::Rejected;
			pField->ErrorID = pRspInfo->ErrorID;
			strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
			strcat(pField->Text, "OnRspOrderInsert");
			m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
		}
	}
}

void CTraderApi::OnErrRtnOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo)
{
	OrderIDType orderId = { 0 };

	if (pInputOrder)
	{
		sprintf(orderId, "%s:%s", m_RspUserLogin__.SessionID, pInputOrder->UserOrderLocalID);
	}
	else
	{
		IsErrorRspInfo(pRspInfo, 0, true);
	}

	unordered_map<string, OrderField*>::iterator it = m_id_platform_order.find(orderId);
	if (it == m_id_platform_order.end())
	{
		// û�ҵ�����Ӧ�������ʾ������
		//assert(false);
	}
	else
	{
		// �ҵ��ˣ�Ҫ����״̬
		// ��ʹ���ϴε�״̬
		OrderField* pField = it->second;
		pField->ExecType = ExecType::ExecRejected;
		pField->Status = OrderStatus::Rejected;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		strcat(pField->Text, "OnErrRtnOrderInsert");
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRtnTrade(CUstpFtdcTradeField *pTrade)
{
	OnTrade(pTrade);
}

int CTraderApi::ReqOrderAction(OrderIDType* szIds, int count, OrderIDType* pOutput)
{
	unordered_map<string, CUstpFtdcOrderField*>::iterator it = m_id_api_order.find(szIds[0]);
	if (it == m_id_api_order.end())
	{
		sprintf((char*)pOutput, "%d", -100);
		return -100;
	}
	else
	{
		// �ҵ��˶���
		return ReqOrderAction(it->second, count, pOutput);
	}
}

int CTraderApi::ReqOrderAction(CUstpFtdcOrderField *pOrder, int count, OrderIDType* pOutput)
{
	if (nullptr == m_pApi)
		return 0;

	CUstpFtdcOrderActionField body = {0};

	strcpy(body.BrokerID, pOrder->BrokerID);
	strcpy(body.InvestorID, pOrder->InvestorID);
	strcpy(body.UserID, pOrder->UserID);

	///��������
	strcpy(body.UserOrderLocalID, pOrder->UserOrderLocalID);

	///����������
	strcpy(body.ExchangeID,pOrder->ExchangeID);
	///�������
	strcpy(body.OrderSysID,pOrder->OrderSysID);
	///������־
	body.ActionFlag = USTP_FTDC_AF_Delete;

	int nRet = 0;
	{
		lock_guard<mutex> cl(m_csOrderRef);
		sprintf(body.UserOrderActionLocalID, "%012lld", m_nMaxOrderRef);
		++m_nMaxOrderRef;
		nRet = m_pApi->ReqOrderAction(&body, ++m_lRequestID);
		if (nRet < 0)
		{
			sprintf(m_orderAction_Id, "%d", nRet);
		}
		else
		{
			memset(m_orderAction_Id, 0, sizeof(OrderIDType));
		}
	}
	strncpy((char*)pOutput, m_orderAction_Id, sizeof(OrderIDType));

	return nRet;
}

void CTraderApi::OnRspOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	OrderIDType orderId = { 0 };

	if (pOrderAction)
	{
		sprintf(orderId, "%s:%s", m_RspUserLogin__.SessionID, pOrderAction->UserOrderLocalID);
	}
	else
	{
		IsErrorRspInfo(pRspInfo, nRequestID, bIsLast);
	}

	unordered_map<string, OrderField*>::iterator it = m_id_platform_order.find(orderId);
	if (it == m_id_platform_order.end())
	{
		// û�ҵ�����Ӧ�������ʾ������
		//assert(false);
	}
	else
	{
		if (pRspInfo->ErrorID == 0)
		{
			OrderField* pField = it->second;
			strcpy(pField->ID, orderId);
			pField->ExecType = ExecType::ExecCancelled;
			pField->Status = OrderStatus::Cancelled;
			pField->ErrorID = pRspInfo->ErrorID;
			strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
			strcat(pField->Text, "OnRspOrderAction");
			m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
		}
		else
		{
			// �ҵ��ˣ�Ҫ����״̬
			// ��ʹ���ϴε�״̬
			OrderField* pField = it->second;
			strcpy(pField->ID, orderId);
			pField->ExecType = ExecType::ExecCancelReject;
			pField->ErrorID = pRspInfo->ErrorID;
			strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
			strcat(pField->Text, "OnRspOrderAction");
			m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
		}
	}
}

void CTraderApi::OnErrRtnOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo)
{
	OrderIDType orderId = { 0 };

	if (pOrderAction)
	{
		sprintf(orderId, "%s:%s", m_RspUserLogin__.SessionID, pOrderAction->UserOrderLocalID);
	}
	else
	{
		IsErrorRspInfo(pRspInfo, 0, true);
	}

	unordered_map<string, OrderField*>::iterator it = m_id_platform_order.find(orderId);
	if (it == m_id_platform_order.end())
	{
		// û�ҵ�����Ӧ�������ʾ������
		//assert(false);
	}
	else
	{
		// �ҵ��ˣ�Ҫ����״̬
		// ��ʹ���ϴε�״̬
		OrderField* pField = it->second;
		strcpy(pField->ID, orderId);
		pField->ExecType = ExecType::ExecCancelReject;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		strcat(pField->Text, "OnErrRtnOrderAction");
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRtnOrder(CUstpFtdcOrderField *pOrder)
{
	OnOrder(pOrder);
}

//char* CTraderApi::ReqQuoteInsert(
//	int QuoteRef,
//	QuoteField* pQuote)
//{
//	if (nullptr == m_pApi)
//		return 0;
//
//	CUstpFtdcInputQuoteField body = {0};
//
//	strcpy(body.BrokerID, m_RspUserInvestor.BrokerID);
//	strcpy(body.InvestorID, m_RspUserInvestor.InvestorID);
//	strcpy(body.UserID, m_RspUserInvestor.UserID);
//
//	//��Լ,Ŀǰֻ�Ӷ���1��ȡ
//	strncpy(body.InstrumentID, pQuote->InstrumentID, sizeof(TUstpFtdcInstrumentIDType));
//	//��ƽ
//	body.AskOffsetFlag = OpenCloseType_2_TUstpFtdcOffsetFlagType(pQuote->AskOpenClose);
//	body.BidOffsetFlag = OpenCloseType_2_TUstpFtdcOffsetFlagType(pQuote->BidOpenClose);
//	//Ͷ��
//	body.AskHedgeFlag = HedgeFlagType_2_TUstpFtdcHedgeFlagType(pQuote->AskHedgeFlag);
//	body.BidHedgeFlag = HedgeFlagType_2_TUstpFtdcHedgeFlagType(pQuote->BidHedgeFlag);
//
//	//�۸�
//	body.AskPrice = pQuote->AskPrice;
//	body.BidPrice = pQuote->BidPrice;
//
//	//����
//	body.AskVolume = (int)pQuote->AskQty;
//	body.BidVolume = (int)pQuote->BidQty;
//
//	long long nRet = 0;
//	{
//		//���ܱ���̫�죬m_nMaxOrderRef��û�иı���ύ��
//		lock_guard<mutex> cl(m_csOrderRef);
//
//		if (QuoteRef < 0)
//		{
//			nRet = m_nMaxOrderRef;
//			sprintf(body.UserQuoteLocalID, "%012lld", m_nMaxOrderRef);
//			sprintf(body.AskUserOrderLocalID, "%012lld", m_nMaxOrderRef);
//			sprintf(body.BidUserOrderLocalID, "%012lld", ++m_nMaxOrderRef);
//			++m_nMaxOrderRef;
//		}
//		else
//		{
//			nRet = QuoteRef;
//			sprintf(body.UserQuoteLocalID, "%012lld", QuoteRef);
//			sprintf(body.AskUserOrderLocalID, "%012lld", QuoteRef);
//			sprintf(body.BidUserOrderLocalID, "%012lld", ++QuoteRef);
//			++QuoteRef;
//		}
//
//		//�����浽���У�����ֱ�ӷ���
//		int n = m_pApi->ReqQuoteInsert(&body, ++m_lRequestID);
//		if (n < 0)
//		{
//			nRet = n;
//			return nullptr;
//		}
//		else
//		{
//			// ���ڸ���������ҵ�ԭ���������ڽ�����Ӧ��֪ͨ
//			sprintf(m_orderInsert_Id, "%s:%012lld", m_RspUserLogin__.SessionID, nRet);
//
//			QuoteField* pField = (QuoteField*)m_msgQueue->new_block(sizeof(QuoteField));
//			memcpy(pField, pQuote, sizeof(QuoteField));
//			strcpy(pField->ID, m_orderInsert_Id);
//			strcpy(pField->AskID, m_orderInsert_Id);
//			sprintf(pField->BidID, "%s:%012lld", m_RspUserLogin__.SessionID, nRet + 1);
//
//			m_id_platform_quote.insert(pair<string, QuoteField*>(m_orderInsert_Id, pField));
//		}
//	}
//
//	return m_orderInsert_Id;
//}
//
//void CTraderApi::OnRspQuoteInsert(CUstpFtdcInputQuoteField *pInputQuote, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//	OrderIDType quoteId = { 0 };
//	if (pInputQuote)
//	{
//		sprintf(quoteId, "%s:%s", m_RspUserLogin__.SessionID, pInputQuote->UserQuoteLocalID);
//	}
//	else
//	{
//		IsErrorRspInfo(pRspInfo, nRequestID, bIsLast);
//	}
//
//	unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
//	if (it == m_id_platform_quote.end())
//	{
//		// û�ҵ�����Ӧ�������ʾ������
//		//assert(false);
//	}
//	else
//	{
//		if (pRspInfo->ErrorID == 0)
//		{
//			QuoteField* pField = it->second;
//			//strcpy(pField->ID, quoteId);
//			pField->ExecType = ExecType::ExecNew;
//			pField->Status = OrderStatus::New;
//			pField->ErrorID = pRspInfo->ErrorID;
//			strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
//			strcat(pField->Text, "OnRspQuoteInsert");
//			m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
//		}
//		else
//		{
//			QuoteField* pField = it->second;
//			//strcpy(pField->ID, quoteId);
//			pField->ExecType = ExecType::ExecRejected;
//			pField->Status = OrderStatus::Rejected;
//			pField->ErrorID = pRspInfo->ErrorID;
//			strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
//			strcat(pField->Text, "OnRspQuoteInsert");
//			m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
//		}
//	}
//}
//
//void CTraderApi::OnErrRtnQuoteInsert(CUstpFtdcInputQuoteField *pInputQuote, CUstpFtdcRspInfoField *pRspInfo)
//{
//	OrderIDType quoteId = { 0 };
//	if (pInputQuote)
//	{
//		sprintf(quoteId, "%s:%s", m_RspUserLogin__.SessionID, pInputQuote->UserQuoteLocalID);
//	}
//	else
//	{
//		IsErrorRspInfo(pRspInfo, 0, true);
//	}
//
//	unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
//	if (it == m_id_platform_quote.end())
//	{
//		// û�ҵ�����Ӧ�������ʾ������
//		//assert(false);
//	}
//	else
//	{
//		QuoteField* pField = it->second;
//		//strcpy(pField->ID, quoteId);
//		pField->ExecType = ExecType::ExecRejected;
//		pField->Status = OrderStatus::Rejected;
//		pField->ErrorID = pRspInfo->ErrorID;
//		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
//		strcat(pField->Text, "OnErrRtnQuoteInsert");
//		m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
//	}
//}
//
//void CTraderApi::OnRtnQuote(CUstpFtdcRtnQuoteField *pQuote)
//{
//	OnQuote(pQuote);
//}
//
//int CTraderApi::ReqQuoteAction(const string& szId)
//{
//	unordered_map<string, CUstpFtdcRtnQuoteField*>::iterator it = m_id_api_quote.find(szId);
//	if (it == m_id_api_quote.end())
//	{
//		//// <error id="QUOTE_NOT_FOUND" value="86" prompt="CTP:���۳����Ҳ�����Ӧ����"/>
//		return -100;
//	}
//	else
//	{
//		// �ҵ��˶���
//		ReqQuoteAction(it->second);
//	}
//	return 0;
//}
//
//int CTraderApi::ReqQuoteAction(CUstpFtdcRtnQuoteField *pQuote)
//{
//	if (nullptr == m_pApi)
//		return 0;
//
//	CUstpFtdcQuoteActionField body = {0};
//
//	strcpy(body.BrokerID, pQuote->BrokerID);
//	strcpy(body.InvestorID, pQuote->InvestorID);
//	strcpy(body.UserID, pQuote->UserID);
//
//	///��������
//	strcpy(body.UserQuoteLocalID, pQuote->UserQuoteLocalID);
//
//	///����������
//	strcpy(body.ExchangeID, pQuote->ExchangeID);
//	///�������
//	strcpy(body.QuoteSysID, pQuote->QuoteSysID);
//	///������־
//	body.ActionFlag = USTP_FTDC_AF_Delete;
//	///��Լ����
//	int nRet = 0;
//	{
//		lock_guard<mutex> cl(m_csOrderRef);
//		sprintf(body.UserQuoteActionLocalID, "%012lld", m_nMaxOrderRef);
//		++m_nMaxOrderRef;
//		nRet = m_pApi->ReqQuoteAction(&body, ++m_lRequestID);
//	}
//
//	return nRet;
//}
//
//void CTraderApi::OnRspQuoteAction(CUstpFtdcQuoteActionField *pQuoteAction, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//	OrderIDType quoteId = { 0 };
//	if (pQuoteAction)
//	{
//		sprintf(quoteId, "%s:%s", m_RspUserLogin__.SessionID, pQuoteAction->UserQuoteLocalID);
//	}
//	else
//	{
//		IsErrorRspInfo(pRspInfo, nRequestID, bIsLast);
//	}
//
//	unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
//	if (it == m_id_platform_quote.end())
//	{
//		// û�ҵ�����Ӧ�������ʾ������
//		//assert(false);
//	}
//	else
//	{
//		if (pRspInfo->ErrorID == 0)
//		{
//			QuoteField* pField = it->second;
//			strcpy(pField->ID, quoteId);
//			pField->ExecType = ExecType::ExecCancelled;
//			pField->Status = OrderStatus::Cancelled;
//			pField->ErrorID = pRspInfo->ErrorID;
//			strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
//			strcat(pField->Text, "OnRspQuoteAction");
//			m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
//		}
//		else
//		{
//			// �ҵ��ˣ�Ҫ����״̬
//			// ��ʹ���ϴε�״̬
//			QuoteField* pField = it->second;
//			strcpy(pField->ID, quoteId);
//			pField->ExecType = ExecType::ExecCancelReject;
//			pField->ErrorID = pRspInfo->ErrorID;
//			strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
//			strcat(pField->Text, "OnRspQuoteAction");
//			m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
//		}
//	}
//}
//
//void CTraderApi::OnErrRtnQuoteAction(CUstpFtdcQuoteActionField *pQuoteAction, CUstpFtdcRspInfoField *pRspInfo)
//{
//	OrderIDType quoteId = { 0 };
//	if (pQuoteAction)
//	{
//		sprintf(quoteId, "%s:%s", m_RspUserLogin__.SessionID, pQuoteAction->UserQuoteLocalID);
//	}
//	else
//	{
//		IsErrorRspInfo(pRspInfo, 0, true);
//	}
//
//	unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
//	if (it == m_id_platform_quote.end())
//	{
//		// û�ҵ�����Ӧ�������ʾ������
//		//assert(false);
//	}
//	else
//	{
//		// �ҵ��ˣ�Ҫ����״̬
//		// ��ʹ���ϴε�״̬
//		QuoteField* pField = it->second;
//		strcpy(pField->ID, quoteId);
//		pField->ExecType = ExecType::ExecCancelReject;
//		pField->ErrorID = pRspInfo->ErrorID;
//		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
//		strcat(pField->Text, "OnErrRtnQuoteAction");
//		m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
//	}
//}

void CTraderApi::ReqQryInvestorAccount()
{
	CUstpFtdcQryInvestorAccountField* pBody = (CUstpFtdcQryInvestorAccountField*)m_msgQueue_Query->new_block(sizeof(CUstpFtdcQryInvestorAccountField));

	strcpy(pBody->BrokerID, m_RspUserInvestor.BrokerID);
	strcpy(pBody->UserID, m_RspUserInvestor.UserID);
	strcpy(pBody->InvestorID, m_RspUserInvestor.InvestorID);

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryInvestorAccountField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CUstpFtdcQryInvestorAccountField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryInvestorAccount(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryInvestorAccount((CUstpFtdcQryInvestorAccountField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryInvestorAccount(CUstpFtdcRspInvestorAccountField *pRspInvestorAccount, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pRspInvestorAccount)
		{
			AccountField* pField = (AccountField*)m_msgQueue->new_block(sizeof(AccountField));

			pField->PreBalance = pRspInvestorAccount->PreBalance;
			pField->CurrMargin = pRspInvestorAccount->Margin;
			pField->Commission = pRspInvestorAccount->Fee;
			pField->CloseProfit = pRspInvestorAccount->CloseProfit;
			pField->PositionProfit = pRspInvestorAccount->PositionProfit;
			//pField->Balance = pRspInvestorAccount->DynamicRights;
			pField->Available = pRspInvestorAccount->Available;

			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryTradingAccount, m_msgQueue, m_pClass, bIsLast, 0, pField, sizeof(AccountField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryTradingAccount, m_msgQueue, m_pClass, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}

void CTraderApi::ReqQryInvestorPosition(const string& szInstrumentId)
{
	CUstpFtdcQryInvestorPositionField* pBody = (CUstpFtdcQryInvestorPositionField*)m_msgQueue_Query->new_block(sizeof(CUstpFtdcQryInvestorPositionField));

	strcpy(pBody->BrokerID, m_RspUserInvestor.BrokerID);
	strcpy(pBody->UserID, m_RspUserInvestor.UserID);
	strcpy(pBody->InvestorID, m_RspUserInvestor.InvestorID);

	strncpy(pBody->InstrumentID,szInstrumentId.c_str(),sizeof(TUstpFtdcInstrumentIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryInvestorPositionField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CUstpFtdcQryInvestorPositionField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryInvestorPosition(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryInvestorPosition((CUstpFtdcQryInvestorPositionField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryInvestorPosition(CUstpFtdcRspInvestorPositionField *pRspInvestorPosition, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pRspInvestorPosition)
		{
			PositionField* pField = (PositionField*)m_msgQueue->new_block(sizeof(PositionField));

			strcpy(pField->Symbol, pRspInvestorPosition->InstrumentID);
			strcpy(pField->InstrumentID, pRspInvestorPosition->InstrumentID);
			strcpy(pField->ExchangeID, pRspInvestorPosition->ExchangeID);

			pField->Side = TUstpFtdcDirectionType_2_PositionSide(pRspInvestorPosition->Direction);
			pField->HedgeFlag = TUstpFtdcHedgeFlagType_2_HedgeFlagType(pRspInvestorPosition->HedgeFlag);
			pField->Position = pRspInvestorPosition->Position;
			// ����������ʲô��ϵ����μ���ģ�
			pField->TdPosition = pRspInvestorPosition->Position - pRspInvestorPosition->YdPosition;
			pField->YdPosition = pRspInvestorPosition->YdPosition;

			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInvestorPosition, m_msgQueue, m_pClass, bIsLast, 0, pField, sizeof(PositionField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInvestorPosition, m_msgQueue, m_pClass, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}

void CTraderApi::ReqQryInstrument(const string& szInstrumentId, const string& szExchange)
{
	CUstpFtdcQryInstrumentField* pBody = (CUstpFtdcQryInstrumentField*)m_msgQueue_Query->new_block(sizeof(CUstpFtdcQryInstrumentField));

	strncpy(pBody->ExchangeID, szExchange.c_str(), sizeof(TUstpFtdcExchangeIDType));
	strncpy(pBody->InstrumentID,szInstrumentId.c_str(),sizeof(TUstpFtdcInstrumentIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryInstrumentField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CUstpFtdcQryInstrumentField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryInstrument(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryInstrument((CUstpFtdcQryInstrumentField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryInstrument(CUstpFtdcRspInstrumentField *pRspInstrument, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pRspInstrument)
		{
			InstrumentField* pField = (InstrumentField*)m_msgQueue->new_block(sizeof(InstrumentField));

			strcpy(pField->InstrumentID, pRspInstrument->InstrumentID);
			strcpy(pField->ExchangeID, pRspInstrument->ExchangeID);

			strcpy(pField->Symbol, pRspInstrument->InstrumentID);
			strncpy(pField->ProductID, pRspInstrument->ProductID, sizeof(InstrumentIDType));

			strcpy(pField->InstrumentName, pRspInstrument->InstrumentName);
			pField->Type = CUstpFtdcRspInstrumentField_2_InstrumentType(pRspInstrument);
			pField->VolumeMultiple = pRspInstrument->VolumeMultiple;
			pField->PriceTick = pRspInstrument->PriceTick;
			pField->ExpireDate = GetDate(pRspInstrument->ExpireDate);
			pField->OptionsType = TUstpFtdcOptionsTypeType_2_PutCall(pRspInstrument->OptionsType);
			pField->StrikePrice = pRspInstrument->StrikePrice == DBL_MAX ? 0 : pRspInstrument->StrikePrice;

			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInstrument, m_msgQueue, m_pClass, bIsLast, 0, pField, sizeof(InstrumentField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInstrument, m_msgQueue, m_pClass, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}

void CTraderApi::ReqQryInvestorFee(const string& szInstrumentId)
{
	CUstpFtdcQryInvestorFeeField* pBody = (CUstpFtdcQryInvestorFeeField*)m_msgQueue_Query->new_block(sizeof(CUstpFtdcQryInvestorFeeField));

	strcpy(pBody->BrokerID, m_RspUserInvestor.BrokerID);
	strcpy(pBody->UserID, m_RspUserInvestor.UserID);
	strcpy(pBody->InvestorID, m_RspUserInvestor.InvestorID);

	strncpy(pBody->InstrumentID,szInstrumentId.c_str(),sizeof(TUstpFtdcInstrumentIDType));
}

void CTraderApi::OnRspQryInvestorFee(CUstpFtdcInvestorFeeField *pInvestorFee, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{

}
//
//void CTraderApi::ReqQryInstrumentMarginRate(const string& szInstrumentId,TUstpFtdcHedgeFlagType HedgeFlag)
//{
//	if (nullptr == m_pApi)
//		return;
//
//	SRequest* pRequest = MakeRequestBuf(E_QryInstrumentMarginRateField);
//	if (nullptr == pRequest)
//		return;
//
//	CUstpFtdcQryInstrumentMarginRateField& body = pRequest->QryInstrumentMarginRateField;
//
//	strncpy(body.BrokerID, m_RspUserLogin.BrokerID,sizeof(TUstpFtdcBrokerIDType));
//	strncpy(body.InvestorID, m_RspUserLogin.UserID,sizeof(TUstpFtdcInvestorIDType));
//	strncpy(body.InstrumentID,szInstrumentId.c_str(),sizeof(TUstpFtdcInstrumentIDType));
//	body.HedgeFlag = HedgeFlag;
//
//	AddToSendQueue(pRequest);
//}
//
//void CTraderApi::OnRspQryInstrumentMarginRate(CUstpFtdcInstrumentMarginRateField *pInstrumentMarginRate, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//	//if(m_msgQueue)
//	//	m_msgQueue->Input_OnRspQryInstrumentMarginRate(this,pInstrumentMarginRate,pRspInfo,nRequestID,bIsLast);
//
//	if (bIsLast)
//		ReleaseRequestMapBuf(nRequestID);
//}

void CTraderApi::OnRspError(CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	IsErrorRspInfo(pRspInfo, nRequestID, bIsLast);
}

void CTraderApi::ReqQryOrder()
{
	CUstpFtdcQryOrderField* pBody = (CUstpFtdcQryOrderField*)m_msgQueue_Query->new_block(sizeof(CUstpFtdcQryOrderField));

	strcpy(pBody->BrokerID, m_RspUserInvestor.BrokerID);
	strcpy(pBody->UserID, m_RspUserInvestor.UserID);

	strcpy(pBody->InvestorID, m_RspUserInvestor.InvestorID);

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryOrderField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CUstpFtdcQryOrderField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryOrder(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryOrder((CUstpFtdcQryOrderField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnOrder(CUstpFtdcOrderField *pOrder)
{
	if (nullptr == pOrder)
		return;

	OrderIDType orderId = { 0 };
	sprintf(orderId, "%s:%s", m_RspUserLogin__.SessionID, pOrder->UserOrderLocalID);
	//OrderIDType orderSydId = { 0 };

	{
		// ����ԭʼ������Ϣ�����ڳ���
		unordered_map<string, CUstpFtdcOrderField*>::iterator it = m_id_api_order.find(orderId);
		if (it == m_id_api_order.end())
		{
			// �Ҳ����˶�������ʾ���µ�
			CUstpFtdcOrderField* pField = new CUstpFtdcOrderField();
			memcpy(pField, pOrder, sizeof(CUstpFtdcOrderField));
			m_id_api_order.insert(pair<string, CUstpFtdcOrderField*>(orderId, pField));
		}
		else
		{
			// �ҵ��˶���
			// ��Ҫ�ٸ��Ʊ������һ�ε�״̬������ֻҪ��һ�ε����ڳ������ɣ����£��������ñȽ�
			CUstpFtdcOrderField* pField = it->second;
			memcpy(pField, pOrder, sizeof(CUstpFtdcOrderField));
		}

		//// ����SysID���ڶ���ɽ��ر��붩��
		//sprintf(orderSydId, "%s:%s", pOrder->ExchangeID, pOrder->OrderSysID);
		//m_sysId_orderId.insert(pair<string, string>(orderSydId, orderId));
	}

	{
		// ��API�Ķ���ת�����Լ��Ľṹ��

		OrderField* pField = nullptr;
		unordered_map<string, OrderField*>::iterator it = m_id_platform_order.find(orderId);
		if (it == m_id_platform_order.end())
		{
			// ����ʱ������Ϣ��û�У������Ҳ�����Ӧ�ĵ��ӣ���Ҫ����Order�Ļָ�
			pField = (OrderField*)m_msgQueue->new_block(sizeof(OrderField));

			strcpy(pField->ID, orderId);
			strcpy(pField->InstrumentID, pOrder->InstrumentID);
			strcpy(pField->ExchangeID, pOrder->ExchangeID);
			pField->HedgeFlag = TUstpFtdcHedgeFlagType_2_HedgeFlagType(pOrder->HedgeFlag);
			pField->Side = TUstpFtdcDirectionType_2_OrderSide(pOrder->Direction);
			pField->Price = pOrder->LimitPrice;
			pField->StopPx = pOrder->StopPrice;
			//strcpy(pField->Text, pOrder->StatusMsg);
			pField->OpenClose = TUstpFtdcOffsetFlagType_2_OpenCloseType(pOrder->OffsetFlag);
			pField->Status = CUstpFtdcOrderField_2_OrderStatus(pOrder);
			pField->Qty = pOrder->Volume;
			pField->Type = CUstpFtdcOrderField_2_OrderType(pOrder);
			pField->TimeInForce = CUstpFtdcOrderField_2_TimeInForce(pOrder);
			pField->ExecType = ExecType::ExecNew;
			strcpy(pField->OrderID, pOrder->OrderSysID);
			strcat(pField->Text, "OnRtnOrder");


			// ��ӵ�map�У������������ߵĶ�ȡ������ʧ��ʱ����֪ͨ��
			m_id_platform_order.insert(pair<string, OrderField*>(orderId, pField));
		}
		else
		{
			pField = it->second;
			strcpy(pField->ID, orderId);
			pField->LeavesQty = pOrder->VolumeRemain;
			pField->Price = pOrder->LimitPrice;
			pField->Status = CUstpFtdcOrderField_2_OrderStatus(pOrder);
			pField->ExecType = CUstpFtdcOrderField_2_ExecType(pOrder);
			strcpy(pField->OrderID, pOrder->OrderSysID);
			strcat(pField->Text, "OnRtnOrder");
		}

		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRspQryOrder(CUstpFtdcOrderField *pOrder, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		OnOrder(pOrder);
	}
}

void CTraderApi::ReqQryTrade()
{
	CUstpFtdcQryTradeField* pBody = (CUstpFtdcQryTradeField*)m_msgQueue_Query->new_block(sizeof(CUstpFtdcQryTradeField));

	strcpy(pBody->BrokerID, m_RspUserInvestor.BrokerID);
	strcpy(pBody->UserID, m_RspUserInvestor.UserID);
	strcpy(pBody->InvestorID, m_RspUserInvestor.InvestorID);

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryTradeField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CUstpFtdcQryTradeField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryTrade(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryTrade((CUstpFtdcQryTradeField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnTrade(CUstpFtdcTradeField *pTrade)
{
	if (nullptr == pTrade)
		return;

	TradeField* pField = (TradeField*)m_msgQueue->new_block(sizeof(TradeField));

	strcpy(pField->InstrumentID, pTrade->InstrumentID);
	strcpy(pField->ExchangeID, pTrade->ExchangeID);
	pField->Side = TUstpFtdcDirectionType_2_OrderSide(pTrade->Direction);
	pField->Qty = pTrade->TradeVolume;
	pField->Price = pTrade->TradePrice;
	pField->OpenClose = TUstpFtdcOffsetFlagType_2_OpenCloseType(pTrade->OffsetFlag);
	pField->HedgeFlag = TUstpFtdcHedgeFlagType_2_HedgeFlagType(pTrade->HedgeFlag);
	pField->Commission = 0;//TODO�������Ժ�Ҫ�������
	pField->Time = GetTime(pTrade->TradeTime);
	strcpy(pField->TradeID, pTrade->TradeID);

	//OrderIDType orderSysId = { 0 };
	//sprintf(orderSysId, "%s:%s", pTrade->ExchangeID, pTrade->OrderSysID);
	//hash_map<string, string>::iterator it = m_sysId_orderId.find(orderSysId);
	//if (it == m_sysId_orderId.end())
	//{
	//	// �˳ɽ��Ҳ�����Ӧ�ı���
	//	assert(false);
	//}
	//else
	//{
		// �ҵ���Ӧ�ı���
		//strcpy(pField->ID, it->second.c_str());
		//strcpy(pField->ID, pTrade->UserOrderLocalID);
		sprintf(pField->ID, "%s:%s", m_RspUserLogin__.SessionID, pTrade->UserOrderLocalID);


		m_msgQueue->Input_Copy(ResponeType::OnRtnTrade, m_msgQueue, m_pClass, 0, 0, pField, sizeof(TradeField), nullptr, 0, nullptr, 0);

		unordered_map<string, OrderField*>::iterator it2 = m_id_platform_order.find(pField->ID);
		if (it2 == m_id_platform_order.end())
		{
			// �˳ɽ��Ҳ�����Ӧ�ı���
			//assert(false);
		}
		else
		{
			// ���¶�����״̬
			// �Ƿ�Ҫ֪ͨ�ӿ�
		}
	//}
}

void CTraderApi::OnRspQryTrade(CUstpFtdcTradeField *pTrade, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		OnTrade(pTrade);
	}
}

//void CTraderApi::ReqQryQuote()
//{
//	CThostFtdcQryQuoteField body = { 0 };
//
//	strncpy(body.BrokerID, m_RspUserLogin.BrokerID, sizeof(TThostFtdcBrokerIDType));
//	strncpy(body.InvestorID, m_RspUserLogin.UserID, sizeof(TThostFtdcInvestorIDType));
//
//	m_msgQueue_Query->Input(RequestType::E_QryQuoteField, m_msgQueue_Query, this, 0, 0,
//		&body, sizeof(CThostFtdcQryQuoteField), nullptr, 0, nullptr, 0);
//}
//
//int CTraderApi::_ReqQryQuote(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
//{
//	return m_pApi->ReqQryQuote((CThostFtdcQryQuoteField*)ptr1, ++m_lRequestID);
//}

//void CTraderApi::OnQuote(CUstpFtdcRtnQuoteField *pQuote)
//{
//	if (nullptr == pQuote)
//		return;
//
//	OrderIDType quoteId = { 0 };
//	sprintf(quoteId, "%s:%s", m_RspUserLogin__.SessionID, pQuote->UserQuoteLocalID);
//	OrderIDType orderSydId = { 0 };
//
//	{
//		// ����ԭʼ������Ϣ�����ڳ���
//
//		unordered_map<string, CUstpFtdcRtnQuoteField*>::iterator it = m_id_api_quote.find(quoteId);
//		if (it == m_id_api_quote.end())
//		{
//			// �Ҳ����˶�������ʾ���µ�
//			CUstpFtdcRtnQuoteField* pField = new CUstpFtdcRtnQuoteField();
//			memcpy(pField, pQuote, sizeof(CUstpFtdcRtnQuoteField));
//			m_id_api_quote.insert(pair<string, CUstpFtdcRtnQuoteField*>(quoteId, pField));
//		}
//		else
//		{
//			// �ҵ��˶���
//			// ��Ҫ�ٸ��Ʊ������һ�ε�״̬������ֻҪ��һ�ε����ڳ������ɣ����£��������ñȽ�
//			CUstpFtdcRtnQuoteField* pField = it->second;
//			memcpy(pField, pQuote, sizeof(CUstpFtdcRtnQuoteField));
//		}
//
//		// ����ط��Ƿ�Ҫ������������
//
//		// ����SysID���ڶ���ɽ��ر��붩��
//		//sprintf(orderSydId, "%s:%s", pQuote->ExchangeID, pQuote->QuoteSysID);
//		//m_sysId_quoteId.insert(pair<string, string>(orderSydId, quoteId));
//	}
//
//	{
//		// ��API�Ķ���ת�����Լ��Ľṹ��
//
//		QuoteField* pField = nullptr;
//		unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
//		if (it == m_id_platform_quote.end())
//		{
//			// ����ʱ������Ϣ��û�У������Ҳ�����Ӧ�ĵ��ӣ���Ҫ����Order�Ļָ�
//			pField = (QuoteField*)m_msgQueue->new_block(sizeof(QuoteField));
//
//			strcpy(pField->InstrumentID, pQuote->InstrumentID);
//			strcpy(pField->ExchangeID, pQuote->ExchangeID);
//
//			pField->AskQty = pQuote->AskVolume;
//			pField->AskPrice = pQuote->AskPrice;
//			pField->AskOpenClose = TUstpFtdcOffsetFlagType_2_OpenCloseType(pQuote->AskOffsetFlag);
//			pField->AskHedgeFlag = TUstpFtdcHedgeFlagType_2_HedgeFlagType(pQuote->AskHedgeFlag);
//
//			pField->BidQty = pQuote->BidVolume;
//			pField->BidPrice = pQuote->BidPrice;
//			pField->BidOpenClose = TUstpFtdcOffsetFlagType_2_OpenCloseType(pQuote->BidOffsetFlag);
//			pField->BidHedgeFlag = TUstpFtdcHedgeFlagType_2_HedgeFlagType(pQuote->BidHedgeFlag);
//
//			strcpy(pField->ID, quoteId);
//			strcpy(pField->AskOrderID, pQuote->AskOrderSysID);
//			strcpy(pField->BidOrderID, pQuote->BidOrderSysID);
//
//			//strncpy(pField->Text, pQuote->, sizeof(ErrorMsgType));
//
//			//pField->ExecType = ExecType::ExecNew;
//			pField->Status = CUstpFtdcRtnQuoteField_2_OrderStatus(pQuote);
//			pField->ExecType = ExecType::ExecNew;
//
//
//			// ��ӵ�map�У������������ߵĶ�ȡ������ʧ��ʱ����֪ͨ��
//			m_id_platform_quote.insert(pair<string, QuoteField*>(quoteId, pField));
//		}
//		else
//		{
//			pField = it->second;
//
//			strcpy(pField->ID, quoteId);
//			strcpy(pField->AskOrderID, pQuote->AskOrderSysID);
//			strcpy(pField->BidOrderID, pQuote->BidOrderSysID);
//
//			pField->Status = CUstpFtdcRtnQuoteField_2_OrderStatus(pQuote);
//			pField->ExecType = CUstpFtdcRtnQuoteField_2_ExecType(pQuote);
//		}
//
//		m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
//	}
//}

//void CTraderApi::OnRspQryQuote(CThostFtdcQuoteField *pQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
//	{
//		OnQuote(pQuote);
//	}
//
//	if (bIsLast)
//		ReleaseRequestMapBuf(nRequestID);
//}

void CTraderApi::OnRtnInstrumentStatus(CUstpFtdcInstrumentStatusField *pInstrumentStatus)
{
	//if(m_msgQueue)
	//	m_msgQueue->Input_OnRtnInstrumentStatus(this,pInstrumentStatus);
}

//void CTraderApi::OnRtnForQuote(CUstpFtdcReqForQuoteField *pReqForQuote)
//{
//	QuoteRequestField* pField = (QuoteRequestField*)m_msgQueue->new_block(sizeof(QuoteRequestField));
//
//	pField->TradingDay = GetDate(pReqForQuote->TradingDay);
//	pField->QuoteTime = GetDate(pReqForQuote->ReqForQuoteTime);
//	strcpy(pField->Symbol, pReqForQuote->InstrumentID);
//	strcpy(pField->InstrumentID, pReqForQuote->InstrumentID);
//	strcpy(pField->ExchangeID, pReqForQuote->ExchangeID);
//	sprintf(pField->Symbol, "%s.%s", pField->InstrumentID, pField->ExchangeID);
//	strcpy(pField->QuoteID, pReqForQuote->ReqForQuoteID);
//
//	m_msgQueue->Input_NoCopy(ResponeType::OnRtnQuoteRequest, m_msgQueue, m_pClass, 0, 0,
//		pField, sizeof(QuoteRequestField), nullptr, 0, nullptr, 0);
//}
