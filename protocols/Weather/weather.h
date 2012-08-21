/*
Weather Protocol plugin for Miranda IM
Copyright (C) 2005-2011 Boris Krasnovskiy All Rights Reserved
Copyright (C) 2002-2005 Calvin Che

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* This file contains the includes, weather constants/declarations,
   the structs, and the primitives for some of the functions.
*/

//============  THE INCLUDES  ===========

#define _CRT_SECURE_NO_WARNINGS
#include <m_stdhdr.h>

#include <stdio.h>
#include <io.h>
#include <share.h>
#include <direct.h>
#include <process.h>
#include <time.h>

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>

#include <win2k.h>

#define MIRANDA_VER 0x0A00

#include <newpluginapi.h>
#include <m_system.h>
#include <m_protomod.h>
#include <m_protosvc.h>
#include <m_clist.h>
#include <m_icolib.h>
#include <m_options.h>
#include <m_langpack.h>
#include <m_skin.h>
#include <m_database.h>
#include <m_history.h>
#include <m_utils.h>
#include <m_userinfo.h>
#include <m_netlib.h>
#include <m_ignore.h>
#include <m_findadd.h>
#include <m_button.h>
#include <m_avatars.h>
#include <m_clui.h>
#include <m_clc.h>
#include <m_fontservice.h>
#include <m_skin_eng.h>
#include <m_cluiframes.h>

#include <m_popup.h>

#include "m_weather.h"
#include "resource.h"
#include "version.h"

//============  CONSTANTS  ============

// status
#define NOSTATUSDATA	1

// limits
#define MAX_TEXT_SIZE	4096
#define MAX_DATA_LEN	1024

// db info mangement mode
#define WDBM_REMOVE			1
#define WDBM_DETAILDISPLAY	2

// more info list column width
#define LIST_COLUMN		150

// others
#define NODATA			TranslateT("N/A")
#define UM_SETCONTACT	40000

// weather update error codes
#define INVALID_ID_FORMAT	10
#define INVALID_SVC			11
#define	INVALID_ID			12
#define SVC_NOT_FOUND		20
#define	NETLIB_ERROR		30
#define	DATA_EMPTY			40
#define	DOC_NOT_FOUND		42
#define	DOC_TOO_SHORT		43
#define	UNKNOWN_ERROR		99

// weather update error text
#define E10		TranslateT("Invalid ID format, missing \"/\" (10)")
#define E11		TranslateT("Invalid service (11)")
#define E12		TranslateT("Invalid station (12)")
#define E20		TranslateT("Weather service ini for this station is not found (20)")
#define E30		TranslateT("Netlib error - check your internet connection (30)")
#define E40		TranslateT("Empty data is retrieved (40)")
#define E42		TranslateT("Document not found (42)")
#define E43		TranslateT("Document too short to contain any weather data (43)")
#define E99		TranslateT("Unknown error (99)")

// HTTP error... not all translated
// 100 Continue
// 101 Switching Protocols
// 200 OK
// 201 Created
// 202 Accepted
// 203 Non-Authoritative Information
#define E204	TranslateT("HTTP Error: No content (204)")
// 205 Reset Content
// 206 Partial Content
// 300 Multiple Choices
#define E301	TranslateT("HTTP Error: Data moved (301)")
// 302 Found
// 303 See Other
// 304 Not Modified
#define E305	TranslateT("HTTP Error: Use proxy (305)")
// 306 (Unused)
#define E307	TranslateT("HTTP Error: Temporary redirect (307)")
#define E400	TranslateT("HTTP Error: Bad request (400)")
#define E401	TranslateT("HTTP Error: Unauthorized (401)")
#define E402	TranslateT("HTTP Error: Payment required (402)")
#define E403	TranslateT("HTTP Error: Forbidden (403)")
#define E404	TranslateT("HTTP Error: Not found (404)")
#define E405	TranslateT("HTTP Error: Method not allowed (405)")
// 406 Not Acceptable
#define E407	TranslateT("HTTP Error: Proxy authentication required (407)")
// 408 Request Timeout
// 409 Conflict
#define E410	TranslateT("HTTP Error: Gone (410)")
// 411 Length Required
// 412 Precondition Failed
// 413 Request Entity Too Large
// 414 Request-URI Too Long
// 415 Unsupported Media Type
// 416 Requested Range Not Satisfiable
// 417 Expectation Failed
#define E500	TranslateT("HTTP Error: Internal server error (500)")
// 501 Not Implemented
#define E502	TranslateT("HTTP Error: Bad gateway (502)")
#define E503	TranslateT("HTTP Error: Service unavailable (503)")
#define E504	TranslateT("HTTP Error: Gateway timeout (504)")
// 505 HTTP Version Not Supported

