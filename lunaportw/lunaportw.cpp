/* LunaPort is a Vanguard Princess netplay application
 * Copyright (C) 2009 by Anonymous
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef _MSC_VER
#pragma warning (disable : 4786)
#endif
#include "stdafx.h"

#include "resource.h"

#ifdef DEBUG
#undef DEBUG
#endif
#include "LunaPort.h"
#include "ConManager.h"
#include "Crc32.h"
#include "StageManager.h"
#include "Session.h"
#include "HTTP.h"
#include "Lobby.h"

#include "lunaportwView.h"
#include "aboutdlg.h"
#include "MainFrm.h"
#include "SettingDlg.h"

#include "lunaportw.h"

using namespace std;

CRITICAL_SECTION cur_logwin_monitor;
CLunaportwView *cur_logwin = NULL;
int __log_printf(const char *fmt, ...)
{
	::EnterCriticalSection(&cur_logwin_monitor);
	if(cur_logwin) {
		char str[512];
		va_list args;
		va_start(args, fmt);
		vsprintf_s(str, 512, fmt, args);
		wchar_t *tmp;
		size_t s = strlen(str);
		size_t ws = mbstowcs(0, str, s);
		if(ws >= 0 && ws != (size_t)-1) {
			tmp = (wchar_t *)malloc(sizeof(wchar_t)*(ws+1));
			if(tmp) {
				size_t ts = mbstowcs(tmp, str, s);
				if(ts >= 0 && ts != (size_t)-1) {
					tmp[ws] = L'\0';\
				}
				else {
					free(tmp);
					return 0;
				}
			}
		}
		cur_logwin->InsertText(-1, tmp);
		free(tmp);
	}
	::LeaveCriticalSection(&cur_logwin_monitor);
	return 0;
}
#define printf __log_printf

static unsigned long get_ip_from_ipstr(const char *cp)
{
	char tmp[256];
	strncpy(tmp, cp, 255);
	for(int i = 0; tmp[i] && i < 256; i++) {
		if(tmp[i] == ':') {
			tmp[i] = '\0';
			break;
		}
	}
	return inet_addr(tmp);
}

static int get_port_from_ipstr(const char *cp, int default_port)
{
	for(int i = 0; cp[i] && i < 256; i++) {
		if(cp[i] == ':') return atoi(cp+i+1);
	}
	return default_port;
}

static bool exefile_exists()
{
	char path[_MAX_PATH];
	if(strlen(game_exe) > 1 && game_exe[1] == ':') {
		strcpy(path, game_exe);
	}
	else {
		strcpy(path, dir_prefix);
		strcat(path, "\\");
		strcat(path, game_exe);
	}
	return ::PathFileExistsA(path) ? true : false;
}

static void host_game (int seed, int record_replay, int ask_delay)
{
	HANDLE alive = NULL, serve_specs = NULL, ping_thread = NULL;
	SOCKADDR_IN sa;
	unsigned long peer = 0;
	luna_packet lp;
	int i, d, tmp, esc = 0;
	unsigned int other_stages;
	DWORD pings = 0;

HOST:
	alive = NULL; serve_specs = NULL; ping_thread = NULL; peer = 0; esc = 0; pings = 0;

	if (DEBUG) { l(); fprintf(logfile, "Hosting\n"); u(); }

	ZeroMemory(&sa, sizeof(sa));
	ZeroMemory(&lp, sizeof(lp));

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);
	if (bind(sock, (SOCKADDR *)&sa, sizeof(sa)))
	{
		l(); printf("Couldn't bind socket.\n"); u();
		return;
	}

	create_sems();

	conmanager.init(&sock, port, (DefaultCallback)spec_accept_callback);
	serve_specs = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)serve_spectators, NULL, 0, NULL);

	l(); printf("Waiting for connection to LunaPort...(ESC to abort)\n"); u();
WAIT:
	TerminateThread(serve_specs, 0);
	CloseHandle(serve_specs);
	max_stages = set_max_stages;
	blacklist1[0] = 0; blacklist2[0] = 0;
	stagemanager.init(max_stages);
	serve_specs = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)serve_spectators, NULL, 0, NULL);
	conmanager.clear();
	conmanager.init(&sock, port, (DefaultCallback)spec_accept_callback);
	p1_name[0] = 0; p2_name[0] = 0;
	peer = conmanager.accept(peer_receiver, &esc);

	if (esc)
	{
		TerminateThread(serve_specs, 0);
		CloseHandle(serve_specs);
		max_stages = set_max_stages;
		blacklist1[0] = 0; blacklist2[0] = 0;
		stagemanager.init(max_stages);
		conmanager.clear();
		bye_spectators();
		close_sems();
		closesocket(sock);
		return;
	}

	if (peer == 0)
		goto WAIT;

	local_p = rand() & 1;
	l(); printf("Establishing connection to %s.\nLocal player %u.\n", ip2str(peer), local_p + 1); u();

	simple_luna_packet(&lp, PACKET_TYPE_PNUM, 0);
	lp.value = local_p ? 0 : 1;
	
	if (!conmanager.send(&lp, peer))
	{
		l(); printf("Failure to establish connection, waiting for new connection, no player number.\n");
		conmanager.disconnect(peer);
		goto WAIT;
	}
	simple_luna_packet(&lp, PACKET_TYPE_KGTCH, 0);
	lp.value = kgt_crc;
	if (!conmanager.send(&lp, peer))
	{
		l(); printf("Failure to establish connection, waiting for new connection, no player number.\n");
		conmanager.disconnect(peer);
		goto WAIT;
	}
	simple_luna_packet(&lp, PACKET_TYPE_KGTLN, 0);
	lp.value = kgt_size;
	if (!conmanager.send(&lp, peer))
	{
		l(); printf("Failure to establish connection, waiting for new connection, no player number.\n");
		conmanager.disconnect(peer);
		goto WAIT;
	}
	if (!conmanager.receive(&lp, peer))
	{
		l(); printf("Failure to establish connection, waiting for new connection, no player number.\n");
		conmanager.disconnect(peer);
		goto WAIT;
	}
	if (!PACKET_IS(lp, PACKET_TYPE_KGTST) || lp.value == 0)
	{
		l(); printf("KGT mismatch. Peer is using an incompatible game version.\nWaiting for new connection.\n");
		conmanager.disconnect(peer);
		goto WAIT;
	}
	if (!send_string(own_name, peer))
	{
		l(); printf("Failure to establish connection, waiting for new connection, couldn't send name.\n");
		conmanager.disconnect(peer);
		goto WAIT;
	}
	if (!receive_string(local_p ? p1_name : p2_name, peer))
	{
		l(); printf("Failure to establish connection, waiting for new connection, couldn't receive name.\n");
		conmanager.disconnect(peer);
		goto WAIT;
	}
	
	strcpy(local_p ? p2_name : p1_name, own_name);
	l(); printf("Remote player: %s\n", local_p ? p1_name : p2_name); u();

	simple_luna_packet(&lp, PACKET_TYPE_STAGE, 0);
	lp.value = max_stages;
	if (!conmanager.send(&lp, peer) || !conmanager.receive(&lp, peer) || !PACKET_IS(lp, PACKET_TYPE_STAGE) || !send_string(set_blacklist, peer) || !receive_string(local_p ? blacklist1 : blacklist2, peer))
	{
		l(); printf("Failed to transmit stage information.\n"); u();
		conmanager.disconnect(peer);
		goto WAIT;
	}
	other_stages = lp.value;
	strcpy(local_p ? blacklist2 : blacklist1, set_blacklist);
	max_stages = MIN(max_stages, other_stages);
	stagemanager.init(max_stages);
	stagemanager.blacklist(blacklist1);
	stagemanager.blacklist(blacklist2);
	if (!stagemanager.ok())
	{
		l(); printf("After applying blacklists, no stages (of %u) are left.\nLocal blacklist: %s\nRemote blacklist: %s\n", max_stages, set_blacklist, local_p ? blacklist1 : blacklist2); u();
		conmanager.disconnect(peer);
		goto WAIT;
	}

	simple_luna_packet(&lp, PACKET_TYPE_SEED, 0);
	lp.value = seed;
	if (!conmanager.send(&lp, peer))
	{
		l(); printf("Failure to establish connection, waiting for new connection.\n");
		conmanager.disconnect(peer);
		goto WAIT;
	}

	do
	{
		if (ping_thread != NULL)
		{
			terminate_ping_thread = 1;
			WaitForSingleObject(sem_ping_end, INFINITE);
			TerminateThread(ping_thread, 0);
			CloseHandle(ping_thread);
			ping_thread = NULL;
		}
		pings = 0;
		for (i = 0; i < 10; i++)
		{
			tmp = get_ping(peer);
			if (tmp == -1)
			{
				l(); printf("Failure to establish connection, waiting for new connection.\n");
				conmanager.disconnect(peer);
				goto WAIT;
			}
			pings += tmp;
		}
		pings /= 10; // pings/10 = RTT
		d = (int)(((float)pings + MS_PER_INPUT) * 2.0 / MS_PER_INPUT + SAFETY_DELAY);
		if (d > 0xff)
			d = 0xff;
		if (play_host_sound)
			play_sound();

		if (ask_delay)
		{
			terminate_ping_thread = 0;
			ping_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ping_thread_loop, (void *)peer, 0, NULL);
			l(); printf("\nDetermined ping of %ums. This corresponds to an input delay of %u input requests.\nIf you experience heavy lag, try increasing the delay.\nPlease enter delay (0 = ping again)\n", pings, d); u();
			::SetForegroundWindow(::FindWindowA(NULL, "LunaPort " VERSION));
			class CDelayDlg : public CDialogImpl<CDelayDlg>
			{
			public:
				enum { IDD = IDD_DELAYBOX };

				CComboBox crtl;
				int delay;

				CDelayDlg(int delay) : delay(delay) {}
				// メッセージマップ
				BEGIN_MSG_MAP(CIpDlg)
					MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
					COMMAND_ID_HANDLER(IDOK, OnOK)
				END_MSG_MAP()

				LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
				{
					// スクリーンの中央に配置
					CenterWindow();

					// コントロール設定
					crtl = GetDlgItem(IDC_COMBO1);
					crtl.AddString(_T("PINGを再測定"));
					for(int i = 0; i < 255; i++) {
						wchar_t _w[8];
						swprintf_s(_w, 8, L"%d", i+1);
						crtl.AddString(_w);
					}
					crtl.SetCurSel(delay);

					return 0;
				}

				LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
				{
					delay = crtl.GetCurSel();
					EndDialog(wID);
					return 0;
				}
			} dlg(d);
			dlg.DoModal();
			d = dlg.delay;
			TerminateThread(ping_thread, 0);
			CloseHandle(ping_thread);
			ping_thread = NULL;
		}
		else
		{
			l(); printf("\nDetermined ping of %ums. This corresponds to an input delay of %u input requests.\n", pings, d); u();
		}
	} while (d == 0);

	set_delay(d);
	simple_luna_packet(&lp, PACKET_TYPE_DELAY, 0);
	lp.value = PACKET_VALUE_DELAY(delay, pings);
	if (!conmanager.send(&lp, peer))
	{
		l(); printf("Failure to establish connection, waiting for new connection.\n");
		conmanager.disconnect(peer);
		goto WAIT;
	}

	conmanager.set_async(peer);
	remote_player = peer;
	proc = NULL;
	alive = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)keep_alive, (void *)remote_player, 0, NULL);
	run_game(seed, 1, record_replay, NULL, 0);
	proc = NULL;
	conmanager.disconnect(peer);
	bye_spectators();
	TerminateThread(alive, 0);
	CloseHandle(alive);
	TerminateThread(serve_specs, 0);
	CloseHandle(serve_specs);
	conmanager.clear();
	//clear_socket();
	close_sems();
	closesocket(sock);

	l(); printf("Game finished.\n\n"); u();

	if (keep_hosting)
		goto HOST;
}

static void _WritePrivateProfileIntA(const char *lpAppName, const char *lpKeyName, int val, const char *lpFileName)
{
	char w[32];
	sprintf_s(w, 32, "%d", val);
	::WritePrivateProfileStringA(lpAppName, lpKeyName, w, lpFileName);
}

static void save_config(int max_points, int keep_session_log, const char *session_log)
{
	char config_filename[_MAX_PATH];
	strcpy(config_filename, dir_prefix);
	strcat(config_filename, "\\" INIFILE);
	_WritePrivateProfileIntA("LunaPort", "Port", port, config_filename);
	_WritePrivateProfileIntA("LunaPort", "RecordReplayDefault", record_replay, config_filename);
	_WritePrivateProfileIntA("LunaPort", "AllowSpectatorsDefault", allow_spectators, config_filename);
	_WritePrivateProfileIntA("LunaPort", "Stages", set_max_stages, config_filename);
	_WritePrivateProfileIntA("LunaPort", "AskDelay", ask_delay, config_filename);
	_WritePrivateProfileIntA("LunaPort", "AskSpectate", ask_spectate, config_filename);
	_WritePrivateProfileIntA("LunaPort", "KeepHosting", keep_hosting, config_filename);
	_WritePrivateProfileIntA("LunaPort", "DisplayFramerate", display_framerate, config_filename);
	_WritePrivateProfileIntA("LunaPort", "DisplayInputrate", display_inputrate, config_filename);
	_WritePrivateProfileIntA("LunaPort", "DisplayNames", display_names, config_filename);
	_WritePrivateProfileIntA("LunaPort", "DisplayLobbyComments", display_lobby_comments, config_filename);
	_WritePrivateProfileIntA("LunaPort", "UseBlacklistLocal", blacklist_local, config_filename);
	_WritePrivateProfileIntA("LunaPort", "MaxPoints", max_points, config_filename);
	_WritePrivateProfileIntA("LunaPort", "KeepSessionLog", keep_session_log, config_filename);
	_WritePrivateProfileIntA("LunaPort", "PlayHostConnect", play_host_sound, config_filename);
	_WritePrivateProfileIntA("LunaPort", "PlayLobbyChange", play_lobby_sound, config_filename);
	_WritePrivateProfileIntA("LunaPort", "CheckExeCRC", check_exe, config_filename);
	::WritePrivateProfileStringA("LunaPort", "GameExe", game_exe, config_filename);
	::WritePrivateProfileStringA("LunaPort", "Replays", replays_dir, config_filename);
	::WritePrivateProfileStringA("LunaPort", "PlayerName", own_name, config_filename);
	::WritePrivateProfileStringA("LunaPort", "StageBlacklist", set_blacklist, config_filename);
	::WritePrivateProfileStringA("LunaPort", "SessionLog", session_log, config_filename);
	::WritePrivateProfileStringA("LunaPort", "Lobby", lobby_url, config_filename);
	::WritePrivateProfileStringA("LunaPort", "LobbyComment", lobby_comment, config_filename);
	::WritePrivateProfileStringA("LunaPort", "Sound", sound, config_filename);
}

int record_replay, ask_delay;

static WSADATA wsa;
static char ip_str[NET_STRING_BUFFER], tmp[256];
static char filename[_MAX_PATH];
static SECURITY_ATTRIBUTES sa;
static OPENFILENAMEA ofn;
static int in = 10, old_in = 10;
static int max_points = 2;
static int keep_session_log = 1;
static char session_log[_MAX_PATH];
static bool exit_lobby = false;
static int lobby_port, lobby_spec;

unsigned int __stdcall lunaport_serve(void *_in)
{
	int in = (int)_in;
	switch (in)
	{
	case 1:
		SetEvent(event_waiting);
		host_game((rand()<<16) + rand(), record_replay, ask_delay);
		break;
	case 2:
		do {
			class CIpDlg : public CDialogImpl<CIpDlg>
			{
			public:
				enum { IDD = IDD_IPBOX };

				CIPAddressCtrl ip_crtl;
				CEdit port_crtl;
				DWORD addr;
				WORD port;

				CIpDlg(DWORD addr, WORD port) : addr(addr), port(port) {}
				// メッセージマップ
				BEGIN_MSG_MAP(CIpDlg)
					MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
					COMMAND_ID_HANDLER(IDOK, OnOK)
					COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
				END_MSG_MAP()

				LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
				{
					// スクリーンの中央に配置
					CenterWindow();

					// コントロール設定
					ip_crtl = GetDlgItem(IDC_IPADDRESS1);
					ip_crtl.SetAddress(addr);
					port_crtl = GetDlgItem(IDC_EDIT1);
					wchar_t _w[8];
					swprintf_s(_w, 8, L"%d", port);
					port_crtl.AppendText(_w);

					return 0;
				}

				LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
				{
					ip_crtl.GetAddress(&addr);
					wchar_t _w[8];
					port_crtl.GetLine(0, _w, 8);
					port = _wtoi(_w);
					EndDialog(wID);
					return 0;
				}

				LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
				{
					EndDialog(wID);
					return 0;
				}
			} dlg((strlen(ip_str) > 0 && get_ip_from_ipstr(ip_str) != INADDR_NONE) ? htonl(get_ip_from_ipstr(ip_str)) : MAKEIPADDRESS(192, 168, 1, 1), get_port_from_ipstr(ip_str, port));
			if(IDOK == dlg.DoModal()) {
				strcpy(tmp, ip2str(htonl(dlg.addr)));
				char _w[8];
				sprintf_s(_w, 8, "%d", dlg.port);
				strcat(tmp, ":");
				strcat(tmp, _w);
				if (strlen(tmp))
				{
					if (get_ip_from_ipstr(tmp) == INADDR_NONE || get_ip_from_ipstr(tmp) == 0)
					{
						l(); printf("Invalid IP: %s\n", tmp); u();
					}
					else {
						strcpy(ip_str, tmp);
						break;
					}
				}
			}
			else break;
		} while(true);
		if (get_ip_from_ipstr(ip_str) == INADDR_NONE || get_ip_from_ipstr(ip_str) == 0)
		{
			l(); printf("Invalid IP: %s\n", ip_str); u();
			break;
		}
		SetEvent(event_waiting);
		join_game(ip_str, port, record_replay);
		break;
	case 3:
		SetEvent(event_waiting);
		lobby.host();
		host_game((rand()<<16) + rand(), record_replay, ask_delay);
		lobby.disconnect();
		break;
	case 4:
		exit_lobby = false;
		do
		{
			exit_lobby = lobby.menu(ip_str, &lobby_port, &lobby_spec);
			if (!exit_lobby)
			{
				if (inet_addr(ip_str) == INADDR_NONE || inet_addr(ip_str) == 0)
				{
					l(); printf("Invalid IP: %s\n", ip_str); u();
					continue;
				}
				SetEvent(event_waiting);
				if (!lobby_spec)
					join_game(ip_str, lobby_port, record_replay);
				else
					spectate_game(ip_str, lobby_port, record_replay);
			}
		} while (!exit_lobby);
		break;
	case 5:
		SetEvent(event_waiting);
		if (blacklist_local)
			stagemanager.blacklist(set_blacklist);
		if (!stagemanager.ok())
		{
			l(); printf("After applying blacklists, no stages (of %u) are left.\nBlacklist: %s\n", max_stages, set_blacklist); u();
			break;
		}
		run_game((rand()<<16) + rand(), 0, record_replay, NULL, 0);
		break;
	case 6:
		if (record_replay)
		{
			record_replay = 0;
			printf("Replay recording disabled.\n");
		}
		else
		{
			record_replay = 1;
			printf("Replay recording enabled.\n");
		}
		break;
	case 7:
		ZeroMemory(&ofn, sizeof(ofn));
		ZeroMemory(filename, _MAX_PATH);
		ofn.hwndOwner = GetForegroundWindow();
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFilter = "Replay Files (*.rpy)\0*.rpy\0";
		ofn.lpstrFile = filename;
		ofn.nMaxFile = _MAX_PATH-1;
		ofn.lpstrInitialDir = replays_dir;
		ofn.lpstrTitle = "Open Replay";
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_ENABLEHOOK | OFN_EXPLORER;
		ofn.lpstrDefExt = "rpy";
		ofn.lpfnHook = ofn_hook;
		if (GetOpenFileNameA(&ofn))
		{
			_chdir(dir_prefix);
			SetEvent(event_waiting);
			run_game(0, 0, -1, filename, 0);
		}
		else
		{
			printf("No replay selected.\n");
		}
		break;
	case 8:
		if (allow_spectators)
		{
			allow_spectators = 0;
			printf("Spectators will be denied.\n");
		}
		else
		{
			allow_spectators = 1;
			printf("Spectators will be allowed to spectate.\n");
		}
		break;
	case 9:
		do {
			class CIpDlg : public CDialogImpl<CIpDlg>
			{
			public:
				enum { IDD = IDD_IPBOX };

				CIPAddressCtrl ip_crtl;
				CEdit port_crtl;
				DWORD addr;
				WORD port;

				CIpDlg(DWORD addr, WORD port) : addr(addr), port(port) {}
				// メッセージマップ
				BEGIN_MSG_MAP(CIpDlg)
					MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
					COMMAND_ID_HANDLER(IDOK, OnOK)
					COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
				END_MSG_MAP()

				LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
				{
					// スクリーンの中央に配置
					CenterWindow();

					SetWindowText(_T("Spectate"));
					// コントロール設定
					CStatic text = GetDlgItem(IDC_STATIC);
					text.SetWindowText(_T("観戦先のIPとポート番号を入力してください"));
					ip_crtl = GetDlgItem(IDC_IPADDRESS1);
					ip_crtl.SetAddress(addr);
					port_crtl = GetDlgItem(IDC_EDIT1);
					wchar_t _w[8];
					swprintf_s(_w, 8, L"%d", port);
					port_crtl.AppendText(_w);

					return 0;
				}

				LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
				{
					ip_crtl.GetAddress(&addr);
					wchar_t _w[8];
					port_crtl.GetLine(0, _w, 8);
					port = _wtoi(_w);
					EndDialog(wID);
					return 0;
				}

				LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
				{
					EndDialog(wID);
					return 0;
				}
			} dlg((strlen(ip_str) > 0 && get_ip_from_ipstr(ip_str) != INADDR_NONE) ? htonl(get_ip_from_ipstr(ip_str)) : MAKEIPADDRESS(192, 168, 1, 1), get_port_from_ipstr(ip_str, port));
			if(IDOK == dlg.DoModal()) {
				strcpy(tmp, ip2str(htonl(dlg.addr)));
				char _w[8];
				sprintf_s(_w, 8, "%d", dlg.port);
				strcat(tmp, ":");
				strcat(tmp, _w);
				if (strlen(tmp))
				{
					if (get_ip_from_ipstr(tmp) == INADDR_NONE || get_ip_from_ipstr(tmp) == 0)
					{
						l(); printf("Invalid IP: %s\n", tmp); u();
					}
					else {
						strcpy(ip_str, tmp);
						break;
					}
				}
			}
			else break;
		} while(true);
		if (get_ip_from_ipstr(ip_str) == INADDR_NONE || get_ip_from_ipstr(ip_str) == 0)
		{
			l(); printf("Invalid IP: %s\n", ip_str); u();
			break;
		}
		SetEvent(event_waiting);
		spectate_game(ip_str, port, record_replay);
		break;
	case 10:
		print_menu(record_replay);
		break;
	case 0:
		break;
	default:
		l(); printf("Unknown menu item: %d\n", in); u();
		in = old_in;
		break;
	}

	if (game_history != NULL)
	{
		free(game_history);
		game_history = NULL;
		game_history_pos = 0;
		spec_pos = 0;
	}
	ResetEvent(event_running);
	ResetEvent(event_waiting);
	spectators.clear();
	p1_name[0] = 0;
	p2_name[0] = 0;
	blacklist1[0] = 0;
	blacklist2[0] = 0;
	write_replay = NULL;
	recording_ended = false;
	max_stages = set_max_stages;
	printf("Done.\n");
	return 0;
}

CAppModule _Module;

int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if(wndMain.CreateEx() == NULL)
	{
		ATLTRACE(_T("メイン ウィンドウの作成に失敗しました！\n"));
		return 0;
	}

	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
	locale::global(locale(locale::classic(), locale(""), LC_CTYPE));

	HRESULT hRes = ::CoInitialize(NULL);
	//もしNT 4.0以上の場合は上の1行を以下のコードで代用できます。
	//HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	//ユニコードレイヤー（MSLU: Windows9x用）使用時のATLウィンドウのサンキング問題を解消します。
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES);	// ほかのコントロール用のフラグを追加してください

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	HMODULE hInstRich = ::LoadLibrary(CRichEditCtrl::GetLibraryName());
	ATLASSERT(hInstRich != NULL);

	if (DEBUG)
	{
		logfile = fopen("debug.log", "w+");
		setvbuf(logfile, NULL, _IONBF, 0);
		fprintf(logfile, "Starting session\n");
	}
	if (BP_PROFILE)
	{
		bpprof = fopen("profile.dat", "wb");
		setvbuf(bpprof, NULL, _IONBF, 0);
	}

	this_proc = ::GetModuleHandle(NULL);
	local_p = 0;
	srand((unsigned int)time(NULL));
	game_history = NULL;
	game_history_pos = 0;
	ip_str[0] = 0;
	tmp[0] = 0;
	write_replay = NULL;
	recording_ended = false;
	::ZeroMemory(game_exe, sizeof(game_exe));
	::ZeroMemory(own_name, sizeof(own_name));
	::ZeroMemory(p1_name, sizeof(p1_name));
	::ZeroMemory(p2_name, sizeof(p2_name));
	::ZeroMemory(set_blacklist, sizeof(set_blacklist));
	::ZeroMemory(blacklist1, sizeof(blacklist1));
	::ZeroMemory(blacklist2, sizeof(blacklist2));
	::ZeroMemory(session_log, sizeof(session_log));

	::ZeroMemory(&sa, sizeof(sa));
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof(sa);
	mutex_print = ::CreateMutex(&sa, FALSE, NULL);
	event_running = ::CreateEventA(&sa, TRUE, FALSE, "");
	event_waiting = ::CreateEventA(&sa, TRUE, FALSE, "");

	::InitializeCriticalSection(&cur_logwin_monitor);

	char exepath[_MAX_PATH+1];
	::GetModuleFileNameA(::GetModuleHandle(NULL), exepath, _MAX_PATH);
	if(!::PathFileExistsA(INIFILE) && inifile_exists(exepath)) {
		strcpy(dir_prefix, exepath);
		for (int i = strlen(dir_prefix) - 1; i >= 0; i--)
			if (dir_prefix[i] == '\\' || dir_prefix[i] == '/')
			{
				dir_prefix[i] = 0;
				break;
			}
		_chdir(dir_prefix);
	}
	else _getcwd(dir_prefix, _MAX_PATH);

	read_config(&port, &record_replay, &allow_spectators, &set_max_stages, &ask_delay, &ask_spectate, &display_framerate,
	            &display_inputrate, &display_names, game_exe, own_name, set_blacklist, &blacklist_local, &check_exe,
				&max_points, &keep_session_log, session_log, lobby_url, lobby_comment, &display_lobby_comments,
				&keep_hosting, sound, &play_host_sound, &play_lobby_sound, replays_dir);
	while(!exefile_exists()) {
		::MessageBox(NULL, _T("ゲームの実行ファイルが見つかりませんでした。\nファイルの位置を設定してください"), _T("LunaPortW Error."), MB_OK | MB_ICONERROR);
		CSettingDlg dlg(game_exe, replays_dir);
		if(IDCANCEL == dlg.DoModal()) break;
		else {
			dlg.GetRefExe(game_exe);
			dlg.GetRefRep(replays_dir);
		}
	}
	if(!::PathFileExistsA(INIFILE)) {
		save_config(max_points, keep_session_log, session_log);
	}

	max_stages = set_max_stages;
	calc_crcs();
	stagemanager.init(max_stages);

	if (!keep_session_log) session_log[0] = 0;
	session.set_options(session_log, max_points, kgt_crc, kgt_size);

	if (WSAStartup(MAKEWORD(2, 0), &wsa))
	{
		MessageBox(NULL, _T("Couldn't initialize winsocks."), _T("LunaPort Error"), MB_OK | MB_ICONERROR);
		return 1;
	}
	curl_global_init(CURL_GLOBAL_ALL);
	lobby.init(lobby_url, kgt_crc, kgt_size, own_name, lobby_comment, port, &lobby_flag, display_lobby_comments, play_lobby_sound);
	atexit(unregister_lobby);

	::CreateDirectoryA(replays_dir, NULL);

	int nRet = Run(lpstrCmdLine, nCmdShow);

	save_config(max_points, keep_session_log, session_log);
	WSACleanup();

	::DeleteCriticalSection(&cur_logwin_monitor);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
