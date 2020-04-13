#include "PanParse.h"
#include <sys/timeb.h>
#include <dbghelp.h>
#include <shlwapi.h>
#include <boost/regex.hpp>
#include <boost/format.hpp>
#include "base64.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/pointer.h"
#include "resource.h"
#pragma comment(lib,"dbghelp.lib")
#pragma comment(lib,"shlwapi.lib")
std::string CBaiduParse::m_vcCodeUrl;
std::string CBaiduParse::m_VerCode;
std::string CBaiduParse::m_SharePass;
DWORD CBaiduParse::WriteFileBuffer(std::string szFileNmae, PVOID pFileBuffer, DWORD dwFileSize)
{
	DWORD dwStates = -1;
	std::string szFilePath, strName;
	size_t npos = -1;
	if (szFileNmae.empty())
		return dwStates;
	npos = szFileNmae.rfind("\\");
	if (npos != std::string::npos)
	{
		szFilePath = szFileNmae.substr(0, npos + 1);
		if (!szFilePath.empty())
		{
			if (MakeSureDirectoryPathExists(szFilePath.c_str()))
			{
				HANDLE hFile = INVALID_HANDLE_VALUE;
				DWORD dwError = NULL;
				hFile = CreateFileA(szFileNmae.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				dwError = ::GetLastError();
				if (hFile != INVALID_HANDLE_VALUE)
				{
					::WriteFile(hFile, pFileBuffer, dwFileSize, &dwStates, NULL);

					::CloseHandle(hFile);
				}
			}
		}
	}

	return dwStates;
}
REQUESTINFO CBaiduParse::ParseBaiduAddr(const std::string strUrl, std::string& strCookies)
{
	std::string strWebUrl = strUrl;
	int nResult = IsPassWordShareUrl(strWebUrl, strCookies);
	HttpRequest BaiduHttp;
	BAIDUREQUESTINFO BaiduInfo;
	REQUESTINFO strRealUrl;
	std::string strDownloadUrl;
	std::string strResponseText;
	BaiduHttp.SetRequestCookies(strCookies);
	BaiduHttp.SetHttpRedirect(true);
	BaiduHttp.Send(GET, strUrl);
	strResponseText = BaiduHttp.GetResponseText();
	strResponseText = Utf8_To_Gbk(strResponseText.c_str());
	if (strResponseText.empty())
		return strRealUrl;
	std::string strJson = GetTextMid(strResponseText, "yunData.setData(", ");");
	BaiduInfo = GetBaiduInfo(strJson);
	strDownloadUrl = "https://pan.baidu.com/api/sharedownload?sign=" + BaiduInfo.sign + \
		"&timestamp=" + BaiduInfo.timestamp + "&channel=chunlei&web=1&app_id=" + BaiduInfo.app_id + \
		"&bdstoken=" + BaiduInfo.bdstoken + "&logid=" + GetLogid() + "&clienttype=0";
	strCookies = BaiduHttp.MergeCookie(strCookies, BaiduHttp.GetResponCookie());
	BaiduInfo.BDCLND = GetTextMid(strCookies, "BDCLND=", ";");
	if (BaiduInfo.BDCLND.empty())
	{
		strJson = "encrypt=0&product=share&uk=%1%&primaryid=%2%&fid_list=%%5B%3%%%5D&path_list=";
		strJson = str(boost::format(strJson) % BaiduInfo.uk % BaiduInfo.shareid % BaiduInfo.fs_id);
	}
	else
	{
		strJson = "encrypt=0&extra=%%7B%%22sekey%%22%%3A%%22%1%%%22%%7D&product=share&uk=%2%&primaryid=%3%&fid_list=%%5B%4%%%5D&path_list=";
		strJson = str(boost::format(strJson) % BaiduInfo.BDCLND % BaiduInfo.uk % BaiduInfo.shareid % BaiduInfo.fs_id);
	}
	/*׼���ύ���ݻ�ȡ��ʵURL��ַ*/
	BaiduHttp.SetRequestHeader("Content-Type", " application/x-www-form-urlencoded; charset=UTF-8");
	BaiduHttp.SetRequestHeader("Accept", " application/json, text/javascript, */*; q=0.01");
	BaiduHttp.SetRequestCookies(strCookies);
	BaiduHttp.Send(POST, strDownloadUrl, strJson);
	strJson = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strJson.c_str());
	if (!dc.IsObject())
		return strRealUrl;
	int nError = -1;
	if (dc.HasMember("errno") && dc["errno"].IsInt())
		nError = dc["errno"].GetInt();
	if (!nError)
	{
		if (dc.HasMember("list") && dc["list"].IsArray())
		{
			for (auto &v : dc["list"].GetArray())
			{
				if (v.HasMember("dlink") && v["dlink"].IsString())
					strRealUrl.strDownloadUrl = v["dlink"].GetString();
			}
		}
	}
	else
	{
		//������Ҫ������֤����
		int index = 0;
		do 
		{
			if (index>5)
				break;
			strRealUrl.strDownloadUrl = GetBaiduAddr(BaiduInfo, strCookies);
			index++;
		} while (strRealUrl.strDownloadUrl.empty());
	}
// 	BaiduHttp.Send(HEAD, strRealUrl.strDownloadUrl);
// 	strRealUrl.strDownloadUrl = GetTextMid(BaiduHttp.GetallResponseHeaders(), "Location: ", "\r\n");
	strRealUrl.strFileName = BaiduInfo.server_filename;
	strRealUrl.strCookies = strCookies;
	return strRealUrl;
}
REQUESTINFO CBaiduParse::ParseBaiduAddrEx(BAIDUREQUESTINFO& BaiduInfo, std::string& strCookies)
{
	HttpRequest BaiduHttp;
	REQUESTINFO strRealUrl;
	std::string strDownloadUrl;
	std::string strJson;
	BaiduInfo.app_id = BaiduInfo.app_id.empty() ? "250528" : BaiduInfo.app_id;
	strDownloadUrl = "https://pan.baidu.com/api/sharedownload?sign=" + BaiduInfo.sign + \
		"&timestamp=" + BaiduInfo.timestamp + "&channel=chunlei&web=1&app_id=" + BaiduInfo.app_id + \
		"&bdstoken=" + BaiduInfo.bdstoken + "&logid=" + GetLogid() + "&clienttype=0";
	BaiduInfo.BDCLND = GetTextMid(strCookies, "BDCLND=", ";");
	if (BaiduInfo.BDCLND.empty())
	{
		strJson = "encrypt=0&product=share&uk=%1%&primaryid=%2%&fid_list=%%5B%3%%%5D&path_list=";
		strJson = str(boost::format(strJson) % BaiduInfo.uk % BaiduInfo.shareid % BaiduInfo.fs_id);
	}
	else
	{
		strJson = "encrypt=0&extra=%%7B%%22sekey%%22%%3A%%22%1%%%22%%7D&product=share&uk=%2%&primaryid=%3%&fid_list=%%5B%4%%%5D&path_list=";
		strJson = str(boost::format(strJson) % BaiduInfo.BDCLND % BaiduInfo.uk % BaiduInfo.shareid % BaiduInfo.fs_id);
	}
	/*׼���ύ���ݻ�ȡ��ʵURL��ַ*/
	BaiduHttp.SetRequestHeader("Content-Type", " application/x-www-form-urlencoded; charset=UTF-8");
	BaiduHttp.SetRequestHeader("Accept", " application/json, text/javascript, */*; q=0.01");
	BaiduHttp.SetRequestCookies(strCookies);
	BaiduHttp.Send(POST, strDownloadUrl, strJson);
	strJson = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strJson.c_str());
	if (!dc.IsObject())
		return strRealUrl;
	int nError = -1;
	if (dc.HasMember("errno") && dc["errno"].IsInt())
		nError = dc["errno"].GetInt();
	if (!nError)
	{
		if (dc.HasMember("list") && dc["list"].IsArray())
		{
			for (auto &v : dc["list"].GetArray())
			{
				if (v.HasMember("dlink") && v["dlink"].IsString())
					strRealUrl.strDownloadUrl = v["dlink"].GetString();
			}
		}
	}
	else
	{
		//������Ҫ������֤����
		int index = 0;
		do
		{
			if (index>5)
				break;
			strRealUrl.strDownloadUrl = GetBaiduAddr(BaiduInfo, strCookies);
			index++;
		} while (strRealUrl.strDownloadUrl.empty());
	}
	// 	BaiduHttp.Send(HEAD, strRealUrl.strDownloadUrl);
	// 	strRealUrl.strDownloadUrl = GetTextMid(BaiduHttp.GetallResponseHeaders(), "Location: ", "\r\n");
	strRealUrl.strFileName = BaiduInfo.server_filename;
	strRealUrl.strCookies = strCookies;
	return strRealUrl;
}
std::string CBaiduParse::AnalysisShareUrlInfo(const std::string strUrl, std::string& strCookie)
{
	std::string strWebUrl = strUrl;
	std::string strResultJson;
	int nResult = IsPassWordShareUrl(strWebUrl, strCookie);
	HttpRequest BaiduHttp;
	BAIDUREQUESTINFO BaiduInfo;
	ZeroMemory(&BaiduInfo, sizeof(BAIDUREQUESTINFO));
	REQUESTINFO strRealUrl;
	std::string strDownloadUrl;
	std::string strResponseText;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.SetHttpRedirect(true);
	BaiduHttp.Send(GET, strUrl);
	strResponseText = BaiduHttp.GetResponseText();
	strResponseText = Utf8_To_Gbk(strResponseText.c_str());
	if (strResponseText.empty())
		return strResultJson;
	std::string strJson = GetTextMid(strResponseText, "yunData.setData(", ");");
	BaiduInfo = GetBaiduInfo(strJson);
// 	if (BaiduInfo.n_isdir)
// 	{
// 		strResultJson = GetBaiduShareFileListInfo(Utf8_To_Gbk(BaiduInfo.strPath.c_str()), strCookie, BaiduInfo);
// 	}
// 	else
// 	{
		rapidjson::Document docjson;
		docjson.SetObject();
		rapidjson::Value array_list(rapidjson::kArrayType);
		rapidjson::Value itemObject(rapidjson::kObjectType);
		rapidjson::Value isdir(rapidjson::kNumberType);
		isdir.SetInt(BaiduInfo.n_isdir);
		itemObject.AddMember(rapidjson::StringRef("isdir"), isdir, docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("cookie"), rapidjson::StringRef(strCookie.c_str()), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("name"), rapidjson::StringRef(BaiduInfo.server_filename.c_str()), docjson.GetAllocator());
		std::string strFilesize = BaiduInfo.server_filename.empty() ? "--" : GetFileSizeType(BaiduInfo.nSize);
		rapidjson::Value FileSize(rapidjson::kStringType);
		FileSize.SetString(strFilesize.c_str(), strFilesize.length(), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("Size"), FileSize, docjson.GetAllocator());
		rapidjson::Value nCategory(rapidjson::kNumberType);
		nCategory.SetInt(BaiduInfo.ncategory);
		itemObject.AddMember(rapidjson::StringRef("nCategory"), nCategory, docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("path"), rapidjson::StringRef(BaiduInfo.strPath.c_str()), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("FileType"), rapidjson::StringRef(::PathFindExtensionA(Utf8_To_Gbk(BaiduInfo.server_filename.c_str()).c_str())), docjson.GetAllocator());
		std::string strTime = BaiduInfo.server_filename.empty() ? "-" :timestampToDate(BaiduInfo.server_time);
		rapidjson::Value FileTime(rapidjson::kStringType);
		FileTime.SetString(strTime.c_str(), strTime.length(), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("ChangeTime"), FileTime, docjson.GetAllocator());
		rapidjson::Value fs_id(rapidjson::kStringType);
		fs_id.SetString(BaiduInfo.fs_id.c_str(), BaiduInfo.fs_id.length(), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("fs_id"), fs_id, docjson.GetAllocator());
		array_list.PushBack(itemObject, docjson.GetAllocator());
		rapidjson::Value info(rapidjson::kObjectType);
		info.AddMember(rapidjson::StringRef("uk"), rapidjson::StringRef(BaiduInfo.uk.c_str()), docjson.GetAllocator());
		info.AddMember(rapidjson::StringRef("bdstoken"), rapidjson::StringRef(BaiduInfo.bdstoken.c_str()), docjson.GetAllocator());
		info.AddMember(rapidjson::StringRef("sign"), rapidjson::StringRef(BaiduInfo.sign.c_str()), docjson.GetAllocator());
		info.AddMember(rapidjson::StringRef("shareid"), rapidjson::StringRef(BaiduInfo.shareid.c_str()), docjson.GetAllocator());
		info.AddMember(rapidjson::StringRef("timestamp"), rapidjson::StringRef(BaiduInfo.timestamp.c_str()), docjson.GetAllocator());
		docjson.AddMember(rapidjson::StringRef("info"), info, docjson.GetAllocator());
		docjson.AddMember(rapidjson::StringRef("data"), array_list, docjson.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		docjson.Accept(writer);
		strResultJson = buffer.GetString();
		//WriteFileBuffer(".\\1.txt", (PVOID)strResultJson.c_str(), strResultJson.length());
	//}
	return strResultJson;
}

int CBaiduParse::IsPassWordShareUrl(std::string& strUrl, std::string& strCookies)
{
	int nResult = -1;
	if (strUrl.empty() || strCookies.empty())
	{
		return nResult;
	}
	HttpRequest BaiduHttp;
	int nPos = std::string::npos;
	BaiduHttp.SetRequestCookies(strCookies);
	BaiduHttp.Send(GET,strUrl);
	strCookies = BaiduHttp.MergeCookie(strCookies, BaiduHttp.GetResponCookie());
	std::string&& strRetHeader = BaiduHttp.GetallResponseHeaders();
	nPos = strRetHeader.find("Location:");
	if (nPos != std::string::npos)
	{
		strUrl = GetTextMid(strRetHeader, "Location:", "\r\n");
		if (!strUrl.empty())
		{
			strUrl.erase(0, strUrl.find_first_not_of(" "));
			nPos = strUrl.rfind("surl=");
			if (nPos!=std::string::npos)
			{
				int index = 0;
				int nError = 0;
				do 
				{

					ShowInputPassDlg();
					std::string strKey = strUrl.substr(nPos + lstrlenA("surl="), strUrl.length() - nPos);
					std::string strReferer = "https://pan.baidu.com/share/init?surl=" + strKey;
					strUrl = str(boost::format("https://pan.baidu.com/share/verify?surl=%1%&t=%2%&channel=chunlei&web=1&app_id=250528&bdstoken=null&logid=%3%&clienttype=0") % strKey % getTimeStamp() % GetLogid());
					std::string strPost = str(boost::format("pwd=%1%&vcode=&vcode_str=") % m_SharePass);
					BaiduHttp.SetRequestHeader("Referer", strReferer);
					BaiduHttp.Send(POST, strUrl, strPost);
					std::string strResult = BaiduHttp.GetResponseText();
					rapidjson::Document dc;
					if (index>5)
						break;
					dc.Parse(strResult.c_str());
					if (!dc.IsObject())
					{
						return nResult;
					}
					if (dc.HasMember("errno") && dc["errno"].IsInt())
					{
						nError = dc["errno"].GetInt();
					}
					index++;
				} while (nError != 0);
				strCookies = BaiduHttp.MergeCookie(strCookies, BaiduHttp.GetResponCookie());
				nResult = 1;
			}else
				nResult = 0;
		}
		else
			nResult = 0;
	}
	else
	{
		nResult = 0;
	}
	return nResult;
}

std::string CBaiduParse::AddOfflineDownload(const std::string& strUrl,const std::string& strSavePath, const std::string& strCookie)
{
	std::string  bResult;
	if (strUrl.empty() || strCookie.empty() || strSavePath.empty())
		return bResult;
	BaiduUserInfo userinfo;
	if (!GetloginBassInfo(userinfo, strCookie))
		return bResult;
	std::string strOffLineUrl = str(boost::format(OFF_LINE_DOWNLOAD_URL) % userinfo.bdstoken % URL_Coding(strSavePath.c_str()) % URL_Coding(strUrl.c_str()));
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.Send(GET, strOffLineUrl);
	std::string strTextData = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strTextData.c_str());
	if (!dc.IsObject())
		return bResult;
	if (dc.HasMember("task_id") && dc["task_id"].IsUint64())
	{
		ULONGLONG nUtask_id = dc["task_id"].GetUint64();
		bResult = std::to_string(nUtask_id);
	}
	return bResult;
}

