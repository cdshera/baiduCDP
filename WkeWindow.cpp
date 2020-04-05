#include "WkeWindow.h"
#include <shlwapi.h>
#include <WinInet.h>
#include <io.h>
#include <fstream>
#include <stdexcept>
#include <boost/format.hpp>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "CEncryption.h"
#include "resource.h"
#pragma comment(lib,"Wininet.lib")
#pragma comment(lib,"shlwapi.lib")
#pragma warning(disable:4996)
std::mutex g_mutx;
std::shared_ptr<CWkeWindow> CWkeWindow::Instance = nullptr;
WNDPROC CWkeWindow::m_oldProc = NULL;

void* CWkeWindow::AlloclocalHeap(const std::string& strBuffer)
{ 
	char* pResutPtr = nullptr;
	if (strBuffer.empty())
		return pResutPtr;
	int nLen = strBuffer.length() + 1;
	pResutPtr = new char[nLen];
	if (!pResutPtr)
		return pResutPtr;
	ZeroMemory(pResutPtr, nLen);
	CopyMemory(pResutPtr, strBuffer.c_str(),strBuffer.length());
	return pResutPtr;
}

#if 1
void CWkeWindow::ParseAria2JsonInfo(const std::string& strJSon)
{
	rapidjson::Document dc;
	dc.Parse(strJSon.c_str());
	if (!dc.IsObject())
		return;
	std::string strDownStatus;
	std::string strGidItem;
	if (dc.HasMember("method") && dc["method"].IsString())
	{
		strDownStatus = dc["method"].GetString();
		if (dc.HasMember("params") && dc["params"].IsArray())
		{
			for (auto &v : dc["params"].GetArray())
			{
				if (!v.IsObject())
					continue;
				if (v.HasMember("gid") && v["gid"].IsString())
					strGidItem = v["gid"].GetString();
			}
		}
		if (strDownStatus == ARIA2_DOWNLOAD_COMPLATE)
		{
			PVOID sendPtr = AlloclocalHeap(strGidItem);
			::PostMessage(m_hwnd, ARIA2_DOWNLOAD_COMPLATE_MSG, NULL, (LPARAM)sendPtr);
		}
		else if (strDownStatus == ARIA2_DOWNLOAD_START)
		{
			PVOID sendPtr = AlloclocalHeap(strGidItem);
			::PostMessage(m_hwnd, ARIA2_DOWNLOAD_START_MSG, NULL, (LPARAM)sendPtr);
		}
		else if (strDownStatus == ARIA2_DOWNLOAD_STOP)
		{
			PVOID sendPtr = AlloclocalHeap(strGidItem);
			::PostMessage(m_hwnd, ARIA2_DOWNLOAD_STOP_MSG, NULL, (LPARAM)sendPtr);
		}
		else if (strDownStatus == ARIA2_DOWNLOAD_PAUSE)
		{
			PVOID sendPtr = AlloclocalHeap(strGidItem);
			::PostMessage(m_hwnd, ARIA2_DOWNLOAD_PAUSE_MSG, NULL, (LPARAM)sendPtr);
		}
		else if (strDownStatus == ARIA2_DOWNLOAD_ERROR)
		{
			PVOID sendPtr = AlloclocalHeap(strGidItem);
			::PostMessage(m_hwnd, ARIA2_DOWNLOAD_ERROR_MSG, NULL, (LPARAM)sendPtr);
		}
	}
	if (dc.HasMember("result") && dc["result"].IsArray())
	{
		if (dc["result"].GetArray().Size()==0)
		{
			return;
		}
		//��ʼ��װjson���͸�UI�̴߳�����Ⱦ������
		rapidjson::Document sendJson;
		sendJson.SetObject();
		rapidjson::Value arrlist(rapidjson::kArrayType);
		int nErrorcount = 0;
		for (auto& v : dc["result"].GetArray())
		{
			rapidjson::Value sendItemObj(rapidjson::kObjectType);
			DOWNFILELISTINFO ItemValue;
			ZeroMemory(&ItemValue, sizeof(DOWNFILELISTINFO));
			ULONGLONG totalLength, completedLength, downloadSpeed;
			if (v.HasMember("totalLength") && v["totalLength"].IsString())
				sscanf_s(v["totalLength"].GetString(), "%I64d", &totalLength);
			if (v.HasMember("completedLength") && v["completedLength"].IsString())
				sscanf_s(v["completedLength"].GetString(), "%I64d", &completedLength);
			if (v.HasMember("downloadSpeed") && v["downloadSpeed"].IsString())
				sscanf_s(v["downloadSpeed"].GetString(), "%I64d", &downloadSpeed);
			if (v.HasMember("connections") && v["connections"].IsString())
				ItemValue.connections = v["connections"].GetString();
			if (v.HasMember("gid") && v["gid"].IsString())
				ItemValue.strFileGid = v["gid"].GetString();
			if (v.HasMember("status") && v["status"].IsString())
				ItemValue.strStatus = v["status"].GetString();
			if (v.HasMember("errorMessage") && v["errorMessage"].IsString() && v.HasMember("errorCode") && v["errorCode"].IsString())
			{
				nErrorcount++;
				ItemValue.nErrCode = atoi(v["errorCode"].GetString());
				ItemValue.strErrMessage = v["errorMessage"].GetString();
			}
			if (v.HasMember("files") && v["files"].IsArray())
			{
				for (auto& keyval : v["files"].GetArray())
				{
					if (keyval.HasMember("path") && keyval["path"].IsString())
					{
						char szName[MAX_PATH];
						ZeroMemory(szName, MAX_PATH);
						std::string szFileName = m_BaiduPare.Utf8_To_Gbk(keyval["path"].GetString());
						ItemValue.strPath = szFileName;
						CopyMemory(szName, szFileName.c_str(), szFileName.length());
						PathStripPathA(szName);
						ItemValue.strFileName = szName;
					}


				}
			}
			ItemValue.Downloadpercentage = (size_t)Getpercentage(completedLength, totalLength);
			DOWNLOADSPEEDINFO szFileSize = GetFileSizeType(downloadSpeed);
			int isRate = 0;
			if (szFileSize.strUnit =="B")
			{
				ItemValue.DownloadSpeed = str(boost::format("%.1f B") % szFileSize.dwSize);
				isRate = 1;
			}else if (szFileSize.strUnit == "KB")
			{
				ItemValue.DownloadSpeed = str(boost::format("%.1f KB") % szFileSize.dwSize);
				if(szFileSize.dwSize<100)
					isRate = 1;
			}
			else if (szFileSize.strUnit == "MB")
			{
				ItemValue.DownloadSpeed = str(boost::format("%.1f MB") % szFileSize.dwSize);
			}
			else if (szFileSize.strUnit == "GB")
			{
				ItemValue.DownloadSpeed = str(boost::format("%.1f GB") % szFileSize.dwSize);
			}
			rapidjson::Value isRateLimiting(rapidjson::kNumberType);	//�Ƿ�����
			isRateLimiting.SetInt(isRate);
			sendItemObj.AddMember(rapidjson::StringRef("isRateLimiting"), isRateLimiting, sendJson.GetAllocator());
			rapidjson::Value connections(rapidjson::kStringType);//��ǰ������������������
			connections.SetString(ItemValue.connections.c_str(), ItemValue.connections.length(), sendJson.GetAllocator());
			sendItemObj.AddMember(rapidjson::StringRef("connections"), connections, sendJson.GetAllocator());
			rapidjson::Value downloadSpeedItem(rapidjson::kStringType);//�����ٶ�ÿ��
			downloadSpeedItem.SetString(ItemValue.DownloadSpeed.c_str(), ItemValue.DownloadSpeed.length(), sendJson.GetAllocator());
			sendItemObj.AddMember(rapidjson::StringRef("downloadSpeed"), downloadSpeedItem, sendJson.GetAllocator());
			rapidjson::Value FileName(rapidjson::kStringType);//��ǰ������ļ���
			FileName.SetString(ItemValue.strFileName.c_str(), ItemValue.strFileName.length(), sendJson.GetAllocator());
			sendItemObj.AddMember(rapidjson::StringRef("name"), FileName, sendJson.GetAllocator());
			rapidjson::Value Gid(rapidjson::kStringType);//��ǰ����GIDΨһ��ʶ��
			Gid.SetString(ItemValue.strFileGid.c_str(), ItemValue.strFileGid.length(), sendJson.GetAllocator());
			sendItemObj.AddMember(rapidjson::StringRef("gid"), Gid, sendJson.GetAllocator());
			rapidjson::Value Downloadpercentage(rapidjson::kNumberType);//��ǰ���صİٷְ�
			Downloadpercentage.SetUint(ItemValue.Downloadpercentage);
			sendItemObj.AddMember(rapidjson::StringRef("progress"), Downloadpercentage, sendJson.GetAllocator());
			rapidjson::Value nErrCode(rapidjson::kNumberType);//���ش������
			nErrCode.SetUint(ItemValue.nErrCode);
			sendItemObj.AddMember(rapidjson::StringRef("errorCode"), nErrCode, sendJson.GetAllocator());
			rapidjson::Value ErrMessage(rapidjson::kStringType);//���ش���ԭ��
			ErrMessage.SetString(ItemValue.strErrMessage.c_str(), ItemValue.strErrMessage.length(), sendJson.GetAllocator());
			sendItemObj.AddMember(rapidjson::StringRef("errorMessage"), ErrMessage, sendJson.GetAllocator());
			rapidjson::Value status(rapidjson::kStringType);//�����ļ�״̬
			status.SetString(ItemValue.strStatus.c_str(), ItemValue.strStatus.length(), sendJson.GetAllocator());
			sendItemObj.AddMember(rapidjson::StringRef("status"), status, sendJson.GetAllocator());
			rapidjson::Value strPath(rapidjson::kStringType);//�����ļ�·��
			strPath.SetString(ItemValue.strPath.c_str(), ItemValue.strPath.length(), sendJson.GetAllocator());
			sendItemObj.AddMember(rapidjson::StringRef("path"), strPath, sendJson.GetAllocator());
			arrlist.PushBack(sendItemObj, sendJson.GetAllocator());
		}
		//���͸�UI�߳�
		sendJson.AddMember(rapidjson::StringRef("data"), arrlist, sendJson.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		sendJson.Accept(writer);
		std::string strResultJson = buffer.GetString();
		strResultJson = m_BaiduPare.Gbk_To_Utf8(strResultJson.c_str());
		PVOID pSendDataPtr = AlloclocalHeap(strResultJson);
		if (!nErrorcount)
		{
			::PostMessage(m_hwnd, ARIA2_UPDATE_TELLACTIVE_LIST_MSG, NULL, (LPARAM)pSendDataPtr);
		}


	}
	if (dc.HasMember("result") && dc["result"].IsObject())
	{
		rapidjson::Value itemObj = dc["result"].GetObjectW();
		if (itemObj.HasMember("numActive") && itemObj["numActive"].IsString())
			numActive = atol(itemObj["numActive"].GetString());
		if (itemObj.HasMember("numStopped") && itemObj["numStopped"].IsString())
			numStopped = atol(itemObj["numStopped"].GetString());
		if (itemObj.HasMember("numWaiting") && itemObj["numWaiting"].IsString())
			numWaiting = atol(itemObj["numWaiting"].GetString());
	}
}
#endif
DOWNLOADSPEEDINFO CWkeWindow::GetFileSizeType(double dSize)
{
	DOWNLOADSPEEDINFO szFileSize;
	ZeroMemory(&szFileSize, sizeof(DOWNLOADSPEEDINFO));
	if (dSize <1024)
	{
		szFileSize.dwSize = m_BaiduPare.roundEx(dSize);
		szFileSize.strUnit = "B";
	}
	else if (dSize >1024 && dSize < 1024 * 1024 * 1024 && dSize <1024 * 1024)
	{
		szFileSize.dwSize = m_BaiduPare.roundEx(dSize / 1024);
		szFileSize.strUnit = "KB";
	}
	else if (dSize >1024 * 1024 && dSize <1024 * 1024 * 1024)
	{
		szFileSize.dwSize = m_BaiduPare.roundEx(dSize / 1024 / 1024);
		szFileSize.strUnit = "MB";
	}
	else if (dSize >1024 * 1024 * 1024)
	{
		szFileSize.dwSize = m_BaiduPare.roundEx(dSize / 1024 / 1024 / 1024);
		szFileSize.strUnit = "GB";
	}
	return szFileSize;
}

double CWkeWindow::Getpercentage(double completedLength, double totalLength)
{
	return completedLength / totalLength * 100;
}


LRESULT CALLBACK CWkeWindow::MainProc(_In_ HWND hwnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	static int index  = 0;
	switch (uMsg)
	{
	case ARIA2_UPDATE_TELLACTIVE_LIST_MSG:	//���µ�ǰ�������ص������б�
	{
				if (lParam)
				{
					const char* pbuffer = (const char*)lParam;
					std::string strJsonData(pbuffer);
					delete pbuffer;
					GetInstance()->UpdateDownloadList(strJsonData);
				}
	}
	break;
	case ARIA2_PURGEDOWNLOAD_MSG: //������ش�����ڴ滺��
	{
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strJsonData(pbuffer);
			LOG(INFO) << "������ش����ڴ滺��" << strJsonData.c_str();
			delete pbuffer;
			GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(strJsonData.c_str()));
		}
	}
	break;
	case ARIA2_RETRYADDURL_MSG: //�����������
	{
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strJsonData(pbuffer);
			delete pbuffer;
			LOG(INFO) << "�����������" << strJsonData.c_str();
			GetInstance()->SendText(strJsonData);
		}
	}
	break;
	case ARIA2_UPDATE_TELLERROR_LIST_MSG://��������ʧ�ܵ��ļ������б�
	{
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strJsonData(pbuffer);
			delete pbuffer;
			static auto updateErrorList = [](const std::string strJosn) {
				jsExecState es = wkeGlobalExec(app.window);
				jsValue thisObject = jsGetGlobal(es, "app");
				jsValue func = jsGet(es, thisObject, "updatedownloadErrorList");
				jsValue jsArg[1] = { jsString(es, strJosn.c_str()) };
				jsCall(es, func, thisObject, jsArg, 1);
			};
			LOG(INFO) << "��������ʧ�ܵ��ļ������б�" << strJsonData.c_str();
			updateErrorList(strJsonData);
		}

	}
	break;
	case ARIA2_DOWNLOAD_COMPLATE_MSG: //ĳ�������������
	{
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strGID(pbuffer);
			delete pbuffer;
				HttpRequest aria2_http;
				std::string strformat = str(boost::format(ARIA2_TELLSTATUS_SENDDATA) % strGID);
				aria2_http.Send(POST, ARIA2_HTTP_REQUESTURL, strformat);
				std::string strResult = aria2_http.GetResponseText();
				DOWNFILELISTINFO strFileName = GetInstance()->GetTellStatusFileName(strResult);
			//�������ݵ�����������б�
			static auto addSussedList = [](const DOWNFILELISTINFO Gid) {
				std::string buffer = str(boost::format("{\"name\":\"%1%\",\"ChangeTime\":\"%2%\",\"path\":\"%3%\"}") % Gid.strFileName % GetInstance()->m_BaiduPare.timestampToDate((ULONGLONG)time(NULL)) % Gid.strPath);
				buffer = GetInstance()->m_BaiduPare.Gbk_To_Utf8(buffer.c_str());
				jsExecState es = wkeGlobalExec(app.window);
				jsValue thisObject = jsGetGlobal(es, "app");
				jsValue func = jsGet(es, thisObject, "updatedownloadSussedList");
				jsValue jsArg[1] = { jsString(es, buffer.c_str()) };
				jsCall(es, func, thisObject, jsArg, 1);
			};
			LOG(INFO) << "ĳ�������������" << strFileName.strFileName.c_str();
			addSussedList(strFileName);
		}
	}
	break;
	case ARIA2_DOWNLOAD_START_MSG: //ĳ������ʼ����
	{
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strGID(pbuffer);
			delete pbuffer;
			LOG(INFO) << "ĳ������ʼ����" << strGID.c_str();

		}
	}
	break;
	case ARIA2_DOWNLOAD_STOP_MSG: //ĳ������ֹͣ���� �û��ֶ�ɾ��
	{
		LOG(INFO) << "ĳ������ֹͣ����";
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strGID(pbuffer);
			delete pbuffer;
			HttpRequest aria2_http;
			std::string strformat = str(boost::format(ARIA2_TELLSTATUS_SENDDATA) % strGID);
			aria2_http.Send(POST, ARIA2_HTTP_REQUESTURL, strformat);
			std::string strResult = aria2_http.GetResponseText();
			DOWNFILELISTINFO strFileName = GetInstance()->GetTellStatusFileName(strResult);
			if (_access(strFileName.strPath.c_str(),0)!=-1)
			{
				DeleteFileA(strFileName.strPath.c_str());
				DeleteFileA((strFileName.strPath+".aria2").c_str());
			}
		}
	}
	break;
	case ARIA2_DOWNLOAD_PAUSE_MSG: //ĳ������ ��ͣ����
	{
		LOG(INFO) << "ĳ������ ��ͣ����";
	}
	break;
	case ARIA2_DOWNLOAD_ERROR_MSG: //ĳ���������س���
	{
			if (lParam)
			{
				const char* pbuffer = (const char*)lParam;
				std::string strGID(pbuffer);
				delete pbuffer;
				std::string strformat = str(boost::format(ARIA2_TELLSTATUS_SENDDATA) % strGID);
				HttpRequest aria2_http;
				aria2_http.Send(POST, ARIA2_HTTP_REQUESTURL, strformat);
				std::string strResult = GetInstance()->m_BaiduPare.Utf8_To_Gbk(aria2_http.GetResponseText().c_str());
				LOG(INFO) << "ĳ���������س��� ����������������" << strformat.c_str();
				static auto updateErrorList = [](const std::string strJosn) {
					jsExecState es = wkeGlobalExec(app.window);
					jsValue thisObject = jsGetGlobal(es, "app");
					jsValue func = jsGet(es, thisObject, "updatedownloadErrorList");
					jsValue jsArg[1] = { jsString(es, strJosn.c_str()) };
					jsCall(es, func, thisObject, jsArg, 1);
				};
				rapidjson::Document dc;
				dc.Parse(strResult.c_str());
				if (!dc.IsObject())
					break;
				if (dc.HasMember("result") && dc["result"].IsObject())
				{
					rapidjson::Value itemObj = dc["result"].GetObjectW();
					if (itemObj.HasMember("errorCode") && itemObj["errorCode"].IsString())
					{
						//��ʼ��װjson���͸�UI�̴߳�����Ⱦ������
						rapidjson::Document sendJson;
						std::string strRetryUrl;
						sendJson.SetObject();
						rapidjson::Value sendItemObj(rapidjson::kObjectType);
						DOWNFILELISTINFO ItemValue;
						ZeroMemory(&ItemValue, sizeof(DOWNFILELISTINFO));
						ULONGLONG totalLength, completedLength, downloadSpeed;
						if (itemObj.HasMember("totalLength") && itemObj["totalLength"].IsString())
							sscanf_s(itemObj["totalLength"].GetString(), "%I64d", &totalLength);
						if (itemObj.HasMember("completedLength") && itemObj["completedLength"].IsString())
							sscanf_s(itemObj["completedLength"].GetString(), "%I64d", &completedLength);
						if (itemObj.HasMember("downloadSpeed") && itemObj["downloadSpeed"].IsString())
							sscanf_s(itemObj["downloadSpeed"].GetString(), "%I64d", &downloadSpeed);
						if (itemObj.HasMember("connections") && itemObj["connections"].IsString())
							ItemValue.connections = itemObj["connections"].GetString();
						if (itemObj.HasMember("gid") && itemObj["gid"].IsString())
							ItemValue.strFileGid = itemObj["gid"].GetString();

						ItemValue.nErrCode = atoi(itemObj["errorCode"].GetString());
						ItemValue.strErrMessage = itemObj["errorMessage"].GetString();

						if (itemObj.HasMember("files") && itemObj["files"].IsArray())
						{
							for (auto& keyval : itemObj["files"].GetArray())
							{
								if (keyval.HasMember("path") && keyval["path"].IsString())
								{
									char szName[MAX_PATH];
									ZeroMemory(szName, MAX_PATH);
									std::string szFileName = keyval["path"].GetString();
									ItemValue.strPath = szFileName;
									CopyMemory(szName, szFileName.c_str(), szFileName.length());
									PathStripPathA(szName);
									ItemValue.strFileName = szName;
								}
								if (keyval.HasMember("uris") && keyval["uris"].IsArray())
								{
									rapidjson::Value urisArr = keyval["uris"].GetArray();
									if (urisArr.Size())
									{
										if (urisArr[0].HasMember("uri") && urisArr[0]["uri"].IsString())
										{
											strRetryUrl = urisArr[0]["uri"].GetString();
										}
									}
								}

							}
						}
						ItemValue.Downloadpercentage = (size_t)GetInstance()->Getpercentage(completedLength, totalLength);
						DOWNLOADSPEEDINFO szFileSize = GetInstance()->GetFileSizeType(downloadSpeed);
						int isRate = 0;
						if (szFileSize.strUnit == "B")
						{
							ItemValue.DownloadSpeed = str(boost::format("%.1f B") % szFileSize.dwSize);
							isRate = 1;
						}
						else if (szFileSize.strUnit == "KB")
						{
							ItemValue.DownloadSpeed = str(boost::format("%.1f KB") % szFileSize.dwSize);
							if (szFileSize.dwSize < 100)
								isRate = 1;
						}
						else if (szFileSize.strUnit == "MB")
						{
							ItemValue.DownloadSpeed = str(boost::format("%.1f MB") % szFileSize.dwSize);
						}
						else if (szFileSize.strUnit == "GB")
						{
							ItemValue.DownloadSpeed = str(boost::format("%.1f GB") % szFileSize.dwSize);
						}
						rapidjson::Value isRateLimiting(rapidjson::kNumberType);	//�Ƿ�����
						isRateLimiting.SetInt(isRate);
						sendItemObj.AddMember(rapidjson::StringRef("isRateLimiting"), isRateLimiting, sendJson.GetAllocator());
						rapidjson::Value connections(rapidjson::kStringType);//��ǰ������������������
						connections.SetString(ItemValue.connections.c_str(), ItemValue.connections.length(), sendJson.GetAllocator());
						sendItemObj.AddMember(rapidjson::StringRef("connections"), connections, sendJson.GetAllocator());
						rapidjson::Value downloadSpeedItem(rapidjson::kStringType);//�����ٶ�ÿ��
						downloadSpeedItem.SetString(ItemValue.DownloadSpeed.c_str(), ItemValue.DownloadSpeed.length(), sendJson.GetAllocator());
						sendItemObj.AddMember(rapidjson::StringRef("downloadSpeed"), downloadSpeedItem, sendJson.GetAllocator());
						rapidjson::Value FileName(rapidjson::kStringType);//��ǰ������ļ���
						FileName.SetString(ItemValue.strFileName.c_str(), ItemValue.strFileName.length(), sendJson.GetAllocator());
						sendItemObj.AddMember(rapidjson::StringRef("name"), FileName, sendJson.GetAllocator());
						rapidjson::Value Gid(rapidjson::kStringType);//��ǰ����GIDΨһ��ʶ��
						Gid.SetString(ItemValue.strFileGid.c_str(), ItemValue.strFileGid.length(), sendJson.GetAllocator());
						sendItemObj.AddMember(rapidjson::StringRef("gid"), Gid, sendJson.GetAllocator());
						rapidjson::Value Downloadpercentage(rapidjson::kNumberType);//��ǰ���صİٷְ�
						Downloadpercentage.SetUint(ItemValue.Downloadpercentage);
						sendItemObj.AddMember(rapidjson::StringRef("progress"), Downloadpercentage, sendJson.GetAllocator());
						rapidjson::Value nErrCode(rapidjson::kNumberType);//���ش������
						nErrCode.SetUint(ItemValue.nErrCode);
						sendItemObj.AddMember(rapidjson::StringRef("errorCode"), nErrCode, sendJson.GetAllocator());
						rapidjson::Value ErrMessage(rapidjson::kStringType);//���ش���ԭ��
						ErrMessage.SetString(ItemValue.strErrMessage.c_str(), ItemValue.strErrMessage.length(), sendJson.GetAllocator());
						sendItemObj.AddMember(rapidjson::StringRef("errorMessage"), ErrMessage, sendJson.GetAllocator());
						rapidjson::Value strPath(rapidjson::kStringType);//�����ļ�·��
						strPath.SetString(ItemValue.strPath.c_str(), ItemValue.strPath.length(), sendJson.GetAllocator());
						sendItemObj.AddMember(rapidjson::StringRef("path"), strPath, sendJson.GetAllocator());
						//���͸�UI�߳�
						sendJson.AddMember(rapidjson::StringRef("data"), sendItemObj, sendJson.GetAllocator());
						rapidjson::StringBuffer buffer;
						rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
						sendJson.Accept(writer);
						std::string strResultJson = buffer.GetString();
						strResultJson = GetInstance()->m_BaiduPare.Gbk_To_Utf8(strResultJson.c_str());
						updateErrorList(strResultJson);
						if (_access(ItemValue.strPath.c_str(), 0) != -1 && _access((ItemValue.strPath + ".aria2").c_str(), 0) != -1)
						{
							DeleteFileA(ItemValue.strPath.c_str());
							DeleteFileA((ItemValue.strPath + ".aria2").c_str());
						}
					}
				}
			}
	}
	break;
	case UI_DOWNLOAD_SHARE_UPDATE_LIST:
	{
		static auto updateDownloadShareListdata = [](const std::string strJosn) {
			jsExecState es = wkeGlobalExec(app.window);
			jsValue thisObject = jsGetGlobal(es, "app");
			jsValue func = jsGet(es, thisObject, "updateDownloadShare");
			jsValue jsArg[1] = { jsString(es, strJosn.c_str()) };
			jsCall(es, func, thisObject, jsArg, 1);
		};
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strGID(pbuffer);
			delete pbuffer;
			updateDownloadShareListdata(strGID);
		}
	}
	break;
	case UI_UPDATE_FOLODER_LIST_MSG:
	{
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strResultJson(pbuffer);
			delete pbuffer;
			jsExecState es = wkeGlobalExec(app.window);
			jsValue thisObject = jsGetGlobal(es, "app");
			bool b = jsIsObject(thisObject);
			jsValue func = jsGet(es, thisObject, "updateTreeListData");
			strResultJson = GetInstance()->m_BaiduPare.Gbk_To_Utf8(strResultJson.c_str());
			jsValue jsArg[1] = { jsString(es, strResultJson.c_str()) };
			jsCall(es, func, thisObject, jsArg, 1);
		}
	}
	break;
	case UI_UPDATE_OFF_LINE_LIST_MSG:
	{
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strResultJson(pbuffer);
			delete pbuffer;
			jsExecState es = wkeGlobalExec(app.window);
			jsValue thisObject = jsGetGlobal(es, "app");
			bool b = jsIsObject(thisObject);
			jsValue func = jsGet(es, thisObject, "updateOfflineTableList");
			//strResultJson = GetInstance()->m_BaiduPare.Gbk_To_Utf8(strResultJson.c_str());
			jsValue jsArg[1] = { jsString(es, strResultJson.c_str()) };
			jsCall(es, func, thisObject, jsArg, 1);
		}
	}
	break;
	case UI_UPDATE_USER_FILE_DATA_MSG:
	{
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strResultJson(pbuffer);
			delete pbuffer;
			jsExecState es = wkeGlobalExec(app.window);
			jsValue thisObject = jsGetGlobal(es, "app");
			bool b = jsIsObject(thisObject);
			jsValue func = jsGet(es, thisObject, "updateBaiduList");
			jsValue jsArg[1] = { jsString(es, strResultJson.c_str()) };
			jsCall(es, func, thisObject, jsArg, 1);
		}
	}
	break;
	case UI_QUIT_MSG:
	{
		::PostQuitMessage(0);
	}
	break;
	case UI_SHOW_ERROR_MSG:
	{
		static auto showErrormsg = [](std::string& Strmsg) {
			std::string buffer = Strmsg;
			buffer = GetInstance()->m_BaiduPare.Gbk_To_Utf8(buffer.c_str());
			jsExecState es = wkeGlobalExec(app.window);
			jsValue thisObject = jsGetGlobal(es, "app");
			jsValue func = jsGet(es, thisObject, "showErrorMessage");
			jsValue jsArg[1] = { jsString(es, buffer.c_str()) };
			jsCall(es, func, thisObject, jsArg, 1);
		};
		if (lParam)
		{
			const char* pbuffer = (const char*)lParam;
			std::string strResultJson(pbuffer);
			delete pbuffer;
			showErrormsg(strResultJson);
		}
	}
	default:
		break;
	}
	return CallWindowProc(m_oldProc, hwnd, uMsg, wParam, lParam);
}

