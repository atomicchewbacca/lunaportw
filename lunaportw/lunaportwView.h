// lunaportwView.h : CLunaportwView �N���X�̃C���^�[�t�F�C�X
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

class CLunaportwView : public CWindowImpl<CLunaportwView, CRichEditCtrl>
{
public:
	DECLARE_WND_SUPERCLASS(NULL, CRichEditCtrl::GetWndClassName())

	~CLunaportwView();

	BOOL PreTranslateMessage(MSG* pMsg);

	void Initialize();

	BEGIN_MSG_MAP(CLunaportwView)
		//MESSAGE_HANDLER(WM_CREATE, OnCreate)
	END_MSG_MAP()

	// �n���h���[�̃v���g�^�C�v �i�������K�v�ȏꍇ�̓R�����g���O���Ă��������j:
	//LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
};