REGEXVALUE CBaiduParse::QueryOffLineList(const std::string& strCookie)
{
	REGEXVALUE strResult;
	ZeroMemory(&strResult, sizeof(REGEXVALUE));
	if (strCookie.empty())
		return strResult;
	BaiduUserInfo userinfo;
	if (!GetloginBassInfo(userinfo, strCookie))
		return strResult;
	std::string strQueryUrl = str(boost::format(OFF_LINE_QUERY_ALL_URL) % userinfo.bdstoken);
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.Send(GET, strQueryUrl);
	std::string strTextData = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strTextData.c_str());
	if (!dc.IsObject())
		return strResult;
	if (dc.HasMember("task_info") && dc["task_info"].IsArray())
	{
		for (auto& v: dc["task_info"].GetArray())
		{
			if (v.HasMember("task_id") && v["task_id"].IsString())
			{
				strResult.push_back(std::string(v["task_id"].GetString()));
			}
		}
	}
	return strResult;
}

std::string CBaiduParse::QueryTaskIdListStatus(const REGEXVALUE& TaskIdList, const std::string& strCookie)
{
	std::string strResult,strTaskIdList;
	if (TaskIdList.empty() || strCookie.empty())
		return strResult;
	BaiduUserInfo userinfo;
	if (!GetloginBassInfo(userinfo, strCookie))
		return strResult;
	if (TaskIdList.size()==1)
	{
		strTaskIdList = TaskIdList.at(0);
	}
	else
	{
		for (size_t i =0;i<TaskIdList.size();i++)
		{
			if (i < TaskIdList.size()-1)
			{
				strTaskIdList += TaskIdList.at(i) + ",";
			}
			else
			{
				strTaskIdList += TaskIdList.at(i);
			}
		}
	}
	std::string strQueryUrl = str(boost::format(QUERY_LIST_TASKIDS_URL) % userinfo.bdstoken % URL_Coding(strTaskIdList.c_str()));
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.Send(GET, strQueryUrl);
	std::string strTextData = BaiduHttp.GetResponseText();
	rapidjson::Document dc,datajson;
	dc.Parse(strTextData.c_str());
	if (!dc.IsObject())
		return strResult;
	datajson.SetObject();
	if (dc.HasMember("task_info") && dc["task_info"].IsObject())
	{
		rapidjson::Value task_info = dc["task_info"].GetObjectW();
		rapidjson::Value arraylist(rapidjson::kArrayType);
		for (size_t i =0;i < TaskIdList.size();i++)
		{
			rapidjson::Value addItem(rapidjson::kObjectType);
			if (task_info.HasMember(TaskIdList[i].c_str()) && task_info[TaskIdList[i].c_str()].IsObject())
			{
				rapidjson::Value itemObj = task_info[TaskIdList[i].c_str()].GetObjectW();
				int nStatus = 0;
				std::string strFileSize;
				std::string strtask_name, strProgress;
				ULONGLONG uLfinished_size=0,uLFileSize=0;
				if (itemObj.HasMember("status") && itemObj["status"].IsString())
					nStatus = atoi(itemObj["status"].GetString());
				if (itemObj.HasMember("file_size") && itemObj["file_size"].IsString())
				{
					uLFileSize = _atoi64(itemObj["file_size"].GetString());
					strFileSize = GetFileSizeType(uLFileSize);
				}
				if (itemObj.HasMember("task_name") && itemObj["task_name"].IsString())
					strtask_name = itemObj["task_name"].GetString();
				if (itemObj.HasMember("finished_size") && itemObj["finished_size"].IsString())
				{
					uLfinished_size = _atoi64(itemObj["finished_size"].GetString());;
				}
				if (uLFileSize)
				{
					int && t_result = ((double)uLfinished_size / (double)uLFileSize) * 100;
					strProgress = std::to_string(t_result) + "%";
				}
				rapidjson::Value status(rapidjson::kNumberType);
				rapidjson::Value FileSize(rapidjson::kStringType);
				rapidjson::Value FileName(rapidjson::kStringType);
				rapidjson::Value TaskID(rapidjson::kStringType);
				rapidjson::Value progress(rapidjson::kStringType);
				status.SetInt(nStatus);
				progress.SetString(strProgress.c_str(), strProgress.length(), datajson.GetAllocator());
				FileSize.SetString(strFileSize.c_str(), strFileSize.length(), datajson.GetAllocator());
				FileName.SetString(strtask_name.c_str(), strtask_name.length(), datajson.GetAllocator());
				TaskID.SetString(TaskIdList[i].c_str(), TaskIdList[i].length(), datajson.GetAllocator());
				addItem.AddMember(rapidjson::StringRef("progress"), progress, datajson.GetAllocator());
				addItem.AddMember(rapidjson::StringRef("task_id"), TaskID, datajson.GetAllocator());
				addItem.AddMember(rapidjson::StringRef("status"), status, datajson.GetAllocator());
				addItem.AddMember(rapidjson::StringRef("Size"), FileSize, datajson.GetAllocator());
				addItem.AddMember(rapidjson::StringRef("name"), FileName, datajson.GetAllocator());

				arraylist.PushBack(addItem, datajson.GetAllocator());
			}
		}
		datajson.AddMember(rapidjson::StringRef("data"), arraylist, datajson.GetAllocator());
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		datajson.Accept(writer);
		strResult = buffer.GetString();
	}
	return strResult;
}