void CWkeWindow::AddShareFileItem(REQUESTINFO item)
{
	bool bResult = false;
	for (size_t i =0;i<m_ShareFileArray.size();i++)
	{
		auto & value = m_ShareFileArray.at(i);
		if (value.strFileName == item.strFileName)
		{
			bResult = true;
			break;
		}
	}
	if (!bResult)
		m_ShareFileArray.push_back(item);
}

void CWkeWindow::GetShareFileItem(const std::string& strFileName,REQUESTINFO& Result)
{

	for (size_t i=0;i<m_ShareFileArray.size();i++)
	{
		auto & value = m_ShareFileArray.at(i);
		if (value.strFileName == strFileName)
		{
			Result.strCookies = value.strCookies;
			Result.strDownloadUrl = value.strDownloadUrl;
			Result.strFileName = value.strFileName;
			Result.strSavePath = value.strSavePath;
			break;
		}
	}
}

bool CWkeWindow::IsRetryCountFinish(const std::string& strFileName)
{
	bool bResult = false;
	for (size_t i =0;i<m_RetryArray.size();i++)
	{
		auto & value = m_RetryArray.at(i);
		if (value.strFileName == strFileName && value.nCount > 10)
		{
			bResult = true;
			break;
		}
	}
	return bResult;
}

