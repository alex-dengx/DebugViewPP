// (C) Copyright Gert-Jan de Vos and Jan Wilmans 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)

// Repository at: https://github.com/djeedjay/DebugViewPP/

#include "stdafx.h"

#include <algorithm>
#include <boost/utility.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include "CobaltFusion/AtlWinExt.h"
#include "CobaltFusion/scope_guard.h"
#include "CobaltFusion/make_unique.h"
#include "CobaltFusion/stringbuilder.h"
#include "CobaltFusion/hstream.h"
#include "CobaltFusion/Math.h"
#include "Win32/Registry.h"
#include "DebugView++Lib/ProcessReader.h"
#include "DebugView++Lib/DbgviewReader.h"
#include "DebugView++Lib/SocketReader.h"
#include "DebugView++Lib/FileReader.h"
#include "DebugView++Lib/FileIO.h"
#include "DebugView++Lib/LogFilter.h"
#include "Resource.h"
#include "RunDlg.h"
#include "HistoryDlg.h"
#include "FilterDlg.h"
#include "SourcesDlg.h"
#include "AboutDlg.h"
#include "FileOptionDlg.h"
#include "LogView.h"
#include "MainFrame.h"

namespace fusion {
namespace debugviewpp {

std::wstring GetPersonalPath()
{
	std::wstring path;
	wchar_t szPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_PERSONAL, nullptr, 0, szPath)))
		path = szPath;
	return path;
}

void CLogViewTabItem::SetView(const std::shared_ptr<CLogView>& pView)
{
	m_pView = pView;
	SetTabView(*pView);
}

CLogView& CLogViewTabItem::GetView()
{
	return *m_pView;
}

BEGIN_MSG_MAP2(CMainFrame)
	MSG_WM_CREATE(OnCreate)
	MSG_WM_CLOSE(OnClose)
	MSG_WM_QUERYENDSESSION(OnQueryEndSession)
	MSG_WM_ENDSESSION(OnEndSession)
	MSG_WM_MOUSEWHEEL(OnMouseWheel)
	MSG_WM_CONTEXTMENU(OnContextMenu)
	MSG_WM_DROPFILES(OnDropFiles)
	MSG_WM_SYSCOMMAND(OnSysCommand)
	MESSAGE_HANDLER_EX(WM_SYSTEMTRAYICON, OnSystemTrayIcon)
	COMMAND_ID_HANDLER_EX(SC_RESTORE, OnScRestore)
	COMMAND_ID_HANDLER_EX(SC_CLOSE, OnScClose)
	COMMAND_ID_HANDLER_EX(ID_FILE_NEWVIEW, OnFileNewTab)
	COMMAND_ID_HANDLER_EX(ID_FILE_OPEN, OnFileOpen)
	COMMAND_ID_HANDLER_EX(ID_FILE_RUN, OnFileRun)
	COMMAND_ID_HANDLER_EX(ID_FILE_SAVE_LOG, OnFileSaveLog)
	COMMAND_ID_HANDLER_EX(ID_APP_EXIT, OnFileExit)	
	COMMAND_ID_HANDLER_EX(ID_FILE_SAVE_VIEW, OnFileSaveView)
	COMMAND_ID_HANDLER_EX(ID_FILE_LOAD_CONFIGURATION, OnFileLoadConfiguration)
	COMMAND_ID_HANDLER_EX(ID_FILE_SAVE_CONFIGURATION, OnFileSaveConfiguration)
	COMMAND_ID_HANDLER_EX(ID_LOG_CLEAR, OnLogClear)
	COMMAND_ID_HANDLER_EX(ID_LOG_PAUSE, OnLogPause)
	COMMAND_ID_HANDLER_EX(ID_LOG_GLOBAL, OnLogGlobal)
	COMMAND_ID_HANDLER_EX(ID_LOG_HISTORY, OnLogHistory)
	COMMAND_ID_HANDLER_EX(ID_LOG_DEBUGVIEW_AGENT, OnLogDebugviewAgent)
	COMMAND_ID_HANDLER_EX(ID_VIEW_FIND, OnViewFind)
	COMMAND_ID_HANDLER_EX(ID_VIEW_FILTER, OnViewFilter)
	COMMAND_ID_HANDLER_EX(ID_VIEW_CLOSE, OnViewClose)
	COMMAND_ID_HANDLER_EX(ID_LOG_SOURCES, OnSources)
	COMMAND_ID_HANDLER_EX(ID_OPTIONS_LINKVIEWS, OnLinkViews)
	COMMAND_ID_HANDLER_EX(ID_OPTIONS_AUTONEWLINE, OnAutoNewline)
	COMMAND_ID_HANDLER_EX(ID_OPTIONS_FONT, OnViewFont)
	COMMAND_ID_HANDLER_EX(ID_OPTIONS_ALWAYSONTOP, OnAlwaysOnTop)
	COMMAND_ID_HANDLER_EX(ID_OPTIONS_HIDE, OnHide)
	COMMAND_ID_HANDLER_EX(ID_APP_ABOUT, OnAppAbout)
	NOTIFY_CODE_HANDLER_EX(CTCN_BEGINITEMDRAG, OnBeginTabDrag)
	NOTIFY_CODE_HANDLER_EX(CTCN_SELCHANGE, OnChangeTab)
	NOTIFY_CODE_HANDLER_EX(CTCN_CLOSE, OnCloseTab)
	NOTIFY_CODE_HANDLER_EX(CTCN_DELETEITEM, OnDeleteTab);
	CHAIN_MSG_MAP(TabbedFrame)
	CHAIN_MSG_MAP(CUpdateUI<CMainFrame>)
	REFLECT_NOTIFICATIONS()
END_MSG_MAP()

LOGFONT& GetDefaultLogFont()
{
	static LOGFONT lf;
	static int initialized = GetObjectW(AtlGetDefaultGuiFont(), sizeof(lf), &lf);
	return lf;
}

