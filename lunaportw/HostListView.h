// lunaportwView.h : CLunaportwView クラスのインターフェイス
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CMainFrame;
class CHostListView : public CWindowImpl<CHostListView, CListViewCtrl>
{
public:
    DECLARE_WND_SUPERCLASS(NULL, CListViewCtrl::GetWndClassName())

	CHostListView(CMainFrame &mainfrm) : mainfrm(mainfrm) {}
	CMainFrame &mainfrm;
	HANDLE refresh_thread;
	//CStatic empty_text;

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP(CHostListView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
        REFLECTED_NOTIFY_CODE_HANDLER(NM_RETURN, OnReturn)
        REFLECTED_NOTIFY_CODE_HANDLER(NM_DBLCLK, OnDblClick)
        REFLECTED_NOTIFY_CODE_HANDLER(NM_RCLICK, OnRClick)
 	END_MSG_MAP()

	// ハンドラーのプロトタイプ （引数が必要な場合はコメントを外してください）:
	//LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnReturn(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled);
	LRESULT OnDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
	LRESULT OnRClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	void refresh();
};