bool CWkeWindow::IsRetryExist(const std::string& strFileName)
{
	bool bResult = false;
	RETRYCOUNT item;
	ZeroMemory(&item, sizeof(RETRYCOUNT));
	for (size_t i = 0; i < m_RetryArray.size(); i++)
	{
		auto & value = m_RetryArray.at(i);
		if (value.strFileName == strFileName)
		{
			bResult = true;
			break;
		}
	}
	if (!bResult)
	{
		item.strFileName = strFileName;
		m_RetryArray.push_back(item);
	}
	return bResult;
}

void CWkeWindow::addRetryCount(const std::string& strFileName)
{
	for (size_t i = 0; i < m_RetryArray.size(); i++)
	{
		auto & value = m_RetryArray.at(i);
		if (value.strFileName == strFileName)
		{
			value.nCount++;
			break;
		}
	}
}

DOWNFILELISTINFO CWkeWindow::GetFileCompletedInfo(const std::string& strGid)
{
	std::string StrJson="{\"data\":";
	DOWNFILELISTINFO strResult;
	ZeroMemory(&strResult, sizeof(DOWNFILELISTINFO));
	jsExecState es = wkeGlobalExec(app.window);
	jsValue thisObject = jsGetGlobal(es, "app");
	jsValue func = jsGet(es, thisObject, "GetBackupListString");
	jsValue rString = jsCall(es, func, thisObject, nullptr, 0);
	jsType Type = jsTypeOf(rString);
	StrJson += jsToTempString(es, rString);
	StrJson += "}";
	rapidjson::Document dc;
	dc.Parse(StrJson.c_str());
	if (!dc.IsObject())
		return strResult;
	if (dc.HasMember("data") && dc["data"].IsArray())
	{
		for (auto& v:dc["data"].GetArray())
		{
			if (v.HasMember("gid") && v["gid"].IsString())
			{
				if (strGid == v["gid"].GetString())
				{
					if (v.HasMember("name") && v["name"].IsString())
						strResult.strFileName = m_BaiduPare.Utf8_To_Gbk(v["name"].GetString());
					if (v.HasMember("path") && v["path"].IsString())
						strResult.strPath = m_BaiduPare.Utf8_To_Gbk(v["path"].GetString());
				}
			}
		}
	}
	return strResult;
}

DOWNFILELISTINFO CWkeWindow::GetTellStatusFileName(const std::string& strJSon)
{
	DOWNFILELISTINFO strResult;
	rapidjson::Document dc;
	dc.Parse(strJSon.c_str());
	if (!dc.IsObject())
	{
		return strResult;
	}
	if (dc.HasMember("result") && dc["result"].IsObject())
	{
		rapidjson::Value itemObj = dc["result"].GetObjectW();


			if (itemObj.HasMember("files") && itemObj["files"].IsArray())
			{
				for (auto& keyval : itemObj["files"].GetArray())
				{
					if (keyval.HasMember("path") && keyval["path"].IsString())
					{
						char szName[MAX_PATH];
						ZeroMemory(szName, MAX_PATH);
						std::string szFileName = m_BaiduPare.Utf8_To_Gbk(keyval["path"].GetString());
						strResult.strPath = szFileName;
						CopyMemory(szName, szFileName.c_str(), szFileName.length());
						PathStripPathA(szName);
						strResult.strFileName = szName;
					}
				}
			}
		}
	return strResult;
}

#if 1
void CWkeWindow::on_socket_init(websocketpp::connection_hdl hdl)
{

}

void CWkeWindow::on_message(websocketpp::connection_hdl hdl, message_ptr msg)
{
	DWORD dwThread = ::GetThreadId(::GetCurrentThread());
	websocketpp::frame::opcode::value opcodes = msg->get_opcode();
	if (opcodes == websocketpp::frame::opcode::text)
	{

		std::string strMsgText = msg->get_payload();
		ParseAria2JsonInfo(strMsgText);
	}
}

void CWkeWindow::on_open(websocketpp::connection_hdl hdl)
{
	m_hdl = hdl;
}

void CWkeWindow::on_close(websocketpp::connection_hdl hdl)
{
	hdl.reset();
}

void CWkeWindow::on_fail(websocketpp::connection_hdl hdl)
{
	LOG(ERROR) << "websoket����ʧ��";
	::PostMessage(m_hwnd, UI_SHOW_ERROR_MSG, NULL, (LPARAM)AlloclocalHeap("websoket����ʧ��,���Ҽ�����Ա�������"));
	hdl.reset();
}

void CWkeWindow::start(std::string uri)
{
	websocketpp::lib::error_code ec;
	client::connection_ptr con = m_WssClient.get_connection(uri, ec);
	if (ec) {
		LOG(ERROR) << "����wss����ʧ��";
		return;
	}
	m_WssClient.connect(con);
	m_WssClient.run();
}
void CWkeWindow::Connect()
{
	try{
		start("ws://127.0.0.1:6810/jsonrpc");
	}
	catch (websocketpp::exception const & e) {
		//std::cout << e.what() << std::endl;
		LOG(ERROR) << e.what();
	}
	catch (std::exception const & e) {
		//std::cout << e.what() << std::endl;
		LOG(ERROR) << e.what();
	}
	catch (...) {
		LOG(ERROR) <<"void CWkeWindow::Connect() δ֪���쳣";
	}
}

void CWkeWindow::SendText(std::string& strText)
{
	if (m_hdl.lock().get())
	{
		//m_BaiduPare.WriteFileBuffer(".\\1.txt",(char*)strText.c_str(), strText.length());
		m_WssClient.send(m_hdl, strText, websocketpp::frame::opcode::text);
	}
}
#endif
BOOL CWkeWindow::RunAria2()
{
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	si.dwFlags = STARTF_USESHOWWINDOW;  // ָ��wShowWindow��Ա��Ч
#if !_DEBUG
	si.wShowWindow = false;          // �˳�Ա��ΪTRUE�Ļ�����ʾ�½����̵������ڣ�
	std::string strCommandlineArg = str(boost::format("aria2c.exe --check-certificate=false --disable-ipv6=true --enable-rpc=true --quiet=true --file-allocation=falloc --max-concurrent-downloads=5  --rpc-allow-origin-all=true --rpc-listen-all=true --rpc-listen-port=6810 --rpc-secret=CDP --stop-with-process=%1%")\
		% std::to_string(GetCurrentProcessId()));
#else
	si.wShowWindow = true;          // �˳�Ա��ΪTRUE�Ļ�����ʾ�½����̵������ڣ�
	std::string strCommandlineArg = str(boost::format("aria2c.exe --check-certificate=false --disable-ipv6=true --enable-rpc=true --quiet=false --file-allocation=falloc --max-concurrent-downloads=5  --rpc-allow-origin-all=true --rpc-listen-all=true --rpc-listen-port=6810 --rpc-secret=CDP --stop-with-process=%1%")\
		% std::to_string(GetCurrentProcessId()));
#endif
	BOOL bRet = ::CreateProcessA(NULL,           // ���ڴ�ָ����ִ���ļ����ļ���
		const_cast<char*>(strCommandlineArg.c_str()),      // �����в���
		NULL,           // Ĭ�Ͻ��̰�ȫ��
		NULL,           // Ĭ���̰߳�ȫ��
		FALSE,          // ָ����ǰ�����ڵľ�������Ա��ӽ��̼̳�
		CREATE_NEW_CONSOLE, // Ϊ�½��̴���һ���µĿ���̨����
		NULL,           // ʹ�ñ����̵Ļ�������
		NULL,           // ʹ�ñ����̵���������Ŀ¼
		&si,
		&pi);
	::CloseHandle(pi.hProcess);
	return bRet;
}
void CALLBACK CWkeWindow::TimeProc(HWND hwnd, UINT message, UINT iTimerID, DWORD dwTime)
{
#if 1
	GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(ARIA2_GETGLOBAL_STATUS));
	if (GetInstance()->numActive > 0 || GetInstance()->numWaiting > 0)
	{
		std::string strWaiting;
		GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(ARIA2_TELLACTICE_SENDDATA));
		strWaiting = str(boost::format(ARIA2_TELLSTOPPED) % GetInstance()->numStopped);
		GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(strWaiting.c_str()));
		strWaiting = str(boost::format(ARIA2_TELLWAITING) % GetInstance()->numWaiting);
		GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(strWaiting.c_str()));
		wkeRunJS(app.window, "app.backupdownloadListinfo = app.downloadListinfo;");
	}
	if (GetInstance()->numActive == 0 && GetInstance()->numWaiting == 0)
	{
		GetInstance()->UpdateDownloadList("{\"data\":[]}");
	}
#endif
}


