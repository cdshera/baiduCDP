#ifndef __WKEWINDOW__
#define __WKEWINDOW__
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include "GlobalHeader.h"
#include "PanParse.h"
#include "shadow_window.h"
#include "glog/logging.h"

// #define GOOGLE_GLOG_DLL_DECL
// #define GLOG_NO_ABBREVIATED_SEVERITIES
#ifndef _DEBUG
#pragma comment(lib,"glog.lib")
#else
#pragma  comment(lib,"glogd.lib")
#endif
typedef websocketpp::client<websocketpp::config::asio_client> client;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
// pull out the type of messages sent by our config
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;
typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
typedef client::connection_ptr connection_ptr;
typedef struct downFileListInfo
{
	std::string connections; //��ǰ�������ӷ���������
	std::string DownloadSpeed; // �����ٶ�
	size_t Downloadpercentage;//���ؽ��Ȱٷֱ�
	std::string strFileName;
	std::string strFileGid;
	UINT nErrCode;
	std::string strErrMessage;
	std::string strStatus;	//��ǰ�ļ�������״̬
	std::string strPath;	//��ǰ�ļ����·��
}DOWNFILELISTINFO;
typedef std::vector<DOWNFILELISTINFO> ActiceDownloadList;
/*
��������ǰ��״̬
activeĿǰ��������
waiting���ڶ����е�����; ����δ����
paused��ͣ���ء�
error����������ֹͣ�����ء�
completeֹͣ��������ء�
removed�û�ɾ��������
*/
#define ARIA2_STATUS_ACTIVE "active"
#define ARIA2_STATUS_WAITING "waiting"
#define ARIA2_STATUS_PAUSED "paused"
#define ARIA2_STATUS_ERROR "error"
#define ARIA2_STATUS_COMPLETE "complete"
#define ARIA2_STATUS_REMOVED "removed"
typedef struct downloadStatus
{
	std::string strFileGid;	//��������Ψһ�ı�ʶ��
	std::string strFileStatus;
}DOWNLOADSTATUS;
/*����ʧ�����Դ���*/
typedef struct retryCount
{
	std::string strFileName;
	UINT nCount;
}RETRYCOUNT;
/*
�Զ�����Ϣ
*/
#define ARIA2_ADDURL_MSG WM_USER + 0x101
#define ARIA2_DOWNLOAD_START_MSG WM_USER + 0x102
#define ARIA2_DOWNLOAD_COMPLATE_MSG WM_USER + 0x103
#define ARIA2_DOWNLOAD_PAUSE_MSG WM_USER + 0x104
#define ARIA2_DOWNLOAD_STOP_MSG WM_USER + 0x105
#define ARIA2_DOWNLOAD_ERROR_MSG WM_USER + 0x106
#define ARIA2_UPDATE_TELLACTIVE_LIST_MSG WM_USER +0x107
#define ARIA2_UPDATE_TELLERROR_LIST_MSG WM_USER +0x108
#define ARIA2_PURGEDOWNLOAD_MSG WM_USER +0x109
#define ARIA2_RETRYADDURL_MSG WM_USER +0x110
#define UI_DOWNLOAD_SHARE_UPDATE_LIST WM_USER + 0x111
#define UI_UPDATE_FOLODER_LIST_MSG WM_USER + 0x112
#define UI_UPDATE_OFF_LINE_LIST_MSG WM_USER + 0x113
#define UI_UPDATE_USER_FILE_DATA_MSG WM_USER +0x114
#define UI_QUIT_MSG WM_USER +0x115
#define UI_SHOW_ERROR_MSG WM_USER + 0x116
/*
�Զ�����Ϣ����
*/
/*Aria2����״̬*/
#define ARIA2_DOWNLOAD_START "aria2.onDownloadStart"
#define ARIA2_DOWNLOAD_COMPLATE "aria2.onDownloadComplete"
#define ARIA2_DOWNLOAD_PAUSE "aria2.onDownloadPause"
#define ARIA2_DOWNLOAD_STOP "aria2.onDownloadStop"
#define ARIA2_DOWNLOAD_ERROR "aria2.onDownloadError"
/*Aria2����״̬end*/
/*���Ͳ�ѯ�Ĺ̶�����*/
#define ARIA2_TELLACTICE_SENDDATA "{\"method\":\"aria2.tellActive\",\"params\":[\"token:CDP\",[\"gid\",\"status\",\"files\",\"totalLength\",\"completedLength\",\"downloadSpeed\",\"connections\",\"errorCode\",\"errorMessage\"]],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_TELLSTATUS_SENDDATA "{\"method\":\"aria2.tellStatus\",\"params\":[\"token:CDP\",\"%1%\",[\"gid\",\"status\",\"files\",\"totalLength\",\"completedLength\",\"downloadSpeed\",\"connections\",\"errorCode\",\"errorMessage\"]],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_GETGLOBAL_STATUS "{\"method\":\"aria2.getGlobalStat\",\"params\":[\"token:CDP\"],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_TELLSTOPPED "{\"method\":\"aria2.tellStopped\",\"params\":[\"token:CDP\",0,%1%,[\"gid\",\"status\",\"totalLength\",\"completedLength\",\"downloadSpeed\",\"connections\",\"errorCode\",\"errorMessage\",\"files\"]],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_TELLWAITING "{\"method\":\"aria2.tellWaiting\",\"params\":[\"token:CDP\",0,%1%,[\"gid\",\"status\",\"totalLength\",\"completedLength\",\"downloadSpeed\",\"connections\",\"errorCode\",\"errorMessage\",\"files\"]],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_PURGEDOWNLOAD_RESULT "{\"method\":\"aria2.removeDownloadResult\",\"params\":[\"token:CDP\",\"%1%\"],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_UNPAUSE "{\"method\":\"aria2.unpause\",\"params\":[\"token:CDP\",\"%1%\"],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_PAUSE "{\"method\":\"aria2.pause\",\"params\":[\"token:CDP\",\"%1%\"],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_REMOVE "{\"method\":\"aria2.remove\",\"params\":[\"token:CDP\",\"%1%\"],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_FORCEPAUSEALL "{\"method\":\"aria2.forcePauseAll\",\"params\":[\"token:CDP\"],\"id\":1,\"jsonrpc\":\"2.0\"}"
#define ARIA2_UNAUSEALL "{\"method\":\"aria2.unpauseAll\",\"params\":[\"token:CDP\"],\"id\":1,\"jsonrpc\":\"2.0\"}"
//ARIA2http����Э��
#define ARIA2_HTTP_REQUESTURL "http://127.0.0.1:6810/jsonrpc"
//�ö�ʱ�����·��Ͳ�ѯ����
#define UPDTAE_UI_TIMEID 508
//API����������
#define API_DOMAIN_NAME "https://api.baiducdp.com"
typedef struct DownloadSpeedinfo
{
	std::string strUnit;	//��λ
	float dwSize;
}DOWNLOADSPEEDINFO;
enum ShadowStatus
{
	SS_ENABLED = 1,
	SS_VISABLE = 1 << 1,	
	SS_PARENTVISIBLE = 1 << 2,	
	SS_DISABLEDBYAERO = 1 << 3
};
class CWkeWindow
{
public:
	~CWkeWindow();
	static CWkeWindow* GetInstance();
private:
	explicit CWkeWindow();
	CWkeWindow(const CWkeWindow&);
//	CWkeWindow& operator =(const CWkeWindow&);
	static std::shared_ptr<CWkeWindow> Instance;
private:
	