// defaults constants
#define C_DEFAULT "%n  [%t, %c]"
#define N_DEFAULT "%c\nTemperature: %t\nFeel-Like: %f\nPressure: %p\nWind: %i  %w\nHumidity: %m\nDew Point: %e\nVisibility: %v\n\nSun Rise: %r\nSun Set: %y\n\n5 Days Forecast:\n%[Forecast Day 1]\n%[Forecast Day 2]\n%[Forecast Day 3]\n%[Forecast Day 4]\n%[Forecast Day 5]"
#define B_DEFAULT "Feel-Like: %f\nPressure: %p\nWind: %i  %w\nHumidity: %m\nDew Point: %e\nVisibility: %v\n\nSun Rise: %r\nSun Set: %y\n\n5 Days Forecast:\n%[Forecast Day 1]\n%[Forecast Day 2]\n%[Forecast Day 3]\n%[Forecast Day 4]\n%[Forecast Day 5]"
#define b_DEFAULT "Weather Condition for %n as of %u"
#define X_DEFAULT N_DEFAULT
#define H_DEFAULT "%c, %t (feel-like %f)	Wind: %i %w	Humidity: %m"
#define E_DEFAULT "%n at %u:	%c, %t (feel-like %f)	Wind: %i %w	Humidity: %m"
#define P_DEFAULT "%n   (%u)"
#define p_DEFAULT "%c, %t\nToday:  High %h, Low %l"
#define s_DEFAULT "Temperature: %[Temperature]"


//============  OPTION STRUCT  ============

// option struct
typedef struct {
// main options
	BOOL AutoUpdate;
	BOOL CAutoUpdate;
	BOOL StartupUpdate;
	WORD UpdateTime;
	WORD AvatarSize;
	BOOL NewBrowserWin;
	BOOL NoProtoCondition;
	BOOL UpdateOnlyConditionChanged;
	BOOL RemoveOldData;
	BOOL MakeItalic;
// units
	WORD tUnit;
	WORD wUnit;
	WORD vUnit;
	WORD pUnit;
	WORD dUnit;
	WORD eUnit;
	TCHAR DegreeSign[4];
	BOOL DoNotAppendUnit;
    BOOL NoFrac;
// texts
	TCHAR *cText;
	TCHAR *bTitle;
	TCHAR *bText;
	TCHAR *nText;
	TCHAR *eText;
	TCHAR *hText;
	TCHAR *xText;
	TCHAR *sText;
// advanced
	BOOL DisCondIcon;
// popup options
	BOOL UsePopup;
	BOOL UpdatePopup;
	BOOL AlertPopup;
	BOOL PopupOnChange;
	BOOL ShowWarnings;
// popup colors
	BOOL UseWinColors;
	COLORREF BGColour;
	COLORREF TextColour;
// popup actions
	DWORD LeftClickAction;
	DWORD RightClickAction;
// popup delay
	DWORD pDelay;
// popup texts
	TCHAR *pTitle;
	TCHAR *pText;
// other misc stuff
	TCHAR Default[64];
	HANDLE DefStn;
} MYOPTIONS;

void DestroyOptions(void);

//============  STRUCT USED TO MAKE AN UPDATE LIST  ============

struct WCONTACTLIST {
	HANDLE hContact;
	struct WCONTACTLIST *next;
};

typedef struct WCONTACTLIST UPDATELIST;

extern UPDATELIST *UpdateListHead;
extern UPDATELIST *UpdateListTail;

void DestroyUpdateList(void);

//============  DATA FORMAT STRUCT  ============

#define WID_NORMAL	0
#define WID_SET		1
#define WID_BREAK	2

typedef struct {
	TCHAR *Name;
	TCHAR *Start;
	TCHAR *End;
	TCHAR *Unit;
	char  *Url;
	TCHAR *Break;
	int Type;
} WIDATAITEM;

struct WITEMLIST {
	WIDATAITEM Item;
	struct WITEMLIST *Next;
};

typedef struct WITEMLIST WIDATAITEMLIST;

typedef struct {
	BOOL Available;
	char *SearchURL;
	TCHAR *NotFoundStr;
	WIDATAITEM Name;
} WIIDSEARCH;

typedef struct {
	BOOL Available;
	TCHAR *First;
	WIDATAITEM Name;
	WIDATAITEM ID;
} WINAMESEARCHTYPE;

typedef struct {
	char *SearchURL;
	TCHAR *NotFoundStr;
	TCHAR *SingleStr;
	WINAMESEARCHTYPE Single;
	WINAMESEARCHTYPE Multiple;
} WINAMESEARCH;

struct STRLIST {
	TCHAR *Item;
	struct STRLIST *Next;
};

typedef struct STRLIST WICONDITEM;

