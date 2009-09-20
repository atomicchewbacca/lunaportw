#pragma once
#include "atlwin.h"

class CSettingDlg : public CDialogImpl<CSettingDlg>
{
public:
	enum { IDD = IDD_SETTING };

	CEdit ref_exe;
	CEdit ref_rep;
	wchar_t exe_path[_MAX_PATH];
	wchar_t rep_path[_MAX_PATH];

	CSettingDlg(const char *exe_path, const char *rep_path);
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

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	{
		// スクリーンの中央に配置
		CenterWindow();

		// コントロール設定
		ref_exe = GetDlgItem(IDC_REFEXE_EDIT);
		ref_exe.LimitText(_MAX_PATH);
		ref_exe.SetSelAll();
		ref_exe.ReplaceSel(exe_path);
		ref_rep = GetDlgItem(IDC_REFREP_EDIT);
		ref_rep.LimitText(_MAX_PATH);
		ref_rep.SetSelAll();
		ref_rep.ReplaceSel(rep_path);

		return 0;
	}

	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		ref_exe.GetLine(0, exe_path, _MAX_PATH);
		ref_rep.GetLine(0, rep_path, _MAX_PATH);
		EndDialog(wID);
		return 0;
	}

	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}

	LRESULT OnRefExe(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnRefRep(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
};