void CWkeWindow::runApp(Application* app)
{
	DWORD dwThread = ::GetThreadId(::GetCurrentThread());
	memset(app, 0, sizeof(Application));
//	app->url = L"https://baidu.com/"; // ʹ��hook�ķ�ʽ������Դ
	app->url = L"file:///D:/Demo/BaiduCdpUi/element.html"; // ʹ��hook�ķ�ʽ������Դ
	if (!createWebWindow(app)) {
		PostQuitMessage(0);
		return;
	}
	runMessageLoop(app);
}
std::string CWkeWindow::CarkUrl(const char* url)
{
	std::string result = "http://";
	URL_COMPONENTSA uc;
	char Scheme[1000];
	char HostName[1000];
	char UserName[1000];
	char Password[1000];
	char UrlPath[1000];
	char ExtraInfo[1000];

	uc.dwStructSize = sizeof(uc);
	uc.lpszScheme = Scheme;
	uc.lpszHostName = HostName;
	uc.lpszUserName = UserName;
	uc.lpszPassword = Password;
	uc.lpszUrlPath = UrlPath;
	uc.lpszExtraInfo = ExtraInfo;

	uc.dwSchemeLength = 1000;
	uc.dwHostNameLength = 1000;
	uc.dwUserNameLength = 1000;
	uc.dwPasswordLength = 1000;
	uc.dwUrlPathLength = 1000;
	uc.dwExtraInfoLength = 1000;
	InternetCrackUrlA(url, 0, 0, &uc);
	return (result += HostName);

}
bool CWkeWindow::InintMiniBlink()
{
	bool bResult = false;
	std::vector<wchar_t> mbPath;
	mbPath.resize(MAX_PATH);
	::GetModuleFileName(nullptr, mbPath.data(), MAX_PATH);
	if (!::PathRemoveFileSpec(mbPath.data())) return bResult;
	if (!PathAppendW(mbPath.data(), MBDLL_NAME))return bResult;
	if (!::PathFileExists(mbPath.data()))
	{
		::MessageBox(NULL, L"ȱ��miniblink ���� node.dll�ļ�!", L"����", NULL);
		return bResult;
	}
	wkeSetWkeDllPath(mbPath.data());
	wkeInitialize();
	bResult = true;
	return bResult;
}
void CWkeWindow::quitApplication(Application* app)
{
	if (app->window) {
		wkeDestroyWebWindow(app->window);
		app->window = NULL;
	}
}
void CWkeWindow::runMessageLoop(Application* app)
{
	MSG msg = { 0 };
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}
bool CWkeWindow::createWebWindow(Application* app)
{
	app->window = wkeCreateWebWindow(WKE_WINDOW_TYPE_POPUP, NULL, 0, 0, 1024, 630);
	if (!app->window)
		return false;
	//����ȫ��cookies/����·��
	TCHAR szCookiePath[MAX_PATH];
	ZeroMemory(szCookiePath, MAX_PATH);
	::GetTempPath(MAX_PATH, szCookiePath);
	std::wstring szRootPath(szCookiePath);
	wcscat_s(szCookiePath, _T("cookie.tmp"));
	wkeSetCookieJarFullPath(app->window, szCookiePath);
	wkeSetLocalStorageFullPath(app->window, szRootPath.c_str());
	//���������ڵĴ��ھ��
 	m_hwnd = wkeGetWindowHandle(app->window);
	HICON hicon = ::LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_ICON1));
	::SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon);
	::SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hicon);
	LONG styleValue = ::GetWindowLong(m_hwnd, GWL_STYLE);
	styleValue &= ~WS_CAPTION;
	styleValue &= ~WS_MAXIMIZEBOX;
	styleValue &= ~WS_MINIMIZEBOX;
	styleValue &= ~WS_THICKFRAME;
	styleValue &= ~WS_BORDER;
	styleValue &= ~WS_CAPTION;
	::SetWindowLong(m_hwnd, GWL_STYLE, styleValue | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	DWORD dwExStyle = ::GetWindowLong(m_hwnd, GWL_EXSTYLE);
	::SetWindowLong(m_hwnd, GWL_EXSTYLE, dwExStyle | WS_EX_LAYERED);
 	m_oldProc = (WNDPROC)::SetWindowLongPtr(m_hwnd, GWL_WNDPROC, (DWORD)MainProc);
	m_ShadowWnd = BindShadowWindow(m_hwnd);
	if (m_ShadowWnd)
		m_ShadowWnd->Setup(5, 8, 192, 0);
	::SetTimer(m_hwnd, UPDTAE_UI_TIMEID, 1000, TimeProc);
	//���ô��ڵı���
	wkeSetWindowTitleW(app->window, APP_NAME);
	//����Ajax ��������֧��
	wkeSetCspCheckEnable(app->window, false); 
	//�����ڱ��ر�ʱ�Ļص������������û�ѡ���Ƿ�ر�
	wkeOnWindowClosing(app->window, OnCloseCallBack,this);
	//���ô������ٵĻص�����
	wkeOnWindowDestroy(app->window, OnWindowDestroy, this);
	//�����µ�webwindow���ڴ������ô˻ص�
	wkeOnCreateView(app->window, onCreateView, this);
	//document�ĵ����سɹ��ص�����
	wkeOnDocumentReady(app->window, OnDocumentReady, this);
	//������ҳ����ǰ�ص���Ҫ��Ϊ�˼��ر�����Դ
	wkeOnLoadUrlBegin(app->window, onLoadUrlBegin, this);
	//���ô��ھ�����ʾ
	wkeMoveToCenter(app->window);
	//�����������ַ
	wkeLoadURLW(app->window, app->url.c_str());
	//����ȫ��js�󶨺���
	wkeJsBindFunction("sysMenuCallBack", SysMenuJsNativeFunction, this, 1);
	//ΪwebWindow ��һ���жϰٶ��Ƿ��¼�ɹ��ĺ���
	wkeJsBindFunction("isLoginBaidu", IsLoginBaidu, this, 0);
	// ��һ���л��ٶȵ�¼���л�Ŀ¼�ĺ���
	wkeJsBindFunction("SwitchDirPath", SwitchDirPath, this, 1);
	//��һ���л��ٶȵ�¼�������������ļ��ĵĺ���
	wkeJsBindFunction("DownloadUserFile", DownloadUserFile, this, 1);
	//��һ���ٶȵ�¼����������ļ��ĺ���
	wkeJsBindFunction("ShareBaiduFile", ShareBaiduFile, this, 4);
	//��һ��ɾ���ٶȵ�¼�������ļ��ĺ���
	wkeJsBindFunction("DeleteBaiduFile", DeleteBaiduFile, this, 1);
	//��һ�����ط��������ļ��ĺ���
	wkeJsBindFunction("DownShareFile", DownShareFile, this, 1);
	//AraiaPauseStartRemove
	//��һ������aria2������صĺ���
	wkeJsBindFunction("AraiaPauseStartRemove", AraiaPauseStartRemove, this, 2);
	//��һ���˳��ٶȵ�¼�˺ŵĺ���
	wkeJsBindFunction("LogOut", LogOut, this, 0);
	//���ļ������ļ�Ŀ¼
	wkeJsBindFunction("OpenFilePlaceFolder", OpenFilePlaceFolder, this,2);
	//��һ���л�������������Ŀ¼�ĺ���
	wkeJsBindFunction("switchShareFolder", switchShareFolder, this, 7);
	//�����������ذ�ť�������󶨵Ļص�
	wkeJsBindFunction("DownSelectShareFile", DownSelectShareFile, this, 1);
	//�ٶ��ļ������󶨺���
	wkeJsBindFunction("BaiduFileRename", BaiduFileRename, this, 2);
	//��ȡ��ǰ�˺��������ļ���
	wkeJsBindFunction("EnumFolder", EnumFolder, this, 0);
	//���������غ���
	wkeJsBindFunction("OffLineDownload", OffLineDownload, this, 3);
	//����Ƿ���Ҫ����
	wkeJsBindFunction("isUpdate", isUpdate, this, 1);
	//��ָ����ҳ
	wkeJsBindFunction("OpenAssignUrl", OpenAssignUrl, this, 1);
	//����Ӧ��
	wkeJsBindFunction("UpdateApp", UpdateApp, this, 1);
	return true;
}

bool CWkeWindow::OnCloseCallBack(wkeWebView webWindow, void* param)
{
	if (param)
	{
		Application* app = (Application*)param;
		return IDYES == MessageBoxW(NULL, L"ȷ��Ҫ�˳�������", NULL, MB_YESNO | MB_ICONQUESTION);
	}
	return true;
}

void CWkeWindow::OnWindowDestroy(wkeWebView webWindow, void* param)
{
	Application* app = (Application*)param;
	app->window = NULL;
	PostQuitMessage(0);
}

void CWkeWindow::OnDocumentReady(wkeWebView webWindow, void* param)
{
	wkeShowWindow(webWindow, true);
}

bool CWkeWindow::onLoadUrlBegin(wkeWebView webView, void* param, const char* url, void *job)
{
	if (!param)return false;
	std::string localUrl = GetInstance()->CarkUrl(url);
	bool pos = localUrl == HOST_NAME ? true : false;
	if (pos) {
		const utf8* decodeURL = wkeUtilDecodeURLEscape(url);
		if (!decodeURL)
			return false;
		std::string urlString(decodeURL);
		std::string localPath = urlString.substr(sizeof(HOST_NAME)-1);

		std::wstring path = GetInstance()->getResourcesPath(GetInstance()->utf8ToUtf16(localPath));
		std::vector<char> buffer;
		GetInstance()->readJsFile(path.c_str(), &buffer);
#if _DEBUG
		wkeNetSetData(job, buffer.data(), buffer.size());
#else
		std::string fileName = GetInstance()->m_BaiduPare.Unicode_To_Ansi(path.c_str());
		char szNmae[MAX_PATH];
		ZeroMemory(szNmae, MAX_PATH);
		strcpy_s(szNmae, fileName.c_str());
		char szExtName[MAX_PATH];
		char* szExtPtr = nullptr;
		ZeroMemory(szExtName, MAX_PATH);
		szExtPtr = PathFindExtensionA(szNmae);
		if (szExtPtr != nullptr)
			CopyMemory(szExtName, szExtPtr, strlen(szExtPtr));
		PathStripPathA(szNmae);
		fileName = szNmae;
		if (fileName == "index.js" || fileName == "vue.js" || fileName == "element-icons.woff")
		{
			wkeNetSetData(job, buffer.data(), buffer.size());
		}
		else
		{
			std::string key = "0CoJUm6Qyw8W8jud";
			std::string  Deresult;
			Deresult = CCEncryption::AES_Decrypt(key, buffer.data());
			if (!strcmpi(".png", szExtName))
			{
				Deresult = aip::base64_decode(buffer.data());
				wkeNetSetData(job, (char*)Deresult.c_str(), Deresult.length());
			}
			else
			{
				if (fileName == "app.js")
				{
					Deresult = GetInstance()->m_BaiduPare.Gbk_To_Utf8(Deresult.c_str());
				}
				wkeNetSetData(job, (char*)Deresult.c_str(), Deresult.length());
	}
}
#endif
		return true;
	}
	return false;
}

std::wstring CWkeWindow::getResourcesPath(const std::wstring name)
{
	std::wstring  bResult;
	std::wstring temp;
	std::vector<wchar_t> mbPath;
	mbPath.resize(MAX_PATH+1);
	::GetModuleFileName(nullptr, mbPath.data(), MAX_PATH);
	if (!::PathRemoveFileSpec(mbPath.data())) return bResult;
	temp += mbPath.data();
	temp += L"\\";
	bResult = temp + name;
	return bResult;
}