typedef struct {
	WICONDITEM *Head;
	WICONDITEM *Tail;
} WICONDLIST;

typedef struct {
	TCHAR *FileName;
	TCHAR *ShortFileName;
	BOOL Enabled;

	// header
	TCHAR *DisplayName;
	TCHAR *InternalName;
	TCHAR *Description;
	TCHAR *Author;
	TCHAR *Version;
	int InternalVer;
	size_t MemUsed;

	// default
	char  *DefaultURL;
	TCHAR *DefaultMap;
	char  *UpdateURL;
	char  *UpdateURL2;
	char  *UpdateURL3;
	char  *UpdateURL4;
	char *Cookie;
// items
	int UpdateDataCount;
	WIDATAITEMLIST *UpdateData;
	WIDATAITEMLIST *UpdateDataTail;
	WIIDSEARCH IDSearch;
	WINAMESEARCH NameSearch;
	WICONDLIST CondList[10];
} WIDATA;

//============  DATA LIST (LINKED LIST)  ============

struct DATALIST {
	WIDATA Data;
	struct DATALIST *next;
};

typedef struct DATALIST WIDATALIST;

//============  GLOBAL VARIABLES  ============

extern WIDATALIST *WIHead;
extern WIDATALIST *WITail;

extern HINSTANCE hInst;
extern HWND hPopupWindow;
extern HWND hWndSetup;

extern MYOPTIONS opt;

extern unsigned status;
extern unsigned old_status;

extern HANDLE hDataWindowList;
extern HANDLE hNetlibUser, hNetlibHttp;
extern HANDLE hHookWeatherUpdated;
extern HANDLE hHookWeatherError;
extern HANDLE hWindowList;
extern HANDLE hMwinMenu;

extern UINT_PTR timerId;

// check if weather is currently updating
extern BOOL ThreadRunning;

//============  FUNCTION PRIMITIVES  ============

// functions in weather.c
void UpgradeContact(DWORD lastver, HANDLE hContact);

// functions in weather_addstn.c
INT_PTR WeatherAddToList(WPARAM wParam,LPARAM lParam);
BOOL CheckSearch();

int IDSearch(TCHAR *id, const int searchId);
int NameSearch(TCHAR *name, const int searchId);

INT_PTR WeatherBasicSearch(WPARAM wParam,LPARAM lParam);
INT_PTR WeatherCreateAdvancedSearchUI(WPARAM wParam, LPARAM lParam);
INT_PTR WeatherAdvancedSearch(WPARAM wParam, LPARAM lParam);

int WeatherAdd(WPARAM wParam, LPARAM lParam);

// functions used in weather_contacts.c
INT_PTR ViewLog(WPARAM wParam,LPARAM lParam);
INT_PTR LoadForecast(WPARAM wParam,LPARAM lParam);
INT_PTR WeatherMap(WPARAM wParam,LPARAM lParam);

