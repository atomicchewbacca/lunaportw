#include "StdAfx.h"
#include "resource.h"
#include "lunaportwView.h"

#ifdef DEBUG
# undef DEBUG
#endif
#include "LunaPort.h"
//#include "Code.h"
#include "ConManager.h"
#include "Crc32.h"
#include "StageManager.h"
#include "Session.h"
#include "HTTP.h"
#include "Lobby.h"
#include "lunaportw.h"
#include "SettingDlg.h"
#include "ChooseLangDlg.h"

CSettingDlg::CSettingDlg(const char *exe_path, const char *rep_path, const char *player_name, const char *lobby_url, const char *lobby_comment, unsigned int port)
{
	size_t s;
	mbstowcs_s(&s, this->exe_path, _MAX_PATH, exe_path, _MAX_PATH);
	mbstowcs_s(&s, this->rep_path, _MAX_PATH, rep_path, _MAX_PATH);
	mbstowcs_s(&s, this->player_name, NET_STRING_BUFFER, player_name, NET_STRING_BUFFER);
	mbstowcs_s(&s, this->lobby_url, NET_STRING_BUFFER, lobby_url, NET_STRING_BUFFER);
	mbstowcs_s(&s, this->lobby_comment, NET_STRING_BUFFER, lobby_comment, NET_STRING_BUFFER);
	this->port = port;
	chosen_language_id = CChooseLangDlg::cur_language_id;
}

LRESULT CSettingDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
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
	player_name_crtl = GetDlgItem(IDC_EDIT_PNAME);
	player_name_crtl.LimitText(NET_STRING_BUFFER);
	player_name_crtl.SetSelAll();
	player_name_crtl.ReplaceSel(player_name);
	port_number_crtl = GetDlgItem(IDC_EDIT_PORT);
	port_number_crtl.LimitText(6);
	wchar_t _w[8];
	swprintf_s(_w, 8, L"%d", port);
	port_number_crtl.SetSelAll();
	port_number_crtl.ReplaceSel(_w);
	language_crtl = GetDlgItem(IDC_COMBO_LANG);
	for(std::vector< std::basic_string<_TCHAR> >::iterator i = CChooseLangDlg::languages.begin(); i != CChooseLangDlg::languages.end(); ++i) {
		language_crtl.AddString(i->c_str());
	}
	language_crtl.SetCurSel(chosen_language_id);
	::SendMessage(language_crtl.m_hWnd, EM_SETREADONLY, TRUE, 0L);
	lobby_url_crtl = GetDlgItem(IDC_EDIT_LOBBYURL);
	lobby_url_crtl.LimitText(NET_STRING_BUFFER);
	lobby_url_crtl.SetSelAll();
	lobby_url_crtl.ReplaceSel(lobby_url);
	lobby_comment_crtl = GetDlgItem(IDC_EDIT_LOBBYCOMMENT);
	lobby_comment_crtl.LimitText(NET_STRING_BUFFER);
	lobby_comment_crtl.SetSelAll();
	lobby_comment_crtl.ReplaceSel(lobby_comment);

	return 0;
}

LRESULT CSettingDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ref_exe.GetLine(0, exe_path, _MAX_PATH);
	ref_rep.GetLine(0, rep_path, _MAX_PATH);
	player_name_crtl.GetLine(0, player_name, 256);
	lobby_url_crtl.GetLine(0, lobby_url, 256);
	lobby_comment_crtl.GetLine(0, lobby_comment, 256);
	chosen_language_id = language_crtl.GetCurSel();
	wchar_t _w[8];
	port_number_crtl.GetLine(0, _w, 8);
	port = _wtoi(_w);
	EndDialog(wID);
	return 0;
}

void CSettingDlg::GetRefExe(char *exe_path)
{
	size_t s;
	wcstombs_s(&s, exe_path, _MAX_PATH, this->exe_path, _MAX_PATH);
}

void CSettingDlg::GetRefRep(char *rep_path)
{
	size_t s;
	wcstombs_s(&s, rep_path, _MAX_PATH, this->rep_path, _MAX_PATH);
}

LRESULT CSettingDlg::OnRefExe(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	size_t s;
	wchar_t _dir[_MAX_PATH];
	mbstowcs_s(&s, _dir, _MAX_PATH, dir_prefix, _MAX_PATH);
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.hwndOwner = m_hWnd;
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter = _T("Executable Files (*.exe)\0*.exe\0");
	ofn.lpstrFile = exe_path;
	ofn.nMaxFile = _MAX_PATH-1;
	ofn.lpstrInitialDir = _dir;
	ofn.lpstrTitle = _T("ゲームの実行ファイルを指定");
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_EXPLORER;
	ofn.lpstrDefExt = _T("exe");
	if (::GetOpenFileName(&ofn))
	{
		ref_exe.SetSelAll();
		ref_exe.ReplaceSel(exe_path);
	}
	return 0;
}

static int __stdcall BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if(uMsg == BFFM_INITIALIZED) SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	return 0;
}

LRESULT CSettingDlg::OnRefRep(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	size_t s;
	wchar_t _dir[_MAX_PATH], _w[_MAX_PATH];
	if(rep_path[1] == L':') {
		wcscat(_dir, L"\\");
		mbstowcs_s(&s, _dir, _MAX_PATH, replays_dir, _MAX_PATH);
	}
	else {
		mbstowcs_s(&s, _dir, _MAX_PATH, dir_prefix, _MAX_PATH);
		mbstowcs_s(&s, _w, _MAX_PATH, replays_dir, _MAX_PATH);
		wcscat(_dir, L"\\");
		wcscat(_dir, _w);
	}
    LPITEMIDLIST idlist;
	BROWSEINFO  binfo;
	ZeroMemory(&binfo, sizeof(binfo));
    binfo.hwndOwner = m_hWnd;
    binfo.pidlRoot = NULL;
    binfo.pszDisplayName = rep_path;
    binfo.lpszTitle = _T("リプレイを保存するフォルダを指定");
    binfo.ulFlags = BIF_RETURNONLYFSDIRS;
	binfo.lpfn = BrowseCallbackProc;
	binfo.lParam = (LPARAM)_dir;
	if(idlist = ::SHBrowseForFolder(&binfo)) {
		SHGetPathFromIDList(idlist, rep_path);
		::CoTaskMemFree(idlist);
		ref_rep.SetSelAll();
		ref_rep.ReplaceSel(rep_path);
	}
	return 0;
}