std::wstring CWkeWindow::utf8ToUtf16(const std::string& utf8String)
{
	std::wstring sResult;
	int nUTF8Len = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, NULL, NULL);
	wchar_t* pUTF8 = new wchar_t[nUTF8Len + 1];

	ZeroMemory(pUTF8, nUTF8Len + 1);
	MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, pUTF8, nUTF8Len);
	sResult = pUTF8;
	delete[] pUTF8;

	return sResult;
}
DWORD CWkeWindow::ReadFileBuffer(std::string szFileName, PVOID* pFileBuffer)
{
	DWORD dwFileSize = NULL;
	DWORD dwAttribut = NULL;
	std::ifstream file;
	if (szFileName.empty())return dwFileSize;
	dwAttribut = ::GetFileAttributesA(szFileName.c_str());
	if (dwAttribut == INVALID_FILE_ATTRIBUTES || 0 != (dwAttribut & FILE_ATTRIBUTE_DIRECTORY))
	{
		dwFileSize = INVALID_FILE_ATTRIBUTES;
		return dwFileSize;
	}
	file.open(szFileName, std::ios::in | std::ios::binary);
	if (!file.is_open())
		return dwFileSize;
	file.seekg(0l, std::ios::end);
	dwFileSize = file.tellg();
	file.seekg(0l, std::ios::beg);
	*pFileBuffer = new BYTE[dwFileSize+1];
	if (!*pFileBuffer)
	{
		dwFileSize = NULL;
		file.close();
		return dwFileSize;
	}
	ZeroMemory(*pFileBuffer, dwFileSize+1);
	file.read((char*)*pFileBuffer, dwFileSize);
	file.close();
	return dwFileSize;
}
void CWkeWindow::readJsFile(const wchar_t* path, std::vector<char>* buffer)
{
	HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE == hFile) {
		//DebugBreak();
		return;
	}

	DWORD fileSizeHigh;
	const DWORD bufferSize = ::GetFileSize(hFile, &fileSizeHigh);

	DWORD numberOfBytesRead = 0;
	buffer->resize(bufferSize);
	BOOL b = ::ReadFile(hFile, &buffer->at(0), bufferSize, &numberOfBytesRead, nullptr);
	::CloseHandle(hFile);
}
// �ص�:��ȡ��¼�ٶ��̵�cookie
void CWkeWindow::GetLoginCookieCallBack(wkeWebView webWindow, void* param)
{
	if (!param)return;
	wkeShowWindow(webWindow, true);
	std::string Url = wkeGetURL(webWindow);
	int npos = Url.find("disk/home?");
	std::string strTitle = wkeGetTitle(webWindow);
	strTitle = GetInstance()->m_BaiduPare.Utf8_To_Gbk(strTitle.c_str());
	if (strTitle == "���ֻ�" && Url.find("guidebind?") != std::string::npos)
	{
		wkeLoadURL(webWindow, "https://pan.baidu.com/");
	}
	if (npos != std::string::npos)
	{
		GetInstance()->isLogin = true;
		GetInstance()->strCookies = wkeGetCookie(webWindow);
		if (GetInstance()->strCookies.empty())
		{
			GetInstance()->isLogin = false;
			wkeRunJS(app.window, "showErrorMessage('�˳��˺ź���Ҫ���������������������¼�������´򿪱����');");
			return;
		}
		PVOID jsondata = nullptr;
		std::string strJsonData;
		char szModeleName[MAX_PATH];
		ZeroMemory(szModeleName, MAX_PATH);
		::GetModuleFileNameA(NULL, szModeleName, MAX_PATH);
		if (PathRemoveFileSpecA(szModeleName))
			strcat_s(szModeleName, "\\ui\\GlobalConfig.json");
		if (GetInstance()->ReadFileBuffer(szModeleName, &jsondata) && jsondata != nullptr)
		{
			strJsonData = (char*)jsondata;
			delete[] jsondata;
			rapidjson::Document dc;
			dc.Parse(strJsonData.c_str());
			if (!dc.IsObject())
				return;
			if (dc.HasMember("app") && dc["app"].IsObject())
			{
				rapidjson::Value app = dc["app"].GetObjectW();
				if (app.HasMember("UserInfo") && app["UserInfo"].IsObject())
				{
					rapidjson::Value UserInfo = app["UserInfo"].GetObjectW();
					if (UserInfo.HasMember("downloadSavePath") && UserInfo["downloadSavePath"].IsString())
					{
						GetInstance()->m_downloadSavePath = UserInfo["downloadSavePath"].GetString();
					}
				}
			}
			dc.SetObject();
			rapidjson::Value app(rapidjson::kObjectType);
			rapidjson::Value UserInfo(rapidjson::kObjectType);
			UserInfo.AddMember(rapidjson::StringRef("downloadSavePath"), rapidjson::StringRef(GetInstance()->m_downloadSavePath.c_str()), dc.GetAllocator());
			UserInfo.AddMember(rapidjson::StringRef("loginCookie"), rapidjson::StringRef(GetInstance()->strCookies.c_str()), dc.GetAllocator());
			app.AddMember(rapidjson::StringRef("UserInfo"), UserInfo, dc.GetAllocator());
			dc.AddMember(rapidjson::StringRef("app"), app, dc.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			dc.Accept(writer);
			strJsonData = buffer.GetString();
			GetInstance()->m_BaiduPare.WriteFileBuffer(szModeleName,const_cast<char*>(strJsonData.c_str()), strJsonData.length());
		}
		/*
		�Ѿ���¼�ٶ��õ�¼�ٶȵİ�ť����
		*/
		BaiduUserInfo baiduInfo;
		ZeroMemory(&baiduInfo, sizeof(BaiduUserInfo));
		if (GetInstance()->m_BaiduPare.GetloginBassInfo(baiduInfo, GetInstance()->strCookies))
		{
			std::string strData = str(boost::format("app.DiskUsed = '%1%';  app.DiskTotal = '%2%';") % baiduInfo.strDiskUsed % baiduInfo.strDisktotal);
			wkeRunJS(app.window, strData.c_str());
			wkeRunJS(app.window, "app.tablelistisShwo = true;");
			jsExecState es = wkeGlobalExec(app.window);
			jsValue thisObject = jsGetGlobal(es, "app");
			bool b = jsIsObject(thisObject);
			jsValue func = jsGet(es, thisObject, "updateBaiduList");
			std::string argJson = GetInstance()->m_BaiduPare.GetBaiduFileListInfo("/", GetInstance()->strCookies);
			//argJson = Utf8_To_Gbk(argJson.c_str());
			jsValue jsArg[1] = { jsString(es, argJson.c_str()) };
			jsCall(es, func, thisObject, jsArg, 1);
			wkeDestroyWebWindow(webWindow);
			if (::IsWindow(HWND(param)))
			{
				::PostMessage(HWND(param), WM_CLOSE, NULL, NULL);
			}
		}
	}
}
wkeWebView CWkeWindow::onCreateView(wkeWebView webWindow, void* param, wkeNavigationType navType, const wkeString url, const wkeWindowFeatures* features)
{
	if (!param)return webWindow;
	std::string StrUrl(wkeGetString(url));
	::OutputDebugStringA(wkeGetString(url));
	wkeWebView newMainWindow = wkeCreateWebWindow(WKE_WINDOW_TYPE_POPUP, NULL, features->x, features->y, features->width, features->height);
	wkeSetCspCheckEnable(newMainWindow, false);
	wkePerformCookieCommand(newMainWindow,wkeCookieCommandClearAllCookies);
	wkeReload(newMainWindow);
	if (StrUrl.find("pan.baidu.com") != std::string::npos)
	{
		wkeOnDocumentReady(newMainWindow, GetLoginCookieCallBack, param);
		static auto subCreateView = [](wkeWebView webWindow, void* param, wkeNavigationType navType, const wkeString url, const wkeWindowFeatures* features)->wkeWebView {
			RECT rc;
			::GetClientRect((HWND)param, &rc);
			wkeWebView newWindow = wkeCreateWebWindow(WKE_WINDOW_TYPE_CONTROL, (HWND)param, 0, 0, rc.right-rc.left, rc.bottom-rc.top);
			wkeShowWindow(newWindow, true);
			wkePerformCookieCommand(newWindow, wkeCookieCommandClearAllCookies);
			wkeReload(newWindow);
			wkeOnDocumentReady(newWindow, GetLoginCookieCallBack, param);
			return newWindow;
		};
		::SetWindowText(wkeGetWindowHandle(newMainWindow), _T("��¼�ٶ��˺�"));
		wkeOnCreateView(newMainWindow, subCreateView, wkeGetWindowHandle(newMainWindow));
#ifdef _DEBUG
		wkeSetDebugConfig(app.window, "showDevTools", GetInstance()->m_BaiduPare.Gbk_To_Utf8("E:\\Download\\miniblink-181214\\front_end\\inspector.html").c_str());
#endif
	}
	wkeShowWindow(newMainWindow, true);
	return newMainWindow;
}

void CWkeWindow::UpdateDownloadList(const std::string& strJson)
{
	jsExecState es = wkeGlobalExec(app.window);
	jsValue thisObject = jsGetGlobal(es, "app");
	jsValue func = jsGet(es, thisObject, "UpateDownloadlist");
	jsValue jsArg[1] = { jsString(es, strJson.c_str()) };
	jsCall(es, func, thisObject, jsArg, 1);
}

jsValue CWkeWindow::SysMenuJsNativeFunction(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount < 1)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	if (arg0str == CLOSE_MSG)
		GetInstance()->blinkClose();
	else if (arg0str == MAX_MSG)
		GetInstance()->blinkMaximize();
	else if (arg0str == MIN_MSG)
		GetInstance()->blinkMinimize();
	return jsUndefined();
}

jsValue CWkeWindow::LogOut(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount !=0)
		return jsUndefined();
	//��������ļ�json���cookies
	//ɾ����ʱĿ¼�����ɵ�cookie.tmp�ļ�
	TCHAR szCookiePath[MAX_PATH];
	ZeroMemory(szCookiePath, MAX_PATH);
	::GetTempPath(MAX_PATH, szCookiePath);
	wcscat_s(szCookiePath, _T("cookie.tmp"));
	DeleteFile(szCookiePath);
	PVOID jsondata = nullptr;
	std::string strJsonData;
	char szModeleName[MAX_PATH];
	ZeroMemory(szModeleName, MAX_PATH);
	::GetModuleFileNameA(NULL, szModeleName, MAX_PATH);
	if (PathRemoveFileSpecA(szModeleName))
		strcat_s(szModeleName, "\\ui\\GlobalConfig.json");
	if (GetInstance()->ReadFileBuffer(szModeleName, &jsondata) && jsondata != nullptr)
	{
		strJsonData = (char*)jsondata;
		delete[] jsondata;
		rapidjson::Document dc;
		dc.Parse(strJsonData.c_str());
		if (!dc.IsObject())
			return jsUndefined();
		if (dc.HasMember("app") && dc["app"].IsObject())
		{
			rapidjson::Value app = dc["app"].GetObjectW();
			if (app.HasMember("UserInfo") && app["UserInfo"].IsObject())
			{
				rapidjson::Value UserInfo = app["UserInfo"].GetObjectW();
				if (UserInfo.HasMember("downloadSavePath") && UserInfo["downloadSavePath"].IsString())
				{
					GetInstance()->m_downloadSavePath = UserInfo["downloadSavePath"].GetString();
				}
			}
		}
		dc.SetObject();
		rapidjson::Value app(rapidjson::kObjectType);
		rapidjson::Value UserInfo(rapidjson::kObjectType);
		UserInfo.AddMember(rapidjson::StringRef("downloadSavePath"), rapidjson::StringRef(GetInstance()->m_downloadSavePath.c_str()), dc.GetAllocator());
		UserInfo.AddMember(rapidjson::StringRef("loginCookie"), rapidjson::StringRef(""), dc.GetAllocator());
		app.AddMember(rapidjson::StringRef("UserInfo"), UserInfo, dc.GetAllocator());
		dc.AddMember(rapidjson::StringRef("app"), app, dc.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		dc.Accept(writer);
		strJsonData = buffer.GetString();
		GetInstance()->m_BaiduPare.WriteFileBuffer(szModeleName, const_cast<char*>(strJsonData.c_str()), strJsonData.length());
	}
	wkeRunJS(app.window, "app.tablelistisShwo = false;app.DiskUsed = '';  app.DiskTotal = '';");
	GetInstance()->strCookies = "";
	GetInstance()->isLogin = false;
	return jsUndefined();
}

jsValue CWkeWindow::EnumFolder(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount != 0)
		return jsUndefined();
	auto updateSavelist = [es]() {
		rapidjson::Document dcdata;
		GetInstance()->m_BaiduPare.EnumAllFolder("/", GetInstance()->strCookies, dcdata);
		{
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			dcdata.Accept(writer);
			std::string strResultJson = buffer.GetString();
			PostMessage(GetInstance()->m_hwnd, UI_UPDATE_FOLODER_LIST_MSG, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResultJson));
		}
	};
	std::thread upthread(updateSavelist);
	upthread.detach();
	return jsUndefined();
}

