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
	case E_ReqUserLoginField:
		iRet = _ReqUserLogin(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
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

bool CTraderApi::IsErrorRspInfo(CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
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

bool CTraderApi::IsErrorRspInfo(CSecurityFtdcRspInfoField *pRspInfo)
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

	m_pApi = CSecurityFtdcTraderApi::CreateFtdcTraderApi(pszPath);
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
			m_pApi->SubscribePublicTopic((SECURITY_TE_RESUME_TYPE)m_ServerInfo.PublicTopicResumeType);
		if (m_ServerInfo.PrivateTopicResumeType<ResumeType::Undefined)
			m_pApi->SubscribePrivateTopic((SECURITY_TE_RESUME_TYPE)m_ServerInfo.PrivateTopicResumeType);

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Connecting, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		//��ʼ������
		m_pApi->Init();
	}

	return 0;
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

	m_lRequestID = 0;//�����߳��Ѿ�ֹͣ��û�б�Ҫ��ԭ�Ӳ�����
	
	Clear();
}

void CTraderApi::Clear()
{
	for (unordered_map<string, OrderField*>::iterator it = m_id_platform_order.begin(); it != m_id_platform_order.end(); ++it)
		delete it->second;
	m_id_platform_order.clear();

	for (unordered_map<string, CSecurityFtdcOrderField*>::iterator it = m_id_api_order.begin(); it != m_id_api_order.end(); ++it)
		delete it->second;
	m_id_api_order.clear();

	//for (unordered_map<string, QuoteField*>::iterator it = m_id_platform_quote.begin(); it != m_id_platform_quote.end(); ++it)
	//	delete it->second;
	//m_id_platform_quote.clear();

	//for (unordered_map<string, CThostFtdcQuoteField*>::iterator it = m_id_api_quote.begin(); it != m_id_api_quote.end(); ++it)
	//	delete it->second;
	//m_id_api_quote.clear();

	for (unordered_map<string, PositionField*>::iterator it = m_id_platform_position.begin(); it != m_id_platform_position.end(); ++it)
		delete it->second;
	m_id_platform_position.clear();
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
	CSecurityFtdcReqUserLoginField* pBody = (CSecurityFtdcReqUserLoginField*)m_msgQueue_Query->new_block(sizeof(CSecurityFtdcReqUserLoginField));

	strncpy(pBody->BrokerID, m_ServerInfo.BrokerID, sizeof(TSecurityFtdcBrokerIDType));
	strncpy(pBody->UserID, m_UserInfo.UserID, sizeof(TSecurityFtdcInvestorIDType));
	strncpy(pBody->Password, m_UserInfo.Password, sizeof(TSecurityFtdcPasswordType));
	strncpy(pBody->UserProductInfo, m_ServerInfo.UserProductInfo, sizeof(TSecurityFtdcProductInfoType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_ReqUserLoginField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CSecurityFtdcReqUserLoginField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqUserLogin(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Logining, 0, nullptr, 0, nullptr, 0, nullptr, 0);
	return m_pApi->ReqUserLogin((CSecurityFtdcReqUserLoginField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspUserLogin(CSecurityFtdcRspUserLoginField *pRspUserLogin, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

	if (!IsErrorRspInfo(pRspInfo)
		&&pRspUserLogin)
	{
		pField->TradingDay = GetDate(pRspUserLogin->TradingDay);
		pField->LoginTime = GetTime(pRspUserLogin->LoginTime);

		sprintf(pField->SessionID, "%d:%d", pRspUserLogin->FrontID, pRspUserLogin->SessionID);

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Logined, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);

		// ���µ�¼��Ϣ�����ܻ��õ�
		memcpy(&m_RspUserLogin,pRspUserLogin,sizeof(CSecurityFtdcRspUserLoginField));
		m_nMaxOrderRef = atol(pRspUserLogin->MaxOrderRef);
		// �Լ�����ʱID��1��ʼ�����ܴ�0��ʼ
		m_nMaxOrderRef = m_nMaxOrderRef>1 ? m_nMaxOrderRef : 1;
		//ReqSettlementInfoConfirm();
		ReqQryInvestor();

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
		return -1;

	CSecurityFtdcInputOrderField body = {0};

	strncpy(body.BrokerID, m_RspUserLogin.BrokerID, sizeof(TSecurityFtdcBrokerIDType));
	strncpy(body.InvestorID, m_RspUserLogin.UserID, sizeof(TSecurityFtdcInvestorIDType));

	body.MinVolume = 1;
	body.ForceCloseReason = SECURITY_FTDC_FCC_NotForceClose;
	body.IsAutoSuspend = 0;
	body.UserForceClose = 0;
	//body.IsSwapOrder = 0;

	//��Լ
	strncpy(body.InstrumentID, pOrder->InstrumentID, sizeof(TSecurityFtdcInstrumentIDType));
	strncpy(body.ExchangeID, pOrder->ExchangeID, sizeof(TSecurityFtdcExchangeIDType));
	//����
	body.Direction = OrderSide_2_TSecurityFtdcDirectionType(pOrder->Side);
	//��ƽ
	body.CombOffsetFlag[0] = OpenCloseType_2_TSecurityFtdcOffsetFlagType(pOrder->OpenClose);
	//Ͷ��
	body.CombHedgeFlag[0] = HedgeFlagType_2_TSecurityFtdcHedgeFlagType(pOrder->HedgeFlag);
	//����
	body.VolumeTotalOriginal = (int)pOrder->Qty;

	// ���������������õ�һ�������ļ۸񣬻��������������ļ۸���أ�
	//body.LimitPrice = pOrder1->Price;
	sprintf(body.LimitPrice, "%f", pOrder->Price);
	body.StopPrice = pOrder->StopPx;

	// ��Եڶ������д�������еڶ�����������Ϊ�ǽ�����������
	if (count>1)
	{
		body.CombOffsetFlag[1] = OpenCloseType_2_TSecurityFtdcOffsetFlagType(pOrder[1].OpenClose);
		body.CombHedgeFlag[1] = HedgeFlagType_2_TSecurityFtdcHedgeFlagType(pOrder[1].HedgeFlag);
		// ���������Ʋֻ��¹��ܣ�û��ʵ���
		//body.IsSwapOrder = (body.CombOffsetFlag[0] != body.CombOffsetFlag[1]);
	}

	//�۸�
	//body.OrderPriceType = OrderType_2_TSecurityFtdcOrderPriceTypeType(pOrder1->Type);

	// �м����޼�
	switch (pOrder->Type)
	{
	case Market:
	case Stop:
	case MarketOnClose:
	case TrailingStop:
		body.OrderPriceType = SECURITY_FTDC_OPT_BestPrice;
		body.TimeCondition = SECURITY_FTDC_TC_IOC;
		break;
	case Limit:
	case StopLimit:
	case TrailingStopLimit:
	default:
		body.OrderPriceType = SECURITY_FTDC_OPT_LimitPrice;
		body.TimeCondition = SECURITY_FTDC_TC_GFD;
		break;
	}

	// IOC��FOK
	switch (pOrder->TimeInForce)
	{
	case IOC:
		body.TimeCondition = SECURITY_FTDC_TC_IOC;
		body.VolumeCondition = SECURITY_FTDC_VC_AV;
		break;
	case FOK:
		body.TimeCondition = SECURITY_FTDC_TC_IOC;
		body.VolumeCondition = SECURITY_FTDC_VC_CV;
		//body.MinVolume = body.VolumeTotalOriginal; // ����ط��������
		break;
	default:
		body.VolumeCondition = SECURITY_FTDC_VC_AV;
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
		body.ContingentCondition = SECURITY_FTDC_CC_Immediately;
		break;
	default:
		body.ContingentCondition = SECURITY_FTDC_CC_Immediately;
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

		//�����浽���У�����ֱ�ӷ���
		int n = m_pApi->ReqOrderInsert(&body, ++m_lRequestID);
		if (n < 0)
		{
			nRet = n;
			sprintf(m_orderInsert_Id, "%d", nRet);
		}
		else
		{
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

void CTraderApi::OnRspOrderInsert(CSecurityFtdcInputOrderField *pInputOrder, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
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
		strcpy(pField->ID, orderId);
		pField->ExecType = ExecType::ExecRejected;
		pField->Status = OrderStatus::Rejected;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnErrRtnOrderInsert(CSecurityFtdcInputOrderField *pInputOrder, CSecurityFtdcRspInfoField *pRspInfo)
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
		// LTS�ڿ�ʼ����ʱҲ���յ��˻ر������ﰡ
	}
	else
	{
		// �ҵ��ˣ�Ҫ����״̬
		// ��ʹ���ϴε�״̬
		OrderField* pField = it->second;
		strcpy(pField->ID, orderId);
		pField->ExecType = ExecType::ExecRejected;
		pField->Status = OrderStatus::Rejected;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRtnTrade(CSecurityFtdcTradeField *pTrade)
{
	OnTrade(pTrade,false);
}

int CTraderApi::ReqOrderAction(OrderIDType* szIds, int count, OrderIDType* pOutput)
{
	unordered_map<string, CSecurityFtdcOrderField*>::iterator it = m_id_api_order.find(szIds[0]);
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

int CTraderApi::ReqOrderAction(CSecurityFtdcOrderField *pOrder, int count, OrderIDType* pOutput)
{
	if (nullptr == m_pApi)
		return 0;

	CSecurityFtdcInputOrderActionField body = {0};

	///���͹�˾����
	strncpy(body.BrokerID, pOrder->BrokerID,sizeof(TSecurityFtdcBrokerIDType));
	///Ͷ���ߴ���
	strncpy(body.InvestorID, pOrder->InvestorID,sizeof(TSecurityFtdcInvestorIDType));
	///��������
	strncpy(body.OrderRef, pOrder->OrderRef,sizeof(TSecurityFtdcOrderRefType));
	///ǰ�ñ��
	body.FrontID = pOrder->FrontID;
	///�Ự���
	body.SessionID = pOrder->SessionID;
	///����������
	strncpy(body.ExchangeID,pOrder->ExchangeID,sizeof(TSecurityFtdcExchangeIDType));
	///�������
	//strncpy(body.OrderSysID,pOrder->OrderSysID,sizeof(TSecurityFtdcOrderSysIDType));
	///������־
	body.ActionFlag = SECURITY_FTDC_AF_Delete;
	///��Լ����
	strncpy(body.InstrumentID, pOrder->InstrumentID,sizeof(TSecurityFtdcInstrumentIDType));

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

void CTraderApi::OnRspOrderAction(CSecurityFtdcInputOrderActionField *pInputOrderAction, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
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
		pField->ExecType = ExecType::ExecCancelReject;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnErrRtnOrderAction(CSecurityFtdcOrderActionField *pOrderAction, CSecurityFtdcRspInfoField *pRspInfo)
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
		pField->ExecType = ExecType::ExecCancelReject;
		pField->ErrorID = pRspInfo->ErrorID;
		strncpy(pField->Text, pRspInfo->ErrorMsg, sizeof(ErrorMsgType));
		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRtnOrder(CSecurityFtdcOrderField *pOrder)
{
	OnOrder(pOrder,false);
}

void CTraderApi::ReqQryTradingAccount()
{
	CSecurityFtdcQryTradingAccountField* pBody = (CSecurityFtdcQryTradingAccountField*)m_msgQueue_Query->new_block(sizeof(CSecurityFtdcQryTradingAccountField));

	strcpy(pBody->BrokerID, m_RspUserLogin.BrokerID);
	strcpy(pBody->InvestorID, m_RspUserLogin.UserID);

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryTradingAccountField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CSecurityFtdcQryTradingAccountField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryTradingAccount(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryTradingAccount((CSecurityFtdcQryTradingAccountField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryTradingAccount(CSecurityFtdcTradingAccountField *pTradingAccount, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pTradingAccount)
		{
			AccountField* pField = (AccountField*)m_msgQueue->new_block(sizeof(AccountField));

			pField->PreBalance = pTradingAccount->PreBalance;
			pField->CurrMargin = pTradingAccount->CurrMargin;
			//pField->CloseProfit = pTradingAccount->CloseProfit;
			//pField->PositionProfit = pTradingAccount->PositionProfit;
			pField->Balance = pTradingAccount->Balance;
			pField->Available = pTradingAccount->Available;
			pField->Deposit = pTradingAccount->Deposit;
			pField->Withdraw = pTradingAccount->Withdraw;
			pField->FrozenTransferFee = pTradingAccount->FrozenTransferFee;
			pField->FrozenStampTax = pTradingAccount->FrozenStampTax;
			pField->FrozenCommission = pTradingAccount->FrozenCommission;
			pField->FrozenCash = pTradingAccount->FrozenCash;
			pField->TransferFee = pTradingAccount->TransferFee;
			pField->StampTax = pTradingAccount->StampTax;
			pField->Commission = pTradingAccount->Commission;
			pField->CashIn = pTradingAccount->CashIn;

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
	CSecurityFtdcQryInvestorPositionField* pBody = (CSecurityFtdcQryInvestorPositionField*)m_msgQueue_Query->new_block(sizeof(CSecurityFtdcQryInvestorPositionField));

	strcpy(pBody->BrokerID, m_RspUserLogin.BrokerID);
	strcpy(pBody->InvestorID, m_RspUserLogin.UserID);
	strncpy(pBody->InstrumentID, szInstrumentId.c_str(), sizeof(TSecurityFtdcInstrumentIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryInvestorPositionField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CSecurityFtdcQryInvestorPositionField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryInvestorPosition(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryInvestorPosition((CSecurityFtdcQryInvestorPositionField*)ptr1, ++m_lRequestID);
}

// ��ծ��ع����ֲ���Net���ҹ��ൽ��Long�У������ֱ�Ӵӳɽ��з����������ֹ鵽��Net��
// ���п�ƽ��Ͷ�����Ͷ��ǿ�
void CTraderApi::OnRspQryInvestorPosition(CSecurityFtdcInvestorPositionField *pInvestorPosition, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pInvestorPosition)
		{
			PositionIDType positionId = { 0 };
			sprintf(positionId, "%s:%s:%d:%d",
				pInvestorPosition->InstrumentID, pInvestorPosition->ExchangeID,
				TSecurityFtdcPosiDirectionType_2_PositionSide(pInvestorPosition->PosiDirection), TSecurityFtdcHedgeFlagType_2_HedgeFlagType(pInvestorPosition->HedgeFlag));

			PositionField* pField = nullptr;
			unordered_map<string, PositionField*>::iterator it = m_id_platform_position.find(positionId);
			if (it == m_id_platform_position.end())
			{
				pField = (PositionField*)m_msgQueue->new_block(sizeof(PositionField));

				sprintf(pField->Symbol, "%s.%s", pInvestorPosition->InstrumentID, pInvestorPosition->ExchangeID);
				strcpy(pField->InstrumentID, pInvestorPosition->InstrumentID);
				strcpy(pField->ExchangeID, pInvestorPosition->ExchangeID);
				pField->Side = TSecurityFtdcPosiDirectionType_2_PositionSide(pInvestorPosition->PosiDirection);
				pField->HedgeFlag = TSecurityFtdcHedgeFlagType_2_HedgeFlagType(pInvestorPosition->HedgeFlag);

				m_id_platform_position.insert(pair<string, PositionField*>(positionId, pField));
			}
			else
			{
				pField = it->second;
			}

			pField->Position = pInvestorPosition->Position;
			pField->TdPosition = pInvestorPosition->TodayPosition;
			pField->YdPosition = pInvestorPosition->YdPosition;

			// �������ռ�ȫ���ٱ���֪ͨһ��
			if (bIsLast)
			{
				int cnt = 0;
				int count = m_id_platform_position.size();
				for (unordered_map<string, PositionField*>::iterator iter = m_id_platform_position.begin(); iter != m_id_platform_position.end(); iter++)
				{
					++cnt;
					m_msgQueue->Input_Copy(ResponeType::OnRspQryInvestorPosition, m_msgQueue, m_pClass, cnt == count, 0, iter->second, sizeof(PositionField), nullptr, 0, nullptr, 0);
				}
			}
		}
	}
}

void CTraderApi::ReqQryInstrument(const string& szInstrumentId, const string& szExchange)
{
	CSecurityFtdcQryInstrumentField* pBody = (CSecurityFtdcQryInstrumentField*)m_msgQueue_Query->new_block(sizeof(CSecurityFtdcQryInstrumentField));

	strncpy(pBody->InstrumentID, szInstrumentId.c_str(), sizeof(TSecurityFtdcInstrumentIDType));
	strncpy(pBody->ExchangeID, szExchange.c_str(), sizeof(TSecurityFtdcExchangeIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryInstrumentField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CSecurityFtdcQryInstrumentField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryInstrument(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryInstrument((CSecurityFtdcQryInstrumentField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryInstrument(CSecurityFtdcInstrumentField *pInstrument, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pInstrument)
		{
			InstrumentField* pField = (InstrumentField*)m_msgQueue->new_block(sizeof(InstrumentField));

			strncpy(pField->InstrumentID, pInstrument->InstrumentID, sizeof(InstrumentIDType));
			strncpy(pField->ExchangeID, pInstrument->ExchangeID, sizeof(ExchangeIDType));

			sprintf(pField->Symbol,"%s.%s",pInstrument->InstrumentID,pInstrument->ExchangeID);
			sprintf(pField->InstrumentName, "%s(%s)", pInstrument->ExchangeInstID, pInstrument->InstrumentName);
			strncpy(pField->ProductID, pInstrument->ProductID, sizeof(InstrumentIDType));

			//strncpy(pField->InstrumentName, pInstrument->InstrumentName, sizeof(InstrumentNameType));
			pField->Type = CSecurityFtdcInstrumentField_2_InstrumentType(pInstrument);
			pField->VolumeMultiple = pInstrument->VolumeMultiple;
			pField->PriceTick = CSecurityFtdcInstrumentField_2_PriceTick(pInstrument);
			pField->ExpireDate = GetDate(pInstrument->ExpireDate);
			pField->OptionsType = CSecurityFtdcInstrumentField_2_PutCall(pInstrument);
			pField->StrikePrice = pInstrument->ExecPrice;


			// ��Ȩ�ı����
			if (strlen(pInstrument->InstrumentID) == 8)
			{
				strncpy(pField->UnderlyingInstrID, pInstrument->ExchangeInstID, 6);
				sprintf(&pField->UnderlyingInstrID[6], ".%s", pInstrument->ExchangeID);
			}


			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInstrument, m_msgQueue, m_pClass, bIsLast, 0, pField, sizeof(InstrumentField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInstrument, m_msgQueue, m_pClass, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}
//
//void CTraderApi::ReqQryInstrumentCommissionRate(const string& szInstrumentId)
//{
//	if (nullptr == m_pApi)
//		return;
//
//	SRequest* pRequest = MakeRequestBuf(E_QryInstrumentCommissionRateField);
//	if (nullptr == pRequest)
//		return;
//
//	CSecurityFtdcQryInstrumentCommissionRateField& body = pRequest->QryInstrumentCommissionRateField;
//
//	strncpy(body.BrokerID, m_RspUserLogin.BrokerID,sizeof(TSecurityFtdcBrokerIDType));
//	strncpy(body.InvestorID, m_RspUserLogin.UserID,sizeof(TSecurityFtdcInvestorIDType));
//	strncpy(body.InstrumentID,szInstrumentId.c_str(),sizeof(TSecurityFtdcInstrumentIDType));
//
//	AddToSendQueue(pRequest);
//}
//
//void CTraderApi::OnRspQryInstrumentCommissionRate(CSecurityFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//	//if(m_msgQueue)
//	//	m_msgQueue->Input_OnRspQryInstrumentCommissionRate(this,pInstrumentCommissionRate,pRspInfo,nRequestID,bIsLast);
//
//	if (bIsLast)
//		ReleaseRequestMapBuf(nRequestID);
//}

void CTraderApi::OnRspError(CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	IsErrorRspInfo(pRspInfo, nRequestID, bIsLast);
}

void CTraderApi::ReqQryOrder()
{
	CSecurityFtdcQryOrderField* pBody = (CSecurityFtdcQryOrderField*)m_msgQueue_Query->new_block(sizeof(CSecurityFtdcQryOrderField));

	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID, sizeof(TSecurityFtdcBrokerIDType));
	strncpy(pBody->InvestorID, m_RspUserLogin.UserID, sizeof(TSecurityFtdcInvestorIDType));
	
	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryOrderField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CSecurityFtdcQryOrderField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryOrder(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryOrder((CSecurityFtdcQryOrderField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnOrder(CSecurityFtdcOrderField *pOrder, bool bFromQry)
{
	if (nullptr == pOrder)
		return;

	OrderIDType orderId = { 0 };
	sprintf(orderId, "%d:%d:%s", pOrder->FrontID, pOrder->SessionID, pOrder->OrderRef);
	OrderIDType orderSydId = { 0 };

	{
		// ����ԭʼ������Ϣ�����ڳ���

		unordered_map<string, CSecurityFtdcOrderField*>::iterator it = m_id_api_order.find(orderId);
		if (it == m_id_api_order.end())
		{
			// �Ҳ����˶�������ʾ���µ�
			CSecurityFtdcOrderField* pField = new CSecurityFtdcOrderField();
			memcpy(pField, pOrder, sizeof(CSecurityFtdcOrderField));
			m_id_api_order.insert(pair<string, CSecurityFtdcOrderField*>(orderId, pField));
		}
		else
		{
			// �ҵ��˶���
			// ��Ҫ�ٸ��Ʊ������һ�ε�״̬������ֻҪ��һ�ε����ڳ������ɣ����£��������ñȽ�
			CSecurityFtdcOrderField* pField = it->second;
			memcpy(pField, pOrder, sizeof(CSecurityFtdcOrderField));
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
			pField->HedgeFlag = TSecurityFtdcHedgeFlagType_2_HedgeFlagType(pOrder->CombHedgeFlag[0]);
			pField->Side = TSecurityFtdcDirectionType_2_OrderSide(pOrder->Direction);
			pField->Price = atof(pOrder->LimitPrice);
			pField->StopPx = pOrder->StopPrice;
			strncpy(pField->Text, pOrder->StatusMsg, sizeof(ErrorMsgType));
			pField->OpenClose = TSecurityFtdcOffsetFlagType_2_OpenCloseType(pOrder->CombOffsetFlag[0]);
			pField->Status = CSecurityFtdcOrderField_2_OrderStatus(pOrder);
			pField->Qty = pOrder->VolumeTotalOriginal;
			pField->Type = CSecurityFtdcOrderField_2_OrderType(pOrder);
			pField->TimeInForce = CSecurityFtdcOrderField_2_TimeInForce(pOrder);
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
			pField->Price = atof(pOrder->LimitPrice);
			pField->Status = CSecurityFtdcOrderField_2_OrderStatus(pOrder);
			pField->ExecType = CSecurityFtdcOrderField_2_ExecType(pOrder);
			strcpy(pField->OrderID, pOrder->OrderSysID);
			strncpy(pField->Text, pOrder->StatusMsg, sizeof(ErrorMsgType));
		}

		m_msgQueue->Input_Copy(ResponeType::OnRtnOrder, m_msgQueue, m_pClass, 0, 0, pField, sizeof(OrderField), nullptr, 0, nullptr, 0);
	}
}

void CTraderApi::OnRspQryOrder(CSecurityFtdcOrderField *pOrder, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		OnOrder(pOrder, true);
	}
}

void CTraderApi::ReqQryTrade()
{
	CSecurityFtdcQryTradeField* pBody = (CSecurityFtdcQryTradeField*)m_msgQueue_Query->new_block(sizeof(CSecurityFtdcQryTradeField));

	strcpy(pBody->BrokerID, m_RspUserLogin.BrokerID);
	strcpy(pBody->InvestorID, m_RspUserLogin.UserID);

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryTradeField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CSecurityFtdcQryTradeField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryTrade(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryTrade((CSecurityFtdcQryTradeField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnTrade(CSecurityFtdcTradeField *pTrade, bool bFromQry)
{
	if (nullptr == pTrade)
		return;

	TradeField* pField = (TradeField*)m_msgQueue->new_block(sizeof(TradeField));

	strcpy(pField->InstrumentID, pTrade->InstrumentID);
	strcpy(pField->ExchangeID, pTrade->ExchangeID);
	pField->Side = TSecurityFtdcDirectionType_2_OrderSide(pTrade->Direction);
	pField->Qty = pTrade->Volume;
	pField->Price = atof(pTrade->Price);
	pField->OpenClose = TSecurityFtdcOffsetFlagType_2_OpenCloseType(pTrade->OffsetFlag);
	pField->HedgeFlag = TSecurityFtdcHedgeFlagType_2_HedgeFlagType(pTrade->HedgeFlag);
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

		OnTrade(pField, bFromQry);
	}
}

void CTraderApi::OnTrade(TradeField *pTrade, bool bFromQry)
{
	PositionIDType positionId = { 0 };
	sprintf(positionId, "%s:%s:%d:%d",
		pTrade->InstrumentID, pTrade->ExchangeID, TradeField_2_PositionSide(pTrade), pTrade->HedgeFlag);

	PositionField* pField = nullptr;
	unordered_map<string, PositionField*>::iterator it = m_id_platform_position.find(positionId);
	if (it == m_id_platform_position.end())
	{
		pField = (PositionField*)m_msgQueue->new_block(sizeof(PositionField));

		sprintf(pField->Symbol, "%s.%s", pTrade->InstrumentID, pTrade->ExchangeID);
		strcpy(pField->InstrumentID, pTrade->InstrumentID);
		strcpy(pField->ExchangeID, pTrade->ExchangeID);
		pField->Side = TradeField_2_PositionSide(pTrade);
		pField->HedgeFlag = TSecurityFtdcHedgeFlagType_2_HedgeFlagType(pTrade->HedgeFlag);

		m_id_platform_position.insert(pair<string, PositionField*>(positionId, pField));
	}
	else
	{
		pField = it->second;
	}

	if (pTrade->Side == OrderSide::Buy)
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

void CTraderApi::OnRspQryTrade(CSecurityFtdcTradeField *pTrade, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		OnTrade(pTrade,true);
	}
}

void CTraderApi::ReqQryInvestor()
{
	CSecurityFtdcQryInvestorField* pBody = (CSecurityFtdcQryInvestorField*)m_msgQueue_Query->new_block(sizeof(CSecurityFtdcQryInvestorField));

	strncpy(pBody->BrokerID, m_RspUserLogin.BrokerID, sizeof(TSecurityFtdcBrokerIDType));
	strncpy(pBody->InvestorID, m_RspUserLogin.UserID, sizeof(TSecurityFtdcInvestorIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_QryInvestorField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(CSecurityFtdcQryInvestorField), nullptr, 0, nullptr, 0);
}

int CTraderApi::_ReqQryInvestor(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	return m_pApi->ReqQryInvestor((CSecurityFtdcQryInvestorField*)ptr1, ++m_lRequestID);
}

void CTraderApi::OnRspQryInvestor(CSecurityFtdcInvestorField *pInvestor, CSecurityFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast))
	{
		if (pInvestor)
		{
			memcpy(&m_Investor, pInvestor, sizeof(CSecurityFtdcInvestorField));

			InvestorField* pField = (InvestorField*)m_msgQueue->new_block(sizeof(InvestorField));
			
			strcpy(pField->BrokerID, pInvestor->BrokerID);
			strcpy(pField->InvestorID, pInvestor->InvestorID);
			strcpy(pField->InvestorName, pInvestor->InvestorName);
			strcpy(pField->IdentifiedCardNo, pInvestor->IdentifiedCardNo);
			pField->IdentifiedCardType = TSecurityFtdcIdCardTypeType_2_IdCardType(pInvestor->IdentifiedCardType);

			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInvestor, m_msgQueue, m_pClass, bIsLast, 0, pField, sizeof(InvestorField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInvestor, m_msgQueue, m_pClass, bIsLast, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}