CMainFrame::CMainFrame() :
	m_filterNr(1),
	m_findDlg(*this),
	m_linkViews(false),
	m_hide(false),
	m_lineBuffer(7000),
	m_tryGlobal(HasGlobalDBWinReaderRights()),
	m_logFileName(L"DebugView++.dblog"),
	m_txtFileName(L"MessagesInTheCurrentView.dblog"),
	m_configFileName(L"DebugView++.dbconf"),
	m_initialPrivateBytes(ProcessInfo::GetPrivateBytes()),
	m_logfont(GetDefaultLogFont()),
	m_logSources(true),
	m_pLocalReader(nullptr),
	m_pGlobalReader(nullptr),
	m_pDbgviewReader(nullptr)
{
	m_notifyIconData.cbSize = 0;
}

CMainFrame::~CMainFrame()
{
}

void CMainFrame::SetLogging()
{
	m_logWriter = make_unique<FileWriter>(GetPersonalPath() + L"\\DebugView++ Logfiles\\debugview.dblog", m_logFile);
}

void CMainFrame::OnException()
{
	MessageBox(L"Unknown Exception", LoadString(IDR_APPNAME).c_str(), MB_ICONERROR | MB_OK);
}

void CMainFrame::OnException(const std::exception& ex)
{
	MessageBox(WStr(ex.what()).c_str(), LoadString(IDR_APPNAME).c_str(), MB_ICONERROR | MB_OK);
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	return TabbedFrame::PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle()
{
	UpdateUI();
	UIUpdateToolBar();
	UIUpdateStatusBar();
	return FALSE;
}

LRESULT CMainFrame::OnCreate(const CREATESTRUCT* /*pCreate*/)
{
	m_notifyIconData.cbSize = 0;

	HWND hWndCmdBar = m_cmdBar.Create(*this, rcDefault, nullptr, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	m_cmdBar.AttachMenu(GetMenu());
	m_cmdBar.LoadImages(IDR_MAINFRAME);
	SetMenu(nullptr);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	CReBarCtrl rebar(m_hWndToolBar);

	AddSimpleReBarBand(hWndCmdBar);

	HWND hWndToolBar = CreateSimpleToolBarCtrl(rebar, IDR_MAINFRAME, false, ATL_SIMPLE_TOOLBAR_PANE_STYLE);	 // DrMemory: LEAK 1696 direct bytes 
	AddSimpleReBarBand(hWndToolBar, nullptr, true);
	UIAddToolBar(hWndToolBar);

	m_findDlg.Create(rebar);
	AddSimpleReBarBand(m_findDlg, L"Find: ", false, 10000);
	SizeSimpleReBarBands();

	rebar.LockBands(true);
	rebar.SetNotifyWnd(*this);

	m_hWndStatusBar = m_statusBar.Create(*this);
	int paneIds[] = { ID_DEFAULT_PANE, ID_SELECTION_PANE, ID_VIEW_PANE, ID_LOGFILE_PANE, ID_MEMORY_PANE };
	m_statusBar.SetPanes(paneIds, 5, false);
	UIAddStatusBar(m_hWndStatusBar);

	CreateTabWindow(*this, rcDefault, CTCS_CLOSEBUTTON | CTCS_DRAGREARRANGE);

	AddFilterView(L"View");
	HideTabControl();

	SetLogFont();
	LoadSettings();

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != nullptr);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	m_logSources.SubscribeToUpdate([this] { return OnUpdate(); });
	DragAcceptFiles(true);

	// Resume can throw if a second debugview is running
	// so do not rely on any commands executed afterwards
	if (!IsDBWinViewerActive())
		Resume();
	return 0;
}

void CMainFrame::OnClose()
{
	if (m_timer)
		KillTimer(m_timer); 

	SaveSettings();
	DestroyWindow();

	if (m_notifyIconData.cbSize)
	{
		Shell_NotifyIcon(NIM_DELETE, &m_notifyIconData);
		m_notifyIconData.cbSize = 0;
	}

#ifdef CONSOLE_DEBUG
	fclose(stdout);
	FreeConsole();
#endif
}

LRESULT CMainFrame::OnQueryEndSession(WPARAM, LPARAM)
{
	// MSDN: 
	// The WM_QUERYENDSESSION message is sent when the user chooses to end the session or when an application calls one of the system shutdown functions
	// When an application returns TRUE for this message, it receives the WM_ENDSESSION message.
	// Each application should return TRUE or FALSE immediately upon receiving this message, and defer any cleanup operations until it receives the WM_ENDSESSION message.
	return TRUE;
}

LRESULT CMainFrame::OnEndSession(WPARAM, LPARAM)
{
	OnClose();
	return TRUE;
}

void CMainFrame::UpdateUI()
{
	UpdateStatusBar();

	UISetCheck(ID_VIEW_TIME, GetView().GetClockTime());
	UISetCheck(ID_VIEW_PROCESSCOLORS, GetView().GetViewProcessColors());
	UISetCheck(ID_VIEW_SCROLL, GetView().GetAutoScroll());
	UISetCheck(ID_VIEW_SCROLL_STOP, GetView().GetAutoScrollStop());
	UISetCheck(ID_VIEW_BOOKMARK, GetView().GetBookmark());

	for (int id = ID_VIEW_COLUMN_FIRST; id <= ID_VIEW_COLUMN_LAST; ++id)
		UISetCheck(id, GetView().IsColumnViewed(id));

	UISetCheck(ID_OPTIONS_LINKVIEWS, m_linkViews);
	UIEnable(ID_OPTIONS_LINKVIEWS, GetTabCtrl().GetItemCount() > 1);
	UISetCheck(ID_OPTIONS_AUTONEWLINE, m_logSources.GetAutoNewLine());
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, GetAlwaysOnTop());
	UISetCheck(ID_OPTIONS_HIDE, m_hide);
	UISetCheck(ID_LOG_PAUSE, !m_pLocalReader);
	UIEnable(ID_LOG_GLOBAL, !!m_pLocalReader);
	UISetCheck(ID_LOG_GLOBAL, m_tryGlobal);
}

std::wstring FormatUnits(int n, const std::wstring& unit)
{
	if (n == 0)
		return L"";
	if (n == 1)
		return wstringbuilder() << n << " " << unit;
	return wstringbuilder() << n << " " << unit << "s";
}

