#include "stdafx.h"
#include "MdUserApi.h"
#include "../include/QueueEnum.h"
#include "../include/QueueHeader.h"

#include "../include/ApiHeader.h"
#include "../include/ApiStruct.h"

#include "../include/toolkit.h"
#include "../include/ApiProcess.h"
#include "../QuantBox_XSpeedStock_Trade/TypeConvert.h"

#include "../QuantBox_Queue/MsgQueue.h"

#include <string.h>
#include <cfloat>

#include <mutex>
#include <vector>
#include <assert.h>

using namespace std;

char* ExchangeID_2to3(char* exchange)
{
	switch (exchange[1])
	{
	case 'H':
		return "SSE";
	case 'Z':
		return "SZE";
	case 'K':
		return "HK";
	default:
		break;
	}
	return "SSE";
}

void* __stdcall Query(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	// ���ڲ����ã����ü���Ƿ�Ϊ��
	CMdUserApi* pApi = (CMdUserApi*)pApi2;
	pApi->QueryInThread(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
	return nullptr;
}

void CMdUserApi::QueryInThread(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	int iRet = 0;
	switch (type)
	{
	case E_Init:
		iRet = _Init();
		break;
	case E_StockUserLoginField:
		iRet = _ReqStockUserLogin(type, pApi1, pApi2, double1, double2, ptr1, size1, ptr2, size2, ptr3, size3);
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

void CMdUserApi::Register(void* pCallback, void* pClass)
{
	m_pClass = pClass;
	if (m_msgQueue == nullptr)
		return;

	m_msgQueue_Query->Register(Query,this);
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

CMdUserApi::CMdUserApi(void)
{
	m_pApi = nullptr;
	m_lRequestID = 0;
	m_nSleep = 1;

	// �Լ�ά��������Ϣ����
	m_msgQueue = new CMsgQueue();
	m_msgQueue_Query = new CMsgQueue();

	m_msgQueue_Query->Register(Query,this);
	m_msgQueue_Query->StartThread();
}

CMdUserApi::~CMdUserApi(void)
{
	Disconnect();
}


ConfigInfoField* CMdUserApi::Config(ConfigInfoField* pConfigInfo)
{
	return nullptr;
}

bool CMdUserApi::IsErrorRspInfo_Output(struct DFITCSECRspInfoField *pRspInfo)
{
	bool bRet = ((pRspInfo) && (pRspInfo->errorID != 0));
	
	if (bRet)
	{
		ErrorField* pField = (ErrorField*)m_msgQueue->new_block(sizeof(ErrorField));

		pField->ErrorID = pRspInfo->errorID;
		strcpy(pField->ErrorMsg, pRspInfo->errorMsg);

		m_msgQueue->Input_NoCopy(ResponeType::OnRtnError, m_msgQueue, m_pClass, true, 0, pField, sizeof(ErrorField), nullptr, 0, nullptr, 0);
	}
	return bRet;
}

bool CMdUserApi::IsErrorRspInfo(struct DFITCSECRspInfoField *pRspInfo)
{
	bool bRet = ((pRspInfo) && (pRspInfo->errorID != 0));

	return bRet;
}

void CMdUserApi::Connect(const string& szPath,
	ServerInfoField* pServerInfo,
	UserInfoField* pUserInfo)
{
	m_szPath = szPath;
	memcpy(&m_ServerInfo, pServerInfo, sizeof(ServerInfoField));
	memcpy(&m_UserInfo, pUserInfo, sizeof(UserInfoField));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_Init, m_msgQueue_Query, this, 0, 0,
		nullptr, 0, nullptr, 0, nullptr, 0);
}

int CMdUserApi::_Init()
{
	m_pApi = DFITCSECMdApi::CreateDFITCMdApi();

	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Initialized, 0, nullptr, 0, nullptr, 0, nullptr, 0);

	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Connecting, 0, nullptr, 0, nullptr, 0, nullptr, 0);
	//��ʼ������
	int iRet = m_pApi->Init(m_ServerInfo.Address, this);
	if (0 == iRet)
	{
	}
	else
	{
		RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

		pField->ErrorID = iRet;
		strcpy(pField->ErrorMsg, "���ӳ�ʱ");

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
	}
	return iRet;
}

void CMdUserApi::ReqStockUserLogin()
{
	DFITCSECReqUserLoginField* pBody = (DFITCSECReqUserLoginField*)m_msgQueue_Query->new_block(sizeof(DFITCSECReqUserLoginField));

	strncpy(pBody->accountID, m_UserInfo.UserID, sizeof(DFITCSECAccountIDType));
	strncpy(pBody->passWord, m_UserInfo.Password, sizeof(DFITCSECPasswordType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_StockUserLoginField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(DFITCSECReqUserLoginField), nullptr, 0, nullptr, 0);
}

int CMdUserApi::_ReqStockUserLogin(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Logining, 0, nullptr, 0, nullptr, 0, nullptr, 0);

	DFITCSECReqUserLoginField* pBody = (DFITCSECReqUserLoginField*)ptr1;
	pBody->requestID = ++m_lRequestID;
	return m_pApi->ReqStockUserLogin(pBody);
}

void CMdUserApi::Disconnect()
{
	// �����ѯ����
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
		//m_pApi->RegisterSpi(NULL);
		m_pApi->Release();
		m_pApi = NULL;

		// ȫ����ֻ�����һ��
		m_msgQueue->Clear();
		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		// ��������
		m_msgQueue->Process();
	}

	// ������Ӧ����
	if (m_msgQueue)
	{
		m_msgQueue->StopThread();
		m_msgQueue->Register(nullptr,nullptr);
		m_msgQueue->Clear();
		delete m_msgQueue;
		m_msgQueue = nullptr;
	}
}


