#include "UpdateEx.h"
#include <shlwapi.h>
#include <thread>
#include "curl/curl.h"

#include <WinInet.h>
#include <TlHelp32.h>
#include <tchar.h>
#include "MyMiniZip.h"
#include "resource.h"
#include "base64.h"
#pragma comment(lib,"Wininet.lib")
#pragma comment(lib,"shlwapi.lib")
#ifdef _DEBUG
#pragma comment(lib,"libcurld.lib")
#else
#pragma comment(lib,"libcurl.lib")
#include "CEncryption.h"
#endif
std::shared_ptr<CUpdateEx> CUpdateEx::_Instaance = nullptr;
WNDPROC CUpdateEx::m_oldProc = NULL;

bool CdownLoad::m_isDownloadSucceed = false;
std::string CUpdateEx::CarkUrl(const char* url)
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

std::wstring CUpdateEx::getResourcesPath(const std::wstring name)
{
	std::wstring  bResult;
	std::wstring temp;
	std::vector<wchar_t> mbPath;
	mbPath.resize(MAX_PATH + 1);
	::GetModuleFileName(nullptr, mbPath.data(), MAX_PATH);
	if (!::PathRemoveFileSpec(mbPath.data())) return bResult;
	temp += mbPath.data();
	temp += L"\\";
	bResult = temp + name;
	return bResult;
}
void CUpdateEx::readJsFile(const wchar_t* path, std::vector<char>* buffer)
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
std::wstring CUpdateEx::utf8ToUtf16(const std::string& utf8String)
{
	std::wstring sResult;
	int nUTF8Len = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, NULL, NULL);
	auto pUTF8 = new wchar_t[nUTF8Len + 1];

	ZeroMemory(pUTF8, nUTF8Len + 1);
	MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, pUTF8, nUTF8Len);
	sResult = pUTF8;
	delete[] pUTF8;

	return sResult;
}
void CUpdateEx::RunApp()
{
	ZeroMemory(&app, sizeof(Application));
	app.url = L"http://baiducdp.com/ui/update.html"; // ʹ��hook�ķ�ʽ������Դ
	app.window = wkeCreateWebWindow(WKE_WINDOW_TYPE_POPUP, NULL, 0, 0, 400, 250);
	if (app.window == NULL)
		::PostQuitMessage(0);
	//����ȫ��cookies/����·��
	TCHAR szCookiePath[MAX_PATH];
	ZeroMemory(szCookiePath, MAX_PATH);
	::GetTempPath(MAX_PATH, szCookiePath);
	std::wstring szRootPath(szCookiePath);
	wcscat_s(szCookiePath, L"cookie.tmp");
	wkeSetCookieJarFullPath(app.window, szCookiePath);
	wkeSetLocalStorageFullPath(app.window, szRootPath.c_str());
	//���������ڵĴ��ھ��
	m_hwnd = wkeGetWindowHandle(app.window);
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
	HICON hicon = ::LoadIcon(g_hInstance, MAKEINTRESOURCEW(IDI_ICON1));
	SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hicon);
	SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon);
	m_oldProc = (WNDPROC)::SetWindowLongPtr(m_hwnd, GWL_WNDPROC, (DWORD)MainProc);
	//���ô��ھ�����ʾ
	wkeMoveToCenter(app.window);
	//������ҳ����ǰ�ص���Ҫ��Ϊ�˼��ر�����Դ
	wkeOnLoadUrlBegin(app.window, [](wkeWebView webView, void* param, const char* url, void *job)->bool {
		if (!param)return false;
		std::string localUrl = GetInstance()->CarkUrl(url);
		bool pos = localUrl == HOST_NAME ? true : false;
		if (pos) {
			const utf8* decodeURL = wkeUtilDecodeURLEscape(url);
			if (!decodeURL)
				return false;
			std::string urlString(decodeURL);
			std::string localPath = urlString.substr(sizeof(HOST_NAME) - 1);

			std::wstring path = GetInstance()->getResourcesPath(GetInstance()->utf8ToUtf16(localPath));
			std::vector<char> buffer;
			GetInstance()->readJsFile(path.c_str(), &buffer);
#if _DEBUG
			wkeNetSetData(job, buffer.data(), buffer.size());
#else
			std::string fileName = GetInstance()->Unicode_To_Ansi(path.c_str());
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
				if (!strcmpi(".png", szExtName) || !strcmpi(".gif", szExtName))
				{
					Deresult = aip::base64_decode(buffer.data());
					wkeNetSetData(job, (char*)Deresult.c_str(), Deresult.length());
				}
				else
				{
					if (fileName == "app.js")
					{
						Deresult = GetInstance()->Gbk_To_Utf8(Deresult.c_str());
					}
					wkeNetSetData(job, (char*)Deresult.c_str(), Deresult.length());
				}
			}
#endif
			return true;
		}
		return false;
	},this);
	//�����������ַ
	wkeLoadURLW(app.window, app.url.c_str());
	//document�ĵ����سɹ��ص�����
	wkeOnDocumentReady(app.window, [](wkeWebView webWindow, void* param) {
		wkeShowWindow(webWindow, true);
	}, this);
	//�ĵ����سɹ�
	wkeJsBindFunction("onloading", [](jsExecState es, void* param)->jsValue {
		if(!param)return jsUndefined();
		GetInstance()->m_downloadPtr.reset(new CdownLoad(GetInstance()->m_downloadUrl, GetInstance()->m_UnZipPath));
		auto threadProc = std::bind(&CdownLoad::start, GetInstance()->m_downloadPtr.get(), GetInstance()->m_hwnd);
 		std::thread ThreadDownload(threadProc);
 		ThreadDownload.detach();
		return jsUndefined();
	}, this, 0);
	MSG msg = { 0 };
	while (GetMessageW(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
}
void CUpdateEx::SetDownloadUrl(const std::string& strUrl) {
	this->m_downloadUrl = strUrl;
}
void CUpdateEx::SetSavePath(const std::string& strPath) {
	this->m_UnZipPath = strPath;
}

std::string CUpdateEx::GetTextMid(const std::string strSource, const std::string strLeft, const std::string strRight)
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

LRESULT CALLBACK CUpdateEx::MainProc(_In_ HWND hwnd, _In_ UINT uMsg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	switch (uMsg)
	{
	case UPDATE_PROGRESS_VALUE:
	{
		if (lParam)
		{
			DWORD dwProgess = lParam;
			::OutputDebugStringA(std::to_string(dwProgess).c_str());
			OutputDebugStringA("\r\n");
			if (GetInstance()->app.window)
			{
				std::string script = "app.progress =" + std::to_string(dwProgess) + ";";
				wkeRunJS(GetInstance()->app.window, script.c_str());
			}
			if (dwProgess>=100)
			{
				if (GetInstance()->app.window)
				{
					std::string script = "app.progress =" + std::to_string(dwProgess) + ";";
					script = "app.msgText =\"���ڽ�ѹ��Դ\";";
					script = GetInstance()->Gbk_To_Utf8(script.c_str());
					wkeRunJS(GetInstance()->app.window, script.c_str());
				}
			}
		}
	}
	break;
	case UI_QUIT_MSG:
	{
		if (GetInstance()->app.window)
		{
			std::string script = "app.msgText =\"��װ���\";";
			script = GetInstance()->Gbk_To_Utf8(script.c_str());
			wkeRunJS(GetInstance()->app.window, script.c_str());
		}
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));
		si.dwFlags = STARTF_USESHOWWINDOW;  // ָ��wShowWindow��Ա��Ч
		si.wShowWindow = true;          // �˳�Ա��ΪTRUE�Ļ�����ʾ�½����̵������ڣ�
		std::string strCommandlineArg;
		CHAR szPath[MAX_PATH];
		ZeroMemory(szPath, MAX_PATH);
		::GetModuleFileNameA(NULL, szPath, MAX_PATH);
		if (PathRemoveFileSpecA(szPath))
		{
			strcat_s(szPath, "\\BaiduCdp.exe");
			strCommandlineArg = szPath;
#if !_DEBUG
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
#endif
		}
		::PostQuitMessage(0); 
	}
		break;
	default:
		break;
	}
	return CallWindowProc(m_oldProc, hwnd, uMsg, wParam, lParam);
}