std::string CBaiduParse::DeleteOffLineTask(const REGEXVALUE& TaskIdList, const std::string& strCookie)
{
	std::string strResult, strTaskIdList;
	if (TaskIdList.empty() || strCookie.empty())
		return strResult;
	BaiduUserInfo userinfo;
	if (!GetloginBassInfo(userinfo, strCookie))
		return strResult;
	if (TaskIdList.size() == 1)
	{
		strTaskIdList = TaskIdList.at(0);
	}
	else
	{
		for (size_t i = 0; i < TaskIdList.size(); i++)
		{
			if (i < TaskIdList.size() - 1)
			{
				strTaskIdList += TaskIdList.at(i) + ",";
			}
			else
			{
				strTaskIdList += TaskIdList.at(i);
			}
		}
	}
	std::string strQueryUrl = str(boost::format(OFF_LINE_DELETE_TASKID_URL) % userinfo.bdstoken % URL_Coding(strTaskIdList.c_str()));
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.Send(GET, strQueryUrl);
	std::string strTextData = BaiduHttp.GetResponseText();
	return strResult;
}

std::string CBaiduParse::GetBaiduAddr(BAIDUREQUESTINFO baiduinfo, const std::string strCookies)
{

	std::string strRealUrl;
	std::string strVcCode;
	VERCODEINFO verCinfo = GetVerCodeinfo(baiduinfo, strCookies);
	m_vcCodeUrl = verCinfo.image;
	std::string strJson;
	//��ʾ��֤�����봰��
	ShowInputVerCodeDlg();
	HttpRequest BaiduHttp;
	strVcCode = m_VerCode;
	std::string strDownloadUrl = "https://pan.baidu.com/api/sharedownload?sign=" + baiduinfo.sign + \
		"&timestamp=" + baiduinfo.timestamp + "&channel=chunlei&web=1&app_id=" + baiduinfo.app_id + \
		"&bdstoken=" + baiduinfo.bdstoken + "&logid=" + GetLogid() + "&clienttype=0";
	baiduinfo.BDCLND = GetTextMid(strCookies, "BDCLND=", ";");
	if (baiduinfo.BDCLND.empty())
	{
		strJson = "encrypt=0&product=share&vcode_input=%1%&vcode_str=%2%&uk=%3%&primaryid=%4%&fid_list=%%5B%5%%%5D";
		strJson = str(boost::format(strJson) % strVcCode % verCinfo.verCode % baiduinfo.uk % baiduinfo.shareid % baiduinfo.fs_id);
	}
	else
	{
		strJson = "encrypt=0&extra=%%7B%%22sekey%%22%%3A%%22%1%%%22%%7D&product=share&vcode_input=%2%&vcode_str=%3%&uk=%4%&primaryid=%5%&fid_list=%%5B%6%%%5D&path_list=";
		strJson = str(boost::format(strJson) % baiduinfo.BDCLND % strVcCode % verCinfo.verCode % baiduinfo.uk % baiduinfo.shareid % baiduinfo.fs_id);
	}
	/*׼���ύ���ݻ�ȡ��ʵURL��ַ*/
	BaiduHttp.SetRequestHeader("Content-Type", " application/x-www-form-urlencoded; charset=UTF-8");
	BaiduHttp.SetRequestHeader("Accept", " application/json, text/javascript, */*; q=0.01");
	BaiduHttp.SetRequestCookies(strCookies);
	BaiduHttp.Send(POST, strDownloadUrl, strJson);
	strJson = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strJson.c_str());
	if (!dc.IsObject())
		return strRealUrl;
	int nError = -1;
	if (dc.HasMember("errno") && dc["errno"].IsInt())
		nError = dc["errno"].GetInt();
	if (!nError)
	{
		if (dc.HasMember("list") && dc["list"].IsArray())
		{
			for (auto &v : dc["list"].GetArray())
			{
				if (v.HasMember("dlink") && v["dlink"].IsString())
					strRealUrl = v["dlink"].GetString();
			}
		}
	}
	return strRealUrl;
}
VERCODEINFO CBaiduParse::GetVerCodeinfo(BAIDUREQUESTINFO baiduinfo, const std::string strCookies)
{
	VERCODEINFO result;
	HttpRequest BaiduHttp;
	std::string strJson;
	std::string strVerCodeUrl = "https://pan.baidu.com/api/getvcode?prod=pan&t=0.48382518029895166&channel=chunlei&web=1&app_id=" + baiduinfo.app_id + "&bdstoken=" + baiduinfo.bdstoken + "&logid=" + GetLogid() + "&clienttype=0";
	BaiduHttp.SetRequestHeader("Accept", " application/json, text/javascript, */*; q=0.01");
	BaiduHttp.SetRequestHeader("Accept-Language", "zh-Hans-CN,zh-Hans;q=0.8,en-US;q=0.5,en;q=0.3");
	BaiduHttp.SetRequestCookies(strCookies);
	BaiduHttp.Send(GET, strVerCodeUrl);
	strJson = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strJson.c_str());
	if (!dc.IsObject())
		return result;
	if (dc.HasMember("img") && dc["img"].IsString())
		result.image = dc["img"].GetString();
	if (dc.HasMember("vcode") && dc["vcode"].IsString())
		result.verCode = dc["vcode"].GetString();
	return result;
}