void CMdUserApi::Subscribe(const string& szInstrumentIDs, const string& szExchageID)
{
	if(NULL == m_pApi)
		return;

	vector<char*> vct;
	set<char*> st;

	lock_guard<mutex> cl(m_csMapInstrumentIDs);

	set<string> _setInstrumentIDs;
	map<string, set<string> >::iterator it = m_mapInstrumentIDs.find(szExchageID);
	if (it != m_mapInstrumentIDs.end())
	{
		_setInstrumentIDs = it->second;
	}
	
	// �������ڲ���ʽ
	set<string> _setInstrumentIDs2;
	
	char* pBuf2 = GetSetFromString(szInstrumentIDs.c_str(), _QUANTBOX_SEPS_, vct, st, 1, _setInstrumentIDs2, 1, szExchageID.c_str());
	delete[] pBuf2;

	string _szInstrumentIDs;
	for (set<string>::iterator i = _setInstrumentIDs2.begin(); i != _setInstrumentIDs2.end(); ++i)
	{
		_szInstrumentIDs.append(*i);
		_szInstrumentIDs.append(";");
	}


	char* pBuf = GetSetFromString(_szInstrumentIDs.c_str(), _QUANTBOX_SEPS_, vct, st, 1, _setInstrumentIDs);
	m_mapInstrumentIDs[szExchageID] = _setInstrumentIDs;

	if (vct.size()>0)
	{
		//ת���ַ�������
		char** pArray = new char*[vct.size()];
		for (size_t j = 0; j<vct.size(); ++j)
		{
			pArray[j] = vct[j];
		}

		//����
		m_pApi->SubscribeStockMarketData(pArray, (int)vct.size(), ++m_lRequestID);

		delete[] pArray;
	}
	delete[] pBuf;
}

void CMdUserApi::Subscribe(const set<string>& instrumentIDs, const string& szExchageID)
{
	if(NULL == m_pApi)
		return;

	string szInstrumentIDs;
	for(set<string>::iterator i=instrumentIDs.begin();i!=instrumentIDs.end();++i)
	{
		szInstrumentIDs.append(*i);
		szInstrumentIDs.append(";");
	}

	if (szInstrumentIDs.length()>1)
	{
		Subscribe(szInstrumentIDs, szExchageID);
	}
}