INT_PTR EditSettings(WPARAM wParam,LPARAM lParam);
INT_PTR CALLBACK DlgProcChange(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

int ContactDeleted(WPARAM wParam,LPARAM lParam);

BOOL IsMyContact(HANDLE hContact);

// functions in weather_conv.c
BOOL is_number(char *s);

void GetTemp(TCHAR *tempchar, TCHAR *unit, TCHAR *str);
void GetSpeed(TCHAR *tempchar, TCHAR *unit, TCHAR *str);
void GetPressure(TCHAR *tempchar, TCHAR *unit, TCHAR *str);
void GetDist(TCHAR *tempchar, TCHAR *unit, TCHAR *str);
void GetElev(TCHAR *tempchar, TCHAR *unit, TCHAR *str);

WORD GetIcon(const TCHAR* cond, WIDATA *Data);
void CaseConv(TCHAR *str);
void TrimString(char *str);
void TrimString(WCHAR *str);
void ConvertBackslashes(char *str);
char *GetSearchStr(char *dis);

TCHAR *GetDisplay(WEATHERINFO *w, const TCHAR *dis, TCHAR* str);
INT_PTR GetDisplaySvcFunc(WPARAM wParam, LPARAM lParam);

void GetSvc(TCHAR *pszID);
void GetID(TCHAR *pszID);

TCHAR *GetError(int code);

// functions in weather_data.c
void GetStationID(HANDLE hContact, TCHAR* id, size_t idlen);
WEATHERINFO LoadWeatherInfo(HANDLE Change);
int DBGetData(HANDLE hContact, char *setting, DBVARIANT *dbv);
int DBGetStaticString(HANDLE hContact, const char *szModule, const char *valueName, TCHAR *dest, size_t dest_len);

void EraseAllInfo(DWORD lastver);

void LoadStationData(TCHAR *pszFile, TCHAR *pszShortFile, WIDATA *Data);
void GetDataValue(WIDATAITEM *UpdateData, TCHAR *Data, TCHAR** szInfo);
void ConvertDataValue(WIDATAITEM *UpdateData, TCHAR *Data);
void wSetData(char **Data, const char *Value);
void wSetData(WCHAR **Data, const char *Value);
void wSetData(WCHAR **Data, const WCHAR *Value);
void wfree(char **Data);
void wfree(WCHAR **Data);

void DBDataManage(HANDLE hContact, WORD Mode, WPARAM wParam, LPARAM lParam);
int GetWeatherDataFromDB(const char *szSetting, LPARAM lparam);

// functions in weather_http.c
int InternetDownloadFile (char *szUrl, char *cookie, TCHAR** szData);
void NetlibInit();
void NetlibHttpDisconnect(void);

// functions in weather_ini.c
void WIListAdd(WIDATA Data);
WIDATA* GetWIData(TCHAR *pszServ);

BOOL IsContainedInCondList(const TCHAR *pszStr, WICONDLIST *List);

void DestroyWIList();
BOOL LoadWIData(BOOL dial);
void FreeWIData(WIDATA *Data);

INT_PTR CALLBACK DlgProcSetup(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// functions in weather_info.c
void GetINIInfo(TCHAR *pszSvc);

void MoreVarList();

// functions in weather_opt.c
void SetTextDefault(const char* in);
void LoadOptions();
void SaveOptions();

int OptInit(WPARAM wParam,LPARAM lParam);

INT_PTR CALLBACK OptionsProc(HWND hdlg,UINT msg,WPARAM wparam,LPARAM lparam);
void SetIconDefault();
void RemoveIconSettings();

BOOL CALLBACK TextOptionsProc(HWND hdlg,UINT msg,WPARAM wparam,LPARAM lparam);
BOOL CALLBACK AdvOptionsProc(HWND hdlg,UINT msg,WPARAM wparam,LPARAM lparam);
INT_PTR CALLBACK DlgProcText(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgPopUpOpts(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// functions in weather_popup.c
int WeatherPopup(WPARAM wParam, LPARAM lParam);
int WeatherError(WPARAM wParam, LPARAM lParam);
int WPShowMessage(TCHAR* lpzText, WORD kind);

LRESULT CALLBACK PopupDlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK PopupWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// functions in weather_svcs.c
void InitServices(void);
void DestroyServices(void);

INT_PTR WeatherSetStatus(WPARAM new_status, LPARAM lParam);
INT_PTR WeatherGetCaps(WPARAM wParam, LPARAM lParam);
INT_PTR WeatherGetName(WPARAM wParam, LPARAM lParam);
INT_PTR WeatherGetStatus(WPARAM wParam, LPARAM lParam);
INT_PTR WeatherLoadIcon(WPARAM wParam, LPARAM lParam);

void UpdateMenu(BOOL State);
void UpdatePopupMenu(BOOL State);
void AddMenuItems();
void AvatarDownloaded(HANDLE hContact);

// functions in weather_update.c
int UpdateWeather(HANDLE hContact);

int RetrieveWeather(HANDLE hContact, WEATHERINFO *winfo);

void UpdateAll(BOOL AutoUpdate, BOOL RemoveOld);
void UpdateThreadProc(LPVOID hWnd);
INT_PTR UpdateSingleStation(WPARAM wParam,LPARAM lParam);
INT_PTR UpdateAllInfo(WPARAM wParam,LPARAM lParam);
INT_PTR UpdateSingleRemove(WPARAM wParam,LPARAM lParam);
INT_PTR UpdateAllRemove(WPARAM wParam,LPARAM lParam);

int GetWeatherData(HANDLE hContact);

void CALLBACK timerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
void CALLBACK timerProc2(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

// function from multiwin module
void InitMwin(void);
void DestroyMwin(void);
INT_PTR Mwin_MenuClicked(WPARAM wParam, LPARAM lParam);
int BuildContactMenu(WPARAM wparam, LPARAM lparam);
void UpdateMwinData(HANDLE hContact);
void removeWindow(HANDLE hContact);

// functions in weather_userinfo.c
int UserInfoInit(WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgProcUIPage(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DlgProcMoreData(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

#define WM_UPDATEDATA WM_USER + 2687

int BriefInfo(WPARAM wParam, LPARAM lParam);
INT_PTR BriefInfoSvc(WPARAM wParam, LPARAM lParam);
void LoadBriefInfoText(HWND hwndDlg, HANDLE hContact);
INT_PTR CALLBACK DlgProcBrief(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void InitIcons(void);
HICON  LoadIconEx(const char* name, BOOL big);
HANDLE GetIconHandle(const char* name);
void   ReleaseIconEx(HICON hIcon);