long long CBaiduParse::getTimeStamp()
{
	timeb t;
	ftime(&t);
	return t.time * 1000 + t.millitm;
}

BAIDUREQUESTINFO CBaiduParse::GetBaiduInfo(const std::string strJson)
{
	BAIDUREQUESTINFO baiduInfo;
	rapidjson::Document dc;
	dc.Parse(strJson.c_str());
	if (!dc.IsObject())
		return baiduInfo;
	if (dc.HasMember("bdstoken") && dc["bdstoken"].IsString())
		baiduInfo.bdstoken = dc["bdstoken"].GetString();
	if (dc.HasMember("shareid") && dc["shareid"].IsInt64())
		baiduInfo.shareid = std::to_string(dc["shareid"].GetInt64());
	if (dc.HasMember("uk") && dc["uk"].IsUint64())
		baiduInfo.uk = std::to_string(dc["uk"].GetUint64());
	if (dc.HasMember("sign") && dc["sign"].IsString())
		baiduInfo.sign = dc["sign"].GetString();
	if (dc.HasMember("timestamp") && dc["timestamp"].IsInt())
		baiduInfo.timestamp = std::to_string(dc["timestamp"].GetInt());
	if (dc.HasMember("file_list") && dc["file_list"].IsObject())
	{
		rapidjson::Value file_list = dc["file_list"].GetObject();
		if (file_list.HasMember("list") && file_list["list"].IsArray())
		{
			for (auto& v : file_list["list"].GetArray())
			{
				if (v.HasMember("app_id") && v["app_id"].IsString())
				{
					baiduInfo.app_id = v["app_id"].GetString();
				}
				if (v.HasMember("server_filename") && v["server_filename"].IsString())
				{
					baiduInfo.server_filename = v["server_filename"].GetString();
				}
				if (v.HasMember("path") && v["path"].IsString())
				{
					baiduInfo.strPath = v["path"].GetString();
				}
				if (v.HasMember("isdir") && v["isdir"].IsUint())
				{
					baiduInfo.n_isdir = v["isdir"].GetUint();
				}
				if (v.HasMember("category") && v["category"].IsUint())
				{
					baiduInfo.ncategory = v["category"].GetUint();
				}
				if (v.HasMember("server_ctime") && v["server_ctime"].IsUint64())
				{
					baiduInfo.server_time = v["server_ctime"].GetUint64();
				}
				if (v.HasMember("size") && v["size"].IsUint64())
				{
					baiduInfo.nSize = v["size"].GetUint64();
				}
				if (v.HasMember("fs_id") && v["fs_id"].IsUint64())
				{
					baiduInfo.fs_id = std::to_string(v["fs_id"].GetUint64());
				}
				
			}
		}
	}
	return baiduInfo;
}

std::string CBaiduParse::Unicode_To_Ansi(const wchar_t* szbuff)
{
	std::string result;
	CHAR* MultPtr = nullptr;
	int MultLen = -1;
	MultLen = ::WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, szbuff, -1, NULL, NULL, NULL, NULL);
	MultPtr = new CHAR[MultLen + 1];
	if (MultPtr)
	{
		ZeroMemory(MultPtr, MultLen + 1);
		::WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, szbuff, -1, MultPtr, MultLen, NULL, NULL);
		result = MultPtr;
		delete[] MultPtr;
		MultPtr = nullptr;
	}
	return result;
}

std::wstring CBaiduParse::Ansi_To_Unicode(const char* szbuff)
{
	std::wstring result;
	WCHAR* widePtr = nullptr;
	int wideLen = -1;
	wideLen = ::MultiByteToWideChar(CP_ACP, NULL, szbuff, -1, NULL, NULL);
	widePtr = new WCHAR[wideLen + 1];
	if (widePtr)
	{
		ZeroMemory(widePtr, (wideLen + 1) * sizeof(WCHAR));
		::MultiByteToWideChar(CP_ACP, NULL, szbuff, -1, widePtr, wideLen);
		result = widePtr;
		delete[] widePtr;
		widePtr = nullptr;
	}
	return result;
}

std::string CBaiduParse::Gbk_To_Utf8(const char* szBuff)
{
	std::string resault;
	int widLen = 0;
	int MultiLen = 0;
	WCHAR* widPtr = nullptr;
	CHAR* MulitPtr = nullptr;
	widLen = ::MultiByteToWideChar(CP_ACP, NULL, szBuff, -1, NULL, NULL);//��ȡת������Ҫ�Ŀռ�
	widPtr = new WCHAR[widLen + 1];
	if (!widPtr)
		return resault;
	ZeroMemory(widPtr, (widLen + 1) * sizeof(WCHAR));
	if (!::MultiByteToWideChar(CP_ACP, NULL, szBuff, -1, widPtr, widLen))
	{
		delete[] widPtr;
		widPtr = nullptr;
		return resault;
	}
	MultiLen = ::WideCharToMultiByte(CP_UTF8, NULL, widPtr, -1, NULL, NULL, NULL, NULL);
	if (MultiLen)
	{
		MulitPtr = new CHAR[MultiLen + 1];
		if (!MulitPtr)
		{
			delete[] widPtr;
			widPtr = nullptr;
			return resault;
		}
		ZeroMemory(MulitPtr, (MultiLen + 1) * sizeof(CHAR));
		::WideCharToMultiByte(CP_UTF8, NULL, widPtr, -1, MulitPtr, MultiLen, NULL, NULL);
		resault = MulitPtr;
		delete[] MulitPtr;
		MulitPtr = nullptr;
	}
	delete[] widPtr;
	widPtr = nullptr;
	return resault;
}

std::string CBaiduParse::Utf8_To_Gbk(const char* szBuff)
{
	std::string resault;
	int widLen = 0;
	int MultiLen = 0;
	WCHAR* widPtr = nullptr;
	CHAR* MulitPtr = nullptr;
	widLen = ::MultiByteToWideChar(CP_UTF8, NULL, szBuff, -1, NULL, NULL);//��ȡת������Ҫ�Ŀռ�
	widPtr = new WCHAR[widLen + 1];
	if (!widPtr)
		return resault;
	ZeroMemory(widPtr, (widLen + 1) * sizeof(WCHAR));
	if (!::MultiByteToWideChar(CP_UTF8, NULL, szBuff, -1, widPtr, widLen))
	{
		delete[] widPtr;
		widPtr = nullptr;
		return resault;
	}
	MultiLen = ::WideCharToMultiByte(CP_ACP, NULL, widPtr, -1, NULL, NULL, NULL, NULL);
	if (MultiLen)
	{
		MulitPtr = new CHAR[MultiLen + 1];
		if (!MulitPtr)
		{
			delete[] widPtr;
			widPtr = nullptr;
			return resault;
		}
		ZeroMemory(MulitPtr, (MultiLen + 1) * sizeof(CHAR));
		::WideCharToMultiByte(CP_ACP, NULL, widPtr, -1, MulitPtr, MultiLen, NULL, NULL);
		resault = MulitPtr;
		delete[] MulitPtr;
		MulitPtr = nullptr;
	}
	delete[] widPtr;
	widPtr = nullptr;
	return resault;
}


