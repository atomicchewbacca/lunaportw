#pragma once
#include "atlwin.h"

class CSettingDlg : public CDialogImpl<CSettingDlg>
{
public:
	enum { IDD = IDD_SETTING };

	CEdit ref_exe;
	CEdit ref_rep;
	CEdit player_name_crtl;
	CEdit port_number_crtl;
	CEdit lobby_url_crtl;
	CEdit lobby_comment_crtl;
	CComboBox language_crtl;
	wchar_t exe_path[_MAX_PATH];
	wchar_t rep_path[_MAX_PATH];
	wchar_t player_name[NET_STRING_BUFFER];
	wchar_t lobby_url[NET_STRING_BUFFER];
	wchar_t lobby_comment[NET_STRING_BUFFER];
	unsigned int port;
	int chosen_language_id;

	CSettingDlg(const char *exe_path, const char *rep_path, const char *player_name, const char *lobby_url, const char *lobby_comment, unsigned int port);
	void GetRefExe(char *exe_path);
	void GetRefRep(char *rep_path);

	// メッセージマップ
	BEGIN_MSG_MAP(CSettingDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_REFEXE_BUTTON, OnRefExe)
		COMMAND_ID_HANDLER(IDC_REFREP_BUTTON, OnRefRep)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}

	LRESULT OnRefExe(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnRefRep(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};