void CMdUserApi::Unsubscribe(const string& szInstrumentIDs, const string& szExchageID)
{
	if(NULL == m_pApi)
		return;

	vector<char*> vct;
	set<char*> st;

	lock_guard<mutex> cl(m_csMapInstrumentIDs);

	set<string> _setInstrumentIDs;
	map<string, set<string> >::iterator it = m_mapInstrumentIDs.find(szExchageID);
	if (it != m_mapInstrumentIDs.end())
	{
		_setInstrumentIDs = it->second;
	}

	// �������ڲ���ʽ
	set<string> _setInstrumentIDs2;

	char* pBuf2 = GetSetFromString(szInstrumentIDs.c_str(), _QUANTBOX_SEPS_, vct, st, 1, _setInstrumentIDs2, 1, szExchageID.c_str());
	delete[] pBuf2;

	string _szInstrumentIDs;
	for (set<string>::iterator i = _setInstrumentIDs2.begin(); i != _setInstrumentIDs2.end(); ++i)
	{
		_szInstrumentIDs.append(*i);
		_szInstrumentIDs.append(";");
	}

	char* pBuf = GetSetFromString(_szInstrumentIDs.c_str(), _QUANTBOX_SEPS_, vct, st, -1, _setInstrumentIDs);
	m_mapInstrumentIDs[szExchageID] = _setInstrumentIDs;

	if (vct.size()>0)
	{
		//ת���ַ�������
		char** pArray = new char*[vct.size()];
		for (size_t j = 0; j<vct.size(); ++j)
		{
			pArray[j] = vct[j];
		}

		//����
		m_pApi->UnSubscribeStockMarketData(pArray, (int)vct.size(), ++m_lRequestID);

		delete[] pArray;
	}
	delete[] pBuf;
}

void CMdUserApi::OnFrontConnected()
{
	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Connected, 0, nullptr, 0, nullptr, 0, nullptr, 0);

	//���ӳɹ����Զ������¼
	ReqStockUserLogin();
	//ReqSOPUserLogin();
	//ReqFASLUserLogin();
}

void CMdUserApi::OnFrontDisconnected(int nReason)
{
	RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

	//����ʧ�ܷ��ص���Ϣ��ƴ�Ӷ��ɣ���Ҫ��Ϊ��ͳһ���
	pField->ErrorID = nReason;
	GetOnFrontDisconnectedMsg(nReason, pField->ErrorMsg);

	m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
}

void CMdUserApi::OnRspStockUserLogin(struct DFITCSECRspUserLoginField * pRspUserLogin, struct DFITCSECRspInfoField * pRspInfo)
{
	RspUserLoginField* pField = (RspUserLoginField*)m_msgQueue->new_block(sizeof(RspUserLoginField));

	if (!IsErrorRspInfo(pRspInfo)
		&&pRspUserLogin)
	{
		pField->TradingDay = pRspUserLogin->tradingDay;
		pField->LoginTime = GetTime(pRspUserLogin->loginTime);

		sprintf(pField->SessionID, "%d:%d", pRspUserLogin->frontID, pRspUserLogin->sessionID);

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Logined, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Done, 0, nullptr, 0, nullptr, 0, nullptr, 0);

		//�п��ܶ����ˣ������Ƕ������������¶���
		map<string, set<string> > mapOld = m_mapInstrumentIDs;//�����ϴζ��ĵĺ�Լ

		for (map<string, set<string> >::iterator i = mapOld.begin(); i != mapOld.end(); ++i)
		{
			string strkey = i->first;
			set<string> setValue = i->second;

			Subscribe(setValue, strkey);//����
		}

		////�п��ܶ����ˣ������Ƕ������������¶���
		//mapOld = m_setQuoteInstrumentIDs;//�����ϴζ��ĵĺ�Լ
		//SubscribeQuote(mapOld, "");//����
	}
	else
	{
		pField->ErrorID = pRspInfo->errorID;
		strncpy(pField->ErrorMsg, pRspInfo->errorMsg, sizeof(ErrorMsgType));

		m_msgQueue->Input_NoCopy(ResponeType::OnConnectionStatus, m_msgQueue, m_pClass, ConnectionStatus::Disconnected, 0, pField, sizeof(RspUserLoginField), nullptr, 0, nullptr, 0);
	}
}