CUpdateEx::CUpdateEx()
	:m_hwnd(NULL)
	,m_downloadUrl("")
	,m_UnZipPath("")
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
		exit(0);
	}
}

CUpdateEx* CUpdateEx::GetInstance()
{
	if (_Instaance == nullptr) {
		if (!_Instaance.get()) {
			_Instaance.reset(new CUpdateEx);
		}
	}
	return _Instaance.get();
}
std::string CUpdateEx::Unicode_To_Ansi(const wchar_t* szbuff)
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

std::string CUpdateEx::Gbk_To_Utf8(const char* szBuff)
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

CUpdateEx::~CUpdateEx()
{
}
bool CUpdateEx::InintMiniBlink()
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
size_t CdownLoad::write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
	return written;
}
CdownLoad::CdownLoad(const std::string& strUrl, const std::string& strPath)
	:m_strUrl(strUrl)
	,m_strPath(strPath)
{
	m_strPath += std::to_string(::GetTickCount()) + ".tmp";
}

void CdownLoad::start(HWND progress_callbackex)
{
	CURL *curl_handle;
	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();
	struct curl_slist* headerList = nullptr;
	curl_easy_setopt(curl_handle, CURLOPT_URL, m_strUrl.c_str());
	headerList = curl_slist_append(headerList, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/69.0.3497.100 Safari/537.36");
	headerList = curl_slist_append(headerList, "accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/apng,*/*;q=0.8");
	headerList = curl_slist_append(headerList, "accept-language: zh-CN,zh;q=0.9");
	headerList = curl_slist_append(headerList, "upgrade-insecure-requests: 1");
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, FALSE);
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, false);	// ��֤�Է���SSL֤��
	curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, false);	//����������֤֤�������
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, (long)1);
	curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, progress_callbackex);
	curl_easy_setopt(curl_handle, CURLOPT_PROGRESSFUNCTION, progress_callback);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
	auto pagefile = fopen(m_strPath.c_str(), "wb");
	if (pagefile)
	{
		CURLcode dwCurlCode;
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
		dwCurlCode = curl_easy_perform(curl_handle);
		std::string  content;
		if (CURLE_OK != dwCurlCode)
		{
			content = curl_easy_strerror(dwCurlCode);
			fclose(pagefile);
			PostMessage(progress_callbackex, UI_QUIT_MSG, NULL, NULL);
			return;
		}
		fclose(pagefile);
		if (m_isDownloadSucceed)
		{
			detectionProcessIsExist(MAIN_APP_PROCESS_NAME);
			//��ѹ
			MyMiniZip UnZip;
			CHAR szPath[MAX_PATH];
			ZeroMemory(szPath, MAX_PATH);
			::GetModuleFileNameA(NULL, szPath, MAX_PATH);
			if (PathRemoveFileSpecA(szPath))
			{
				strcat_s(szPath, "\\");
				UnZip.unZipPackageToLoacal(m_strPath, szPath);
			}
		}
	}
	if (headerList)
	{
		curl_slist_free_all(headerList);
		headerList = nullptr;
	}
	if (curl_handle)
	{
		curl_easy_cleanup(curl_handle);
		curl_handle = nullptr;
	}
	curl_global_cleanup();	//�ر�curl����
	PostMessage(progress_callbackex, UI_QUIT_MSG, NULL, NULL);
}

