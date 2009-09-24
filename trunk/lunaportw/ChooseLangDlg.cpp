#include "StdAfx.h"
#include "resource.h"
#include "ChooseLangDlg.h"

std::vector< std::basic_string<_TCHAR> > CChooseLangDlg::languages;

int CChooseLangDlg::cur_language_id = 0;

void CChooseLangDlg::ListUpLanguage()
{
	languages.clear();
	languages.push_back(_T("english(default)"));
	_TCHAR langdir[_MAX_PATH+1];
	::GetModuleFileName(_Module.GetModuleInstance(), langdir, _MAX_PATH);
	for(int i = _tcslen(langdir) - 1; i >= 0; i--) {
		if(langdir[i] == _T('\\') || langdir[i] == _T('/')) {
			langdir[i] = 0;
			break;
		}
	}
	_tcscat(langdir, _T("\\language\\*.dll"));
	CFindFile dir;
	if(dir.FindFile(langdir)) {
		do {
			if(!dir.IsDirectory() && !dir.IsDots()) languages.push_back(std::basic_string<_TCHAR>(dir.GetFileTitle()));
		} while(dir.FindNextFile());
	}
}

void CChooseLangDlg::ChangeLanguage(const _TCHAR *language)
{
	int idx = 0;
	if(_tcscmp(_T(""), language) == 0 || _tcscmp(_T("english(default)"), language) == 0) {
		cur_language_id = 0;
		if(_Module.m_hInstResource != _Module.GetModuleInstance()) ::FreeLibrary(_Module.m_hInstResource);
		_Module.m_hInstResource = _Module.GetModuleInstance();
		return;
	}
	for(std::vector< std::basic_string<_TCHAR> >::iterator i = languages.begin(); i != languages.end(); ++i, idx++) {
		if((*i) == language) {
			_TCHAR langpath[_MAX_PATH+1];
			::GetModuleFileName(_Module.GetModuleInstance(), langpath, _MAX_PATH);
			for(int i = _tcslen(langpath) - 1; i >= 0; i--) {
				if(langpath[i] == _T('\\') || langpath[i] == _T('/')) {
					langpath[i] = 0;
					break;
				}
			}
			_tcscat(langpath, _T("\\language\\"));
			_tcscat(langpath, language);
			cur_language_id = idx;
			HMODULE hPlugin = ::LoadLibrary(langpath);
			ATLASSERT(hPlugin != NULL);
			if(_Module.m_hInstResource != _Module.GetModuleInstance()) ::FreeLibrary(_Module.m_hInstResource);
			_Module.m_hInstResource = hPlugin;
			break;
		}
	}
}

const _TCHAR *CChooseLangDlg::CurrentLanguage()
{
	return languages[cur_language_id].c_str();
}

const _TCHAR *CChooseLangDlg::ChosenLanguage()
{
	return languages[chosen_language_id].c_str();
}

LRESULT CChooseLangDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// スクリーンの中央に配置
	CenterWindow();

	// コントロール設定
	crtl = GetDlgItem(IDC_COMBO1);
	for(std::vector< std::basic_string<_TCHAR> >::iterator i = languages.begin(); i != languages.end(); ++i) {
		crtl.AddString(i->c_str());
	}
	chosen_language_id = cur_language_id;
	crtl.SetCurSel(chosen_language_id);

	return 0;
}

LRESULT CChooseLangDlg::OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	chosen_language_id = crtl.GetCurSel();
	EndDialog(wID);
	return 0;
}
