#pragma once
#include "atlwin.h"
#include <vector>
#include <string>

class CChooseLangDlg : public CDialogImpl<CChooseLangDlg>
{
public:
	enum { IDD = IDD_CHOOSELANG };

	CComboBox crtl;
	int chosen_language_id;
	
	static std::vector< std::basic_string<_TCHAR> > languages;
	static int cur_language_id;

	static void ListUpLanguage();

	static void ChangeLanguage(const _TCHAR *language);

	static const _TCHAR *CurrentLanguage();

	const _TCHAR *ChosenLanguage();

	// メッセージマップ
	BEGIN_MSG_MAP(CChooseLangDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
	END_MSG_MAP()
	
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};
