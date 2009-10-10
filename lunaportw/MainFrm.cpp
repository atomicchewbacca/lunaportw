// MainFrm.cpp : CMainFrame クラスの実装
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"
#include "lunaportwView.h"
#include "HostListView.h"
#include "MainFrm.h"

#ifdef DEBUG
# undef DEBUG
#endif
#include "LunaPort.h"
//#include "Code.h"
#include "ConManager.h"
#include "Crc32.h"
#include "StageManager.h"
#include "Session.h"
#include "HTTP.h"
#include "Lobby.h"

#include "lunaportw.h"
#include "SettingDlg.h"
#include "ChooseLangDlg.h"

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	if(CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle()
{
	UIUpdateToolBar();
	return FALSE;
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// コマンドバー ウィンドウの作成
	HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	// メニューのアタッチ
	m_CmdBar.AttachMenu(GetMenu());
	// コマンドバー画像の読み込み
	m_CmdBar.LoadImages(IDR_MAINFRAME);
	// 以前のメニューの削除
	SetMenu(NULL);

	HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_TOOLBAR, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	AddSimpleReBarBand(hWndCmdBar);
	AddSimpleReBarBand(hWndToolBar, NULL, TRUE);

	CreateSimpleStatusBar();

	m_hWndClient = m_view.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CLIENTEDGE);

	UIAddToolBar(hWndToolBar);
	UISetCheck(ID_VIEW_TOOLBAR, 1);
	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	UISetCheck(ID_OPT_REPLAYRECORDING, record_replay);
	UISetCheck(ID_OPT_ALLOWSPECTATOR, allow_spectators);
	UISetCheck(ID_OPT_AUTODELAY, !ask_delay);

	// メッセージ フィルターおよび画面更新用のオブジェクト登録
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	CMenuHandle menuMain = m_CmdBar.GetMenu();
	m_view.SetWindowMenu(menuMain.GetSubMenu(WINDOW_MENU_POSITION));

	m_LogView.Create(m_view, rcDefault, NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL |
		ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL,
		WS_EX_CLIENTEDGE);
	m_view.AddPage(m_LogView.m_hWnd, CString(MAKEINTRESOURCE(IDS_LOGWINDOWTITLE)));

	m_view.m_tab.SetFocus();

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	CRect rc;
	if(GetWindowRect(&rc)) {
		char config_filename[_MAX_PATH];
		strcpy(config_filename, dir_prefix);
		strcat(config_filename, "\\"INIFILE);
		char w[32];
		sprintf_s(w, 32, "%d", rc.left);
		::WritePrivateProfileStringA("MainWindow", "Left", w, config_filename);
		sprintf_s(w, 32, "%d", rc.top);
		::WritePrivateProfileStringA("MainWindow", "Top", w, config_filename);
		sprintf_s(w, 32, "%d", rc.right);
		::WritePrivateProfileStringA("MainWindow", "Right", w, config_filename);
		sprintf_s(w, 32, "%d", rc.bottom);
		::WritePrivateProfileStringA("MainWindow", "Bottom", w, config_filename);
	}

	bHandled = FALSE;
	return 1;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnGameLocal(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	for(int i = 0; i < m_view.GetPageCount(); i++) {
		if(m_view.GetPageHWND(i) == m_LogView.m_hWnd) {
			m_view.SetActivePage(i);
			break;
		}
	}

	do_lunaport(5);

	return 0;
}

LRESULT CMainFrame::OnGameHost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	for(int i = 0; i < m_view.GetPageCount(); i++) {
		if(m_view.GetPageHWND(i) == m_LogView.m_hWnd) {
			m_view.SetActivePage(i);
			break;
		}
	}

	do_lunaport(1);

	return 0;
}

LRESULT CMainFrame::OnGameJoin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	for(int i = 0; i < m_view.GetPageCount(); i++) {
		if(m_view.GetPageHWND(i) == m_LogView.m_hWnd) {
			m_view.SetActivePage(i);
			break;
		}
	}

	do_lunaport(2);

	return 0;
}

LRESULT CMainFrame::OnGameSpectate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	for(int i = 0; i < m_view.GetPageCount(); i++) {
		if(m_view.GetPageHWND(i) == m_LogView.m_hWnd) {
			m_view.SetActivePage(i);
			break;
		}
	}

	do_lunaport(9);

	return 0;
}

LRESULT CMainFrame::OnGameWatchReplay(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	for(int i = 0; i < m_view.GetPageCount(); i++) {
		if(m_view.GetPageHWND(i) == m_LogView.m_hWnd) {
			m_view.SetActivePage(i);
			break;
		}
	}

	do_lunaport(7);

	return 0;
}

