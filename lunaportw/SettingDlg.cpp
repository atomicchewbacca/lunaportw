#include "StdAfx.h"
#include "resource.h"
#include "SettingDlg.h"
#include "lunaportwView.h"

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

CSettingDlg::CSettingDlg(const char *exe_path, const char *rep_path)
{
	size_t s;
	mbstowcs_s(&s, this->exe_path, _MAX_PATH, exe_path, _MAX_PATH);
	mbstowcs_s(&s, this->rep_path, _MAX_PATH, rep_path, _MAX_PATH);
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
