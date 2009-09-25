// lunaportwView.cpp : CLunaportwView クラスの実装
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "lunaportwView.h"
#ifdef DEBUG
# undef DEBUG
#endif
#include "LunaPort.h"

extern CRITICAL_SECTION cur_logwin_monitor;
extern CLunaportwView *cur_logwin;

CLunaportwView::~CLunaportwView()
{
	::EnterCriticalSection(&cur_logwin_monitor);
	if(cur_logwin == this) cur_logwin = NULL;
	::LeaveCriticalSection(&cur_logwin_monitor);
}

LRESULT CLunaportwView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	SetFont(AtlGetDefaultGuiFont());
	SetReadOnly();
	AppendText(_T("Lunaport ")_T(VERSION)_T("\n"));
	::EnterCriticalSection(&cur_logwin_monitor);
	cur_logwin = this;
	::LeaveCriticalSection(&cur_logwin_monitor);
	bHandled = FALSE;
	return 0;
}

LRESULT CLunaportwView::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	::EnterCriticalSection(&cur_logwin_monitor);
	if(cur_logwin == this) cur_logwin = NULL;
	::LeaveCriticalSection(&cur_logwin_monitor);
	bHandled = FALSE;
	return 0;
}

BOOL CLunaportwView::PreTranslateMessage(MSG* pMsg)
{
	pMsg;
	return FALSE;
}

#if 0
LRESULT CLunaportwView::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CPaintDC dc(m_hWnd);

	//TODO: 描画コードの追加

	return 0;
}
#endif