std::wstring FormatDuration(double seconds)
{
	int minutes = FloorTo<int>(seconds / 60);
	seconds -= 60 * minutes;

	int hours = minutes / 60;
	minutes -= 60 * hours;

	int days = hours / 24;
	hours -= 24 * days;

	if (days > 0)
		return wstringbuilder() << FormatUnits(days, L"day") << L" " << FormatUnits(hours, L"hour");

	if (hours > 0)
		return wstringbuilder() << FormatUnits(hours, L"hour") << L" " << FormatUnits(minutes, L"minute");

	if (minutes > 0)
		return wstringbuilder() << FormatUnits(minutes, L"minute") << L" " << FormatUnits(FloorTo<int>(seconds), L"second");

	static const wchar_t* units[] = { L"s", L"ms", L"�s", L"ns", nullptr };
	const wchar_t** unit = units;
	while (*unit != nullptr && seconds > 0 && seconds < 1)
	{
		seconds *= 1e3;
		++unit;
	}

	return wstringbuilder() << std::fixed << std::setprecision(3) << seconds << L" " << *unit;
}

std::wstring FormatDateTime(const SYSTEMTIME& systemTime)
{
	int size = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &systemTime, nullptr, nullptr, 0);
	size += GetDateFormat(LOCALE_USER_DEFAULT, 0, &systemTime, nullptr, nullptr, 0);
	std::vector<wchar_t> buf(size);

	int offset = GetDateFormat(LOCALE_USER_DEFAULT, 0, &systemTime, nullptr, buf.data(), size);
	buf[offset - 1] = ' ';
	GetTimeFormat(LOCALE_USER_DEFAULT, 0, &systemTime, nullptr, buf.data() + offset, size);
	return std::wstring(buf.data(), size - 1);
}

std::wstring FormatDateTime(const FILETIME& fileTime)
{
	return FormatDateTime(Win32::FileTimeToSystemTime(Win32::FileTimeToLocalFileTime(fileTime)));
}

std::wstring FormatBytes(size_t size)
{
	static const wchar_t* units[] = { L"bytes", L"kB", L"MB", L"GB", L"TB", L"PB", L"EB", nullptr };
	const wchar_t** unit = units;
	const int kb = 1024;
	while (size / kb > 0 && unit[1] != nullptr)
	{
		size = size / kb;
		++unit;
	}

	return wstringbuilder() << size << L" " << *unit; 
}

std::wstring CMainFrame::GetSelectionInfoText(const std::wstring& label, const SelectionInfo& selection) const
{
	if (selection.count == 0)
		return std::wstring();

	if (selection.count == 1)
		return label + L": " + FormatDateTime(m_logFile[selection.beginLine].systemTime);

	double dt = m_logFile[selection.endLine].time - m_logFile[selection.beginLine].time;
	return wstringbuilder() << label << L": " << FormatDuration(dt) << L" (" << selection.count << " lines)";
}

SelectionInfo CMainFrame::GetLogFileRange() const
{
	if (m_logFile.Empty())
		return SelectionInfo();

	return SelectionInfo(0, m_logFile.Count() - 1, m_logFile.Count());
}

void CMainFrame::UpdateStatusBar()
{
	auto isearch = GetView().GetHighlightText();
	std::wstring search = wstringbuilder() << L"Searching: \"" << isearch << L"\"";
	UISetText(ID_DEFAULT_PANE,
		isearch.empty() ? (m_pLocalReader ? L"Ready" : L"Paused") : search.c_str());
	UISetText(ID_SELECTION_PANE, GetSelectionInfoText(L"Selected", GetView().GetSelectedRange()).c_str());
	UISetText(ID_VIEW_PANE, GetSelectionInfoText(L"View", GetView().GetViewRange()).c_str());
	UISetText(ID_LOGFILE_PANE, GetSelectionInfoText(L"Log", GetLogFileRange()).c_str());

	size_t memoryUsage = ProcessInfo::GetPrivateBytes() - m_initialPrivateBytes;
	if (memoryUsage < 0)
		memoryUsage = 0;
	UISetText(ID_MEMORY_PANE, FormatBytes(memoryUsage).c_str());
}

void CMainFrame::ProcessLines(const Lines& lines)
{
	if (lines.empty())
		return;

	// design decision: filtering is done on the UI thread, see CLogView::Add
	// changing this would introduces extra thread and thus complexity. Do that only if it solves a problem.

	int views = GetViewCount();
	for (int i = 0; i < views; ++i)
		GetView(i).BeginUpdate();

	for (auto it = lines.begin(); it != lines.end(); ++it)
		AddMessage(Message(it->time, it->systemTime, it->pid, it->processName, it->message));

	for (int i = 0; i < views; ++i)
	{
		if (GetView(i).EndUpdate() && GetTabCtrl().GetCurSel() != i)
		{
			SetModifiedMark(i, true);
			GetTabCtrl().UpdateLayout();
			GetTabCtrl().Invalidate();
		}
	}
}

bool CMainFrame::OnUpdate()
{
	auto lines = m_logSources.GetLines();
	if (lines.empty())
		return false;
	ProcessLines(lines);
	return true;
}

bool CMainFrame::OnMouseWheel(UINT nFlags, short zDelta, CPoint /*pt*/)
{
	if ((nFlags & MK_CONTROL) == 0)
		return false;

	int size = static_cast<int>(LogFontSizeToPointSize(m_logfont.lfHeight) * std::pow(1.15, zDelta / WHEEL_DELTA) + 0.5);
	size = std::max(size, 4);
	size = std::min(size, 24);
	m_logfont.lfHeight = LogFontSizeFromPointSize(size);
	SetLogFont();
	return true;
}

void CMainFrame::OnContextMenu(HWND hWnd, CPoint pt)
{
	if (hWnd != m_TabCtrl)
	{
		SetMsgHandled(false);
		return;
	}

	CTCHITTESTINFO hit;
	hit.pt = pt;
	m_TabCtrl.ScreenToClient(&hit.pt);
	int item = m_TabCtrl.HitTest(&hit);

	CMenu menuContext;
	menuContext.LoadMenu(IDR_TAB_CONTEXTMENU);
	CMenuHandle menuPopup(menuContext.GetSubMenu(0));

	if (item < 0)
	{
		menuPopup.EnableMenuItem(ID_VIEW_FILTER, MF_BYCOMMAND | MF_GRAYED);
		menuPopup.EnableMenuItem(ID_VIEW_CLEAR, MF_BYCOMMAND | MF_GRAYED);
		menuPopup.EnableMenuItem(ID_VIEW_CLOSE, MF_BYCOMMAND | MF_GRAYED);
	}

	menuPopup.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, *this);
}