jsValue CWkeWindow::OffLineDownload(jsExecState es, void* param)
{
	jsValue bResult = jsInt(0);
	if (!param)return bResult;
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount < 3)
		return bResult;
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return bResult;
	type = jsArgType(es, 1);
	if (JSTYPE_STRING != type)
		return bResult;
	type = jsArgType(es, 2);
	if (JSTYPE_NUMBER != type)
		return bResult;
	jsValue arg0 = jsArg(es, 0);
	jsValue arg1 = jsArg(es, 1);
	jsValue arg2 = jsArg(es, 2);
	std::string argUrl = jsToTempString(es, arg0);
	std::string argPath = jsToTempString(es, arg1);
	int nType = jsToInt(es, arg2);
	if (GetInstance()->strCookies.empty())
		return bResult;
	auto OffDownload = [](int nType,std::string argUrl,std::string argPath) {
		switch (nType)
		{
		case 1:
		{
			//ֻ�Ǹ������������б�
			REGEXVALUE Tasklist;
			ZeroMemory(&Tasklist, sizeof(REGEXVALUE));
			Tasklist = GetInstance()->m_BaiduPare.QueryOffLineList(GetInstance()->strCookies);
			std::string strResult = GetInstance()->m_BaiduPare.QueryTaskIdListStatus(Tasklist, GetInstance()->strCookies);
			if (!strResult.empty())
			{
				PostMessage(GetInstance()->m_hwnd, UI_UPDATE_OFF_LINE_LIST_MSG, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResult));
			}
			else
			{
				strResult = "{\"data\":[]}";
				PostMessage(GetInstance()->m_hwnd, UI_UPDATE_OFF_LINE_LIST_MSG, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResult));
			}
		}
		break;
		case 2:
		{
			//���������������
			REGEXVALUE Tasklist;
			ZeroMemory(&Tasklist, sizeof(REGEXVALUE));
			argUrl = GetInstance()->m_BaiduPare.Utf8_To_Gbk(argUrl.c_str());
			argPath = GetInstance()->m_BaiduPare.Utf8_To_Gbk(argPath.c_str());
			std::string strTaskID = GetInstance()->m_BaiduPare.AddOfflineDownload(argUrl, argPath, GetInstance()->strCookies);
			if (!strTaskID.empty())
			{
				Tasklist = GetInstance()->m_BaiduPare.QueryOffLineList(GetInstance()->strCookies);
				auto is_exist = [strTaskID](const REGEXVALUE& Tasklist)->bool {
					bool bResult = false;
					for (size_t i = 0; i < Tasklist.size(); i++)
					{
						if (Tasklist.at(0) == strTaskID)
						{
							bResult = true;
							break;
						}
					}
					return bResult;
				};
				if (!is_exist(Tasklist))
					Tasklist.push_back(strTaskID);
			}
			std::string strResult = GetInstance()->m_BaiduPare.QueryTaskIdListStatus(Tasklist, GetInstance()->strCookies);
			if (!strResult.empty())
			{
				PostMessage(GetInstance()->m_hwnd, UI_UPDATE_OFF_LINE_LIST_MSG, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResult));
			}
			else
			{
				strResult = "{\"data\":[]}";
				PostMessage(GetInstance()->m_hwnd, UI_UPDATE_OFF_LINE_LIST_MSG, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResult));
			}
		}
		break;
		case 3:
		{
			//���������������
			REGEXVALUE Tasklist;
			ZeroMemory(&Tasklist, sizeof(REGEXVALUE));
			argUrl = GetInstance()->m_BaiduPare.Utf8_To_Gbk(argUrl.c_str());
			rapidjson::Document dc;
			dc.Parse(argUrl.c_str());
			if (!dc.IsObject())
				return;
			if (dc.HasMember("data") && dc["data"].IsArray())
			{
				for (auto & v:dc["data"].GetArray())
				{
					if (v.IsString())
					{
						Tasklist.push_back(v.GetString());
					}
				}
				GetInstance()->m_BaiduPare.DeleteOffLineTask(Tasklist, GetInstance()->strCookies);
				Tasklist.clear();
				Tasklist = GetInstance()->m_BaiduPare.QueryOffLineList(GetInstance()->strCookies);
			}
			std::string strResult = GetInstance()->m_BaiduPare.QueryTaskIdListStatus(Tasklist, GetInstance()->strCookies);
			if (!strResult.empty())
			{
				PostMessage(GetInstance()->m_hwnd, UI_UPDATE_OFF_LINE_LIST_MSG, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResult));
			}
			else
			{
				strResult = "{\"data\":[]}";
				PostMessage(GetInstance()->m_hwnd, UI_UPDATE_OFF_LINE_LIST_MSG, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResult));
			}
		}
		break;
		default:
			break;
		}
	};
	std::thread OffThread(OffDownload,nType,argUrl,argPath);
	OffThread.detach();
	bResult = jsInt(1);
	return bResult;
}

void CWkeWindow::blinkMaximize()
{
	HWND hwnd = getHWND();
	static bool isMax = true;
	if (isMax)
		::PostMessageW(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
	else
		::PostMessageW(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
	isMax = !isMax;
}

void CWkeWindow::blinkMinimize()
{
	HWND hwnd = getHWND();
	::PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}

void CWkeWindow::blinkClose()
{
	HWND hwnd = getHWND();
	::PostMessageW(hwnd, WM_CLOSE, 0, 0);
}
HWND CWkeWindow::getHWND()
{

	if (app.window)
		return wkeGetWindowHandle(app.window);
	return NULL;
}

jsValue CWkeWindow::IsLoginBaidu(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	int argCount = jsArgCount(es);
	if (argCount != 0)
		return jsUndefined();
	jsValue result = jsInt(0);
	if (!GetInstance()->strCookies.empty() && GetInstance()->isLogin)
	{
		/*
		�Ѿ���¼�ٶ��õ�¼�ٶȵİ�ť����
		*/
		wkeRunJS(app.window, "app.tablelistisShwo = true;");
		jsExecState es = wkeGlobalExec(app.window);
		jsValue thisObject = jsGetGlobal(es, "app");
		bool b = jsIsObject(thisObject);
		jsValue func = jsGet(es, thisObject, "updateBaiduList");
		std::string argJson = GetInstance()->m_BaiduPare.GetBaiduFileListInfo("/", GetInstance()->strCookies);
		//argJson = Utf8_To_Gbk(argJson.c_str());
		jsValue jsArg[1] = { jsString(es, argJson.c_str()) };
		jsCall(es, func, thisObject, jsArg, 1);
		result = jsInt(1);
	}
	PVOID jsondata = nullptr;
	std::string strJsonData;
	char szModeleName[MAX_PATH];
	ZeroMemory(szModeleName, MAX_PATH);
	::GetModuleFileNameA(NULL, szModeleName, MAX_PATH);
	if (PathRemoveFileSpecA(szModeleName))
		strcat_s(szModeleName, "\\ui\\GlobalConfig.json");
	if (GetInstance()->ReadFileBuffer(szModeleName, &jsondata) && jsondata != nullptr)
	{
		strJsonData = (char*)jsondata;
		delete[] jsondata;
		rapidjson::Document dc;
		dc.Parse(strJsonData.c_str());
		if(!dc.IsObject())
			return result;
		if (dc.HasMember("app") && dc["app"].IsObject())
		{
			rapidjson::Value app = dc["app"].GetObjectW();
			if (app.HasMember("UserInfo") && app["UserInfo"].IsObject())
			{
				rapidjson::Value UserInfo = app["UserInfo"].GetObjectW();
				if (UserInfo.HasMember("downloadSavePath") && UserInfo["downloadSavePath"].IsString())
				{
					GetInstance()->m_downloadSavePath = UserInfo["downloadSavePath"].GetString();
				}
				if (UserInfo.HasMember("loginCookie") && UserInfo["loginCookie"].IsString())
				{
					GetInstance()->strCookies = UserInfo["loginCookie"].GetString();
				}
			}
		}
		if (!GetInstance()->strCookies.empty())
		{
			BaiduUserInfo baiduInfo;
			ZeroMemory(&baiduInfo, sizeof(BaiduUserInfo));
			auto updateSavelist = [es]() {
				rapidjson::Document dcdata;
				GetInstance()->m_BaiduPare.EnumAllFolder("/", GetInstance()->strCookies, dcdata);
				{
					rapidjson::StringBuffer buffer;
					rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
					dcdata.Accept(writer);
					std::string strResultJson = buffer.GetString();
					PostMessage(GetInstance()->m_hwnd, UI_UPDATE_FOLODER_LIST_MSG, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResultJson));
				}
			};
			std::thread upthread(updateSavelist);
			upthread.detach();
			if (GetInstance()->m_BaiduPare.GetloginBassInfo(baiduInfo, GetInstance()->strCookies))
			{
				if (!baiduInfo.bdstoken.empty() && !baiduInfo.strUserName.empty())
				{
					GetInstance()->isLogin = true;
					/*
					�Ѿ���¼�ٶ��õ�¼�ٶȵİ�ť����
					*/
					std::string strData = str(boost::format("app.tablelistisShwo = true; app.DiskUsed = '%1%';  app.DiskTotal = '%2%';") % baiduInfo.strDiskUsed % baiduInfo.strDisktotal);
					wkeRunJS(app.window, strData.c_str());
					jsExecState es = wkeGlobalExec(app.window);
					jsValue thisObject = jsGetGlobal(es, "app");
					bool b = jsIsObject(thisObject);
					jsValue func = jsGet(es, thisObject, "updateBaiduList");
					std::string argJson = GetInstance()->m_BaiduPare.GetBaiduFileListInfo("/", GetInstance()->strCookies);
					//argJson = Utf8_To_Gbk(argJson.c_str());
					jsValue jsArg[1] = { jsString(es, argJson.c_str()) };
					jsCall(es, func, thisObject, jsArg, 1);
					result = jsInt(1);
				}
			}
		}
	}
	return result;
}

jsValue CWkeWindow::AraiaPauseStartRemove(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	jsValue result = jsInt(0);
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount < 2)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	type = jsArgType(es, 1);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	jsValue arg1 = jsArg(es, 1);
	std::string arg0str = jsToTempString(es, arg0);
	std::string arg1str = jsToTempString(es, arg1);
	if (arg0str == ARIA2_STATUS_ACTIVE)
	{
		std::string strSendText = str(boost::format(ARIA2_UNPAUSE) % arg1str);
		GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(strSendText.c_str()));
	}else if (arg0str == ARIA2_STATUS_PAUSED)
	{
		std::string strSendText = str(boost::format(ARIA2_PAUSE) % arg1str);
		GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(strSendText.c_str()));
	}else if (arg0str == ARIA2_STATUS_REMOVED)
	{
		std::string strSendText = str(boost::format(ARIA2_REMOVE) % arg1str);
		GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(strSendText.c_str()));
	}
	else if (arg0str == "allactive")
	{
		GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(ARIA2_UNAUSEALL));
	}
	else if (arg0str == "allpaused")
	{
		GetInstance()->SendText(GetInstance()->m_BaiduPare.Gbk_To_Utf8(ARIA2_FORCEPAUSEALL));
	}
	return jsInt(1);
}

jsValue CWkeWindow::SwitchDirPath(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	jsValue result = jsInt(0);
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount < 1)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	auto UpdateTable = [](std::string strData) {
		strData = GetInstance()->m_BaiduPare.Utf8_To_Gbk(strData.c_str());
		std::string argJson = GetInstance()->m_BaiduPare.GetBaiduFileListInfo(strData, GetInstance()->strCookies);
		PostMessage(GetInstance()->m_hwnd, UI_UPDATE_USER_FILE_DATA_MSG, NULL, (LPARAM)GetInstance()->AlloclocalHeap(argJson));
	};
	std::thread updateTableThread(UpdateTable, arg0str);
	updateTableThread.detach();
	//jsExecState es = wkeGlobalExec(app.window);
	//arg0str = GetInstance()->m_BaiduPare.Utf8_To_Gbk(arg0str.c_str());
	//jsValue thisObject = jsGetGlobal(es, "app");
	//bool b = jsIsObject(thisObject);
	//jsValue func = jsGet(es, thisObject, "updateBaiduList");
	//jsValue jsArg[1] = { jsString(es, argJson.c_str()) };
	//jsCall(es, func, thisObject, jsArg, 1);
	result = jsInt(1);
	return result;
}

jsValue CWkeWindow::OpenAssignUrl(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount != 1)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	if (arg0str.empty())
		return jsUndefined();
	::ShellExecuteA(NULL, "open", arg0str.c_str(), NULL, NULL, SW_SHOWNORMAL);
	return jsUndefined();
}

jsValue CWkeWindow::UpdateApp(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount != 1)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	if (arg0str.empty())
		return jsUndefined();
	std::string strCommandlineArg;
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	si.dwFlags = STARTF_USESHOWWINDOW;  // ָ��wShowWindow��Ա��Ч
	si.wShowWindow = true;          // �˳�Ա��ΪTRUE�Ļ�����ʾ�½����̵������ڣ�
	CHAR szPath[MAX_PATH];
	CHAR szTemp[MAX_PATH];
	ZeroMemory(szPath, MAX_PATH);
	ZeroMemory(szTemp, MAX_PATH);
	::GetTempPathA(MAX_PATH, szTemp);
	::GetModuleFileNameA(NULL, szPath, MAX_PATH);
	if (PathRemoveFileSpecA(szPath))
	{
		strcat_s(szPath, "\\update.exe %1% %2%");
		strCommandlineArg = szPath;
		strCommandlineArg = str(boost::format(strCommandlineArg) % arg0str % szTemp);
		BOOL bRet = ::CreateProcessA(NULL,           // ���ڴ�ָ����ִ���ļ����ļ���
			const_cast<char*>(strCommandlineArg.c_str()),      // �����в���
			NULL,           // Ĭ�Ͻ��̰�ȫ��
			NULL,           // Ĭ���̰߳�ȫ��
			FALSE,          // ָ����ǰ�����ڵľ�������Ա��ӽ��̼̳�
			NULL, 
			NULL,           // ʹ�ñ����̵Ļ�������
			NULL,           // ʹ�ñ����̵���������Ŀ¼
			&si,
			&pi);
		::CloseHandle(pi.hProcess);
		::PostMessage(GetInstance()->m_hwnd, UI_QUIT_MSG, NULL, NULL);
	}
	return jsUndefined();
}

jsValue CWkeWindow::isUpdate(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount != 1)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	if(arg0str.empty())
		return jsUndefined();
	std::string strUpdateUrl = API_DOMAIN_NAME;
	strUpdateUrl += "/update.php?version=" + arg0str;
	HttpRequest updateHttp;
	updateHttp.Send(GET, strUpdateUrl);
	strUpdateUrl = updateHttp.GetResponseText();
	return jsString(es, strUpdateUrl.c_str());
}

