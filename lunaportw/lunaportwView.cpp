// lunaportwView.cpp : CLunaportwView �N���X�̎���
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
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	pLoop->AddIdleHandler(this);
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

BOOL CLunaportwView::OnIdle()
{
	::EnterCriticalSection(&cur_logwin_monitor);
	if(cur_logwin == this) {
		while(!logmsg_queue.empty()) {
			AppendText(logmsg_queue.front());
			logmsg_queue.pop();
		}
		HideCaret(); // �ʂɏ����Ȃ��Ă��ǂ����c
	}
	::LeaveCriticalSection(&cur_logwin_monitor);
	return FALSE;
}

#if 0
LRESULT CLunaportwView::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CPaintDC dc(m_hWnd);

	//TODO: �`��R�[�h�̒ǉ�

	return 0;
}
#endif