void CMainFrame::HandleDroppedFile(const std::wstring& file)
{
	Pause();
	SetTitle(file);
	using boost::algorithm::iequals;
	auto ext = boost::filesystem::wpath(file).extension().wstring();
	if (iequals(ext, L".exe"))
	{
		m_logSources.AddMessage(stringbuilder() << "Started capturing output of " << Str(file) << "\n");
		Run(file);
	}
	else if (iequals(ext, L".cmd") || iequals(ext, L".bat"))
	{
		m_logSources.AddMessage(stringbuilder() << "Started capturing output of " << Str(file) << "\n");
		m_logSources.AddProcessReader(L"cmd.exe", wstringbuilder() << L"/Q /C \"" << file << "\"");
	}
	else
	{
		if (IsBinaryFileType(IdentifyFile(file)))
		{
			m_logSources.AddBinaryFileReader(file);
		}
		else
		{
			m_logSources.AddDBLogReader(file);
		}
	}
}

void CMainFrame::OnDropFiles(HDROP hDropInfo)
{
	auto guard = make_guard([hDropInfo]() { DragFinish(hDropInfo); });

	if (DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0) == 1)
	{
		std::vector<wchar_t> filename(DragQueryFile(hDropInfo, 0, nullptr, 0) + 1);
		if (DragQueryFile(hDropInfo, 0, filename.data(), filename.size()))
			HandleDroppedFile(std::wstring(filename.data()));
	}
}

LRESULT CMainFrame::OnSysCommand(UINT nCommand, CPoint)
{
	switch (nCommand)
	{
	case SC_MINIMIZE:
		if (!m_hide)
			break;

		if (!m_notifyIconData.cbSize)
		{
			m_notifyIconData.cbSize = sizeof(m_notifyIconData);
			m_notifyIconData.hWnd = *this;
			m_notifyIconData.uID = 1;
			m_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
			m_notifyIconData.uCallbackMessage = WM_SYSTEMTRAYICON;
			m_notifyIconData.hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON));
			CString sWindowText;
			GetWindowText(sWindowText);
			_tcscpy_s(m_notifyIconData.szTip, sWindowText);
			if (!Shell_NotifyIcon(NIM_ADD, &m_notifyIconData))
				break;
		}
		ShowWindow(SW_HIDE);
		return 0;
	}

	SetMsgHandled(false);
	return 0;
}

LRESULT CMainFrame::OnSystemTrayIcon(UINT, WPARAM wParam, LPARAM lParam)
{
	ATLASSERT(wParam == 1);
	wParam;
	switch (lParam)
	{
	case WM_LBUTTONDBLCLK:
		SendMessage(WM_COMMAND, SC_RESTORE);
		break;
	case WM_RBUTTONUP:
		{
			SetForegroundWindow(m_hWnd);
			CMenuHandle menu = GetSystemMenu(false);
			menu.EnableMenuItem(SC_RESTORE, MF_BYCOMMAND | MF_ENABLED);
			menu.EnableMenuItem(SC_MOVE, MF_BYCOMMAND | MF_GRAYED);
			menu.EnableMenuItem(SC_SIZE, MF_BYCOMMAND | MF_GRAYED);
			menu.EnableMenuItem(SC_MINIMIZE, MF_BYCOMMAND | MF_GRAYED);
			menu.EnableMenuItem(SC_MAXIMIZE, MF_BYCOMMAND | MF_GRAYED);
			menu.EnableMenuItem(SC_CLOSE, MF_BYCOMMAND | MF_ENABLED);
			POINT position = Win32::GetCursorPos();
			menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN, position.x, position.y, m_hWnd);
		}
		break;
	}
	return 0;
}

LRESULT CMainFrame::OnScRestore(UINT, INT, HWND)
{
	if (m_notifyIconData.cbSize)
	{
		Shell_NotifyIcon(NIM_DELETE, &m_notifyIconData);
		m_notifyIconData.cbSize = 0;
	}
	ShowWindow(SW_SHOW);
	BringWindowToTop();
	return 0;
}

LRESULT CMainFrame::OnScClose(UINT, INT, HWND)
{
	PostMessage(WM_COMMAND, ID_APP_EXIT);
	return 0;
}

const wchar_t* RegistryPath = L"Software\\Cobalt Fusion\\DebugView++";

int CMainFrame::LogFontSizeFromPointSize(int fontSize)
{
	return -MulDiv(fontSize, GetDeviceCaps(GetDC(), LOGPIXELSY), 72);
}

int CMainFrame::LogFontSizeToPointSize(int logFontSize)
{
	return -MulDiv(logFontSize, 72, GetDeviceCaps(GetDC(), LOGPIXELSY));
}