void CMdUserApi::OnRspError(struct DFITCSECRspInfoField *pRspInfo)
{
	IsErrorRspInfo_Output(pRspInfo);
}

void CMdUserApi::OnRspStockSubMarketData(struct DFITCSECSpecificInstrumentField * pSpecificInstrument, struct DFITCSECRspInfoField * pRspInfo)
{
	//��ģ��ƽ̨��������������ᴥ��������Ҫ�Լ�ά��һ���Ѿ����ĵĺ�Լ�б�
	if (!IsErrorRspInfo_Output(pRspInfo)
		&& pSpecificInstrument)
	{
		lock_guard<mutex> cl(m_csMapInstrumentIDs);

		set<string> _setInstrumentIDs;
		map<string, set<string> >::iterator it = m_mapInstrumentIDs.find(pSpecificInstrument->exchangeID);
		if (it != m_mapInstrumentIDs.end())
		{
			_setInstrumentIDs = it->second;
		}

		_setInstrumentIDs.insert(pSpecificInstrument->securityID);
		m_mapInstrumentIDs[pSpecificInstrument->exchangeID] = _setInstrumentIDs;
	}
}

void CMdUserApi::OnRspStockUnSubMarketData(struct DFITCSECSpecificInstrumentField * pSpecificInstrument, struct DFITCSECRspInfoField * pRspInfo)
{
	//ģ��ƽ̨��������������ᴥ��
	if (!IsErrorRspInfo_Output(pRspInfo)
		&& pSpecificInstrument)
	{
		lock_guard<mutex> cl(m_csMapInstrumentIDs);

		set<string> _setInstrumentIDs;
		map<string, set<string> >::iterator it = m_mapInstrumentIDs.find(pSpecificInstrument->exchangeID);
		if (it != m_mapInstrumentIDs.end())
		{
			_setInstrumentIDs = it->second;
		}
		_setInstrumentIDs.erase(pSpecificInstrument->securityID);
		m_mapInstrumentIDs[pSpecificInstrument->exchangeID] = _setInstrumentIDs;
	}
}