	/*
	�ж��Ƿ񴥷��� �ٶȵ�¼�ɹ��ص�
	*/
	bool isLogin;
	/*
	����ٶȵ�¼�����ЧCookie
	*/
	std::string strCookies;
	/*�ٶ����̽������*/
	CBaiduParse m_BaiduPare;
	/*
	��Ӱ����
	*/
	ShadowWnd* m_ShadowWnd;
	/*
	Wss�������
	*/
	client m_WssClient;
	/*�����ڵĴ��ھ��*/
	HWND m_hwnd;
	//���������ڵ��»ص�����
	static LRESULT CALLBACK MainProc(
		_In_ HWND   hwnd,
		_In_ UINT   uMsg,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam
		);
 static	WNDPROC m_oldProc;
 //�����ڴ���������Ͷ������ڴ���Ҫ�����������ͷ�
 void* AlloclocalHeap(const std::string& strBuffer);
 //aria2����˷���������
 void ParseAria2JsonInfo(const std::string& strJSon);
 //aria2��ǰ�����������
 size_t numActive;
 //aria2 ��ǰֹͣ����������
 size_t numStopped;
 //aria2 ��ǰ�ȴ����ص�����
 size_t numWaiting;
 //�����ļ������·��
 std::string m_downloadSavePath;
 //��ȡ�ļ���С����
 inline DOWNLOADSPEEDINFO GetFileSizeType(double dSize);
 //ȡ�ٷֱ�
 inline double Getpercentage(double completedLength, double totalLength);
 //����ʧ�����Դ���������
 std::vector<RETRYCOUNT> m_RetryArray;
 //����������������ļ�����Ϣ
 std::vector<REQUESTINFO> m_ShareFileArray;
 //��ӷ���������Ϣ������
 void AddShareFileItem(REQUESTINFO item);
 //��ȡ��Ҫ��Ԫ����Ϣ
void GetShareFileItem(const std::string& strFileName, REQUESTINFO& Result);
 //�ж����ԵĴ����Ƿ��Ѿ�����
 bool IsRetryCountFinish(const std::string& strFileName);
 //�ж���Ҫ���Ե������Ƿ���ڲ����ھʹ���
 bool IsRetryExist(const std::string& strFileName);
 //�޸����ԵĴ���
 void addRetryCount(const std::string& strFileName);
 //����Ψһ��ʶ����UI�����ȡ�ļ�����Ϣ
 DOWNFILELISTINFO GetFileCompletedInfo(const std::string& strGid);
 //����������ɺ��json��ȡ�ļ���
 DOWNFILELISTINFO GetTellStatusFileName(const std::string& strJSon);
 private:
#if 1
	/*wss������ػص���ʼ*/
	//socket init�������
	void on_socket_init(websocketpp::connection_hdl);
	//���շ���˷�������Ϣ
	void on_message(websocketpp::connection_hdl hdl, message_ptr);
	//�ͻ��������˽�������ʱ�Ļص�
	void on_open(websocketpp::connection_hdl hdl);
	//���յ��ر�ʱ�Ļص�
	void on_close(websocketpp::connection_hdl);
	//�յ�ʧ����Ϣʱ�Ļص�
	void on_fail(websocketpp::connection_hdl hdl);
	//����������ӳɹ��󷵻����Ӿ��
	websocketpp::connection_hdl m_hdl;
	//���������˵�����
	void start(std::string uri);
	//���ӵ�aria2��WSS�����
	void Connect();
	//�����ı����ݸ������
	 void SendText(std::string& strText);
	/*wss������ػص�����*/
#endif
	//����aria2
	 BOOL RunAria2();
	//wss������Ҫ��������һ���߳�
	std::shared_ptr<std::thread> m_RunThreadPtr;
	//�������ݸ�Aria2�Ķ�ʱ���ص�����
	static void CALLBACK TimeProc(HWND hwnd, UINT message, UINT iTimerID, DWORD dwTime);

public:
	/*
	@��������
	*/
	void runApp(Application* app);
	/*
	@ �����������Ҫ�ĳ��û������ø��ֻص�����
	*/
	bool createWebWindow(Application* app);
	/*
	��Ϣѭ��
	*/
	void runMessageLoop(Application* app);
	/*
	�˳�����
	*/
	void quitApplication(Application* app);
	/*
	��ȡwebWindow���ھ��
	*/
	HWND getHWND();
	void blinkMaximize();
	void blinkMinimize();
	void blinkClose();
	/*
	ϵͳ�˵�JS�ص�����
	*/
	static jsValue SysMenuJsNativeFunction(jsExecState es, void* param);