bool CMainFrame::LoadSettings()
{
	auto mutex = Win32::CreateMutex(nullptr, false, L"Local\\DebugView++");
	Win32::MutexLock lock(mutex.get());

	DWORD x, y, cx, cy;
	CRegKey reg;
	reg.Create(HKEY_CURRENT_USER, RegistryPath);
	if (reg.QueryDWORDValue(L"X", x) == ERROR_SUCCESS && static_cast<int>(x) >= GetSystemMetrics(SM_XVIRTUALSCREEN) &&
		reg.QueryDWORDValue(L"Y", y) == ERROR_SUCCESS && static_cast<int>(y) >= GetSystemMetrics(SM_YVIRTUALSCREEN) &&
		reg.QueryDWORDValue(L"Width", cx) == ERROR_SUCCESS && static_cast<int>(x + cx) <= GetSystemMetrics(SM_CXVIRTUALSCREEN) &&
		reg.QueryDWORDValue(L"Height", cy) == ERROR_SUCCESS && static_cast<int>(y + cy) <= GetSystemMetrics(SM_CYVIRTUALSCREEN))
		SetWindowPos(0, x, y, cx, cy, SWP_NOZORDER);

	m_linkViews = Win32::RegGetDWORDValue(reg, L"LinkViews", 0) != 0;
	m_logSources.SetAutoNewLine(Win32::RegGetDWORDValue(reg, L"AutoNewLine", 1) != 0);
	SetAlwaysOnTop(Win32::RegGetDWORDValue(reg, L"AlwaysOnTop", 0) != 0);

	m_applicationName = Win32::RegGetStringValue(reg, L"ApplicationName", L"DebugView++");
	SetTitle();

	m_hide = Win32::RegGetDWORDValue(reg, L"Hide", 0) != 0;

	auto fontName = Win32::RegGetStringValue(reg, L"FontName", L"").substr(0, LF_FACESIZE - 1);
	int fontSize = Win32::RegGetDWORDValue(reg, L"FontSize", 8);
	if (!fontName.empty())
	{
		LOGFONT lf = { 0 };
		m_logfont = lf;
		std::copy(fontName.begin(), fontName.end(), m_logfont.lfFaceName);
		m_logfont.lfHeight = LogFontSizeFromPointSize(fontSize);
		SetLogFont();
	}

	CRegKey regViews;
	if (regViews.Open(reg, L"Views") == ERROR_SUCCESS)
	{
		for (size_t i = 0; ; ++i)
		{
			CRegKey regView;
			if (regView.Open(regViews, WStr(wstringbuilder() << L"View" << i)) != ERROR_SUCCESS)
				break;

			auto name = Win32::RegGetStringValue(regView);
			if (i == 0)
				GetTabCtrl().GetItem(0)->SetText(name.c_str());
			else
				AddFilterView(name);
			GetView().LoadSettings(regView);
		}
		GetTabCtrl().SetCurSel(Win32::RegGetDWORDValue(regViews, L"Current", 0));
		GetTabCtrl().UpdateLayout();
		GetTabCtrl().Invalidate();
	}

	CRegKey regColors;
	if (regColors.Open(reg, L"Colors") == ERROR_SUCCESS)
	{
		auto colors = ColorDialog::GetCustomColors();
		for (int i = 0; i < 16; ++i)
			colors[i] = Win32::RegGetDWORDValue(regColors, WStr(wstringbuilder() << L"Color" << i));
	}

	return true;
}

void CMainFrame::SaveSettings()
{
	auto mutex = Win32::CreateMutex(nullptr, false, L"Local\\DebugView++");
	Win32::MutexLock lock(mutex.get());

	auto placement = Win32::GetWindowPlacement(*this);

	CRegKey reg;
	reg.Create(HKEY_CURRENT_USER, RegistryPath);
	reg.SetDWORDValue(L"X", placement.rcNormalPosition.left);
	reg.SetDWORDValue(L"Y", placement.rcNormalPosition.top);
	reg.SetDWORDValue(L"Width", placement.rcNormalPosition.right - placement.rcNormalPosition.left);
	reg.SetDWORDValue(L"Height", placement.rcNormalPosition.bottom - placement.rcNormalPosition.top);

	reg.SetDWORDValue(L"LinkViews", m_linkViews);
	reg.SetDWORDValue(L"AutoNewLine", m_logSources.GetAutoNewLine());
	reg.SetDWORDValue(L"AlwaysOnTop", GetAlwaysOnTop());
	reg.SetDWORDValue(L"Hide", m_hide);

	reg.SetStringValue(L"FontName", m_logfont.lfFaceName);
	reg.SetDWORDValue(L"FontSize", LogFontSizeToPointSize(m_logfont.lfHeight));

	reg.RecurseDeleteKey(L"Views");

	CRegKey regViews;
	regViews.Create(reg, L"Views");
	regViews.SetDWORDValue(L"Current", GetTabCtrl().GetCurSel());

	int views = GetViewCount();
	for (int i = 0; i < views; ++i)
	{
		CRegKey regView;
		regView.Create(regViews, WStr(wstringbuilder() << L"View" << i));
		regView.SetStringValue(L"", GetView(i).GetName().c_str());
		GetView(i).SaveSettings(regView);
	}

	CRegKey regColors;
	regColors.Create(reg, L"Colors");
	auto colors = ColorDialog::GetCustomColors();
	for (int i = 0; i < 16; ++i)
		regColors.SetDWORDValue(WStr(wstringbuilder() << L"Color" << i), colors[i]);
}

void CMainFrame::FindNext(const std::wstring& text)
{
	if (!GetView().FindNext(text))
		MessageBeep(MB_ICONASTERISK);
}

void CMainFrame::FindPrevious(const std::wstring& text)
{
	if (!GetView().FindPrevious(text))
		MessageBeep(MB_ICONASTERISK);
}

void CMainFrame::AddFilterView()
{
	++m_filterNr;
	CFilterDlg dlg(wstringbuilder() << L"View " << m_filterNr);
	if (dlg.DoModal() != IDOK)
		return;

	AddFilterView(dlg.GetName(), dlg.GetFilters());
	SaveSettings();
}

void CMainFrame::AddFilterView(const std::wstring& name, const LogFilter& filter)
{
	auto pView = std::make_shared<CLogView>(name, *this, m_logFile, filter);
	pView->Create(*this, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);
	pView->SetFont(m_hFont.get());

	auto pItem = make_unique<CLogViewTabItem>();
	pItem->SetText(name.c_str());
	pItem->SetView(pView);

	int newIndex = GetTabCtrl().GetItemCount();
	GetTabCtrl().InsertItem(newIndex, pItem.release());
	GetTabCtrl().SetCurSel(newIndex);
	ShowTabControl();
}

LRESULT CMainFrame::OnBeginTabDrag(NMHDR* pnmh)
{
	auto& nmhdr = *reinterpret_cast<NMCTCITEM*>(pnmh);

	return nmhdr.iItem >= GetViewCount();
}

LRESULT CMainFrame::OnChangeTab(NMHDR* pnmh)
{
	SetMsgHandled(false);

	auto& nmhdr = *reinterpret_cast<NMCTC2ITEMS*>(pnmh);

	if (nmhdr.iItem2 >= 0 && nmhdr.iItem2 < GetViewCount())
		SetModifiedMark(nmhdr.iItem2, false);

	if (!m_linkViews || nmhdr.iItem1 == nmhdr.iItem2 ||
		nmhdr.iItem1 < 0 || nmhdr.iItem1 >= GetViewCount() ||
		nmhdr.iItem2 < 0 || nmhdr.iItem2 >= GetViewCount())
		return 0;

	int line = GetView(nmhdr.iItem1).GetFocusLine();
	GetView(nmhdr.iItem2).SetFocusLine(line);
	
	return 0;
}