//����ص����ñ�֤�˺������췵��
void CMdUserApi::OnStockMarketData(struct DFITCStockDepthMarketDataField *pMarketDataField)
{
	DepthMarketDataNField* pField = (DepthMarketDataNField*)m_msgQueue->new_block(sizeof(DepthMarketDataNField)+sizeof(DepthField)* 10);


	strcpy(pField->InstrumentID, pMarketDataField->staticDataField.securityID);
	pField->Exchange = DFITCSECExchangeIDType_2_ExchangeType(pMarketDataField->staticDataField.exchangeID);

	sprintf(pField->Symbol, "%s.%s", pField->InstrumentID, pMarketDataField->staticDataField.exchangeID);

	pField->TradingDay = pMarketDataField->staticDataField.tradingDay;
	pField->ActionDay = pMarketDataField->staticDataField.tradingDay;
	GetUpdateTime(pMarketDataField->sharedDataField.updateTime, &pField->UpdateTime, &pField->UpdateMillisec);

	pField->LastPrice = pMarketDataField->sharedDataField.latestPrice;
	pField->Volume = pMarketDataField->sharedDataField.tradeQty;
	pField->Turnover = pMarketDataField->sharedDataField.turnover;
	//marketData.OpenInterest = 0;
	//marketData.AveragePrice = 0;

	pField->OpenPrice = pMarketDataField->staticDataField.openPrice;
	//marketData.HighestPrice = 0;
	//marketData.LowestPrice = 0;
	//marketData.ClosePrice = 0;
	//marketData.SettlementPrice = 0;

	pField->UpperLimitPrice = pMarketDataField->staticDataField.upperLimitPrice;
	pField->LowerLimitPrice = pMarketDataField->staticDataField.lowerLimitPrice;
	pField->PreClosePrice = pMarketDataField->staticDataField.preClosePrice;
	//marketData.PreSettlementPrice = pMarketDataField->preSettlementPrice;
	//marketData.PreOpenInterest = pMarketDataField->preOpenInterest;

	InitBidAsk(pField);

	do
	{
		if (pMarketDataField->sharedDataField.bidQty1 == 0)
			break;
		AddBid(pField, pMarketDataField->sharedDataField.bidPrice1, pMarketDataField->sharedDataField.bidQty1, 0);

		if (pMarketDataField->sharedDataField.bidQty2 == 0)
			break;
		AddBid(pField, pMarketDataField->sharedDataField.bidPrice2, pMarketDataField->sharedDataField.bidQty2, 0);

		if (pMarketDataField->sharedDataField.bidQty3 == 0)
			break;
		AddBid(pField, pMarketDataField->sharedDataField.bidPrice3, pMarketDataField->sharedDataField.bidQty3, 0);

		if (pMarketDataField->sharedDataField.bidQty4 == 0)
			break;
		AddBid(pField, pMarketDataField->sharedDataField.bidPrice4, pMarketDataField->sharedDataField.bidQty4, 0);

		if (pMarketDataField->sharedDataField.bidQty5 == 0)
			break;
		AddBid(pField, pMarketDataField->sharedDataField.bidPrice5, pMarketDataField->sharedDataField.bidQty5, 0);
	} while (false);

	do
	{
		if (pMarketDataField->sharedDataField.askQty1 == 0)
			break;
		AddAsk(pField, pMarketDataField->sharedDataField.askPrice1, pMarketDataField->sharedDataField.askQty1, 0);

		if (pMarketDataField->sharedDataField.askQty2 == 0)
			break;
		AddAsk(pField, pMarketDataField->sharedDataField.askPrice2, pMarketDataField->sharedDataField.askQty2, 0);

		if (pMarketDataField->sharedDataField.askQty3 == 0)
			break;
		AddAsk(pField, pMarketDataField->sharedDataField.askPrice3, pMarketDataField->sharedDataField.askQty3, 0);

		if (pMarketDataField->sharedDataField.askQty4 == 0)
			break;
		AddAsk(pField, pMarketDataField->sharedDataField.askPrice4, pMarketDataField->sharedDataField.askQty4, 0);

		if (pMarketDataField->sharedDataField.askQty5 == 0)
			break;
		AddAsk(pField, pMarketDataField->sharedDataField.askPrice5, pMarketDataField->sharedDataField.askQty5, 0);
	} while (false);

	m_msgQueue->Input_NoCopy(ResponeType::OnRtnDepthMarketData, m_msgQueue, m_pClass, DepthLevelType::FULL, 0, pField, pField->Size, nullptr, 0, nullptr, 0);
}

