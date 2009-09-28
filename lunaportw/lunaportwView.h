// lunaportwView.h : CLunaportwView クラスのインターフェイス
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CLunaportwView : public CWindowImpl<CLunaportwView, CRichEditCtrl>, public CRichEditCommands<CLunaportwView>
{
public:
	DECLARE_WND_SUPERCLASS(NULL, CRichEditCtrl::GetWndClassName())

	~CLunaportwView();

	BOOL PreTranslateMessage(MSG* pMsg);

	BEGIN_MSG_MAP(CLunaportwView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		CHAIN_MSG_MAP_ALT(CRichEditCommands<CLunaportwView>, 1)
	END_MSG_MAP()

	// ハンドラーのプロトタイプ （引数が必要な場合はコメントを外してください）:
	//LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
};