std::string CBaiduParse::GetTextMid(const std::string strSource, const std::string strLeft, const std::string strRight)
{
	std::string strResult;
	int nBeigin = 0, nEnd = 0, nLeft = 0;
	if (strSource.empty())return strResult;
	nBeigin = strSource.find(strLeft);
	if (nBeigin == std::string::npos)return strResult;
	nLeft = nBeigin + strLeft.length();
	nEnd = strSource.find(strRight, nLeft);
	if (nEnd == std::string::npos)
	{
		strResult = strSource.substr(nLeft, strSource.length());
		return strResult;
	}
	strResult = strSource.substr(nLeft, nEnd - nLeft);
	return strResult;
}

std::string CBaiduParse::GetLogid(bool isFlag /*= true*/)
{
	std::string strResult;
	srand(ULLONG_MAX);
	CHAR szLogid[0x20];
	CHAR szTime[0x20];
	CHAR szResult[0x50];
	ZeroMemory(szLogid, 0x20);
	ZeroMemory(szTime, 0X20);
	ZeroMemory(szResult, 0X50);
	sprintf_s(szLogid, "%I64u", labs(rand()));
	sprintf_s(szTime, "%I64d", getTimeStamp());
	strcat_s(szResult, szTime);
	strcat_s(szResult, "0.");
	if (isFlag)
	{
		if (lstrlenA(szLogid) >= 16)
			szLogid[16] = 0;
	}
	else
	{
		if (lstrlenA(szLogid) >= 16)
			szLogid[17] = 0;
	}
	strcat_s(szResult, szLogid);
	strResult = szResult;
	strResult = aip::base64_encode(strResult.c_str(), strResult.length());
	return strResult;
}

bool CBaiduParse::GetloginBassInfo(BaiduUserInfo& baseInfo, const std::string strCookie)
{
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.Send(GET, HOME_URL);
	std::string strResult(BaiduHttp.GetResponseText());
//	WriteFileBuffer(".\\1.txt", (char*)strResult.c_str(), strResult.length());
//	strResult = GetTextMid(strResult, "context=", ";\n            var yunData");
	REGEXVALUE rv = GetRegexValue(strResult, "context=(.*?);\\s");
	if (rv.size() < 1)
		return false;
	strResult = rv.at(0);
	//WriteFileBuffer(".\\2.txt", (char*)strResult.c_str(), strResult.length());
	rapidjson::Document dc;
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())
		return false;
	if (dc.HasMember("bdstoken") && dc["bdstoken"].IsString())
		baseInfo.bdstoken = dc["bdstoken"].GetString();
	if (dc.HasMember("username") && dc["username"].IsString())
		baseInfo.strUserName = dc["username"].GetString();
	if (dc.HasMember("photo") && dc["photo"].IsString())
		baseInfo.strHeadImageUrl = dc["photo"].GetString();
	if (dc.HasMember("is_vip") && dc["is_vip"].IsInt())
		baseInfo.is_vip = dc["is_vip"].GetInt();

	BaiduHttp.Send(GET, str(boost::format(DISK_CAPACITY_QUERY) % baseInfo.bdstoken));
	strResult = BaiduHttp.GetResponseText();
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())
		return false;
	if (dc.HasMember("total") && dc["total"].IsUint64())
	{
		baseInfo.strDisktotal = GetFileSizeType(dc["total"].GetUint64());
	}
	if (dc.HasMember("used") && dc["used"].IsUint64())
	{
		baseInfo.strDiskUsed = GetFileSizeType(dc["used"].GetUint64());
	}
	return true;
}

REGEXVALUE CBaiduParse::GetRegexValue(const std::string strvalue, const std::string strRegx)
{
	REGEXVALUE rvResult;
	if (strvalue.empty() || strRegx.empty())
		return rvResult;
	boost::regex expr(strRegx);
	boost::smatch what;
	if (boost::regex_search(strvalue, what, expr))
	{
		size_t length = what.size();
		for (size_t i = 1; i < length;++i)
		{
			if (what[i].matched)
			{
			/*	std::vector<char>val;
				val.resize(what[i].str().length());
				memcpy(val.data(), &what[i].str()[1], what[i].str().length() - 2);*/
				rvResult.push_back(what[i].str());
			}
		}
	}
	return rvResult;
}

void CBaiduParse::ShowInputVerCodeDlg()
{
	::DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_VERCODE), GetDesktopWindow(), ImageProc);
}

INT_PTR CALLBACK CBaiduParse::ShareProc(_In_ HWND hwndDlg, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDC_BTN_OK:
		{
			char szCode[MAX_PATH];
			ZeroMemory(szCode, MAX_PATH);
			::GetDlgItemTextA(hwndDlg, IDC_EDIT_PASS, szCode, MAX_PATH);
			m_SharePass = szCode;
			::EndDialog(hwndDlg, 0);
		}
		break;
		default:
			break;
		}
	}
	break;
	default:
		break;
	}
	return 0;
}

void CBaiduParse::ShowInputPassDlg()
{
	::DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_SHAREPASS), GetDesktopWindow(), ShareProc);
}

std::string CBaiduParse::GetBaiduFileListInfo(const std::string& path, const std::string strCookie)
{
	std::string strResultJson;
	FileTypeArray fileInfoResult;
	std::string strFileUrl,strResult;
	ZeroMemory(&fileInfoResult, sizeof(fileInfoResult));
	if (strCookie.empty() || path.empty())
		return strResultJson;
	BaiduUserInfo userinfo;
	if (!GetloginBassInfo(userinfo, strCookie))
		return strResultJson;
	strFileUrl = str(boost::format(FILE_LIST_URL) % URL_Coding(path.c_str()).c_str() % userinfo.bdstoken % GetLogid() % std::to_string(getTimeStamp()));
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.Send(GET, strFileUrl);
	strResult = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())
		return strResultJson;
	if (dc.HasMember("list") && dc["list"].IsArray())
	{
		for (auto&v:dc["list"].GetArray())
		{
			if (v.IsObject())
			{
				BaiduFileInfo item;
				if (v.HasMember("category") && v["category"].IsInt())
					item.nCategory = v["category"].GetInt();
				if (v.HasMember("isdir") && v["isdir"].IsInt())
					item.nisdir = v["isdir"].GetInt();
				if (v.HasMember("path") && v["path"].IsString())
					item.strPath = v["path"].GetString();
				if (v.HasMember("server_filename") && v["server_filename"].IsString())
					item.server_filename = v["server_filename"].GetString();
				if (v.HasMember("size") && v["size"].IsUint())
					item.size = v["size"].GetUint();
				if (v.HasMember("server_ctime") && v["server_ctime"].IsUint())
					item.server_ctime = v["server_ctime"].GetUint();
				if (v.HasMember("fs_id") && v["fs_id"].IsUint64())
					item.fs_id = std::to_string(v["fs_id"].GetUint64());
				if (!item.nisdir)
					item.strFileType = ::PathFindExtensionA(Utf8_To_Gbk(item.server_filename.c_str()).c_str());
				fileInfoResult.push_back(item);
			}
		}
	}
	rapidjson::Document docjson;
	docjson.SetObject();
	rapidjson::Value UsrName(rapidjson::kStringType);
	UsrName.SetString(userinfo.strUserName.c_str(), userinfo.strUserName.length(), docjson.GetAllocator());
	docjson.AddMember(rapidjson::StringRef("UserName"), UsrName, docjson.GetAllocator());
	rapidjson::Value UsrHeaderImage(rapidjson::kStringType);
	UsrHeaderImage.SetString(userinfo.strHeadImageUrl.c_str(), userinfo.strHeadImageUrl.length(), docjson.GetAllocator());
	docjson.AddMember(rapidjson::StringRef("UserHeader"), UsrHeaderImage, docjson.GetAllocator());
	rapidjson::Value array_list(rapidjson::kArrayType);
	for (size_t i = 0; i < fileInfoResult.size();i++)
	{
		rapidjson::Value itemObject(rapidjson::kObjectType);
		rapidjson::Value isdir(rapidjson::kNumberType);
		isdir.SetInt(fileInfoResult[i].nisdir);
		itemObject.AddMember(rapidjson::StringRef("isdir"), isdir, docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("name"), rapidjson::StringRef(fileInfoResult[i].server_filename.c_str()), docjson.GetAllocator());
		std::string strFilesize = GetFileSizeType(fileInfoResult[i].size);
		rapidjson::Value FileSize(rapidjson::kStringType);
		FileSize.SetString(strFilesize.c_str(), strFilesize.length(), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("Size"), FileSize, docjson.GetAllocator());
		rapidjson::Value nCategory(rapidjson::kNumberType);
		nCategory.SetInt(fileInfoResult[i].nCategory);
		itemObject.AddMember(rapidjson::StringRef("nCategory"), nCategory, docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("path"), rapidjson::StringRef(fileInfoResult[i].strPath.c_str()), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("FileType"), rapidjson::StringRef(fileInfoResult[i].strFileType.c_str()), docjson.GetAllocator());
		std::string strTime = timestampToDate(fileInfoResult[i].server_ctime);
		rapidjson::Value FileTime(rapidjson::kStringType);
		FileTime.SetString(strTime.c_str(), strTime.length(), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("ChangeTime"), FileTime, docjson.GetAllocator());
		rapidjson::Value fs_id(rapidjson::kStringType);
		fs_id.SetString(fileInfoResult[i].fs_id.c_str(), fileInfoResult[i].fs_id.length(), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("fs_id"), fs_id, docjson.GetAllocator());
		array_list.PushBack(itemObject, docjson.GetAllocator());
		
	}
	docjson.AddMember(rapidjson::StringRef("data"), array_list, docjson.GetAllocator());
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	docjson.Accept(writer);
	strResultJson = buffer.GetString();
	/*
	1.txt ->
	{"UserName":"zlj1980617","UserHeader":"https://ss0.bdstatic.com/7Ls0a8Sm1A5BphGlnYG/sys/portrait/item/netdisk.1.4252f7a0.czLfAuhqzwJU_MeMF6zwuA.jpg",
	"data":[{"isdir":1,"name":"�ҵ���Դ","Size":"0.0B","nCategory":6,"path":"/�ҵ���Դ","FileType":"","ChangeTime":"2017-01-24","fs_id":"753569710893671"},{"isdir":0,"name":"20131209_193530.jpg","Size":"2.0 MB","nCategory":3,"path":"/20131209_193530.jpg","FileType":".jpg","ChangeTime":"2013-12-09","fs_id":"1925975276"}]}
	*/

	//WriteFileBuffer(".\\1.txt",(PVOID)strResultJson.c_str(), strResultJson.length());

	return strResultJson;
}

