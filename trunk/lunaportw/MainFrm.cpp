// MainFrm.cpp : CMainFrame クラスの実装
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "aboutdlg.h"
#include "lunaportwView.h"
#include "MainFrm.h"
#include "SettingDlg.h"

#ifdef DEBUG
# undef DEBUG
#endif
//#include "LunaPort.h"
//#include "Code.h"
#include "ConManager.h"
#include "Crc32.h"
#include "StageManager.h"
#include "Session.h"
#include "HTTP.h"
#include "Lobby.h"
#include "lunaportw.h"

CMainFrame::CMainFrame() : lunaport_thread(NULL)
{
}

CMainFrame::~CMainFrame()
{
	if(lunaport_thread) CloseHandle(lunaport_thread);
}

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

	HWND hWndToolBar = CreateSimpleToolBarCtrl(m_hWnd, IDR_MAINFRAME, FALSE, ATL_SIMPLE_TOOLBAR_PANE_STYLE);

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

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

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
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("セッションログ - オフラインゲーム"));

	// TODO: ドキュメント初期化コードの追加
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)5, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameHost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("セッションログ - ホスト"));

	// TODO: ドキュメント初期化コードの追加
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)1, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameJoin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("セッションログ - クライアント"));

	// TODO: ドキュメント初期化コードの追加
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)2, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameSpectate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("セッションログ - 観戦"));

	// TODO: ドキュメント初期化コードの追加
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)9, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameWatchReplay(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("セッションログ - リプレイ再生"));

	// TODO: ドキュメント初期化コードの追加
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)7, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameHostOnLobby(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("セッションログ - ロビーホスト接続"));

	// TODO: ドキュメント初期化コードの追加
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)3, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameListLobbyGames(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("セッションログ - ロビークライアント接続"));

	// TODO: ドキュメント初期化コードの追加
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)4, 0, NULL);

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
	CSettingDlg dlg(game_exe, replays_dir);
	if(IDOK == dlg.DoModal(m_hWnd)) {
		dlg.GetRefExe(game_exe);
		dlg.GetRefRep(replays_dir);
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
	if(nActivePage != -1)
		m_view.RemovePage(nActivePage);
	else
		::MessageBeep((UINT)-1);

	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_view.RemoveAllPages();

	return 0;
}

LRESULT CMainFrame::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int nPage = wID - ID_WINDOW_TABFIRST;
	m_view.SetActivePage(nPage);

	return 0;
}