jsValue CWkeWindow::DownloadUserFile(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	//��ȡ����������
	int argCount = jsArgCount(es);
	if (argCount < 1)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	//arg0str = GetInstance()->m_BaiduPare.Utf8_To_Gbk(arg0str.c_str());
	auto proc = [arg0str]() {
		if (!GetInstance()->DownloadUserLocalFile(arg0str))
			LOG(INFO) << arg0str + "\t����ʧ��";
	};
	std::thread DownloadProc(proc);
	DownloadProc.detach();
	return jsUndefined();
}

jsValue CWkeWindow::ShareBaiduFile(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	int argCount = jsArgCount(es);
	if (argCount < 3)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	std::string strRetResult;
	SHAREFILEINFO shareinfo;
	if (arg0str == "1") //˽�ܷ���
	{
		type = jsArgType(es, 1);
		if (JSTYPE_NUMBER !=type)
			return jsString(es, strRetResult.c_str());
		jsValue arg1 = jsArg(es, 1);
		int period = jsToInt(es, arg1);
		type = jsArgType(es, 2);
		if (JSTYPE_STRING != type)
			return jsString(es, strRetResult.c_str());
		jsValue arg2 = jsArg(es, 2);
		std::string fid_list = jsToTempString(es, arg2);
		type = jsArgType(es, 3);
		if (JSTYPE_STRING != type)
			return jsString(es, strRetResult.c_str());
		jsValue arg3 = jsArg(es, 3);
		std::string pwd = jsToTempString(es, arg3);
		switch (period)
		{
		case 0: //���÷���
		{
					shareinfo.nschannel = 1;
					shareinfo.strpath_list = fid_list;
					shareinfo.strPwd = pwd;
					shareinfo.strperiod = std::to_string(period);
					strRetResult = GetInstance()->m_BaiduPare.ShareBaiduFile(shareinfo, GetInstance()->strCookies);
		}
		break;
		case 7:		//7�����
		{
						shareinfo.nschannel = 1;
						shareinfo.strpath_list = fid_list;
						shareinfo.strPwd = pwd;
						shareinfo.strperiod = std::to_string(period);
						strRetResult = GetInstance()->m_BaiduPare.ShareBaiduFile(shareinfo, GetInstance()->strCookies);
		}
		break;
		case 1:  //1�����
		{
					 shareinfo.nschannel = 1;
					 shareinfo.strpath_list = fid_list;
					 shareinfo.strPwd = pwd;
					 shareinfo.strperiod = std::to_string(period);
					 strRetResult = GetInstance()->m_BaiduPare.ShareBaiduFile(shareinfo, GetInstance()->strCookies);
		}
		break;
		default:
			break;
		}
	}else if (arg0str == "2") //��������
	{
		type = jsArgType(es, 1);
		if (JSTYPE_NUMBER != type)
			return jsString(es, strRetResult.c_str());
		jsValue arg1 = jsArg(es, 1);
		int period = jsToInt(es, arg1);
		type = jsArgType(es, 2);
		if (JSTYPE_STRING != type)
			return jsString(es, strRetResult.c_str());
		jsValue arg2 = jsArg(es, 2);
		std::string fid_list = jsToTempString(es, arg2);
		switch (period)
		{
		case 0: //���÷���
		{
					shareinfo.nschannel = 0;
					shareinfo.strpath_list = fid_list;
					shareinfo.strperiod = std::to_string(period);
					strRetResult = GetInstance()->m_BaiduPare.ShareBaiduFile(shareinfo, GetInstance()->strCookies);
		}
			break;
		case 7:		//7�����
		{
						shareinfo.nschannel = 0;
						shareinfo.strpath_list = fid_list;
						shareinfo.strperiod = std::to_string(period);
						strRetResult = GetInstance()->m_BaiduPare.ShareBaiduFile(shareinfo, GetInstance()->strCookies);
		}
			break;
		case 1:  //1�����
		{
					 shareinfo.nschannel = 0;
					 shareinfo.strpath_list = fid_list;
					 shareinfo.strperiod = std::to_string(period);
					 strRetResult = GetInstance()->m_BaiduPare.ShareBaiduFile(shareinfo, GetInstance()->strCookies);
		}
			break;
		default:
			break;
		}
	}
	return jsString(es, strRetResult.c_str());
}

jsValue CWkeWindow::BaiduFileRename(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	int argCount = jsArgCount(es);
	if (argCount < 2)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	type = jsArgType(es, 1);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	jsValue arg1 = jsArg(es, 1);
	std::string arg0str2 = jsToTempString(es, arg1);
	std::string strResultJson = GetInstance()->m_BaiduPare.BaiduRename(GetInstance()->m_BaiduPare.Utf8_To_Gbk(arg0str.c_str()), GetInstance()->m_BaiduPare.Utf8_To_Gbk(arg0str2.c_str()),GetInstance()->strCookies);
	jsValue thisObject = jsGetGlobal(es, "app");
	jsValue func = jsGet(es, thisObject, "updateBaiduList");
	jsValue jsArg[1] = { jsString(es, strResultJson.c_str()) };
	jsCall(es, func, thisObject, jsArg, 1);
	return jsUndefined();
}

jsValue CWkeWindow::DeleteBaiduFile(jsExecState es, void* param)
{
	if (!param)return jsUndefined();
	int argCount = jsArgCount(es);
	if (argCount < 1)
		return jsUndefined();
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return jsUndefined();
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	std::string argJson = GetInstance()->m_BaiduPare.DeleteBaiduFile(arg0str, GetInstance()->strCookies);
	jsValue thisObject = jsGetGlobal(es, "app");
	jsValue func = jsGet(es, thisObject, "updateBaiduList");
	jsValue jsArg[1] = { jsString(es, argJson.c_str()) };
	jsCall(es, func, thisObject, jsArg, 1);
	return jsUndefined();
}

jsValue CWkeWindow::OpenFilePlaceFolder(jsExecState es, void* param)
{
	jsValue bResult = jsInt(0);
	if (!param)return jsUndefined();
	int argCount = jsArgCount(es);
	if (argCount < 2)
		return bResult;
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return bResult;
	type = jsArgType(es, 1);
	if (JSTYPE_NUMBER != type)
		return bResult;
	jsValue arg0 = jsArg(es, 0);
	jsValue arg1 = jsArg(es, 1);
	std::string arg0str = jsToTempString(es, arg0);
	int nType = jsToInt(es, arg1);
	if (arg0str.empty())
	{
		return bResult;
	}
	arg0str = GetInstance()->m_BaiduPare.Utf8_To_Gbk(arg0str.c_str());
	switch (nType)
	{
	case 1:
	{
		if (_access(arg0str.c_str(), 0) != -1)
		{
			std::string strArg = "/select,";
			arg0str = GetInstance()->replace_all_distinct(arg0str, "/", "\\");
			arg0str = GetInstance()->replace_all_distinct(arg0str, "\\\\", "\\");
			strArg += arg0str;
			ShellExecuteA(NULL, "open", "explorer", strArg.c_str(), NULL, SW_SHOW);
			bResult = jsInt(1);
		}
	}
	break;
	case 2:
	{
		if (_access(arg0str.c_str(), 0) != -1)
		{
			arg0str = GetInstance()->replace_all_distinct(arg0str, "/", "\\");
			arg0str = GetInstance()->replace_all_distinct(arg0str, "\\\\", "\\");
			ShellExecuteA(NULL, "open", arg0str.c_str(), NULL, NULL, SW_SHOW);
			bResult = jsInt(1);
		}
	}
	case 3:
	{
		if (_access(arg0str.c_str(), 0) != -1)
		{
			arg0str = GetInstance()->replace_all_distinct(arg0str, "/", "\\");
			arg0str = GetInstance()->replace_all_distinct(arg0str, "\\\\", "\\");
			DeleteFileA(arg0str.c_str());
			bResult = jsInt(1);
		}
	}
	break;
	break;
	default:
		break;
	}
	return bResult;
}

jsValue CWkeWindow::switchShareFolder(jsExecState es, void* param)
{
	jsValue bResult = jsInt(0);
	if (!param)return jsUndefined();
	int argCount = jsArgCount(es);
	if (argCount < 7)
		return bResult;
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return bResult;
	type = jsArgType(es, 1);
	if (JSTYPE_STRING != type)
		return bResult;
	type = jsArgType(es, 2);
	if (JSTYPE_STRING != type)
		return bResult;
	type = jsArgType(es, 3);
	if (JSTYPE_STRING != type)
		return bResult;
	type = jsArgType(es, 4);
	if (JSTYPE_STRING != type)
		return bResult;
	type = jsArgType(es, 5);
	if (JSTYPE_STRING != type)
		return bResult;
	type = jsArgType(es, 6);
	if (JSTYPE_STRING != type)
		return bResult;
	jsValue arg0 = jsArg(es, 0);
	jsValue arg1 = jsArg(es, 1);
	jsValue arg2 = jsArg(es, 2);
	jsValue arg3 = jsArg(es, 3);
	jsValue arg4 = jsArg(es, 4);
	jsValue arg5 = jsArg(es, 5);
	jsValue arg6 = jsArg(es, 6);
	std::string strPath = jsToTempString(es, arg0);
	std::string strCookie = jsToTempString(es, arg1);
	strPath = GetInstance()->m_BaiduPare.Utf8_To_Gbk(strPath.c_str());
	BAIDUREQUESTINFO userinfo;
	ZeroMemory(&userinfo, sizeof(BAIDUREQUESTINFO));
	userinfo.uk = jsToTempString(es, arg2);
	userinfo.shareid = jsToTempString(es, arg3);
	userinfo.bdstoken = jsToTempString(es, arg4);
	userinfo.timestamp = jsToTempString(es, arg5);
	userinfo.sign = jsToTempString(es, arg6);
	auto SwithFolder = [strPath, strCookie](const BAIDUREQUESTINFO& info) {
		std::string strResultJosn = GetInstance()->m_BaiduPare.GetBaiduShareFileListInfo(strPath, strCookie, info);
		::PostMessage(GetInstance()->m_hwnd, UI_DOWNLOAD_SHARE_UPDATE_LIST, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResultJosn));
	};
	std::thread DownThread(SwithFolder, userinfo);
	DownThread.detach();
	bResult = jsInt(1);
	return bResult;
}

jsValue CWkeWindow::DownShareFile(jsExecState es, void* param)
{
	jsValue bResult = jsInt(0);
	if (!param)return jsUndefined();
	int argCount = jsArgCount(es);
	if (argCount < 1)
		return bResult;
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return bResult;
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	int nType = GetInstance()->JudgeDownUrlType(arg0str);
	switch (nType)
	{
	case 1: /*�ٶ�����*/
	{
				std::string strTempCookie(GetInstance()->strCookies);
				if (strTempCookie.empty())
					return bResult;
				//�Ż��������ʹ���̲߳�Ȼ������
				auto BaiduDownloadThreadProc = [arg0str](std::string& TempCookie) {
					std::string strResultJosn = GetInstance()->m_BaiduPare.AnalysisShareUrlInfo(arg0str, TempCookie);
					::PostMessage(GetInstance()->m_hwnd, UI_DOWNLOAD_SHARE_UPDATE_LIST, NULL, (LPARAM)GetInstance()->AlloclocalHeap(strResultJosn));
				};
				std::thread DownThread(BaiduDownloadThreadProc, strTempCookie);
				DownThread.detach();
				bResult = jsInt(1);
	}
	break;
	case 2: /*��������*/
	{

	}
	break;
	case 3:/*��ͨ����*/
	{

	}
	break;
	default:
	{
			   REQUESTINFO rResult;
			   std::string strJson;
			   ZeroMemory(&rResult, sizeof(REQUESTINFO));
			   rResult.strDownloadUrl = arg0str;
			   rResult.strSavePath = GetInstance()->m_downloadSavePath;
			   strJson = GetInstance()->CreateDowndAria2Json(rResult);
			   GetInstance()->SendText(strJson);
			   bResult = jsInt(1);
	}
		break;
	}
	return bResult;
}

jsValue CWkeWindow::DownSelectShareFile(jsExecState es, void* param)
{
	jsValue bResult = jsInt(0);
	if (!param)return jsUndefined();
	int argCount = jsArgCount(es);
	if (argCount < 1)
		return bResult;
	jsType type = jsArgType(es, 0);
	if (JSTYPE_STRING != type)
		return bResult;
	jsValue arg0 = jsArg(es, 0);
	std::string arg0str = jsToTempString(es, arg0);
	arg0str = GetInstance()->m_BaiduPare.Utf8_To_Gbk(arg0str.c_str());
	typedef std::function<void(const std::string& strJson)> argfn;
	argfn  fn = std::bind(&CWkeWindow::DownloadUiShareFile, (CWkeWindow*)param, _1);
	std::thread SelectThread(fn, arg0str);
	SelectThread.detach();
	bResult = jsInt(1);
	return bResult;
}