bool CBaiduParse::EnumAllFolder(const std::string& path, const std::string strCookie, rapidjson::Document& datajson)
{
	bool strResultJson = false;
	FileTypeArray fileInfoResult;
	std::string strFileUrl, strResult;
	ZeroMemory(&fileInfoResult, sizeof(fileInfoResult));
	if (strCookie.empty() || path.empty())
		return strResultJson;
	BaiduUserInfo userinfo;
	if (!GetloginBassInfo(userinfo, strCookie))
		return strResultJson;
	strFileUrl = str(boost::format(FILE_LIST_URL) % URL_Coding(path.c_str()).c_str() % userinfo.bdstoken % GetLogid() % std::to_string(getTimeStamp()));
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.Send(GET, strFileUrl);
	strResult = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())
		return strResultJson;
	if (dc.HasMember("list") && dc["list"].IsArray())
	{
		for (size_t i = 0; i < dc["list"].GetArray().Size(); i++)
		{
			rapidjson::Value& v = dc["list"][i];
			if (v.IsObject())
			{
				BaiduFileInfo item;
				if (v.HasMember("isdir") && v["isdir"].IsInt())
					item.nisdir = v["isdir"].GetInt();
				if (v.HasMember("path") && v["path"].IsString())
					item.strPath = Utf8_To_Gbk(v["path"].GetString());
				if (v.HasMember("server_filename") && v["server_filename"].IsString())
					item.server_filename = Utf8_To_Gbk(v["server_filename"].GetString());
				if (item.nisdir)
				{

					std::string strPath = str(boost::format(path + "data/%1%") % i);
					rapidjson::Value strval(rapidjson::kObjectType);
					rapidjson::Value path(rapidjson::kStringType);
					rapidjson::Value name(rapidjson::kStringType);
					path.SetString(item.strPath.c_str(), item.strPath.length(), datajson.GetAllocator());
					name.SetString(item.server_filename.c_str(), item.server_filename.length(), datajson.GetAllocator());
					strval.AddMember(rapidjson::StringRef("path"), path, datajson.GetAllocator());
					strval.AddMember(rapidjson::StringRef("label"), name, datajson.GetAllocator());
					rapidjson::Pointer(rapidjson::StringRef(strPath.c_str())).Set(datajson, strval);
					EnumAllFolder(item.strPath + "/", strCookie, datajson);
					strResultJson = true;
				}
			}
		}
	}
	return strResultJson;
}

std::string CBaiduParse::GetBaiduShareFileListInfo(const std::string& path, const std::string strCookie, BAIDUREQUESTINFO userinfo)
{
	std::string strResultJson;
	FileTypeArray fileInfoResult;
	std::string strFileUrl, strResult;
	ZeroMemory(&fileInfoResult, sizeof(fileInfoResult));
	if (strCookie.empty() || path.empty())
		return strResultJson;
	strFileUrl = str(boost::format(SHARE_FILE_LIST_URL) %userinfo.uk % userinfo.shareid % URL_Coding(path.c_str()).c_str() % userinfo.bdstoken % GetLogid());
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.Send(GET, strFileUrl);
	strResult = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())
		return strResultJson;
	if (dc.HasMember("list") && dc["list"].IsArray())
	{
		for (auto&v : dc["list"].GetArray())
		{
			if (v.IsObject())
			{
				BaiduFileInfo item;
				if (v.HasMember("category") && v["category"].IsInt())
					item.nCategory = v["category"].GetInt();
				if (v.HasMember("isdir") && v["isdir"].IsInt())
					item.nisdir = v["isdir"].GetInt();
				if (v.HasMember("path") && v["path"].IsString())
					item.strPath = v["path"].GetString();
				if (v.HasMember("server_filename") && v["server_filename"].IsString())
					item.server_filename = v["server_filename"].GetString();
				if (v.HasMember("size") && v["size"].IsUint())
					item.size = v["size"].GetUint();
				if (v.HasMember("server_ctime") && v["server_ctime"].IsUint())
					item.server_ctime = v["server_ctime"].GetUint();
				if (v.HasMember("fs_id") && v["fs_id"].IsUint64())
					item.fs_id = std::to_string(v["fs_id"].GetUint64());
				if (!item.nisdir)
					item.strFileType = ::PathFindExtensionA(Utf8_To_Gbk(item.server_filename.c_str()).c_str());
				fileInfoResult.push_back(item);
			}
		}
	}
	rapidjson::Document docjson;
	docjson.SetObject();
	rapidjson::Value array_list(rapidjson::kArrayType);
	for (size_t i = 0; i < fileInfoResult.size(); i++)
	{
		rapidjson::Value itemObject(rapidjson::kObjectType);
		rapidjson::Value isdir(rapidjson::kNumberType);
		isdir.SetInt(fileInfoResult[i].nisdir);
		itemObject.AddMember(rapidjson::StringRef("isdir"), isdir, docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("cookie"), rapidjson::StringRef(strCookie.c_str()), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("name"), rapidjson::StringRef(fileInfoResult[i].server_filename.c_str()), docjson.GetAllocator());
		std::string strFilesize = GetFileSizeType(fileInfoResult[i].size);
		rapidjson::Value FileSize(rapidjson::kStringType);
		FileSize.SetString(strFilesize.c_str(), strFilesize.length(), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("Size"), FileSize, docjson.GetAllocator());
		rapidjson::Value nCategory(rapidjson::kNumberType);
		nCategory.SetInt(fileInfoResult[i].nCategory);
		itemObject.AddMember(rapidjson::StringRef("nCategory"), nCategory, docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("path"), rapidjson::StringRef(fileInfoResult[i].strPath.c_str()), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("FileType"), rapidjson::StringRef(fileInfoResult[i].strFileType.c_str()), docjson.GetAllocator());
		std::string strTime = timestampToDate(fileInfoResult[i].server_ctime);
		rapidjson::Value FileTime(rapidjson::kStringType);
		FileTime.SetString(strTime.c_str(), strTime.length(), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("ChangeTime"), FileTime, docjson.GetAllocator());
		rapidjson::Value fs_id(rapidjson::kStringType);
		fs_id.SetString(fileInfoResult[i].fs_id.c_str(), fileInfoResult[i].fs_id.length(), docjson.GetAllocator());
		itemObject.AddMember(rapidjson::StringRef("fs_id"), fs_id, docjson.GetAllocator());
		array_list.PushBack(itemObject, docjson.GetAllocator());

	}
	rapidjson::Value info(rapidjson::kObjectType);
	info.AddMember(rapidjson::StringRef("uk"), rapidjson::StringRef(userinfo.uk.c_str()), docjson.GetAllocator());
	info.AddMember(rapidjson::StringRef("bdstoken"), rapidjson::StringRef(userinfo.bdstoken.c_str()), docjson.GetAllocator());
	info.AddMember(rapidjson::StringRef("sign"), rapidjson::StringRef(userinfo.sign.c_str()), docjson.GetAllocator());
	info.AddMember(rapidjson::StringRef("shareid"), rapidjson::StringRef(userinfo.shareid.c_str()), docjson.GetAllocator());
	info.AddMember(rapidjson::StringRef("timestamp"), rapidjson::StringRef(userinfo.timestamp.c_str()), docjson.GetAllocator());
	docjson.AddMember(rapidjson::StringRef("info"), info, docjson.GetAllocator());
	docjson.AddMember(rapidjson::StringRef("data"), array_list, docjson.GetAllocator());
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	docjson.Accept(writer);
	strResultJson = buffer.GetString();
	//WriteFileBuffer(".\\1.txt",(PVOID)strResultJson.c_str(), strResultJson.length());
	return strResultJson;
}