	/*
	�˳���ǰ�˺�
	*/
	static jsValue LogOut(jsExecState es, void* param);
	/*ö���û���ǰ�˻������ļ���*/
	static jsValue EnumFolder(jsExecState es, void* param);
	/*��ȡ��ǰ�������ص��б�������������������*/
	static jsValue OffLineDownload (jsExecState es, void* param);
	/*
	��֤�Ƿ��Ѿ���Ч�ĵ�¼�˰ٶ�����
	*/
	static jsValue IsLoginBaidu(jsExecState es, void* param);
	/*
	��ͣ/�ָ�/ɾ������
	*/
	static jsValue AraiaPauseStartRemove(jsExecState es, void* param);
	/*
	�����ļ��ص�����
	*/
	static jsValue ShareBaiduFile(jsExecState es, void* param);
	/*�������ٶ��û��ļ�*/
	static jsValue BaiduFileRename(jsExecState es, void* param);
	/*
	ɾ���ļ������ļ���
	*/
	static jsValue DeleteBaiduFile(jsExecState es, void* param);
	/*
	����˹رյĻص����������� true �����ٴ��ڣ����� false ʲô��������
	*/
	static bool OnCloseCallBack(wkeWebView webWindow, void* param);
	/*���ļ�����Ŀ¼*/
	static jsValue OpenFilePlaceFolder(jsExecState es, void* param);
	/*�л�������������Ŀ¼*/
	static jsValue switchShareFolder(jsExecState es, void* param);
	/*
	�ص�������������
	*/
	static void OnWindowDestroy(wkeWebView webWindow, void* param);
	/*
	�ص����ĵ����سɹ�
	*/
	static void OnDocumentReady(wkeWebView webWindow, void* param);
	/*
	�ص� ������ҳ����ʱ�Ļص� ��������hook
	*/
	static bool onLoadUrlBegin(wkeWebView webView, void* param, const char* url, void *job);
	// �ص�:��ȡ��¼�ٶ��̵�cookie
	static void GetLoginCookieCallBack(wkeWebView webWindow, void* param);
	/*
	�л�����Ŀ¼
	*/
	static jsValue SwitchDirPath(jsExecState es, void* param);
	/*��ָ����ַ*/
	static jsValue OpenAssignUrl(jsExecState es, void* param);
	/*����Ӧ��*/
	static jsValue UpdateApp(jsExecState es, void* param);
	/*����Ƿ���Ҫ����*/
	static jsValue isUpdate(jsExecState es, void* param);
	/*
	�����û����̵��ļ�
	*/
	static jsValue DownloadUserFile(jsExecState es, void* param);
	/*���ط�������*/
	static jsValue DownShareFile(jsExecState es, void* param);
	/*�����û�ѡ��ķ�������*/
	static jsValue DownSelectShareFile(jsExecState es, void* param);
	/*�ж����ط����������ʲô��������
	 1���ٶ����� 2���������� 3����ͨ���� 0 ����֪�������ص�ַ
	 */
	int JudgeDownUrlType(const std::string& strUrl);
	/*
	��װAria2���ص�json��Ϣ
	*/
	std::string CreateDowndAria2Json(REQUESTINFO& Downdinfo);
	//�ַ����滻
	std::string& replace_all_distinct(std::string& str, const std::string& old_value, const std::string& new_value);
	// ���ذٶ������ڵ��ļ�
	bool DownloadUserLocalFile(const std::string& strJsonData);
	//����UI���洫�����������ļ�json Ͷ�ݸ�aria2����
	void DownloadUiShareFile(const std::string & strJson);
	/* �ص��������µ�ҳ�棬����˵������ window.open ���ߵ���� <a target="_blank" .../>*/
   static wkeWebView onCreateView(wkeWebView webWindow, void* param, wkeNavigationType navType, const wkeString url, const wkeWindowFeatures* features);
   /*�����������ص��б����ݵ�UI����*/
   void UpdateDownloadList(const std::string& strJson);
   /*
	�ֽ�URL���ӵ�ַ��ȡ������
	*/
	std::string CarkUrl(const char* url);
	/*
	��ȡ��Դ��Ŀ¼
	*/
	std::wstring getResourcesPath(const std::wstring name);
	/*
	����ת��
	*/
	std::wstring utf8ToUtf16(const std::string& utf8String);
	/*
	��ȡ�����ļ���Դ
	*/
	void readJsFile(const wchar_t* path, std::vector<char>* buffer);
	/*��ȡ�ļ�*/
	DWORD ReadFileBuffer(std::string szFileName, PVOID* pFileBuffer);
	/*
	@ ��ʼ��miniblink
	*/
	bool InintMiniBlink();
};
#endif
