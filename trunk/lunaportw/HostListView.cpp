// lunaportwView.cpp : CHostListView クラスの実装
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "HostListView.h"
#include "lunaportwView.h"
#include "MainFrm.h"

#ifdef DEBUG
# undef DEBUG
#endif
#include "LunaPort.h"
#include "ConManager.h"
#include "Crc32.h"
#include "StageManager.h"
#include "Session.h"
#include "Lobby.h"

#include "lunaportw.h"

static unsigned int __stdcall downloadlist(void *_in)
{
	((CHostListView *)_in)->refresh();
}

LRESULT CHostListView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	LRESULT lRet = DefWindowProc();

    SetFont(AtlGetDefaultGuiFont());
    SetExtendedListViewStyle(LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);

    // リストビューにカラム挿入
    CRect rcList;
    ::GetClientRect(GetParent(), rcList);
    int nScrollWidth = GetSystemMetrics(SM_CXVSCROLL);
    int n3DEdge = GetSystemMetrics(SM_CXEDGE);
    InsertColumn(0, CString(MAKEINTRESOURCE(IDS_LOBBYCOL_NAME)), LVCFMT_LEFT, 190, -1);
    InsertColumn(1, CString(MAKEINTRESOURCE(IDS_LOBBYCOL_IPADDR)), LVCFMT_LEFT, 100, -1);
    InsertColumn(2, CString(MAKEINTRESOURCE(IDS_LOBBYCOL_STATUS)), LVCFMT_LEFT, 64, -1);
    InsertColumn(3, CString(MAKEINTRESOURCE(IDS_LOBBYCOL_COMMENT)), LVCFMT_LEFT, rcList.Width() - 354 - nScrollWidth - n3DEdge * 2, -1);

	//empty_text.Create(GetWindow(GW_CHILD), rcDefault, NULL, WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | SS_CENTER | SS_CENTERIMAGE, 0);
	//empty_text.CenterWindow();
	//empty_text.SetWindowText(_T("No host available."));
	
	//if(refresh_thread) ::CloseHandle(refresh_thread);
	//refresh_thread = (HANDLE)::_beginthreadex(NULL, 0, downloadlist, (void *)this, 0, NULL);

	bHandled = TRUE;
	return 0;
}

LRESULT CHostListView::OnDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL &bHandled)
{
    LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE)pnmh;

	wchar_t *str = NULL;
	if(GetItemText(pnmia->iItem, 1, str) && str) {
		for(int i = 0; i < mainfrm.m_view.GetPageCount(); i++) {
			if(mainfrm.m_view.GetPageHWND(i) == mainfrm.m_LogView.m_hWnd) {
				mainfrm.m_view.SetActivePage(i);
				break;
			}
		}

		char ip_str[256];
		size_t t;
		::wcstombs_s(&t, ip_str, 256, str, wcslen(str));
		set_lunaport_param(ip_str, 0);
		do_lunaport(4);
	}

	bHandled = TRUE;
	return 0;
}

LRESULT CHostListView::OnReturn(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL &bHandled)
{
	wchar_t *str = NULL;
	if(GetSelectedIndex() >= 0 && GetItemText(GetSelectedIndex(), 1, str) && str) {
		for(int i = 0; i < mainfrm.m_view.GetPageCount(); i++) {
			if(mainfrm.m_view.GetPageHWND(i) == mainfrm.m_LogView.m_hWnd) {
				mainfrm.m_view.SetActivePage(i);
				break;
			}
		}

		char ip_str[256];
		size_t t;
		::wcstombs_s(&t, ip_str, 256, str, wcslen(str));
		set_lunaport_param(ip_str, 0);
		do_lunaport(4);
	}

	bHandled = TRUE;
	return 0;
}

LRESULT CHostListView::OnRClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
    // マウスカーソルの位置を取得
    CPoint pt;
    GetCursorPos(&pt);

    // その位置にポップアップメニューを表示
    // ただし選択後のメッセージは親ウィンドウへ通知
    CMenu cPopup;
    cPopup.LoadMenu(IDR_LOBBY_POPUP);
    cPopup.GetSubMenu(0).TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, pt.x, pt.y, GetParent());

    return 0;
}


BOOL CHostListView::PreTranslateMessage(MSG* pMsg)
{
	pMsg;
	return FALSE;
}

void CHostListView::refresh()
{
	lobby.loadgamelist(); // ToDo : 別スレにする

	DeleteAllItems();

	if(lobby.games.empty()) {
		//empty_text.ShowWindow(TRUE);
		EnableWindow(FALSE);
		AddItem(0, 0, _T("No host available."));
	}
	else {
		EnableWindow(TRUE);
		//empty_text.ShowWindow(FALSE);
		// リストビューにアイテム追加
		for(std::vector<lplobby_record>::iterator i = lobby.games.begin(); i != lobby.games.end(); ++i) {
			int nIndex = GetItemCount();
			wchar_t _num[8];
			swprintf_s(_num, 8, L"%d", (*i).port);
			AddItem(nIndex, 0, CString((*i).name));
			AddItem(nIndex, 1, CString((*i).ip)+":"+_num);
			AddItem(nIndex, 2, CString(MAKEINTRESOURCE(IDS_PLAYERSTATUS_WAITING+(*i).state)));
			AddItem(nIndex, 3, CString((*i).comment));
		}
	}
}