std::string CBaiduParse::URL_Coding(const char* szSource, bool isletter /*= true*/, bool isUtf8 /*= true*/)
{
	CHAR szTemp[20];
	CHAR* buffer = nullptr;
	std::string strBuffer;
	std::string result;
	std::string strTemp;
	if (isUtf8)
		strBuffer = Gbk_To_Utf8(szSource);
	else
		strBuffer = szSource;
	int lens = strBuffer.length();
	buffer = new CHAR[lens + 1];
	if (!buffer)return result;
	ZeroMemory(buffer, lens + 1);
	memcpy(buffer, strBuffer.c_str(), lens);
	for (int i = 0; i<lens; i++)
	{
		ZeroMemory(szTemp, 20);
		if (isletter)
		{
			BYTE cbyte = *(buffer + i);
			if (cbyte > 44 && cbyte < 58 && 47 != cbyte)		//0-9
			{
				sprintf_s(szTemp, "%c", cbyte);
				strTemp = szTemp;
				result += strTemp;
			}
			else if (cbyte > 'A' && cbyte <= 'Z')	//A-Z
			{
				sprintf_s(szTemp, "%c", cbyte);
				strTemp = szTemp;
				result += strTemp;
			}
			else if (cbyte > 'a' && cbyte <= 'z')	//a-z
			{
				sprintf_s(szTemp, "%c", cbyte);
				strTemp = szTemp;
				result += strTemp;
			}
			else
			{
				sprintf_s(szTemp, "%02X", *(PBYTE)(buffer + i));
				strTemp = "%";
				strTemp += szTemp;
				result += strTemp;
			}
		}
		else
		{
			sprintf_s(szTemp, "%02X", *(PBYTE)(buffer + i));
			strTemp = "%";
			strTemp += szTemp;
			result += strTemp;
		}
	}
	delete[] buffer;
	return result;
}

std::string CBaiduParse::UnEscape(const char* strSource)
{
	std::string strResult;
	int nDestStep = 0;
	int nLength = strlen(strSource);
	if (!nLength || nLength < 6) return strResult;
	char* pResult = new char[nLength + 1];
	wchar_t* pWbuufer = nullptr;
	if (!pResult)
	{
		pResult = NULL;
		return strResult;
	}
	ZeroMemory(pResult, nLength + 1);
	for (int nPos = 0; nPos < nLength; nPos++)
	{
		if (strSource[nPos] == '\\' && strSource[nPos + 1] == 'u')
		{
			char szTemp[5];
			char szSource[5];
			ZeroMemory(szTemp, 5);
			ZeroMemory(szSource, 5);
			CopyMemory(szSource, (char*)strSource + nPos + 2, 4);
			sscanf_s(szSource, "%04X", szTemp);
			CopyMemory(pResult + nDestStep, szTemp, 4);
			nDestStep += 2;
		}
	}
	nDestStep += 2;
	pWbuufer = new wchar_t[nDestStep];
	if (!pWbuufer)
	{
		delete[] pWbuufer;
		pWbuufer = nullptr;
		return strResult;
	}
	ZeroMemory(pWbuufer, nDestStep);
	CopyMemory(pWbuufer, pResult, nDestStep);
	delete[] pResult;
	pResult = nullptr;
	CHAR* MultPtr = nullptr;
	int MultLen = -1;
	MultLen = ::WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, pWbuufer, -1, NULL, NULL, NULL, NULL);
	MultPtr = new CHAR[MultLen + 1];
	if (MultPtr)
	{
		ZeroMemory(MultPtr, MultLen + 1);
		::WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, pWbuufer, -1, MultPtr, MultLen, NULL, NULL);
		strResult = MultPtr;
		delete[] MultPtr;
		MultPtr = nullptr;
	}
	delete[] pWbuufer;
	pWbuufer = nullptr;
	return strResult;
}

float CBaiduParse::roundEx(float Floatnum)
{
	return (int)(Floatnum * 100 + 0.5) / 100.0;
}

std::string CBaiduParse::GetFileSizeType(double dSize)
{
	std::string szFileSize;
	if (dSize <1024)
	{
		szFileSize = str(boost::format("%.1f B") %roundEx(dSize));
	}
	else if (dSize >1024 && dSize < 1024 * 1024 * 1024 && dSize <1024 * 1024)
	{
		szFileSize = str(boost::format("%.1f KB") %roundEx(dSize / 1024));
	}
	else if (dSize >1024 * 1024 && dSize <1024 * 1024 * 1024)
	{
		szFileSize = str(boost::format("%.1f MB") %roundEx(dSize / 1024 / 1024));
	}
	else if (dSize >1024 * 1024 * 1024)
	{
		szFileSize = str(boost::format("%.1f GB") %roundEx(dSize / 1024 / 1024 / 1024));
	}
	return szFileSize;
}

std::string CBaiduParse::timestampToDate(ULONGLONG ctime)
{
	struct tm time;
	time_t tick = (time_t)ctime;
	::localtime_s(&time, &tick);
	char sztime[256];
	ZeroMemory(sztime, 256);
	::strftime(sztime, 256, "%Y-%m-%d", &time);
	return std::string(sztime);
}

DATETIME CBaiduParse::GetDateTime(const std::string& strTime)
{
	DATETIME rDateTime;
	ZeroMemory(&rDateTime, sizeof(DATETIME));
	if (strTime.empty())
		return rDateTime;
	std::vector<std::string> resultVec;
	std::string split = strTime + "-";
	std::string pattern = "-";
	std::string strsub;
	size_t pos = 0;
	size_t npos = 0;
	npos = split.find(pattern, pos);
	while (npos != std::string::npos)
	{
		strsub = split.substr(pos, npos - pos);
		pos = npos + pattern.length();
		npos = split.find(pattern, pos);
		resultVec.push_back(strsub);
	}
	if (resultVec.size()==3)
	{
		rDateTime.nYear = atoi(resultVec.at(0).c_str());
		rDateTime.nMonth = atoi(resultVec.at(1).c_str());
		rDateTime.nDay = atoi(resultVec.at(2).c_str());
	}
	return rDateTime;
}

std::string CBaiduParse::GetBaiduLocalFileAddr(const std::string path, const std::string strCookie)
{
	std::string strResult;
	if (path.empty() || strCookie.empty())
		return strResult;
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.SetRequestHeader("User-Agent", "netdisk;6.0.0.12;PC;PC-Windows;10.0.16299;WindowsBaiduYunGuanJia");
	BaiduHttp.SetRequestHeader("Accept", " image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, */*");
	BaiduHttp.SetRequestHeader("Host", " d.pcs.baidu.com");
	std::string strRequestUrl = str(boost::format(DOWN_LOCAL_FILE) % path.c_str());
	BaiduHttp.Send(GET, strRequestUrl);
	strResult = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strResult.c_str());
	if (!dc.IsObject())
		return std::string("");
	if (dc.HasMember("urls") && dc["urls"].IsArray())
	{
		for (auto& v:dc["urls"].GetArray())
		{
			if (v.HasMember("url") && v["url"].IsString())
			{
				strResult = v["url"].GetString();
				break;
			}
		}
	}
	return strResult;
}