void CMainFrame::SetModifiedMark(int tabindex, bool modified)
{
	auto name = GetView(tabindex).GetName();
	if (modified)
		name += L"*";

//	GetTabCtrl().GetItem(nmhdr.iItem2)->SetHighlighted(modified)
	GetTabCtrl().GetItem(tabindex)->SetText(name.c_str());
}

LRESULT CMainFrame::OnCloseTab(NMHDR* pnmh)
{
	auto& nmhdr = *reinterpret_cast<NMCTCITEM*>(pnmh);
	CloseView(nmhdr.iItem);
	return 0;
}

LRESULT CMainFrame::OnDeleteTab(NMHDR* pnmh)
{
	auto& nmhdr = *reinterpret_cast<NMCTCITEM*>(pnmh);

	if (nmhdr.iItem >= 0 && nmhdr.iItem < GetViewCount())
		GetView(nmhdr.iItem).DestroyWindow();

	return FALSE;
}

void CMainFrame::OnFileNewTab(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	AddFilterView();
}

void CMainFrame::SaveLogFile(const std::wstring& filename)
{
	UISetText(0, WStr(wstringbuilder() << "Saving " << filename));
	Win32::ScopedCursor cursor(::LoadCursor(nullptr, IDC_WAIT));

	std::ofstream fs;
	OpenLogFile(fs, filename);
	int count = m_logFile.Count();
	for (int i = 0; i < count; ++i)
	{
		auto msg = m_logFile[i];
		WriteLogFileMessage(fs, msg.time, msg.systemTime, msg.processId, msg.processName, msg.text);
	}
	fs.close();
	if (!fs)
		Win32::ThrowLastError(filename);

	m_logFileName = filename;
	UpdateStatusBar();
}

void CMainFrame::SaveViewFile(const std::wstring& filename)
{
	UISetText(0, WStr(wstringbuilder() << "Saving " << filename));
	Win32::ScopedCursor cursor(::LoadCursor(nullptr, IDC_WAIT));
	GetView().Save(filename);
	m_txtFileName = filename;
	UpdateStatusBar();
}

struct View
{
	int index;
	std::string name;
	bool clockTime;
	bool processColors;
	LogFilter filters;
};

void CMainFrame::LoadConfiguration(const std::wstring& fileName)
{
	boost::property_tree::ptree pt;
	boost::property_tree::read_xml(Str(fileName), pt, boost::property_tree::xml_parser::trim_whitespace);

	auto autoNewline = pt.get<bool>("DebugViewPP.AutoNewline");
	auto linkViews = pt.get<bool>("DebugViewPP.LinkViews");

	auto viewsPt = pt.get_child("DebugViewPP.Views");
	std::vector<View> views;
	for (auto it = viewsPt.begin(); it != viewsPt.end(); ++it)
	{
		if (it->first == "View")
		{
			View view;
			auto& viewPt = it->second;
			view.index = viewPt.get<bool>("Index");
			view.name = viewPt.get<std::string>("Name");
			view.clockTime = viewPt.get<bool>("ClockTime");
			view.processColors = viewPt.get<bool>("ProcessColors");
			view.filters.messageFilters = MakeFilters(viewPt.get_child("MessageFilters"));
			view.filters.processFilters = MakeFilters(viewPt.get_child("ProcessFilters"));

			views.push_back(view);
		}
	}
	std::sort(views.begin(), views.end(), [](const View& v1, const View& v2) { return v1.index < v2.index; });

	m_logSources.SetAutoNewLine(autoNewline);
	m_linkViews = linkViews;

	for (int i = 0; i < static_cast<int>(views.size()); ++i)
	{
		if (i >= GetViewCount())
			AddFilterView(WStr(views[i].name), views[i].filters);
		else
			GetView(i).SetFilters(views[i].filters);

		auto& logView = GetView(i);
		logView.SetClockTime(views[i].clockTime);
		logView.SetViewProcessColors(views[i].processColors);
	}

	size_t i = GetViewCount();
	while (i > views.size())
	{
		--i;
		CloseView(i);
	}
}

void CMainFrame::SaveConfiguration(const std::wstring& fileName)
{
#if BOOST_VERSION < 105600
	boost::property_tree::xml_writer_settings<char> settings('\t', 1);
#else
	boost::property_tree::xml_writer_settings<std::string> settings('\t', 1);
#endif

	boost::property_tree::ptree mainPt;
	mainPt.put("AutoNewline", m_logSources.GetAutoNewLine());
	mainPt.put("LinkViews", m_linkViews);

	int views = GetViewCount();
	for (int i = 0; i < views; ++i)
	{
		auto& logView = GetView(i);
		auto filters = logView.GetFilters();
		boost::property_tree::ptree viewPt;
		viewPt.put("Index", i);
		viewPt.put("Name", Str(logView.GetName()).str());
		viewPt.put("ClockTime", logView.GetClockTime());
		viewPt.put("ProcessColors", logView.GetViewProcessColors());
		viewPt.put_child("MessageFilters", MakePTree(filters.messageFilters));
		viewPt.put_child("ProcessFilters", MakePTree(filters.processFilters));
		mainPt.add_child("Views.View", viewPt);
	}

	boost::property_tree::ptree pt;
	pt.add_child("DebugViewPP", mainPt);

	boost::property_tree::write_xml(Str(fileName), pt, std::locale(), settings);
}

void CMainFrame::OnFileOpen(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CFileOptionDlg dlg(true, L"Keep file open", L".dblog", m_logFileName.c_str(), OFN_FILEMUSTEXIST,
		L"DebugView++ Log Files (*.dblog)\0*.dblog\0"
		L"DebugView Log Files (*.log)\0*.log\0"
		L"All Files (*.*)\0*.*\0\0",
		0);
	dlg.m_ofn.nFilterIndex = 0;
	dlg.m_ofn.lpstrTitle = L"Load Log File";
	if (dlg.DoModal() == IDOK)
	{
		if (dlg.Option())
			LoadAsync(dlg.m_szFileName); // todo: tails by default, should be made optional, also suppress internal messages about 'removed' logsource
		else
			Load(std::wstring(dlg.m_szFileName));
	}
}

