// MainFrm.cpp : CMainFrame �N���X�̎���
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
	// �R�}���h�o�[ �E�B���h�E�̍쐬
	HWND hWndCmdBar = m_CmdBar.Create(m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
	// ���j���[�̃A�^�b�`
	m_CmdBar.AttachMenu(GetMenu());
	// �R�}���h�o�[�摜�̓ǂݍ���
	m_CmdBar.LoadImages(IDR_MAINFRAME);
	// �ȑO�̃��j���[�̍폜
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

	// ���b�Z�[�W �t�B���^�[����щ�ʍX�V�p�̃I�u�W�F�N�g�o�^
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
	m_view.AddPage(pView->m_hWnd, _T("�Z�b�V�������O - �I�t���C���Q�[��"));

	// TODO: �h�L�������g�������R�[�h�̒ǉ�
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)5, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameHost(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("�Z�b�V�������O - �z�X�g"));

	// TODO: �h�L�������g�������R�[�h�̒ǉ�
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)1, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameJoin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("�Z�b�V�������O - �N���C�A���g"));

	// TODO: �h�L�������g�������R�[�h�̒ǉ�
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)2, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameSpectate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("�Z�b�V�������O - �ϐ�"));

	// TODO: �h�L�������g�������R�[�h�̒ǉ�
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)9, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameWatchReplay(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("�Z�b�V�������O - ���v���C�Đ�"));

	// TODO: �h�L�������g�������R�[�h�̒ǉ�
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)7, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameHostOnLobby(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("�Z�b�V�������O - ���r�[�z�X�g�ڑ�"));

	// TODO: �h�L�������g�������R�[�h�̒ǉ�
	pView->Initialize();
	if(lunaport_thread) ::CloseHandle(lunaport_thread);
	lunaport_thread = (HANDLE)::_beginthreadex(NULL, 0, lunaport_serve, (void *)3, 0, NULL);

	return 0;
}

LRESULT CMainFrame::OnGameListLobbyGames(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CLunaportwView* pView = new CLunaportwView;
	pView->Create(m_view, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_SAVESEL, WS_EX_CLIENTEDGE);
	m_view.AddPage(pView->m_hWnd, _T("�Z�b�V�������O - ���r�[�N���C�A���g�ڑ�"));

	// TODO: �h�L�������g�������R�[�h�̒ǉ�
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
	static BOOL bVisible = TRUE;	// ������Ԃŉ�
	bVisible = !bVisible;
	CReBarCtrl rebar = m_hWndToolBar;
	int nBandIndex = rebar.IdToIndex(ATL_IDW_BAND_FIRST + 1);	// �c�[���o�[��2��
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