std::string CBaiduParse::ShareBaiduFile(SHAREFILEINFO shareFileinfo, const std::string strCookie)
{
	std::string strResult;
	std::string sharDat;
	if (strCookie.empty())
		return strResult;
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduUserInfo userinfo;
	if (!GetloginBassInfo(userinfo, strCookie))
		return strResult;
	std::string strShareUrl;
	if (shareFileinfo.nschannel)
	{
		strShareUrl = str(boost::format(SHARE_FILE_URL_1) % userinfo.bdstoken % GetLogid());
		sharDat = str(boost::format("schannel=4&channel_list=%%5B%%5D&period=%1%&pwd=%2%&fid_list=%%5B%3%%%5D") % shareFileinfo.strperiod % shareFileinfo.strPwd % shareFileinfo.strpath_list);
	}
	else
	{
		strShareUrl = str(boost::format(SHARE_FILE_URL_2) % userinfo.bdstoken % GetLogid());
		sharDat = str(boost::format("schannel=0&channel_list=%%5B%%5D&period=%1%&path_list=%%5B%%22%2%%%22%%5D") % shareFileinfo.strperiod % URL_Coding(Utf8_To_Gbk(shareFileinfo.strpath_list.c_str()).c_str()));
	}
	BaiduHttp.Send(POST, strShareUrl, sharDat);
	strResult = BaiduHttp.GetResponseText();
	return strResult;
}

std::string CBaiduParse::DeleteBaiduFile(const std::string strJsonData, const std::string strCookie)
{
	std::string bResult;
	if (strJsonData.empty() || strCookie.empty())
		return bResult;
	rapidjson::Document dc;
	dc.Parse(strJsonData.c_str());
	if (!dc.IsObject())
		return bResult;
	if (!dc.HasMember("data") && !dc["data"].IsArray())
	{
		return bResult;
	}
	rapidjson::Value datajson = dc["data"].GetArray();
	std::string strSupDir;
	std::string strSendData="[";
	for (size_t i = 0; i < datajson.Size(); i++)
	{
		if (i < (datajson.Size()-1))
		{
			if (datajson[i].HasMember("path") && datajson[i]["path"].IsString())
			{
				std::string strTempFile(Utf8_To_Gbk(datajson[i]["path"].GetString()));
				strSendData += "\"" + strTempFile + "\",";
			}
		}
		else{
			std::string strTempFile(Utf8_To_Gbk(datajson[i]["path"].GetString()));
			int npos = strTempFile.rfind("/");
			if (npos != std::string::npos)
			{
				strSupDir = strTempFile.substr(0, npos);
				if (strSupDir.empty())
					strSupDir = "/";
			}
			strSendData += "\"" + strTempFile + "\"]";
		}
	}
	strSendData = URL_Coding(strSendData.c_str());
	std::string strDeleteUrl("https://pan.baidu.com/api/filemanager?opera=delete&async=1&channel=chunlei&web=1&app_id=250528&clienttype=0&bdstoken=");
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(strCookie);
	BaiduHttp.SetRequestHeader("Accept", "image/gif, image/x-xbitmap, image/jpeg, image/pjpeg, */*");
	BaiduHttp.SetRequestHeader("Content-Type", "application/x-www-form-urlencoded");
	BaiduUserInfo userinfo;
	if (!GetloginBassInfo(userinfo, strCookie))
		return bResult;
	strDeleteUrl += userinfo.bdstoken;
	BaiduHttp.Send(POST, strDeleteUrl, "filelist="+strSendData);
	strDeleteUrl = BaiduHttp.GetResponseText();
	dc.Parse(strDeleteUrl.c_str());
	if (!dc.IsObject())
		return bResult;
	if (!dc.HasMember("errno") && !dc["errno"].IsInt())
		return bResult;
	int errorn = dc["errno"].GetInt();
	if (errorn == 0)
		bResult = GetBaiduFileListInfo(strSupDir, strCookie);
	return bResult;
}


std::string CBaiduParse::BaiduRename(const std::string& strPath, const std::string& strNewName, const std::string strCookie)
{
	std::string strResult;
	std::string T_strCookie = strCookie;
	if (strPath.empty() || strNewName.empty() || strCookie.empty())
		return strResult;
	std::string strSend = "%%5B%%7B%%22path%%22%%3A%%22%1%%%22%%2C%%22newname%%22%%3A%%22%2%%%22%%7D%%5D";
	strSend = str(boost::format(strSend) % URL_Coding(strPath.c_str()) % URL_Coding(strNewName.c_str()));
	//filelist=
	HttpRequest BaiduHttp;
	BaiduHttp.SetRequestCookies(T_strCookie);
	BaiduUserInfo userinfo;
	if (!GetloginBassInfo(userinfo, T_strCookie))
		return strResult;
	std::string strRenameUrl = str(boost::format(RENAME_FILE_URL) % userinfo.bdstoken);
	BaiduHttp.Send(POST, strRenameUrl, "filelist=" + strSend);
	strRenameUrl = BaiduHttp.GetResponseText();
	rapidjson::Document dc;
	dc.Parse(strRenameUrl.c_str());
	if (!dc.IsObject())
		return strResult;
	int nErrorcode = 0;
	if (dc.HasMember("errno") && dc["errno"].IsInt())
	{
		nErrorcode = dc["errno"].GetInt();
		if (nErrorcode!=0)
			return strResult;
		else
		{
			std::string strSupDir;
			int npos = strPath.rfind("/");
			if (npos != std::string::npos)
			{
				strSupDir = strPath.substr(0, npos);
				if (strSupDir.empty())
					strSupDir = "/";
			}
			strResult = GetBaiduFileListInfo(strSupDir, T_strCookie);
		}
	}
	return strResult;
}

CBaiduParse::CBaiduParse()
{

}

INT_PTR CALLBACK CBaiduParse::ImageProc(_In_ HWND hwndDlg, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
						  HWND imageHwnd = ::GetDlgItem(hwndDlg, IDC_STATIC_IMG);
						  HttpRequest baiduHttp;
						  baiduHttp.Send(GET, m_vcCodeUrl);
						  responseData imageData = baiduHttp.GetResponseBody();
						  CImage img;
						  HBITMAP bithandel = NULL, hMole = NULL;
						  if (LodcomImage(imageData.data(), imageData.size(), img))
						  {
							  hMole = img.Detach();
							  bithandel = (HBITMAP)::SendMessage(imageHwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hMole);
							  DeleteObject(bithandel);
						  }

	}
		break;
	case WM_COMMAND:
	{
					   switch (LOWORD(wParam))
					   {
					   case IDC_BTN_OK:
					   {
										  char szCode[MAX_PATH];
										  ZeroMemory(szCode, MAX_PATH);
										  ::GetDlgItemTextA(hwndDlg, IDC_EDIT_CODE, szCode, MAX_PATH);
										  m_VerCode = szCode;
										  ::EndDialog(hwndDlg, 0);
					   }
						   break;
					   default:
						   break;
					   }
	}
	default:
		break;
	}
	return 0;
}

BOOL CBaiduParse::LodcomImage(LPVOID PmemIm, ULONG dwLen, CImage& ImgObj)
{
	LPVOID	  m_ImageBuf = NULL;
	BOOL bRet = FALSE;
	HGLOBAL	 Hglobal = ::GlobalAlloc(GMEM_MOVEABLE, dwLen);
	if (!Hglobal)
	{
		return bRet;
	}
	m_ImageBuf = ::GlobalLock(Hglobal);			//����ȫ�ַ�����ڴ�Ȼ����
	memset(m_ImageBuf, 0, dwLen);
	memcpy(m_ImageBuf, PmemIm, dwLen);
	::GlobalUnlock(Hglobal);		//�������
	IStream	 *Pstream = NULL;
	HRESULT hr = ::CreateStreamOnHGlobal(Hglobal, TRUE, &Pstream);
	if (FAILED(hr))return bRet;
	hr = ImgObj.Load(Pstream);
	if (FAILED(hr))return bRet;
	Pstream->Release();
	bRet = TRUE;
	::GlobalFree(Hglobal);		//�ͷ�ȫ���ڴ�����
	return bRet;
}



CBaiduParse::~CBaiduParse()
{

}