LRESULT CMainFrame::OnGameHostOnLobby(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	for(int i = 0; i < m_view.GetPageCount(); i++) {
		if(m_view.GetPageHWND(i) == m_LogView.m_hWnd) {
			m_view.SetActivePage(i);
			break;
		}
	}

	do_lunaport(3);

	return 0;
}

LRESULT CMainFrame::OnGameListLobbyGames(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if(!m_LobbyView.IsWindow()) {
		m_LobbyView.Create(m_view, rcDefault, NULL,
				WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | 
				LVS_SINGLESEL | LVS_REPORT | LVS_SHOWSELALWAYS,
				WS_EX_CLIENTEDGE);
		m_view.AddPage(m_LobbyView.m_hWnd, CString(MAKEINTRESOURCE(IDS_LOBBYWINDOWTITLE)) + CString(lobby_url));
		m_LobbyView.refresh();
	}
	else {
		for(int i = 0; i < m_view.GetPageCount(); i++) {
			if(m_view.GetPageHWND(i) == m_LobbyView.m_hWnd) {
				m_view.SetActivePage(i);
				m_LobbyView.refresh();
				break;
			}
		}
	}

	return 0;
}

LRESULT CMainFrame::OnLobbyJoin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	wchar_t *str = NULL;
	if(m_LobbyView.GetSelectedIndex() >= 0 && m_LobbyView.GetItemText(m_LobbyView.GetSelectedIndex(), 1, str) && str) {
		for(int i = 0; i < m_view.GetPageCount(); i++) {
			if(m_view.GetPageHWND(i) == m_LogView.m_hWnd) {
				m_view.SetActivePage(i);
				break;
			}
		}

		char ip_str[256];
		size_t t;
		::wcstombs_s(&t, ip_str, 256, str, wcslen(str));
		set_lunaport_param(ip_str, 0);
		do_lunaport(4);
	}

	return 0;
}

LRESULT CMainFrame::OnLobbySpectate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	wchar_t *str = NULL;
	if(m_LobbyView.GetSelectedIndex() >= 0 && m_LobbyView.GetItemText(m_LobbyView.GetSelectedIndex(), 1, str) && str) {
		for(int i = 0; i < m_view.GetPageCount(); i++) {
			if(m_view.GetPageHWND(i) == m_LogView.m_hWnd) {
				m_view.SetActivePage(i);
				break;
			}
		}

		char ip_str[256];
		size_t t;
		::wcstombs_s(&t, ip_str, 256, str, wcslen(str));
		set_lunaport_param(ip_str, 1);
		do_lunaport(4);
	}

	return 0;
}

LRESULT CMainFrame::OnOptReplayRecording(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	record_replay = !record_replay;
	UISetCheck(ID_OPT_REPLAYRECORDING, record_replay);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnOptAllowSpectator(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	allow_spectators = !allow_spectators;
	UISetCheck(ID_OPT_ALLOWSPECTATOR, allow_spectators);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnOptAutoDelay(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ask_delay = !ask_delay;
	UISetCheck(ID_OPT_AUTODELAY, !ask_delay);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnOptSetting(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CSettingDlg dlg(game_exe, replays_dir, own_name, lobby_url, lobby_comment, port);
	if(IDOK == dlg.DoModal(m_hWnd)) {
		dlg.GetRefExe(game_exe);
		dlg.GetRefRep(replays_dir);
		size_t s;
		wcstombs_s(&s, own_name, NET_STRING_BUFFER, dlg.player_name, NET_STRING_BUFFER);
		wcstombs_s(&s, lobby_url, NET_STRING_BUFFER, dlg.lobby_url, NET_STRING_BUFFER);
		wcstombs_s(&s, lobby_comment, NET_STRING_BUFFER, dlg.lobby_comment, NET_STRING_BUFFER);
		port = dlg.port;
		CChooseLangDlg::cur_language_id = dlg.chosen_language_id;
	}
	return 0;
}

LRESULT CMainFrame::OnViewToolBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	static BOOL bVisible = TRUE;	// 初期状態で可視
	bVisible = !bVisible;
	CReBarCtrl rebar = m_hWndToolBar;
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// ツールバーは2つ目
	rebar.ShowBand(nBandIndex, bVisible);
	UISetCheck(ID_VIEW_TOOLBAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BOOL bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainFrame::OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nActivePage = m_view.GetActivePage();
	if(nActivePage != -1 && m_view.GetPageHWND(nActivePage) != m_LogView.m_hWnd)
		m_view.RemovePage(nActivePage);
	else
		::MessageBeep((UINT)-1);

	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	//m_view.RemoveAllPages();
	for(int i = 0; i < m_view.GetPageCount(); i++) {
		if(m_view.GetPageHWND(i) != m_LogView.m_hWnd) m_view.RemovePage(i);
	}

	return 0;
}

LRESULT CMainFrame::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nPage = wID - ID_WINDOW_TABFIRST;
	m_view.SetActivePage(nPage);

	return 0;
}