void CMainFrame::Run(const std::wstring& pathName)
{
	if (!pathName.empty())
		m_runDlg.SetPathName(pathName);

	if (m_runDlg.DoModal() == IDOK)
	{
		m_logSources.AddProcessReader(m_runDlg.GetPathName(), m_runDlg.GetArguments());
	}
}

void CMainFrame::OnFileRun(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	Run();
}

void CMainFrame::Load(const std::wstring& filename)
{
	std::ifstream file(filename);
	if (!file)
		Win32::ThrowLastError(filename);

	WIN32_FILE_ATTRIBUTE_DATA fileInfo = { 0 };
	GetFileAttributesEx(filename.c_str(), GetFileExInfoStandard, &fileInfo);
	SetTitle(filename);
	Load(file, boost::filesystem::wpath(filename).filename().string(), fileInfo.ftCreationTime);
}

void CMainFrame::LoadAsync(const std::wstring& filename)
{
	SetTitle(filename);
	Pause();
	ClearLog();
	m_logSources.AddDBLogReader(filename);
}

void CMainFrame::SetTitle(const std::wstring& title)
{
	std::wstring windowText = title.empty() ? m_applicationName : L"[" + title + L"] - " + m_applicationName;
	SetWindowText(windowText.c_str());
}

void CMainFrame::Load(HANDLE hFile)
{
	hstream file(hFile);
	FILETIME ft = Win32::GetSystemTimeAsFileTime();
	Load(file, "", ft);
}

void CMainFrame::Load(std::istream& file, const std::string& name, FILETIME fileTime)
{
	Win32::ScopedCursor cursor(::LoadCursor(nullptr, IDC_WAIT));

	Pause();
	ClearLog();

	Line line;
	line.processName = name;
	line.systemTime = fileTime;
	while (ReadLogFileMessage(file, line))
		AddMessage(Message(line.time, line.systemTime, line.pid, line.processName, line.message));
}

void CMainFrame::CapturePipe(HANDLE hPipe)
{
	m_logSources.AddPipeReader(Win32::GetParentProcessId(), hPipe);
}

void CMainFrame::OnFileExit(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	PostMessage(WM_CLOSE);
}

void CMainFrame::OnFileSaveLog(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CFileDialog dlg(false, L".dblog", m_logFileName.c_str(), OFN_OVERWRITEPROMPT,
		L"DebugView++ Log Files (*.dblog)\0*.dblog\0"
		L"All Files (*.*)\0*.*\0\0", 0);
	dlg.m_ofn.nFilterIndex = 0;
	dlg.m_ofn.lpstrTitle = L"Save all messages in memory buffer";
	if (dlg.DoModal() == IDOK)
		SaveLogFile(dlg.m_szFileName);
}

void CMainFrame::OnFileSaveView(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CFileDialog dlg(false, L".dblog", m_txtFileName.c_str(), OFN_OVERWRITEPROMPT,
		L"DebugView++ Log Files (*.dblog)\0*.dblog\0"
		L"All Files (*.*)\0*.*\0\0");
	dlg.m_ofn.nFilterIndex = 0;
	dlg.m_ofn.lpstrTitle = L"Save the messages in the current view";
	if (dlg.DoModal() == IDOK)
		SaveViewFile(dlg.m_szFileName);
}

void CMainFrame::OnFileLoadConfiguration(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CFileDialog dlg(true, L".dbconf", m_configFileName.c_str(), OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		L"DebugView++ Configuration Files (*.dbconf)\0*.dbconf\0\0");
	dlg.m_ofn.nFilterIndex = 0;
	dlg.m_ofn.lpstrTitle = L"Load View Configuration";
	if (dlg.DoModal() == IDOK)
		LoadConfiguration(dlg.m_szFileName);
}

void CMainFrame::OnFileSaveConfiguration(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CFileDialog dlg(false, L".dbconf", m_configFileName.c_str(), OFN_OVERWRITEPROMPT,
		L"DebugView++ Configuration Files (*.dbconf)\0*.dbconf\0\0");
	dlg.m_ofn.nFilterIndex = 0;
	dlg.m_ofn.lpstrTitle = L"Save View Configuration";
	if (dlg.DoModal() == IDOK)
		SaveConfiguration(dlg.m_szFileName);
}

void CMainFrame::ClearLog()
{
	// First Clear LogFile so views reset their m_firstLine:
	m_logFile.Clear();
	m_logSources.Reset();
	int views = GetViewCount();
	for (int i = 0; i < views; ++i)
	{
		GetView(i).Clear();
		SetModifiedMark(i, false);
		GetTabCtrl().UpdateLayout();
		GetTabCtrl().Invalidate();
	}
}

void CMainFrame::OnLogClear(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	ClearLog();
}

void CMainFrame::OnLinkViews(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	m_linkViews = !m_linkViews;
}

void CMainFrame::OnAutoNewline(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	m_logSources.SetAutoNewLine(!m_logSources.GetAutoNewLine());
}

void CMainFrame::OnHide(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	m_hide = !m_hide;
}

