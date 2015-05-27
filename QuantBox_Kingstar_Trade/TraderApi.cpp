#include "stdafx.h"
#include "TraderApi.h"

#include "../include/QueueEnum.h"
#include "../include/QueueHeader.h"

#include "../include/ApiHeader.h"
#include "../include/ApiStruct.h"

#include "../include/toolkit.h"

#include "../QuantBox_Queue/MsgQueue.h"

#include "TypeConvert.h"

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
	case E_ReqAuthenticateField:
		iRet = _ReqAuthenticate(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_ReqUserLoginField:
		iRet = _ReqUserLogin(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_SettlementInfoConfirmField:
		iRet = _ReqSettlementInfoConfirm(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryTradingAccountField:
		iRet = _ReqQryTradingAccount(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryInvestorPositionField:
		iRet = _ReqQryInvestorPosition(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryInstrumentField:
		iRet = _ReqQryInstrument(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryInvestorField:
		iRet = _ReqQryInvestor(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryOrderField:
		iRet = _ReqQryOrder(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryTradeField:
		iRet = _ReqQryTrade(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
		break;
	case E_QryQuoteField:
		iRet = _ReqQryQuote(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
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

void CTraderApi::Register(void* pCallback, void* pClass)
{
	m_pClass = pClass;
	if (m_msgQueue == nullptr)
		return;

	m_msgQueue_Query->Register((void*)Query, this);
	m_msgQueue->Register(pCallback, this);
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

CTraderApi::CTraderApi(void)
{
	m_pApi = nullptr;
	m_lRequestID = 0;
	m_nSleep = 1;

	// �Լ�ά��������Ϣ����
	m_msgQueue = new CMsgQueue();
	m_msgQueue_Query = new CMsgQueue();

	m_msgQueue_Query->Register((void*)Query, this);
	m_msgQueue_Query->StartThread();
}


CTraderApi::~CTraderApi(void)
{
	Disconnect();
}

bool CTraderApi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
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

bool CTraderApi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	bool bRet = ((pRspInfo) && (pRspInfo->ErrorID != 0));

	return bRet;
}

void CTraderApi::Connect(const string& szPath,
	ServerInfoField* pServerInfo,
	UserInfoField* pUserInfo,
	int count)
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

	m_pApi = CThostFtdcTraderApi::CreateFtdcTraderApi(pszPath);
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
			m_pApi->SubscribePublicTopic((THOST_TE_RESUME_TYPE)m_ServerInfo.PublicTopicResumeType);
		if (m_ServerInfo.PrivateTopicResumeType<ResumeType::Undefined)
			m_pApi->SubscribePrivateTopic((THOST_TE_RESUME_TYPE)m_ServerInfo.PrivateTopicResumeType);

		//��ʼ������
		m_pApi->Init();
		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Connecting, 0, nullptr, 0, nullptr, 0, nullptr, 0);
	}

	return 0;
}

void CTraderApi::OnFrontConnected()
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Connected, 0, nullptr, 0, nullptr, 0, nullptr, 0);

	//���ӳɹ����Զ�������֤���¼
	if (strlen(m_ServerInfo.AuthCode)>0
		&& strlen(m_ServerInfo.UserProductInfo)>0)
	{
		//������֤�������֤
		ReqAuthenticate();
	}
	else
	{
		ReqUserLogin();
	}
}

void CTraderApi::OnFrontDisconnected(int nReason)
{
	RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

	//����ʧ�ܷ��ص���Ϣ��ƴ�Ӷ��ɣ���Ҫ��Ϊ��ͳһ���
	pField->ErrorID = nReason;
	GetOnFrontDisconnectedMsg(nReason, pField->ErrorMsg);

	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
}

void CTraderApi::ReqAuthenticate()
{
	CThostFtdcReqAuthenticateField* pBody = (CThostFtdcReqAuthenticateField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcReqAuthenticateField));

	strncpy(pBody->BrokerID, m_ServerInfo.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(pBody->UserID, m_UserInfo.UserID, sizeof(TThostFtdcInvestorIDType));
	strncpy(pBody->UserProductInfo, m_ServerInfo.UserProductInfo, sizeof(TThostFtdcProductInfoType));
	strncpy(pBody->AuthCode, m_ServerInfo.AuthCode, sizeof(TThostFtdcAuthCodeType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_ReqAuthenticateField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcReqAuthenticateField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqAuthenticate(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Authorizing, 0, nullptr, 0, nullptr, 0, nullptr, 0);
	return m_pApi->ReqAuthenticate((CThostFtdcReqAuthenticateField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo)
		&& pRspAuthenticateField)
	{
		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Authorized, 0, nullptr, 0, nullptr, 0, nullptr, 0);

		ReqUserLogin();
	}
	else
	{
		RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->ErrorMsg, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::ReqUserLogin()
{
	CThostFtdcReqUserLoginField* pBody = (CThostFtdcReqUserLoginField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcReqUserLoginField));

	strncpy(pBody->BrokerID, m_ServerInfo.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(pBody->UserID, m_UserInfo.UserID, sizeof(TThostFtdcInvestorIDType));
	strncpy(pBody->Password, m_UserInfo.Password, sizeof(TThostFtdcPasswordType));
	strncpy(pBody->UserProductInfo, m_ServerInfo.UserProductInfo, sizeof(TThostFtdcProductInfoType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_ReqUserLoginField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcReqUserLoginField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqUserLogin(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Logining, 0, nullptr, 0, nullptr, 0, nullptr, 0);
	return m_pApi->ReqUserLogin((CThostFtdcReqUserLoginField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

	if (!IsErrorRspInfo(pRspInfo)
		&& pRspUserLogin)
	{
		pField->TradingDay = GetDate(pRspUserLogin->TradingDay);
		pField->LoginTime = GetTime(pRspUserLogin->LoginTime);

		sprintf(pField->SessionID, "%d:%d", pRspUserLogin->FrontID, pRspUserLogin->SessionID);

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Logined, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);

		// ���µ�¼��Ϣ�����ܻ��õ�
		memcpy(&m_RspUserLogin, pRspUserLogin, sizeof(CThostFtdcRspUserLoginField));
		m_nMaxOrderRef = atol(pRspUserLogin->MaxOrderRef);
		// �Լ�����ʱID��1��ʼ�����ܴ�0��ʼ
		m_nMaxOrderRef = m_nMaxOrderRef>1 ? m_nMaxOrderRef : 1;
		ReqSettlementInfoConfirm();
		ReqQryInvestor();
	}
	else
	{
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->ErrorMsg, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::ReqSettlementInfoConfirm()
{
	CThostFtdcSettlementInfoConfirmField* pBody = (CThostFtdcSettlementInfoConfirmField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcSettlementInfoConfirmField));

	strncpy(pBody->BrokerID, m_ServerInfo.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(pBody->InvestorID, m_UserInfo.UserID, sizeof(TThostFtdcInvestorIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_SettlementInfoConfirmField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcSettlementInfoConfirmField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqSettlementInfoConfirm(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Confirming, 0, nullptr, 0, nullptr, 0, nullptr, 0);
	return m_pApi->ReqSettlementInfoConfirm((CThostFtdcSettlementInfoConfirmField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo)
		&& pSettlementInfoConfirm)
	{
		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Confirmed, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Done, 0, nullptr, 0, nullptr, 0, nullptr, 0);

		if (m_ServerInfo.PrivateTopicResumeType > ResumeType::Restart
			&& (m_ServerInfo.PrivateTopicResumeType<ResumeType::Undefined))
		{
			ReqQryOrder();
			//ReqQryTrade();
			ReqQryQuote();
		}
	}
	else
	{
		RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->ErrorMsg, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::Disconnect()
{
	if (m_msgQueue_Query)
	{
		m_msgQueue_Query->StopThread();
		m_msgQueue_Query->Register(nullptr, nullptr);
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
		m_msgQueue->Register(nullptr, nullptr);
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

	for (unordered_map<string, CThostFtdcOrderField*>::iterator it = m_id_api_order.begin(); it != m_id_api_order.end(); ++it)
		delete it->second;
	m_id_api_order.clear();

	for (unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.begin(); it != m_id_platform_quote.end(); ++it)
		delete it->second;
	m_id_platform_quote.clear();

	for (unordered_map<string, CThostFtdcQuoteField*>::iterator it = m_id_api_quote.begin(); it != m_id_api_quote.end(); ++it)
		delete it->second;
	m_id_api_quote.clear();

	for (unordered_map<string, PositionField*>::iterator it = m_id_platform_position.begin(); it != m_id_platform_position.end(); ++it)
		delete it->second;
	m_id_platform_position.clear();
}

int CTraderApi::ReqOrderInsert(
	OrderField* pOrder,
	int count,
	OrderIDType* pInOut)
{
	int OrderRef = -1;
	if (nullptr == m_pApi)
		return -1;

	CThostFtdcInputOrderField body = { 0 };

	strncpy(body.BrokerID, m_RspUserLogin.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(body.InvestorID, m_RspUserLogin.UserID, sizeof(TThostFtdcInvestorIDType));

	body.MinVolume = 1;
	body.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	body.IsAutoSuspend = 0;
	body.UserForceClose = 0;
	body.IsSwapOrder = 0;

	//��Լ
	strncpy(body.InstrumentID, pOrder->InstrumentID, sizeof(TThostFtdcInstrumentIDType));
	//����
	body.Direction = OrderSide_2_TThostFtdcDirectionType(pOrder->Side);
	//��ƽ
	body.CombOffsetFlag[0] = OpenCloseType_2_TThostFtdcOffsetFlagType(pOrder->OpenClose);
	//Ͷ��
	body.CombHedgeFlag[0] = HedgeFlagType_2_TThostFtdcHedgeFlagType(pOrder->HedgeFlag);
	//����
	body.VolumeTotalOriginal = (int)pOrder->Qty;

	// ���������������õ�һ�������ļ۸񣬻��������������ļ۸���أ�
	body.LimitPrice = pOrder->Price;
	body.StopPrice = pOrder->StopPx;

	// ��Եڶ������д�������еڶ�����������Ϊ�ǽ�����������
	if (count>1)
	{
		body.CombOffsetFlag[1] = OpenCloseType_2_TThostFtdcOffsetFlagType(pOrder[1].OpenClose);
		body.CombHedgeFlag[1] = HedgeFlagType_2_TThostFtdcHedgeFlagType(pOrder[1].HedgeFlag);
		// ���������Ʋֻ��¹��ܣ�û��ʵ���
		body.IsSwapOrder = (body.CombOffsetFlag[0] != body.CombOffsetFlag[1]);
	}

	// �м����޼�
	switch (pOrder->Type)
	{
	case Market:
	case Stop:
	case MarketOnClose:
	case TrailingStop:
		body.OrderPriceType = THOST_FTDC_OPT_AnyPrice;
		body.TimeCondition = THOST_FTDC_TC_IOC;
		body.LimitPrice = 0;
		break;
	case Limit:
	case StopLimit:
	case TrailingStopLimit:
	default:
		body.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		body.TimeCondition = THOST_FTDC_TC_GFD;
		break;
	}

	// IOC��FOK
	switch (pOrder->TimeInForce)
	{
	case IOC:
		body.TimeCondition = THOST_FTDC_TC_IOC;
		body.VolumeCondition = THOST_FTDC_VC_AV;
		break;
	case FOK:
		body.TimeCondition = THOST_FTDC_TC_IOC;
		body.VolumeCondition = THOST_FTDC_VC_CV;
		//body.MinVolume = body.VolumeTotalOriginal; // ����ط��������
		break;
	default:
		body.VolumeCondition = THOST_FTDC_VC_AV;
		break;
	}

	// ������
	switch (pOrder->Type)
	{
	case Stop:
	case TrailingStop:
	case StopLimit:
	case TrailingStopLimit:
		// ������û�в��ԣ�������
		body.ContingentCondition = THOST_FTDC_CC_Immediately;
		break;
	default:
		body.ContingentCondition = THOST_FTDC_CC_Immediately;
		break;
	}

	int nRet = 0;
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
		sprintf(body.OrderRef, "%d", nRet);

		// ����ƽ̨��Խ�ٶȣ��������Ҫע�͵�
		//WriteLog("CTP:ReqOrderInsert:%s %d", body.InstrumentID, nRet);


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
			sprintf(m_orderInsert_Id, "%d:%d:%d", m_RspUserLogin.FrontID, m_RspUserLogin.SessionID, nRet);

			OrderField* pField = (OrderField*)m_msgQueue->new_block(sizeof(OrderField));
			memcpy(pField, pOrder, sizeof(OrderField));
			strcpy(pField->ID, m_orderInsert_Id);
			m_id_platform_order.insert(pair<string, OrderField*>(m_orderInsert_Id, pField));

		}
		strncpy((char*)pInOut, m_orderInsert_Id, sizeof(OrderIDType));
	}

	return nRet;
}

void CTraderApi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	OrderIDType orderId = { 0 };

	if (pInputOrder)
	{
		sprintf(orderId, "%d:%d:%s", m_RspUserLogin.FrontID, m_RspUserLogin.SessionID, pInputOrder->OrderRef);
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
		// �ҵ��ˣ�Ҫ����״̬
		// ��ʹ���ϴε�״̬
		OrderField* pField = it->second;
		pField->ExecType = ExecType::ExecRejected;
		pField->Status = OrderStatus::Rejected;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
	OrderIDType orderId = { 0 };

	if (pInputOrder)
	{
		sprintf(orderId, "%d:%d:%s", m_RspUserLogin.FrontID, m_RspUserLogin.SessionID, pInputOrder->OrderRef);
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
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	OnTrade(pTrade);
}

int CTraderApi::ReqOrderAction(OrderIDType* szIds, int count, OrderIDType* pOutput)
{
	unordered_map<string, CThostFtdcOrderField*>::iterator it = m_id_api_order.find(szIds[0]);
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

int CTraderApi::ReqOrderAction(CThostFtdcOrderField *pOrder, int count, OrderIDType* pOutput)
{
	if (nullptr == m_pApi)
		return 0;

	CThostFtdcInputOrderActionField body = { 0 };

	///���͹�˾����
	strncpy(body.BrokerID, pOrder->BrokerID, sizeof(TThostFtdcBrokerIDType));
	///Ͷ���ߴ���
	strncpy(body.InvestorID, pOrder->InvestorID, sizeof(TThostFtdcInvestorIDType));
	///��������
	strncpy(body.OrderRef, pOrder->OrderRef, sizeof(TThostFtdcOrderRefType));
	///ǰ�ñ��
	body.FrontID = pOrder->FrontID;
	///�Ự���
	body.SessionID = pOrder->SessionID;
	///����������
	strncpy(body.ExchangeID, pOrder->ExchangeID, sizeof(TThostFtdcExchangeIDType));
	///�������
	strncpy(body.OrderSysID, pOrder->OrderSysID, sizeof(TThostFtdcOrderSysIDType));
	///������־
	body.ActionFlag = THOST_FTDC_AF_Delete;
	///��Լ����
	strncpy(body.InstrumentID, pOrder->InstrumentID, sizeof(TThostFtdcInstrumentIDType));

	int nRet = m_pApi->ReqOrderAction(&body, ++m_lRequestID);
	if (nRet < 0)
	{
		sprintf(m_orderAction_Id, "%d", nRet);
	}
	else
	{
		memset(m_orderAction_Id, 0, sizeof(OrderIDType));
	}
	strncpy((char*)pOutput, m_orderAction_Id, sizeof(OrderIDType));


	return nRet;
}

void CTraderApi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	OrderIDType orderId = { 0 };
	if (pInputOrderAction)
	{
		sprintf(orderId, "%d:%d:%s", pInputOrderAction->FrontID, pInputOrderAction->SessionID, pInputOrderAction->OrderRef);
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
		// �ҵ��ˣ�Ҫ����״̬
		// ��ʹ���ϴε�״̬
		OrderField* pField = it->second;
		strcpy(pField->ID, orderId);
		pField->ExecType = ExecType::ExecCancelReject;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
	OrderIDType orderId = { 0 };
	if (pOrderAction)
	{
		sprintf(orderId, "%d:%d:%s", pOrderAction->FrontID, pOrderAction->SessionID, pOrderAction->OrderRef);
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
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	OnOrder(pOrder);
}

char* CTraderApi::ReqQuoteInsert(
	QuoteField* pQuote,
	OrderIDType* pAskRef,
	OrderIDType* pBidRef)
{
	int QuoteRef = -1;
	if (nullptr == m_pApi)
		return nullptr;

	CThostFtdcInputQuoteField body = { 0 };

	strcpy(body.BrokerID, m_RspUserLogin.BrokerID);
	strcpy(body.InvestorID, m_RspUserLogin.UserID);

	//��Լ,Ŀǰֻ�Ӷ���1��ȡ
	strncpy(body.InstrumentID, pQuote->InstrumentID, sizeof(TThostFtdcInstrumentIDType));
	//��ƽ
	body.AskOffsetFlag = OpenCloseType_2_TThostFtdcOffsetFlagType(pQuote->AskOpenClose);
	body.BidOffsetFlag = OpenCloseType_2_TThostFtdcOffsetFlagType(pQuote->BidOpenClose);
	//Ͷ��
	body.AskHedgeFlag = HedgeFlagType_2_TThostFtdcHedgeFlagType(pQuote->AskHedgeFlag);
	body.BidHedgeFlag = HedgeFlagType_2_TThostFtdcHedgeFlagType(pQuote->BidHedgeFlag);

	//�۸�
	body.AskPrice = pQuote->AskPrice;
	body.BidPrice = pQuote->BidPrice;

	//����
	body.AskVolume = (int)pQuote->AskQty;
	body.BidVolume = (int)pQuote->BidQty;

	strncpy(body.ForQuoteSysID, pQuote->QuoteReqID, sizeof(TThostFtdcOrderSysIDType));

	int nRet = 0;
	{
		//���ܱ���̫�죬m_nMaxOrderRef��û�иı���ύ��
		lock_guard<mutex> cl(m_csOrderRef);

		if (QuoteRef < 0)
		{
			nRet = m_nMaxOrderRef;
			sprintf(body.QuoteRef, "%d", m_nMaxOrderRef);
			sprintf(body.AskOrderRef, "%d", m_nMaxOrderRef);
			sprintf(body.BidOrderRef, "%d", ++m_nMaxOrderRef);
			++m_nMaxOrderRef;
		}
		else
		{
			nRet = QuoteRef;
			sprintf(body.QuoteRef, "%d", QuoteRef);
			sprintf(body.AskOrderRef, "%d", QuoteRef);
			sprintf(body.BidOrderRef, "%d", ++QuoteRef);
			++QuoteRef;
		}

		//�����浽���У�����ֱ�ӷ���
		int n = m_pApi->ReqQuoteInsert(&body, ++m_lRequestID);
		if (n < 0)
		{
			nRet = n;
			return nullptr;
		}
		else
		{
			sprintf(m_orderInsert_Id, "%d:%d:%d", m_RspUserLogin.FrontID, m_RspUserLogin.SessionID, nRet);

			QuoteField* pField = (QuoteField*)m_msgQueue->new_block(sizeof(QuoteField));
			memcpy(pField, pQuote, sizeof(QuoteField));
			strcpy(pField->ID, m_orderInsert_Id);
			strcpy(pField->AskID, m_orderInsert_Id);
			sprintf(pField->BidID, "%d:%d:%d", m_RspUserLogin.FrontID, m_RspUserLogin.SessionID, nRet + 1);

			m_id_platform_quote.insert(pair<string, QuoteField*>(m_orderInsert_Id, pField));
		}
	}

	return m_orderInsert_Id;
}

void CTraderApi::OnRspQuoteInsert(CThostFtdcInputQuoteField *pInputQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	OrderIDType quoteId = { 0 };

	if (pInputQuote)
	{
		sprintf(quoteId, "%d:%d:%s", m_RspUserLogin.FrontID, m_RspUserLogin.SessionID, pInputQuote->QuoteRef);
	}
	else
	{
		IsErrorRspInfo(pRspInfo, nRequestID, bIsLast);
	}

	unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
	if (it == m_id_platform_quote.end())
	{
		// û�ҵ�����Ӧ�������ʾ������
		//assert(false);
	}
	else
	{
		// �ҵ��ˣ�Ҫ����״̬
		// ��ʹ���ϴε�״̬
		QuoteField* pField = it->second;
		pField->ExecType = ExecType::ExecRejected;
		pField->Status = OrderStatus::Rejected;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnErrRtnQuoteInsert(CThostFtdcInputQuoteField *pInputQuote, CThostFtdcRspInfoField *pRspInfo)
{
	OrderIDType quoteId = { 0 };

	if (pInputQuote)
	{
		sprintf(quoteId, "%d:%d:%s", m_RspUserLogin.FrontID, m_RspUserLogin.SessionID, pInputQuote->QuoteRef);
	}
	else
	{
		IsErrorRspInfo(pRspInfo, 0, true);
	}

	unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
	if (it == m_id_platform_quote.end())
	{
		// û�ҵ�����Ӧ�������ʾ������
		//assert(false);
	}
	else
	{
		// �ҵ��ˣ�Ҫ����״̬
		// ��ʹ���ϴε�״̬
		QuoteField* pField = it->second;
		pField->ExecType = ExecType::ExecRejected;
		pField->Status = OrderStatus::Rejected;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRtnQuote(CThostFtdcQuoteField *pQuote)
{
	OnQuote(pQuote);
}

int CTraderApi::ReqQuoteAction(const string& szId, OrderIDType* pOutput)
{
	unordered_map<string, CThostFtdcQuoteField*>::iterator it = m_id_api_quote.find(szId);
	if (it == m_id_api_quote.end())
	{
		sprintf((char*)pOutput, "%d", -100);
		return -100;
	}
	else
	{
		// �ҵ��˶���
		ReqQuoteAction(it->second, pOutput);
	}
	return 0;
}

int CTraderApi::ReqQuoteAction(CThostFtdcQuoteField *pQuote, OrderIDType* pOutput)
{
	if (nullptr == m_pApi)
		return 0;

	CThostFtdcInputQuoteActionField body = { 0 };

	///���͹�˾����
	strcpy(body.BrokerID, pQuote->BrokerID);
	///Ͷ���ߴ���
	strcpy(body.InvestorID, pQuote->InvestorID);
	///��������
	strcpy(body.QuoteRef, pQuote->QuoteRef);
	///ǰ�ñ��
	body.FrontID = pQuote->FrontID;
	///�Ự���
	body.SessionID = pQuote->SessionID;
	///����������
	strcpy(body.ExchangeID, pQuote->ExchangeID);
	///�������
	strcpy(body.QuoteSysID, pQuote->QuoteSysID);
	///������־
	body.ActionFlag = THOST_FTDC_AF_Delete;
	///��Լ����
	strcpy(body.InstrumentID, pQuote->InstrumentID);

	int nRet = m_pApi->ReqQuoteAction(&body, ++m_lRequestID);
	if (nRet < 0)
	{
		sprintf(m_orderAction_Id, "%d", nRet);
	}
	else
	{
		memset(m_orderAction_Id, 0, sizeof(OrderIDType));
	}
	strncpy((char*)pOutput, m_orderAction_Id, sizeof(OrderIDType));

	return nRet;
}

void CTraderApi::OnRspQuoteAction(CThostFtdcInputQuoteActionField *pInputQuoteAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	OrderIDType quoteId = { 0 };
	if (pInputQuoteAction)
	{
		sprintf(quoteId, "%d:%d:%s", pInputQuoteAction->FrontID, pInputQuoteAction->SessionID, pInputQuoteAction->QuoteRef);
	}
	else
	{
		IsErrorRspInfo(pRspInfo, nRequestID, bIsLast);
	}

	unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
	if (it == m_id_platform_quote.end())
	{
		// û�ҵ�����Ӧ�������ʾ������
		//assert(false);
	}
	else
	{
		// �ҵ��ˣ�Ҫ����״̬
		// ��ʹ���ϴε�״̬
		QuoteField* pField = it->second;
		strcpy(pField->ID, quoteId);
		//sprintf(pField->AskID, "%d:%d:%s", pInputQuoteAction->FrontID, pInputQuoteAction->SessionID, pInputQuoteAction->);
		//sprintf(pField->BidID, "%d:%d:%s", pInputQuoteAction->FrontID, pInputQuoteAction->SessionID, pInputQuoteAction->QuoteRef);
		pField->ExecType = ExecType::ExecCancelReject;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnErrRtnQuoteAction(CThostFtdcQuoteActionField *pQuoteAction, CThostFtdcRspInfoField *pRspInfo)
{
	OrderIDType quoteId = { 0 };

	if (pQuoteAction)
	{
		sprintf(quoteId, "%d:%d:%s", pQuoteAction->FrontID, pQuoteAction->SessionID, pQuoteAction->QuoteRef);
	}
	else
	{
		IsErrorRspInfo(pRspInfo, 0, true);
	}

	unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
	if (it == m_id_platform_quote.end())
	{
		// û�ҵ�����Ӧ�������ʾ������
		//assert(false);
	}
	else
	{
		// �ҵ��ˣ�Ҫ����״̬
		// ��ʹ���ϴε�״̬
		QuoteField* pField = it->second;
		strcpy(pField->ID, quoteId);
		pField->ExecType = ExecType::ExecCancelReject;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::ReqQryTradingAccount()
{
	CThostFtdcQryTradingAccountField* pBody = (CThostFtdcQryTradingAccountField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcQryTradingAccountField));

	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(pBody->InvestorID, m_RspUserLogin.UserID, sizeof(TThostFtdcInvestorIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryTradingAccountField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcQryTradingAccountField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryTradingAccount(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryTradingAccount((CThostFtdcQryTradingAccountField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pTradingAccount)
		{
			AccountField* pField = (AccountField*)m_msgQueue->new_block(sizeof(AccountField));

			pField->PreBalance = pTradingAccount->PreBalance;
			pField->CurrMargin = pTradingAccount->CurrMargin;
			pField->Commission = pTradingAccount->Commission;
			pField->CloseProfit = pTradingAccount->CloseProfit;
			pField->PositionProfit = pTradingAccount->PositionProfit;
			pField->Balance = pTradingAccount->Balance;
			pField->Available = pTradingAccount->Available;

			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryTradingAccount, m_msgQueue, m_pClass, bIsLast, 0, pField, sizeof(AccountField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryTradingAccount, m_msgQueue, m_pClass, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}

void CTraderApi::ReqQryInvestorPosition(const string& szInstrumentId, const string& szExchange)
{
	CThostFtdcQryInvestorPositionField* pBody = (CThostFtdcQryInvestorPositionField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcQryInvestorPositionField));

	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(pBody->InvestorID, m_RspUserLogin.UserID, sizeof(TThostFtdcInvestorIDType));
	strncpy(pBody->InstrumentID, szInstrumentId.c_str(), sizeof(TThostFtdcInstrumentIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryInvestorPositionField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcQryInvestorPositionField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryInvestorPosition(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryInvestorPosition((CThostFtdcQryInvestorPositionField*)ptr1, ++m_lRequestID);
}

// ����������ѯ���ͽ�����ȫ������
// ����Ǻ��ڵĳɽ��ر�����ֻ���ظ��µļ�¼
// �����н�����ͬʱ�н�������ĳֲ�ʱ��ֻ���ؽ���������������
// ������������Ŀǰû�����⣬������Ҳֻ������
void CTraderApi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pInvestorPosition)
		{
			PositionIDType positionId = { 0 };
			sprintf(positionId, "%s:%d:%c",
				pInvestorPosition->InstrumentID, TThostFtdcPosiDirectionType_2_PositionSide(pInvestorPosition->PosiDirection), pInvestorPosition->HedgeFlag);

			PositionField* pField = nullptr;
			unordered_map<string, PositionField*>::iterator it = m_id_platform_position.find(positionId);
			if (it == m_id_platform_position.end())
			{
				pField = (PositionField*)m_msgQueue->new_block(sizeof(PositionField));

				strcpy(pField->Symbol, pInvestorPosition->InstrumentID);
				strcpy(pField->InstrumentID, pInvestorPosition->InstrumentID);
				//strcpy(pField->ExchangeID, );
				pField->Side = TThostFtdcPosiDirectionType_2_PositionSide(pInvestorPosition->PosiDirection);
				pField->HedgeFlag = TThostFtdcHedgeFlagType_2_HedgeFlagType(pInvestorPosition->HedgeFlag);

				m_id_platform_position.insert(pair<string, PositionField*>(positionId, pField));
			}
			else
			{
				pField = it->second;
			}

			pField->Position = pInvestorPosition->Position;
			pField->TdPosition = pInvestorPosition->TodayPosition;
			pField->YdPosition = pInvestorPosition->Position - pInvestorPosition->TodayPosition;

			// �������ռ�ȫ���ٱ���֪ͨһ�Σ�Ϊ��Ҫ����������Ϊ������������¼�����Ҽ���һ������
			if (bIsLast)
			{
				int cnt = 0;
				size_t count = m_id_platform_position.size();
				for (unordered_map<string, PositionField*>::iterator iter = m_id_platform_position.begin(); iter != m_id_platform_position.end(); iter++)
				{
					++cnt;
					m_msgQueue->Input_Copy(ResponeType::OnRspQryInvestorPosition, m_msgQueue, m_pClass, cnt == count, 0, iter->second, sizeof(PositionField), nullptr, 0, nullptr, 0);
				}
			}
			//XRespone(ResponeType::OnRspQryInvestorPosition, m_msgQueue, this, bIsLast, 0, pField, sizeof(PositionField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_Copy(ResponeType::OnRspQryInvestorPosition, m_msgQueue, m_pClass, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}

void CTraderApi::ReqQryInstrument(const string& szInstrumentId, const string& szExchange)
{
	CThostFtdcQryInstrumentField* pBody = (CThostFtdcQryInstrumentField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcQryInstrumentField));

	strncpy(pBody->InstrumentID, szInstrumentId.c_str(), sizeof(TThostFtdcInstrumentIDType));
	strncpy(pBody->ExchangeID, szExchange.c_str(), sizeof(TThostFtdcExchangeIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryInstrumentField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcQryInstrumentField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryInstrument(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryInstrument((CThostFtdcQryInstrumentField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pInstrument)
		{
			InstrumentField* pField = (InstrumentField*)m_msgQueue->new_block(sizeof(InstrumentField));

			strncpy(pField->InstrumentID, pInstrument->InstrumentID, sizeof(InstrumentIDType));
			strncpy(pField->ExchangeID, pInstrument->ExchangeID, sizeof(ExchangeIDType));

			strncpy(pField->Symbol, pInstrument->InstrumentID, sizeof(SymbolType));
			strncpy(pField->ProductID, pInstrument->ProductID, sizeof(InstrumentIDType));

			strncpy(pField->InstrumentName, pInstrument->InstrumentName, sizeof(InstrumentNameType));
			pField->Type = CThostFtdcInstrumentField_2_InstrumentType(pInstrument);
			pField->VolumeMultiple = pInstrument->VolumeMultiple;
			pField->PriceTick = pInstrument->PriceTick;
			pField->ExpireDate = GetDate(pInstrument->ExpireDate);
			pField->OptionsType = TThostFtdcOptionsTypeType_2_PutCall(pInstrument->OptionsType);
			pField->StrikePrice = (pInstrument->StrikePrice < DBL_EPSILON || pInstrument->StrikePrice == DBL_MAX) ? 0 : pInstrument->StrikePrice;

			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInstrument, m_msgQueue, m_pClass, bIsLast, 0, pField, sizeof(InstrumentField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInstrument, m_msgQueue, m_pClass, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}

//void CTraderApi::ReqQryInstrumentCommissionRate(const string& szInstrumentId)
//{
//	CThostFtdcQryInstrumentCommissionRateField body = {0};
//
//	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID,sizeof(TThostFtdcBrokerIDType));
//	strncpy(pBody->InvestorID, m_RspUserLogin.UserID,sizeof(TThostFtdcInvestorIDType));
//	strncpy(pBody->InstrumentID,szInstrumentId.c_str(),sizeof(TThostFtdcInstrumentIDType));
//}
//
//void CTraderApi::OnRspQryInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//
//}

//void CTraderApi::ReqQryInstrumentMarginRate(const string& szInstrumentId,TThostFtdcHedgeFlagType HedgeFlag)
//{
//	CThostFtdcQryInstrumentMarginRateField body = {0};
//
//	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID,sizeof(TThostFtdcBrokerIDType));
//	strncpy(pBody->InvestorID, m_RspUserLogin.UserID,sizeof(TThostFtdcInvestorIDType));
//	strncpy(pBody->InstrumentID,szInstrumentId.c_str(),sizeof(TThostFtdcInstrumentIDType));
//	pBody->HedgeFlag = HedgeFlag;
//
//	//AddToSendQueue(pRequest);
//}

//void CTraderApi::OnRspQryInstrumentMarginRate(CThostFtdcInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//
//}
//
//void CTraderApi::ReqQrySettlementInfo(const string& szTradingDay)
//{
//	if (nullptr == m_pApi)
//		return;
//
//	CThostFtdcQrySettlementInfoField body = {0};
//
//	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID, sizeof(TThostFtdcBrokerIDType));
//	strncpy(pBody->InvestorID, m_RspUserLogin.UserID, sizeof(TThostFtdcInvestorIDType));
//	strncpy(pBody->TradingDay, szTradingDay.c_str(), sizeof(TThostFtdcDateType));
//}
//
//void CTraderApi::OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
//	{
//		if (pSettlementInfo)
//		{
//			SettlementInfoField field = { 0 };
//			strncpy(field.TradingDay, pSettlementInfo->TradingDay, sizeof(TThostFtdcDateType));
//			strncpy(field.Content, pSettlementInfo->Content, sizeof(TThostFtdcContentType));
//
//			m_msgQueue->Input(ResponeType::OnRspQrySettlementInfo, m_msgQueue, this, bIsLast, 0, &field, sizeof(SettlementInfoField), nullptr, 0, nullptr, 0);
//		}
//		else
//		{
//			m_msgQueue->Input(ResponeType::OnRspQrySettlementInfo, m_msgQueue, this, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
//		}
//	}
//}

void CTraderApi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	IsErrorRspInfo(pRspInfo, nRequestID, bIsLast);
}

void CTraderApi::ReqQryOrder()
{
	CThostFtdcQryOrderField* pBody = (CThostFtdcQryOrderField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcQryOrderField));

	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(pBody->InvestorID, m_RspUserLogin.UserID, sizeof(TThostFtdcInvestorIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryOrderField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcQryOrderField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryOrder(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryOrder((CThostFtdcQryOrderField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnOrder(CThostFtdcOrderField *pOrder)
{
	if (nullptr == pOrder)
		return;

	OrderIDType orderId = { 0 };
	sprintf(orderId, "%d:%d:%s", pOrder->FrontID, pOrder->SessionID, pOrder->OrderRef);
	OrderIDType orderSydId = { 0 };

	{
		// ����ԭʼ������Ϣ�����ڳ���

		unordered_map<string, CThostFtdcOrderField*>::iterator it = m_id_api_order.find(orderId);
		if (it == m_id_api_order.end())
		{
			// �Ҳ����˶�������ʾ���µ�
			CThostFtdcOrderField* pField = new CThostFtdcOrderField();
			memcpy(pField, pOrder, sizeof(CThostFtdcOrderField));
			m_id_api_order.insert(pair<string, CThostFtdcOrderField*>(orderId, pField));
		}
		else
		{
			// �ҵ��˶���
			// ��Ҫ�ٸ��Ʊ������һ�ε�״̬������ֻҪ��һ�ε����ڳ������ɣ����£��������ñȽ�
			CThostFtdcOrderField* pField = it->second;
			memcpy(pField, pOrder, sizeof(CThostFtdcOrderField));
		}

		// ����SysID���ڶ���ɽ��ر��붩��
		sprintf(orderSydId, "%s:%s", pOrder->ExchangeID, pOrder->OrderSysID);
		m_sysId_orderId.insert(pair<string, string>(orderSydId, orderId));
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
			pField->HedgeFlag = TThostFtdcHedgeFlagType_2_HedgeFlagType(pOrder->CombHedgeFlag[0]);
			pField->Side = TThostFtdcDirectionType_2_OrderSide(pOrder->Direction);
			pField->Price = pOrder->LimitPrice;
			pField->StopPx = pOrder->StopPrice;
			strncpy(pField->Text, pOrder->StatusMsg, sizeof(ErrorMsgType));
			pField->OpenClose = TThostFtdcOffsetFlagType_2_OpenCloseType(pOrder->CombOffsetFlag[0]);
			pField->Status = CThostFtdcOrderField_2_OrderStatus(pOrder);
			pField->Qty = pOrder->VolumeTotalOriginal;
			pField->Type = CThostFtdcOrderField_2_OrderType(pOrder);
			pField->TimeInForce = CThostFtdcOrderField_2_TimeInForce(pOrder);
			pField->ExecType = ExecType::ExecNew;
			strcpy(pField->OrderID, pOrder->OrderSysID);


			// ��ӵ�map�У������������ߵĶ�ȡ������ʧ��ʱ����֪ͨ��
			m_id_platform_order.insert(pair<string, OrderField*>(orderId, pField));
		}
		else
		{
			pField = it->second;
			strcpy(pField->ID, orderId);
			pField->LeavesQty = pOrder->VolumeTotal;
			pField->Price = pOrder->LimitPrice;
			pField->Status = CThostFtdcOrderField_2_OrderStatus(pOrder);
			pField->ExecType = CThostFtdcOrderField_2_ExecType(pOrder);
			strcpy(pField->OrderID, pOrder->OrderSysID);
			strncpy(pField->Text, pOrder->StatusMsg, sizeof(ErrorMsgType));
		}

		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		OnOrder(pOrder);
	}
}

void CTraderApi::ReqQryTrade()
{
	CThostFtdcQryTradeField* pBody = (CThostFtdcQryTradeField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcQryTradeField));

	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(pBody->InvestorID, m_RspUserLogin.UserID, sizeof(TThostFtdcInvestorIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryTradeField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcQryTradeField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryTrade(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryTrade((CThostFtdcQryTradeField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnTrade(CThostFtdcTradeField *pTrade)
{
	if (nullptr == pTrade)
		return;

	TradeField* pField = (TradeField*)m_msgQueue->new_block(sizeof(TradeField));
	strcpy(pField->InstrumentID, pTrade->InstrumentID);
	strcpy(pField->ExchangeID, pTrade->ExchangeID);
	pField->Side = TThostFtdcDirectionType_2_OrderSide(pTrade->Direction);
	pField->Qty = pTrade->Volume;
	pField->Price = pTrade->Price;
	pField->OpenClose = TThostFtdcOffsetFlagType_2_OpenCloseType(pTrade->OffsetFlag);
	pField->HedgeFlag = TThostFtdcHedgeFlagType_2_HedgeFlagType(pTrade->HedgeFlag);
	pField->Commission = 0;//TODO�������Ժ�Ҫ�������
	pField->Time = GetTime(pTrade->TradeTime);
	strcpy(pField->TradeID, pTrade->TradeID);

	OrderIDType orderSysId = { 0 };
	sprintf(orderSysId, "%s:%s", pTrade->ExchangeID, pTrade->OrderSysID);
	unordered_map<string, string>::iterator it = m_sysId_orderId.find(orderSysId);
	if (it == m_sysId_orderId.end())
	{
		// �˳ɽ��Ҳ�����Ӧ�ı���
		//assert(false);
	}
	else
	{
		// �ҵ���Ӧ�ı���
		strcpy(pField->ID, it->second.c_str());

		m_msgQueue->Input_Copy(ResponeType::OnRtnTrade, m_msgQueue, m_pClass, 0, 0, pField, sizeof(TradeField), nullptr, 0, nullptr, 0);

		unordered_map<string, OrderField*>::iterator it2 = m_id_platform_order.find(it->second);
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

		OnTrade(pField);
	}
}

void CTraderApi::OnTrade(TradeField *pTrade)
{
	PositionIDType positionId = { 0 };
	sprintf(positionId, "%s:%d:%c",
		pTrade->InstrumentID, TradeField_2_PositionSide(pTrade), pTrade->HedgeFlag);

	PositionField* pField = nullptr;
	unordered_map<string, PositionField*>::iterator it = m_id_platform_position.find(positionId);
	if (it == m_id_platform_position.end())
	{
		pField = (PositionField*)m_msgQueue->new_block(sizeof(PositionField));

		strcpy(pField->Symbol, pTrade->InstrumentID);
		strcpy(pField->InstrumentID, pTrade->InstrumentID);
		pField->Side = TradeField_2_PositionSide(pTrade);
		pField->HedgeFlag = TThostFtdcHedgeFlagType_2_HedgeFlagType(pTrade->HedgeFlag);

		m_id_platform_position.insert(pair<string, PositionField*>(positionId, pField));
	}
	else
	{
		pField = it->second;
	}

	if (pTrade->OpenClose == OpenCloseType::Open)
	{
		pField->Position += pTrade->Qty;
		pField->TdPosition += pTrade->Qty;
	}
	else
	{
		pField->Position -= pTrade->Qty;
		if (pTrade->OpenClose == OpenCloseType::CloseToday)
		{
			pField->TdPosition -= pTrade->Qty;
		}
		else
		{
			pField->YdPosition -= pTrade->Qty;
			// �������ı����ɸ������ӽ��쿪ʼ������
			if (pField->YdPosition<0)
			{
				pField->TdPosition += pField->YdPosition;
				pField->YdPosition = 0;
			}
		}

		// �������ֱ�����²�ѯ
		if (pField->Position < 0 || pField->TdPosition < 0 || pField->YdPosition < 0)
		{
			ReqQryInvestorPosition("", "");
			return;
		}
	}

	m_msgQueue->Input_Copy(ResponeType::OnRspQryInvestorPosition, m_msgQueue, m_pClass, false, 0, pField, sizeof(PositionField), nullptr, 0, nullptr, 0);
}

void CTraderApi::OnRspQryTrade(CThostFtdcTradeField *pTrade, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		OnTrade(pTrade);
	}
}

void CTraderApi::ReqQryQuote()
{
	CThostFtdcQryQuoteField* pBody = (CThostFtdcQryQuoteField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcQryQuoteField));

	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(pBody->InvestorID, m_RspUserLogin.UserID, sizeof(TThostFtdcInvestorIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryQuoteField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcQryQuoteField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryQuote(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryQuote((CThostFtdcQryQuoteField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnQuote(CThostFtdcQuoteField *pQuote)
{
	if (nullptr == pQuote)
		return;

	OrderIDType quoteId = { 0 };
	sprintf(quoteId, "%d:%d:%s", pQuote->FrontID, pQuote->SessionID, pQuote->QuoteRef);
	OrderIDType orderSydId = { 0 };

	{
		// ����ԭʼ������Ϣ�����ڳ���

		unordered_map<string, CThostFtdcQuoteField*>::iterator it = m_id_api_quote.find(quoteId);
		if (it == m_id_api_quote.end())
		{
			// �Ҳ����˶�������ʾ���µ�
			CThostFtdcQuoteField* pField = new CThostFtdcQuoteField();
			memcpy(pField, pQuote, sizeof(CThostFtdcQuoteField));
			m_id_api_quote.insert(pair<string, CThostFtdcQuoteField*>(quoteId, pField));
		}
		else
		{
			// �ҵ��˶���
			// ��Ҫ�ٸ��Ʊ������һ�ε�״̬������ֻҪ��һ�ε����ڳ������ɣ����£��������ñȽ�
			CThostFtdcQuoteField* pField = it->second;
			memcpy(pField, pQuote, sizeof(CThostFtdcQuoteField));
		}

		// ����ط��Ƿ�Ҫ������������

		// ����SysID���ڶ���ɽ��ر��붩��
		//sprintf(orderSydId, "%s:%s", pQuote->ExchangeID, pQuote->QuoteSysID);
		//m_sysId_quoteId.insert(pair<string, string>(orderSydId, quoteId));
	}

	{
		// ��API�Ķ���ת�����Լ��Ľṹ��

		QuoteField* pField = nullptr;
		unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.find(quoteId);
		if (it == m_id_platform_quote.end())
		{
			// ����ʱ������Ϣ��û�У������Ҳ�����Ӧ�ĵ��ӣ���Ҫ����Order�Ļָ�
			pField = (QuoteField*)m_msgQueue->new_block(sizeof(QuoteField));

			strcpy(pField->InstrumentID, pQuote->InstrumentID);
			strcpy(pField->ExchangeID, pQuote->ExchangeID);

			pField->AskQty = pQuote->AskVolume;
			pField->AskPrice = pQuote->AskPrice;
			pField->AskOpenClose = TThostFtdcOffsetFlagType_2_OpenCloseType(pQuote->AskOffsetFlag);
			pField->AskHedgeFlag = TThostFtdcHedgeFlagType_2_HedgeFlagType(pQuote->AskHedgeFlag);

			pField->BidQty = pQuote->BidVolume;
			pField->BidPrice = pQuote->BidPrice;
			pField->BidOpenClose = TThostFtdcOffsetFlagType_2_OpenCloseType(pQuote->BidOffsetFlag);
			pField->BidHedgeFlag = TThostFtdcHedgeFlagType_2_HedgeFlagType(pQuote->BidHedgeFlag);

			strcpy(pField->ID, quoteId);
			strcpy(pField->AskOrderID, pQuote->AskOrderSysID);
			strcpy(pField->BidOrderID, pQuote->BidOrderSysID);

			strncpy(pField->Text, pQuote->StatusMsg, sizeof(ErrorMsgType));

			//pField->ExecType = ExecType::ExecNew;
			pField->Status = CThostFtdcQuoteField_2_OrderStatus(pQuote);
			pField->ExecType = ExecType::ExecNew;


			// ��ӵ�map�У������������ߵĶ�ȡ������ʧ��ʱ����֪ͨ��
			m_id_platform_quote.insert(pair<string, QuoteField*>(quoteId, pField));
		}
		else
		{
			pField = it->second;

			strcpy(pField->ID, quoteId);
			strcpy(pField->AskOrderID, pQuote->AskOrderSysID);
			strcpy(pField->BidOrderID, pQuote->BidOrderSysID);

			pField->Status = CThostFtdcQuoteField_2_OrderStatus(pQuote);
			pField->ExecType = CThostFtdcQuoteField_2_ExecType(pQuote);

			strncpy(pField->Text, pQuote->StatusMsg, sizeof(ErrorMsgType));
		}

		m_msgQueue->Input_Copy(ResponeType::OnRtnQuote, m_msgQueue, m_pClass, 0, 0, pField, sizeof(QuoteField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRspQryQuote(CThostFtdcQuoteField *pQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		OnQuote(pQuote);
	}
}

void CTraderApi::OnRtnInstrumentStatus(CThostFtdcInstrumentStatusField *pInstrumentStatus)
{
}

void CTraderApi::ReqQryInvestor()
{
	CThostFtdcQryInvestorField* pBody = (CThostFtdcQryInvestorField*)m_msgQueue_Query->new_block(sizeof(CThostFtdcQryInvestorField));

	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID, sizeof(TThostFtdcBrokerIDType));
	strncpy(pBody->InvestorID, m_RspUserLogin.UserID, sizeof(TThostFtdcInvestorIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryInvestorField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CThostFtdcQryInvestorField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryInvestor(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryInvestor((CThostFtdcQryInvestorField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryInvestor(CThostFtdcInvestorField *pInvestor, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pInvestor)
		{
			memcpy(&m_Investor, pInvestor, sizeof(CThostFtdcInvestorField));

			InvestorField* pField = (InvestorField*)m_msgQueue->new_block(sizeof(InvestorField));

			strcpy(pField->BrokerID, pInvestor->BrokerID);
			strcpy(pField->InvestorID, pInvestor->InvestorID);
			strcpy(pField->InvestorName, pInvestor->InvestorName);
			strcpy(pField->IdentifiedCardNo, pInvestor->IdentifiedCardNo);
			pField->IdentifiedCardType = TThostFtdcIdCardTypeType_2_IdCardType(pInvestor->IdentifiedCardType);

			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInvestor, m_msgQueue, m_pClass, bIsLast, 0, pField, sizeof(InvestorField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInvestor, m_msgQueue, m_pClass, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}