int CWkeWindow::JudgeDownUrlType(const std::string& strUrl)
{
	int nResult = 0;
	if (strUrl.empty())
		return nResult;
	std::string StrHost;
	StrHost = CarkUrl(strUrl.c_str());
	if (!_strcmpi(StrHost.c_str(), "http://pan.baidu.com"))
		nResult = 1;
	else if (!_strcmpi(StrHost.c_str(), "http://www.lanzous.com"))
		nResult = 2;
	else if (StrHost.find("ctfile.com")!=std::string::npos)
	{
		nResult = 3;
	}
	return nResult;
}

std::string CWkeWindow::CreateDowndAria2Json(REQUESTINFO& Downdinfo)
{
	std::string strResultJson;
	if (Downdinfo.strDownloadUrl.empty())
		return strResultJson;
	rapidjson::Document dc;
	dc.SetObject();
	rapidjson::Value Method(rapidjson::kStringType);
 	const char method[] = "aria2.addUri";
	Method.SetString(method, strlen(method), dc.GetAllocator());
	dc.AddMember(rapidjson::StringRef("method"), Method, dc.GetAllocator());
	rapidjson::Value Params(rapidjson::kArrayType);
	rapidjson::Value token(rapidjson::kStringType);
	token.SetString("token:CDP", strlen("token:CDP"), dc.GetAllocator());
	Params.PushBack(token, dc.GetAllocator());
	rapidjson::Value downloadUrlList(rapidjson::kArrayType);
	for (size_t i = 0; i < 1;i++)
	{
		rapidjson::Value downloadUrl(rapidjson::kStringType);
		downloadUrl.SetString(Downdinfo.strDownloadUrl.c_str(), Downdinfo.strDownloadUrl.length(), dc.GetAllocator());
		downloadUrlList.PushBack(downloadUrl,dc.GetAllocator());
	}
	Params.PushBack(downloadUrlList,dc.GetAllocator());
	rapidjson::Value itemObj(rapidjson::kObjectType);
	rapidjson::Value out(rapidjson::kStringType);
	out.SetString(Downdinfo.strFileName.c_str(), Downdinfo.strFileName.length(), dc.GetAllocator());
	itemObj.AddMember(rapidjson::StringRef("out"), out, dc.GetAllocator());
	rapidjson::Value dir(rapidjson::kStringType);
	dir.SetString(Downdinfo.strSavePath.c_str(), Downdinfo.strSavePath.length(), dc.GetAllocator());
	itemObj.AddMember(rapidjson::StringRef("dir"), dir, dc.GetAllocator());
	rapidjson::Value user_agent(rapidjson::kStringType);
	if (Downdinfo.strUserAegnt.empty())
	{
		Downdinfo.strUserAegnt = USER_AGENT;
	}
	user_agent.SetString(Downdinfo.strUserAegnt.c_str(), Downdinfo.strUserAegnt.length(), dc.GetAllocator());
	itemObj.AddMember(rapidjson::StringRef("user-agent"), user_agent, dc.GetAllocator());
	rapidjson::Value max_tries(rapidjson::kStringType);
	max_tries.SetString("10", strlen("10"), dc.GetAllocator());
	itemObj.AddMember(rapidjson::StringRef("max-tries"), max_tries, dc.GetAllocator());
	rapidjson::Value timeout(rapidjson::kStringType);
	timeout.SetString("5", strlen("5"), dc.GetAllocator());
	itemObj.AddMember(rapidjson::StringRef("timeout"), timeout, dc.GetAllocator());
	rapidjson::Value connect_timeout(rapidjson::kStringType);
	connect_timeout.SetString("5", strlen("5"), dc.GetAllocator());
	itemObj.AddMember(rapidjson::StringRef("connect-timeout"), connect_timeout, dc.GetAllocator());
	rapidjson::Value split(rapidjson::kStringType);
	split.SetString("128", strlen("128"), dc.GetAllocator());
	itemObj.AddMember(rapidjson::StringRef("split"), split, dc.GetAllocator());
	rapidjson::Value retry_wait(rapidjson::kStringType);
	retry_wait.SetString("10", strlen("10"), dc.GetAllocator());
	itemObj.AddMember(rapidjson::StringRef("retry-wait"), retry_wait, dc.GetAllocator());
	Params.PushBack(itemObj, dc.GetAllocator());
	dc.AddMember(rapidjson::StringRef("params"), Params, dc.GetAllocator());
	rapidjson::Value id(rapidjson::kNumberType);
	id.SetInt(1);
	dc.AddMember(rapidjson::StringRef("id"), id, dc.GetAllocator());
	rapidjson::Value jsonrpc(rapidjson::kStringType);
	jsonrpc.SetString("2.0", strlen("2.0"), dc.GetAllocator());
	dc.AddMember(rapidjson::StringRef("jsonrpc"), jsonrpc, dc.GetAllocator());
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	dc.Accept(writer);
	strResultJson = buffer.GetString();
	strResultJson = m_BaiduPare.Gbk_To_Utf8(strResultJson.c_str());
	return strResultJson;
}
std::string& CWkeWindow::replace_all_distinct(std::string& str, const std::string& old_value, const std::string& new_value)
{
	for (std::string::size_type pos(0); pos != std::string::npos; pos += new_value.length()) {
		if ((pos = str.find(old_value, pos)) != std::string::npos)
			str.replace(pos, old_value.length(), new_value);
		else   break;
	}
	return   str;
}
bool CWkeWindow::DownloadUserLocalFile(const std::string& strJsonData)
{
	bool bResult = false;
	const std::string& strCookie = GetInstance()->strCookies;
	if (strJsonData.empty())
	{
		return bResult;
	}
	rapidjson::Document dc;
	dc.Parse(strJsonData.c_str());
	if (!dc.IsObject())
		return bResult;
	if (dc.HasMember("data") && dc["data"].IsArray())
	{
		for (auto& v : dc["data"].GetArray())
		{
			if (v.HasMember("isdir") && v["isdir"].IsUint() && v.HasMember("path") && v["path"].IsString())
			{
				UINT nIsdir = v["isdir"].GetUint();
				std::string strPath = v["path"].GetString();
				strPath = m_BaiduPare.Utf8_To_Gbk(strPath.c_str());
				std::string strResultJson;
				if (nIsdir)
				{
					strResultJson = m_BaiduPare.GetBaiduFileListInfo(strPath, strCookie);
					DownloadUserLocalFile(strResultJson);
				}
				else
				{
					//OutputDebugStringA(strPath.c_str());
					strResultJson = m_BaiduPare.GetBaiduLocalFileAddr(m_BaiduPare.URL_Coding(strPath.c_str()), strCookie);
					if (!strResultJson.empty())
					{
						REQUESTINFO rResult;
						std::string strJson;
						ZeroMemory(&rResult, sizeof(REQUESTINFO));
						rResult.strDownloadUrl = strResultJson;
						rResult.strCookies = strCookies;
						rResult.strFileName = strPath;
						rResult.strSavePath = m_downloadSavePath;
						rResult.strUserAegnt = NETDISK_USER_AGENT;
						std::string strAddurl = CreateDowndAria2Json(rResult);
						if (!strAddurl.empty())
						{
							SendText(strAddurl);
							bResult = true;
						}
					}
					//CreateDowndAria2Json
				}
			}
		}
	}
	return bResult;
}

void CWkeWindow::DownloadUiShareFile(const std::string & strJson)
{
	if (strJson.empty())
	{
		return;
	}
	rapidjson::Document dc;
	dc.Parse(strJson.c_str());
	if (!dc.IsObject())
		return;
	if (dc.HasMember("data") && dc["data"].IsArray())
	{
		BAIDUREQUESTINFO info;
		ZeroMemory(&info, sizeof(BAIDUREQUESTINFO));
		if (dc.HasMember("info") && dc["info"].IsObject())
		{
			rapidjson::Value J_info = dc["info"].GetObjectW();
			if (J_info.HasMember("uk") && J_info["uk"].IsString())
				info.uk = J_info["uk"].GetString();
			if (J_info.HasMember("bdstoken") && J_info["bdstoken"].IsString())
				info.bdstoken = J_info["bdstoken"].GetString();
			if (J_info.HasMember("sign") && J_info["sign"].IsString())
				info.sign = J_info["sign"].GetString();
			if (J_info.HasMember("shareid") && J_info["shareid"].IsString())
				info.shareid = J_info["shareid"].GetString();
			if (J_info.HasMember("timestamp") && J_info["timestamp"].IsString())
				info.timestamp = J_info["timestamp"].GetString();
		}
		for (auto &v : dc["data"].GetArray())
		{
			UINT nIsdir = 0;
			std::string T_strCookies;
			if (v.HasMember("isdir") && v["isdir"].IsUint())
				nIsdir = v["isdir"].GetUint();
			if (v.HasMember("cookie") && v["cookie"].IsString())
				T_strCookies = v["cookie"].GetString();
			if (v.HasMember("name") && v["name"].IsString())
				info.server_filename = v["name"].GetString();
			if (v.HasMember("path") && v["path"].IsString())
				info.strPath = v["path"].GetString();
			if (v.HasMember("fs_id") && v["fs_id"].IsString())
				info.fs_id = v["fs_id"].GetString();
			if (nIsdir)
			{
				std::string T_strJson = m_BaiduPare.GetBaiduShareFileListInfo(info.strPath.c_str(), T_strCookies, info);
				DownloadUiShareFile(GetInstance()->m_BaiduPare.Utf8_To_Gbk(T_strJson.c_str()));
			}
			else
			{
				auto BaiduDownloadThreadProc = [T_strCookies](BAIDUREQUESTINFO TempInfo) {
					REQUESTINFO rResult;
					std::string strJson;
					std::string strCookies = T_strCookies;
					ZeroMemory(&rResult, sizeof(REQUESTINFO));
					rResult = GetInstance()->m_BaiduPare.ParseBaiduAddrEx(TempInfo, strCookies);
				/*	rResult.strFileName = GetInstance()->m_BaiduPare.Utf8_To_Gbk(rResult.strFileName.c_str());*/
					rResult.strSavePath = GetInstance()->m_downloadSavePath;
					//GetInstance()->AddShareFileItem(rResult);
					HttpRequest BaiduHttp;
					BaiduHttp.SetRequestHeader("Connection", "keep-alive");
					BaiduHttp.SetRequestHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
					BaiduHttp.SetRequestHeader("Upgrade-Insecure-Requests", "1");
					std::string strHost = GetInstance()->CarkUrl(rResult.strDownloadUrl.c_str());
					int nPos = strHost.find("//");
					if (nPos != std::string::npos)
					{
						strHost = strHost.substr(nPos + 2, strHost.length() - (nPos + 2));
					}
					BaiduHttp.SetRequestHeader("Host", strHost);
					BaiduHttp.SetRequestCookies(rResult.strCookies);
					BaiduHttp.Send(HEAD, rResult.strDownloadUrl);
					rResult.strDownloadUrl = GetInstance()->m_BaiduPare.GetTextMid(BaiduHttp.GetallResponseHeaders(), "Location: ", "\r\n");
					strJson = GetInstance()->CreateDowndAria2Json(rResult);
					GetInstance()->SendText(strJson);
				};
				BaiduDownloadThreadProc(info);
			}
		}
	}
}

CWkeWindow::CWkeWindow()
:isLogin(false)
,strCookies("")
,m_hwnd(NULL)
,numActive(NULL)
,numStopped(NULL)
,numWaiting(NULL)
,m_downloadSavePath("")
,m_ShadowWnd(nullptr)
{
	try
	{
		if (!InintMiniBlink())
		{
			throw std::logic_error("Environment initialization failed");
		}
	}
	catch (...)
	{
		//���Դ����־ֱ�ӽ�������
		LOG(INFO) << "��ʼ��miniblink����ʧ��";
		exit(0);
	}
#if 1
	/*
	��ʼ��ASIO
	*/
	m_WssClient.init_asio();
	// Register our handlers
	m_WssClient.set_socket_init_handler(bind(&CWkeWindow::on_socket_init, this, ::_1));
	m_WssClient.set_message_handler(bind(&CWkeWindow::on_message, this, ::_1, ::_2));
	m_WssClient.set_open_handler(bind(&CWkeWindow::on_open, this, ::_1));
	m_WssClient.set_close_handler(bind(&CWkeWindow::on_close, this, ::_1));
	m_WssClient.set_fail_handler(bind(&CWkeWindow::on_fail, this, ::_1));
#endif
	if (RunAria2())
	{
		m_RunThreadPtr.reset(new std::thread(&CWkeWindow::Connect, this));
	}
}

CWkeWindow::~CWkeWindow()
{
	if (m_RunThreadPtr)
	{
		if (m_RunThreadPtr->joinable())
		{
			if(m_hdl.lock().get())
			{
				m_WssClient.close(m_hdl, websocketpp::close::status::going_away, "close");
			}
			m_RunThreadPtr->join();
		}
		m_RunThreadPtr.reset();
	}
}
CWkeWindow* CWkeWindow::GetInstance()
{
	if (Instance == NULL)
	{
		g_mutx.lock();
		if (!Instance.get())
		{
			//std::shared_ptr<CWkeWindow> temp = std::shared_ptr<CWkeWindow>(new CWkeWindow);
			Instance.reset(new CWkeWindow);
		}
		g_mutx.unlock();
	}
	return Instance.get();
}