bool CMainFrame::GetAlwaysOnTop() const
{
	return (GetWindowLong(GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

void CMainFrame::SetAlwaysOnTop(bool value)
{
	SetWindowPos(value ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void CMainFrame::OnAlwaysOnTop(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	SetAlwaysOnTop(!GetAlwaysOnTop());
}

bool CMainFrame::IsPaused() const
{
	return !m_pLocalReader;
}

void CMainFrame::Pause()
{
	SetTitle(L"Paused");
	m_logSources.AddMessage("<paused>");
	if (m_pLocalReader)
	{
		m_logSources.Remove(m_pLocalReader);
		m_pLocalReader = nullptr;
	}
	if (m_pGlobalReader)
	{
		m_logSources.Remove(m_pGlobalReader);
		m_pGlobalReader = nullptr;
	}
}

void CMainFrame::Resume()
{
	SetTitle();

	if (!m_pLocalReader)
	{
		try 
		{
			m_pLocalReader = m_logSources.AddDBWinReader(false);
		}
		catch (std::exception&)
		{
			MessageBox(
				L"Unable to capture Win32 Messages.\n"
				L"\n"
				L"Another DebugView++ (or similar application) might be running.\n",
				m_applicationName.c_str(), MB_ICONERROR | MB_OK);
			return;
		}
	}

	if (m_tryGlobal)
	{
		try
		{
			m_pGlobalReader = m_logSources.AddDBWinReader(true);
		}
		catch (std::exception&)
		{
			MessageBox(
				L"Unable to capture Global Win32 Messages.\n"
				L"\n"
				L"Make sure you have appropriate permissions.\n"
				L"\n"
				L"You may need to start this application by right-clicking it and selecting\n"
				L"'Run As Administator' even if you have administrator rights.",
				m_applicationName.c_str(), MB_ICONERROR | MB_OK);
			m_tryGlobal = false;
		}
	}

	std::wstring title = L"Paused";
	if (m_pLocalReader && m_pGlobalReader)
	{
		title = L"Capture Win32 & Global Win32 Messages";
	}
	else if (m_pLocalReader)
	{
		title = L"Capture Win32";
	} 
	else if (m_pGlobalReader)
	{
		title = L"Capture Global Win32";
	}
	SetTitle(title);
}

void CMainFrame::OnLogPause(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	if (IsPaused())
		Resume();
	else
		Pause();
}

void CMainFrame::OnLogGlobal(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	m_tryGlobal = !m_pGlobalReader;
	
	if (m_pLocalReader && m_tryGlobal)
	{
		Resume();
	}
	else
	{
		m_logSources.Remove(m_pGlobalReader);
		m_pGlobalReader = nullptr;
	}
}

void CMainFrame::OnLogHistory(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CHistoryDlg dlg(m_logFile.GetHistorySize(), m_logFile.GetHistorySize() == 0);
	if (dlg.DoModal() == IDOK)
		m_logFile.SetHistorySize(dlg.GetHistorySize());
}

std::wstring GetExecutePath()
{
	using namespace boost;
	auto path = filesystem::system_complete(filesystem::path( Win32::GetCommandLineArguments()[0]));
	return path.remove_filename().c_str();
}

void CMainFrame::OnLogDebugviewAgent(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	if (!m_pDbgviewReader)
	{
		std::string dbgview = stringbuilder() << GetExecutePath() << "\\dbgview.exe";
		if (FileExists(dbgview.c_str()))
		{
			std::string cmd = stringbuilder() << "start \"\" " << dbgview << " /a";
			system(cmd.c_str());
		}
		m_pDbgviewReader = m_logSources.AddDbgviewReader("127.0.0.1");
	}
	else
	{
		m_logSources.Remove(m_pDbgviewReader);
		m_pDbgviewReader = nullptr;
	}
}

void CMainFrame::OnViewFilter(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	int tabIdx = GetTabCtrl().GetCurSel();

	CFilterDlg dlg(GetView().GetName(), GetView().GetFilters());
	if (dlg.DoModal() != IDOK)
		return;

	GetTabCtrl().GetItem(tabIdx)->SetText(dlg.GetName().c_str());
	GetTabCtrl().UpdateLayout();
	GetTabCtrl().Invalidate();
	GetView().SetName(dlg.GetName());
	GetView().SetFilters(dlg.GetFilters());
	SaveSettings();
}

void CMainFrame::OnViewClose(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CloseView(GetTabCtrl().GetCurSel());
}

void CMainFrame::OnSources(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CSourcesDlg dlg(m_sourceInfos);
	if (dlg.DoModal() != IDOK)
		return;

	auto pSources = m_logSources.GetSources();
	for (auto it = pSources.begin(); it != pSources.end(); ++it)
	{
		if (dynamic_cast<DbgviewReader*>(*it) || dynamic_cast<SocketReader*>(*it))
			m_logSources.Remove(*it);
	}

	auto sourceInfos = dlg.GetSourceInfos();
	for (auto it = sourceInfos.begin(); it != sourceInfos.end(); ++it)
	{
		if (it->enabled)
			AddLogSource(*it);
	}
	m_sourceInfos = sourceInfos;
}

void CMainFrame::AddLogSource(const SourceInfo& info)
{
	switch (info.type)
	{
	case SourceType::DebugViewAgent:
		m_logSources.AddDbgviewReader(Str(info.address));
		break;
	case SourceType::Udp:
		m_logSources.AddUDPReader(info.port);
		break;
	case SourceType::Tcp:
		throw std::exception("SourceType::Tcp not implememted");
	default:
		// do nothing
		throw std::exception("SourceType not implememted");
	}
}

void CMainFrame::CloseView(int i)
{
	int views = GetViewCount();
	if (i >= 0 && i < views)
	{
		GetTabCtrl().DeleteItem(i, false);
		GetTabCtrl().SetCurSel(i == views - 1 ? i - 1 : i);
		if (GetViewCount() == 1)
			HideTabControl();
	}
}

void CMainFrame::OnViewFind(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	m_findDlg.SetFocus();
}

void CMainFrame::OnViewFont(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CFontDialog dlg(&m_logfont, CF_SCREENFONTS);
	if (dlg.DoModal(*this) == IDOK)
	{
		m_logfont = dlg.m_lf;
		SetLogFont();
	}
}

void CMainFrame::SetLogFont()
{
	Win32::HFont hFont(CreateFontIndirect(&m_logfont));
	if (!hFont)
		return;

	int views = GetViewCount();
	for (int i = 0; i < views; ++i)
		GetView(i).SetFont(hFont.get());
	m_hFont = std::move(hFont);
}

void CMainFrame::OnAppAbout(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	CAboutDlg dlg;
	dlg.DoModal();
}

int CMainFrame::GetViewCount() const
{
	return const_cast<CMainFrame&>(*this).GetTabCtrl().GetItemCount();
}

CLogView& CMainFrame::GetView(int i)
{
	assert(i >= 0 && i < GetViewCount());
	return GetTabCtrl().GetItem(i)->GetView();
}

CLogView& CMainFrame::GetView()
{
	return GetView(std::max(0, GetTabCtrl().GetCurSel()));
}

void CMainFrame::AddMessage(const Message& message)
{
	int beginIndex = m_logFile.BeginIndex();
	int index = m_logFile.EndIndex();
	m_logFile.Add(message);
	int views = GetViewCount();
	for (int i = 0; i < views; ++i)
		GetView(i).Add(beginIndex, index, message);
}

} // namespace debugviewpp 
} // namespace fusion