//void CMdUserApi::OnRspSubForQuoteRsp(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast)
//		&& pSpecificInstrument)
//	{
//		lock_guard<mutex> cl(m_csMapQuoteInstrumentIDs);
//
//		m_setQuoteInstrumentIDs.insert(pSpecificInstrument->InstrumentID);
//	}
//}
//
//void CMdUserApi::OnRspUnSubForQuoteRsp(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
//{
//	if (!IsErrorRspInfo(pRspInfo, nRequestID, bIsLast)
//		&& pSpecificInstrument)
//	{
//		lock_guard<mutex> cl(m_csMapQuoteInstrumentIDs);
//
//		m_setQuoteInstrumentIDs.erase(pSpecificInstrument->InstrumentID);
//	}
//}
//
//void CMdUserApi::OnRtnForQuoteRsp(CThostFtdcForQuoteRspField *pForQuoteRsp)
//{
//	//if (m_msgQueue)
//	//	m_msgQueue->Input_OnRtnForQuoteRsp(this, pForQuoteRsp);
//
//
//	//XCall(m_msgQueue, ResponeType::OnConnectionStatus, 0, 0, 0, this, &field, nullptr);
//}

void CMdUserApi::ReqQryInstrument(const string& szInstrumentId, const string& szExchange)
{
	ReqStockAvailableQuotQry(szInstrumentId, "SH");
	ReqStockAvailableQuotQry(szInstrumentId, "SZ");
	ReqStockAvailableQuotQry(szInstrumentId, "HK");
}

void CMdUserApi::ReqStockAvailableQuotQry(const string& szInstrumentId, const string& szExchange)
{
	DFITCReqQuotQryField* pBody = (DFITCReqQuotQryField*)m_msgQueue_Query->new_block(sizeof(DFITCReqQuotQryField));

	strcpy(pBody->accountID, m_UserInfo.UserID);
	strncpy(pBody->exchangeID, szExchange.c_str(), sizeof(DFITCSECExchangeIDType));

	m_msgQueue_Query->Input_NoCopy(RequestType::E_ReqSOPQuotQryField, m_msgQueue_Query, this, 0, 0,
		pBody, sizeof(DFITCReqQuotQryField), nullptr, 0, nullptr, 0);
}

int CMdUserApi::_ReqStockAvailableQuotQry(char type, void* pApi1, void* pApi2, double double1, double double2, void* ptr1, int size1, void* ptr2, int size2, void* ptr3, int size3)
{
	DFITCReqQuotQryField* pBody = (DFITCReqQuotQryField*)ptr1;
	pBody->requestID = ++m_lRequestID;
	return m_pApi->ReqStockAvailableQuotQry(pBody);
}

void CMdUserApi::OnRspStockAvailableQuot(struct DFITCRspQuotQryField * pAvailableQuotInfo, struct DFITCSECRspInfoField * pRspInfo, bool flag)
{
	if (!IsErrorRspInfo(pRspInfo))
	{
		if (pAvailableQuotInfo)
		{
			InstrumentField* pField = (InstrumentField*)m_msgQueue->new_block(sizeof(InstrumentField));

			strncpy(pField->InstrumentID, pAvailableQuotInfo->securityID, sizeof(InstrumentIDType));
			strncpy(pField->ExchangeID, pAvailableQuotInfo->exchangeID, sizeof(InstrumentIDType));

			sprintf(pField->Symbol, "%s.%s", pField->InstrumentID, ExchangeID_2to3(pField->ExchangeID));
			strncpy(pField->InstrumentName, pAvailableQuotInfo->securityName, sizeof(InstrumentNameType));

			pField->Type = InstrumentType::Stock;
			//pField->VolumeMultiple = pInstrumentData->contractMultiplier;
			//pField->PriceTick = pInstrumentData->minPriceFluctuation;
			//strncpy(pField->ExpireDate, pInstrumentData->instrumentMaturity, sizeof(DFITCInstrumentMaturityType));
			//pField->OptionsType = TThostFtdcOptionsTypeType_2_PutCall(pInstrument->OptionsType);

			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInstrument, m_msgQueue, m_pClass, flag, 0, pField, sizeof(InstrumentField), nullptr, 0, nullptr, 0);
		}
		else
		{
			m_msgQueue->Input_NoCopy(ResponeType::OnRspQryInstrument, m_msgQueue, m_pClass, flag, 0, nullptr, 0, nullptr, 0, nullptr, 0);
		}
	}
}