CdownLoad::~CdownLoad()
{

}

int CdownLoad::progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	auto hwnd = (HWND)clientp;
	if (::IsWindow(hwnd))
	{
		DWORD dwValue = (DWORD)dlnow / dltotal * 100;
		if (dwValue>=100)
		{
			m_isDownloadSucceed = true;
		}
		PostMessage(hwnd, UPDATE_PROGRESS_VALUE, NULL, (LPARAM)dwValue);
	}
	return CURLE_OK;
}
bool CdownLoad::detectionProcessIsExist(const TCHAR* szProcessName)
{
	bool bRet = false;
	HANDLE helpSnap = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
	if (INVALID_HANDLE_VALUE == helpSnap)
		return bRet;
	PROCESSENTRY32 pe;
	ZeroMemory(&pe, sizeof(PROCESSENTRY32));
	pe.dwSize = sizeof(PROCESSENTRY32);
	bRet = Process32First(helpSnap, &pe);
	if (!bRet)
	{
		CloseHandle(helpSnap);
		return bRet;
	}
	do
	{
		if (!_tcsicmp(pe.szExeFile, szProcessName))
		{
			HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe.th32ProcessID);
			if (hProcess)
			{
				::TerminateProcess(hProcess, NULL);
				::CloseHandle(hProcess);
			}
		}
	} while (Process32Next(helpSnap, &pe));
	::CloseHandle(helpSnap);
	return bRet;
}