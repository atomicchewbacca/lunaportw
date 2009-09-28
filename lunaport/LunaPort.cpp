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

#include <direct.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <winuser.h>
#include <mmsystem.h>
#include <tlhelp32.h>
#include <shlwapi.h>
#include <iostream>
#include <string>
#include <curl/curl.h>
#include "LunaPort.h"
#include "Code.h"
#include "ConManager.h"
#include "Crc32.h"
#include "StageManager.h"
#include "Session.h"
#include "HTTP.h"
#include "Lobby.h"
using namespace std;

// don't like these globals and gotos? tough

FILE *logfile;                 // debug log file
FILE *bpprof;                  // breakpoint time profile file
HANDLE this_proc;              // LunaPort handle
HANDLE proc = 0;               // Vanguard Princess handle
HANDLE sem_recvd_input;        // new remote input has been received
HANDLE sem_ping_end;           // signal end of ping thread
HANDLE mutex_print;            // ensure prints are ordered/atomic
HANDLE mutex_history;          // lock changes to game	history
HANDLE event_running;          // game has started
HANDLE event_waiting;          // game is waiting
HANDLE small_task_thread;      // for small task
DWORD small_task_thread_id;
SOCKET sock;                   // socket used for communication
unsigned long remote_player;   // remote player
int local_p;                   // 0 = local p1, 1 = local p2
int delay = 0;                 // delay of input requests
int lobby_flag = 0;            // currently have registered game on lobby
InputHistory local_history;    // buffer for local input
InputHistory remote_history;   // buffer for remote input
char dir_prefix[_MAX_PATH];             // path prefix
unsigned int *game_history;             // history of the complete game session
unsigned int game_history_pos;          // current position in game history
unsigned int spec_pos;                  // maximum received position of this spectator
spec_map spectators;                    // map of spectators
int allow_spectators;                   // 0 = do not allow spectators
unsigned int port;                      // network port
unsigned int max_stages;                // number of stages
unsigned int set_max_stages;            // number of stages set in lunaport.ini
int rnd_seed;                           // seed of current game
FILE *write_replay;                     // write replay here
bool recording_ended;                   // set after faking a packet, this ensures it is not written to replay file
DWORD kgt_crc;                          // CRC32 of kgt file
long kgt_size;                          // size of kgt file
int blacklist_local;                    // 1 = use stage blacklist for local games too, 0 = don't
int terminate_ping_thread;
int check_exe;
int ask_spectate;
int keep_hosting;
int play_host_sound;
int play_lobby_sound;
int display_framerate;
int display_inputrate;
int display_names;
int display_lobby_comments;
char game_exe[_MAX_PATH], replays_dir[_MAX_PATH];
char own_name[NET_STRING_BUFFER], p1_name[NET_STRING_BUFFER], p2_name[NET_STRING_BUFFER];
char set_blacklist[NET_STRING_BUFFER], blacklist1[NET_STRING_BUFFER], blacklist2[NET_STRING_BUFFER];
char lobby_url[NET_STRING_BUFFER], lobby_comment[NET_STRING_BUFFER];
char sound[_MAX_PATH];
ConManager conmanager;
StageManager stagemanager;
Session session;
Lobby lobby;
bool force_esc = false;

void l () { /*WaitForSingleObject(mutex_print, INFINITE);*/ } // l() to lock print mutex
void u () { /*ReleaseMutex(mutex_print);*/ } // u() to unlock print mutex
void play_sound () { PlaySound(sound, NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT); }
char *ip2str (unsigned long ip)
{
	SOCKADDR_IN sa;
	sa.sin_addr.s_addr = ip;
	return inet_ntoa(sa.sin_addr);
}

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

// implementation of InputHistory (split into separate file later)
InputHistory::InputHistory ()
{
	this->size = 0;
	this->hist = NULL;
	this->d = 0;
	this->read_pos = 0;
	this->write_pos = 0;
	this->mutex = NULL;
}
void InputHistory::init (int d, packet_state init)
{
	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&sa, sizeof(sa));
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof(sa);

	if (mutex != NULL)
	{
		WaitForSingleObject(mutex, 5);
		CloseHandle(mutex);
	}

	this->size = HISTORY_CHUNK_SIZE;
	if (hist != NULL)
		free(hist);
	this->hist = (history_entry *)calloc(size, sizeof(history_entry));
	this->d = d;
	this->read_pos = 0;
	this->write_pos = d;
	this->mutex = CreateMutex(&sa, FALSE, NULL);

	WaitForSingleObject(mutex, INFINITE);
	for (int i = 0; i < d; i++)
	{
		hist[i].state = init;
		hist[i].value = 0;
	}
	ReleaseMutex(mutex);
}
InputHistory::~InputHistory ()
{
	free(hist);
	CloseHandle(mutex);
}
void InputHistory::fake ()
{
	history_entry he;
	WaitForSingleObject(mutex, INFINITE);
	ensure(read_pos + 1);
	for (int i = 0; i < 2; i++)
	{
		he = hist[read_pos+i];
		if (he.state != RECEIVED)
		{
			recording_ended = true;
			he.state = RECEIVED;
			he.value = 0;
			hist[read_pos+i] = he;
		}
	}
	ReleaseMutex(mutex);
}
history_entry InputHistory::get (int *p)
{
	history_entry he;
	WaitForSingleObject(mutex, INFINITE);
	if (p != NULL)
		*p = read_pos;
	he = hist[read_pos];
	if (he.state == RECEIVED || he.state == SENT)
		read_pos++;
	ReleaseMutex(mutex);
	return he;
}
int InputHistory::get_refill ()
{
	int diff;
	WaitForSingleObject(mutex, INFINITE);
	diff = d - (write_pos - read_pos);
	ReleaseMutex(mutex);
	return diff;
}
void InputHistory::ensure (int position)
{
	if (position > size - 1)
	{
		size += HISTORY_CHUNK_SIZE;
		hist = (history_entry *)realloc(hist, size * sizeof(history_entry));
		ZeroMemory(((char *)hist) + size - HISTORY_CHUNK_SIZE * sizeof(history_entry), HISTORY_CHUNK_SIZE * sizeof(history_entry));
	}
}
luna_packet InputHistory::put (int v)
{
	luna_packet lp;
	int min, max, j;
	ZeroMemory(&lp, sizeof(lp));
	WaitForSingleObject(mutex, INFINITE);
	min = MAX(0, write_pos - BACKUP_VALUES);
	max = write_pos;
	lp.header = PACKET_HEADER(PACKET_TYPE_INPUT, 0);
	lp.low_id = min;
	lp.high_id = max;
	ensure(write_pos);
	hist[write_pos].state = SENT;
	hist[write_pos++].value = v;
	lp.value = hist[min].value;
	j = 0;
	for (min++; min <= max; min++)
		lp.backup[j++] = hist[min].value;
	ReleaseMutex(mutex);
	return lp;
}
luna_packet InputHistory::resend ()
{
	luna_packet lp;
	int min, max, j;
	ZeroMemory(&lp, sizeof(lp));
	WaitForSingleObject(mutex, INFINITE);
	min = read_pos;
	max = MIN(MIN(read_pos + d, read_pos + BACKUP_VALUES), write_pos - 1);
	ensure(max);
	lp.header = PACKET_HEADER(PACKET_TYPE_INPUT, 0);
	lp.low_id = min;
	lp.high_id = max;
	lp.value = hist[min].value;
	j = 0;
	for (min++; min <= max; min++)
		lp.backup[j++] = hist[min].value;
	ReleaseMutex(mutex);
	return lp;
}
luna_packet *InputHistory::resend (int low, int high, int *n)
{
	luna_packet *packets;
	int j;
	*n = 0;
	if (high >= write_pos)
	{
		if (DEBUG) { l(); fprintf(logfile, "Resend: Clamping high from %08x to %08x.\n", high, write_pos); u(); }
		high = write_pos - 1;
	}
	if (high - low > 2*d)
		high = low + 2*d;
	if (low > high)
		low = high;
	*n = (int)ceil((double)(high - low + 1)/(double)(1 + BACKUP_VALUES));
	if (DEBUG) { l(); fprintf(logfile, "Resend preparing %d packets.\n", *n); u(); }
	packets = (luna_packet *)calloc(*n, sizeof(luna_packet));
	WaitForSingleObject(mutex, INFINITE);
	ensure(high);
	for (int i = 0; i < *n; i++)
	{
		packets[i].header = PACKET_HEADER(PACKET_TYPE_INPUT, 0);
		packets[i].low_id = low;
		packets[i].high_id = low;
		packets[i].value = hist[low].value;
		j = 0;
		for (low++; low <= high; low++)
		{
			packets[i].high_id++;
			packets[i].backup[j++] = hist[low].value;
		}
		if (DEBUG) { l(); fprintf(logfile, "Resend prepared packet with range %08x to %08x.\n", packets[i].low_id, packets[i].high_id); u(); }
	}
	ReleaseMutex(mutex);
	return packets;
}
luna_packet InputHistory::request ()
{
	luna_packet lp;
	ZeroMemory(&lp, sizeof(lp));
	WaitForSingleObject(mutex, INFINITE);
	lp.header = PACKET_HEADER(PACKET_TYPE_OK, 0); // oops?
	if (hist[read_pos].state != SKIPPED && hist[read_pos].state != EMPTY)
	{
		ReleaseMutex(mutex);
		return lp;
	}

	lp.header = PACKET_HEADER(PACKET_TYPE_AGAIN, 0);
	lp.low_id = read_pos;
	for (lp.high_id = read_pos; (hist[lp.high_id + 1].state == SKIPPED || hist[lp.high_id + 1].state == EMPTY) && lp.high_id + 1 <= read_pos + 2*d; lp.high_id++);

	ReleaseMutex(mutex);
	return lp;
}
int InputHistory::put (luna_packet lp)
{
	int gain = 0, j, min;
	WaitForSingleObject(mutex, INFINITE);
	if (DEBUG) { l(); fprintf(logfile, "Putting packet.\n"); u(); }
	if (read_pos + d * 2 < lp.high_id || lp.high_id - lp.low_id > BACKUP_VALUES || lp.low_id > lp.high_id) // do not accept packets with broken ranges or from too far in the future
	{
		if (DEBUG) { l(); fprintf(logfile, "Ignoring packet, out of range: %08x@%08x-%08x\n", read_pos + d, lp.low_id, lp.high_id); u(); }
		ReleaseMutex(mutex);
		return gain;
	}
	ensure(lp.high_id);
	min = lp.low_id;
	gain += set(min, lp.value);
	j = 0;
	for (min++; min <= lp.high_id; min++)
		gain += set(min, lp.backup[j++]);
	for (min = write_pos; min < lp.low_id; min++)
		if (hist[min].state == EMPTY)
		{
			hist[min].state = SKIPPED;
			gain++;
		}
	write_pos = lp.high_id;
	ReleaseMutex(mutex);
	return gain;
}
int InputHistory::set (int pos, int v)
{
	int gain;
	if (pos < read_pos)
		return 0;
	if (read_pos + d < pos)
		return 0;
	hist[pos].value = v;
	if (hist[pos].state == EMPTY)
		gain = 1;
	else
		gain = 0;
	hist[pos].state = RECEIVED;
	return gain;
}

void update_history (unsigned int v)
{
	WaitForSingleObject(mutex_history, INFINITE);
	if (game_history_pos % HISTORY_CHUNK_SIZE == 0)
		game_history = (unsigned int *)realloc(game_history, 4 * HISTORY_CHUNK_SIZE * (1 + game_history_pos / HISTORY_CHUNK_SIZE));
	game_history[game_history_pos++] = v;
	ReleaseMutex(mutex_history);
	if (write_replay != NULL && !recording_ended)
		fwrite(&v, 1, 4, write_replay);
}

void send_local_input (int v)
{
	luna_packet lp;

	if (DEBUG) { l(); fprintf(logfile, "Local handler putting current v.\n"); u(); }
	lp = local_history.put(v);

	if (DEBUG)
	{
		l(); fprintf(logfile, "LISend: %08x %08x %02x %02x %08x ", lp.low_id, lp.high_id, PACKET_TYPE_LOW(lp), PACKET_VERSION(lp), lp.value);
		for (int i = 0; i < BACKUP_VALUES; i++)
			fprintf(logfile, "%04x ", lp.backup[i]);
		fprintf(logfile, "\n");
		u();
	}

	conmanager.send(&lp, remote_player);
}

int local_handler (int v)
{
	if (DEBUG) { l(); fprintf(logfile, "Local handler sending %08x.\n", v); u(); }
	send_local_input(v);

	if (DEBUG) { l(); fprintf(logfile, "Local handler retrieving current.\n"); u(); }
	v = local_history.get(NULL).value;
	if (DEBUG) { l(); fprintf(logfile, "Local input at: %08x.\n", v); u(); }

	return v;
}

int remote_handler ()
{
	history_entry he;
	luna_packet lp;
	int pos;

	if (DEBUG) { l(); fprintf(logfile, "Remote handler, waiting recvd.\n"); u(); }
	if (WaitForSingleObject(sem_recvd_input, SPIKE_INTERVAL) == WAIT_TIMEOUT)
	{
		int rebuffer = 0;
		if (DEBUG) { l(); fprintf(logfile, "Refilling delay buffer.\n"); u(); }
		rebuffer = local_history.get_refill();
		for (int i = 0; i < rebuffer; i++)
			send_local_input(0);
		WaitForSingleObject(sem_recvd_input, INFINITE);
	}
	if (DEBUG) { l(); fprintf(logfile, "Remote handler, done waiting recvd.\n"); u(); }

	do
	{
		he = remote_history.get(&pos);
		switch (he.state)
		{
		case RECEIVED:
			break;
		default:
			l();
			printf("Lost input %08x. Waiting for resend.\n", pos);
			if (DEBUG) { fprintf(logfile, "Input packet %08x lost.\n", pos); }
			u();
			lp = remote_history.request();
			if (PACKET_IS(lp, PACKET_TYPE_OK))
				break;
			if (DEBUG) { l(); fprintf(logfile, "Asking for resend (%08x).\n", pos); u();}
			if (DEBUG)
			{
				l(); fprintf(logfile, "ReqSend: %08x %08x %02x %02x %08x ", lp.low_id, lp.high_id, PACKET_TYPE_LOW(lp), PACKET_VERSION(lp), lp.value);
				for (int i = 0; i < BACKUP_VALUES; i++)
					fprintf(logfile, "%04x ", lp.backup[i]);
				fprintf(logfile, "\n");
				u();
			}
			conmanager.send(&lp, remote_player);
			Sleep(5);
			break;
		}
	} while (he.state == SKIPPED);
	if (DEBUG) { l(); fprintf(logfile, "Got remote input %08x at %08x.\n", he.value, pos); u(); }

	return he.value;
}

void write_net_string (char *str, FILE *hnd)
{
	int i = 0;
	while (i < NET_STRING_BUFFER - 1 && str[i])
	{
		fwrite(&(str[i]), 1, 1, hnd);
		i++;
	}
	i = 0;
	fwrite(&i, 1, 1, hnd);
}

void read_net_string (char *str, FILE *hnd)
{
	int i = 0;
	char r = '\1';
	while (i < NET_STRING_BUFFER - 1 && r)
	{
		fread(&r, 1, 1, hnd);
		str[i] = r;
		i++;
	}
	str[i] = 0;
}

void clean_filestring (char *str)
{
	for (int i = 0; str[i]; i++)
	{
		switch (str[i])
		{
		case '\\': case '/': case ':': case '*': case '?': case '"': case '<': case '>': case '|':
			str[i] = '_';
			break;
		}
	}
}

static void quit_game ()
{
	HWND game_window = ::FindWindow("KGT2KGAME", NULL);
	if(game_window) ::PostMessage(game_window, WM_CLOSE, 0, 0);
}

static void quit_game_sync()
{
	quit_game();
	if(proc) {
		DWORD ec;
		if(::GetExitCodeProcess(proc, &ec) && ec == STILL_ACTIVE && WAIT_TIMEOUT == ::WaitForSingleObject(proc, 3000)) TerminateProcess(proc, 0);
		if(ec == STILL_ACTIVE) printf("Process terminated.\n");
	}
}

FILE *create_replay_file ()
{
	FILE *hnd;
	char date[_MAX_PATH], path[_MAX_PATH], p1_name_clean[NET_STRING_BUFFER], p2_name_clean[NET_STRING_BUFFER]; // enough
	std::string filename;
	time_t ltime;
	struct tm *tm;

	ZeroMemory(date, sizeof(date));
	ZeroMemory(p1_name_clean, sizeof(p1_name_clean));
	ZeroMemory(p2_name_clean, sizeof(p2_name_clean));
	_tzset();
	time(&ltime);
	tm = localtime(&ltime);
	strftime(date, 200, "%Y-%m-%d-%H-%M-%S", tm);
	strcpy(p1_name_clean, p1_name);
	strcpy(p2_name_clean, p2_name);
	clean_filestring(p1_name_clean);
	clean_filestring(p2_name_clean);
	filename = std::string(date) + ((p1_name[0] && p2_name[0]) ? (std::string("-") + std::string(p1_name_clean) + std::string("-vs-") + std::string(p2_name_clean)) : std::string("")) + std::string(".rpy");
	strcpy(path, replays_dir);
	strcat(path, "\\");
	strcat(path, filename.c_str());
	printf("Saving replay: %s\n", filename.c_str());
	hnd = fopen(path, "wb");
	setvbuf(hnd, NULL, _IONBF, 0);
	fwrite(REPLAY_HEADER_V3, 4, 1, hnd);
	write_net_string(p1_name, hnd);
	write_net_string(p2_name, hnd);
	fwrite(&kgt_crc, 4, 1, hnd);
	fwrite(&kgt_size, 4, 1, hnd);
	fwrite(&max_stages, 4, 1, hnd);
	write_net_string(blacklist1, hnd);
	write_net_string(blacklist2, hnd);
	write_replay = hnd;
	return hnd;
}

FILE *open_replay_file (char *filename, int *seed)
{
	FILE *hnd;
	char buf[4];
	char fin_filename[_MAX_PATH];
	DWORD crc;
	long size;
	int stages;

	ZeroMemory(fin_filename, _MAX_PATH);
	if (strlen(filename) > 1 && filename[1] == ':')
		strcpy(fin_filename, filename);
	else
	{
		strcpy(fin_filename, replays_dir);
		strcat(fin_filename, "\\");
		strcat(fin_filename, filename);
	}
	l(); printf("Playing replay: %s\n", fin_filename); u();
	if ((hnd = fopen(fin_filename, "rb")) == NULL)
	{
		l(); printf("Cannot open replay file.\n"); u();
		return NULL;
	}
	fread(buf, 4, 1, hnd);
	if (!(!strncmp(buf, REPLAY_HEADER, 4) || !strncmp(buf, REPLAY_HEADER_V2, 4) || !strncmp(buf, REPLAY_HEADER_V3, 4)))
	{
		l(); printf("Invalid replay header.\n"); u();
		return NULL;
	}
	stagemanager.init(set_max_stages);
	if (!strncmp(buf, REPLAY_HEADER_V2, 4) || !strncmp(buf, REPLAY_HEADER_V3, 4))
	{
		read_net_string(p1_name, hnd);
		read_net_string(p2_name, hnd);
	}
	if (!strncmp(buf, REPLAY_HEADER_V3, 4))
	{
		fread(&crc, 4, 1, hnd);
		fread(&size, 4, 1, hnd);
		fread(&stages, 4, 1, hnd);
		if (kgt_crc != crc || kgt_size != size)
		{
			l(); printf("Incompatible game version.\nReplay:    CRC=%08X, Size=%u\nAvailable: CRC=%08X, Size=%u\n", crc, size, kgt_crc, kgt_size); u();
			return NULL;
		}
		if (max_stages < stages)
		{
			l(); printf("Less stages available (%u) than required by replay (%u).\n", max_stages, stages); u();
			return NULL;
		}
		max_stages = stages;
		stagemanager.init(max_stages);
		read_net_string(blacklist1, hnd);
		read_net_string(blacklist2, hnd);
		stagemanager.blacklist(blacklist1);
		stagemanager.blacklist(blacklist2);
		if (!stagemanager.ok())
		{
			l(); printf("After applying blacklists, no stages (of %u) are left.\nBlacklist 1: %s\nBlacklist 2: %s\n", max_stages, blacklist1, blacklist2); u();
			return NULL;
		}
	}
	fread(seed, 4, 1, hnd);
	if (DEBUG) { l(); fprintf(logfile, "Read replay seed %08x.\n", *seed); u(); }
	return hnd;
}

bool replay_input_handler (void *address, HANDLE proc_thread, FILE *replay, int record_replay, int network, int single_player, int spectator)
{
	CONTEXT c;

	ZeroMemory(&c, sizeof(c));
	c.ContextFlags = CONTEXT_INTEGER;
	GetThreadContext(proc_thread, &c);

	if (spectator)
	{
		if (DEBUG) { l(); fprintf(logfile, "Waiting for spec recvd.\n"); u(); }
		if (WaitForSingleObject(sem_recvd_input, 0) == WAIT_TIMEOUT)
		{
			Sleep(JUMBO_SLEEP);
			WaitForSingleObject(sem_recvd_input, INFINITE);
		}
		if (DEBUG) { l(); fprintf(logfile, "Done waiting recvd. Waiting for spec history.\n"); u(); }
		WaitForSingleObject(mutex_history, INFINITE);
		c.Eax = game_history[spec_pos++];
		if (DEBUG) { l(); fprintf(logfile, "Got spec input %08x at %08x.\n", c.Eax, spec_pos-1); u(); }
		ReleaseMutex(mutex_history);
		SetThreadContext(proc_thread, &c);
		if (DEBUG) { l(); fprintf(logfile, "Set spec input.\n"); u(); }
		FlushInstructionCache(proc, NULL, 0); // shouldn't hurt
	}
	else
	{
		if (network)
		{
			if (local_p == single_player)
				c.Eax = local_handler(c.Eax);
			else
				c.Eax = remote_handler();
		}
		update_history(c.Eax);
	}

	if (replay != NULL)
	{
		if (record_replay == -1) // playback
		{
			if (!feof(replay) && fread(&(c.Eax), 1, 4, replay) == 4)
			{
				if (DEBUG) { l(); fprintf(logfile, "Read input %08x at %08x.\n", c.Eax, address); u(); }
				SetThreadContext(proc_thread, &c);
				FlushInstructionCache(proc, NULL, 0); // shouldn't hurt
			}
			else {
				l(); printf("Replay ended.\n"); u();
				ReleaseSemaphore(sem_recvd_input, 2, NULL);
				return true;
			}
		}
	}
	return false;
}

void set_caption (void *p);
void send_foreground ();
void spec_handshake (unsigned long peer);
enum SmallTaskMessage {
	STM_SETCAPTION = WM_USER,
	STM_SENDFOREGROUND,
	STM_SPECHANDSHAKE
};
DWORD WINAPI small_task_thread_proc(LPVOID)
{
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		switch(msg.message) {
		case STM_SETCAPTION:
			set_caption((char *)msg.wParam);
			break;
		case STM_SENDFOREGROUND:
			send_foreground();
			break;
		case STM_SPECHANDSHAKE:
			spec_handshake((unsigned long)msg.wParam);
			break;
		}
	}
	return 0;
}

void set_caption (void *p)
{
	HWND game_window = FindWindow("KGT2KGAME", NULL);
	SetWindowText(game_window, (char *)p);
}

void send_foreground ()
{
	HWND game_window = FindWindow("KGT2KGAME", NULL);
	SetWindowPos(game_window, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
}

int replay_control (int init)
{
	static DWORD game_speed = 0xffffffff;
	static DWORD default_speed = 10;
	static DWORD current;
	static bool button_edges[4] = {false, false, false, false};
	DWORD tmp;
	BYTE keystates[256];
	
	if (init == 1)
	{
		game_speed = 0xffffffff;
		default_speed = 10;
		tmp = 10;
		button_edges[0] = false;
		button_edges[1] = false;
		button_edges[2] = false;
		button_edges[3] = false;
		return 0xfffffffe;
	}

	if (game_speed == 0xffffffff)
	{
		ReadProcessMemory(proc, (void *)GAME_SPEED, &game_speed, 4, NULL);
		default_speed = game_speed;
		current = game_speed;
	}

	ReadProcessMemory(proc, (void *)KEYSTATES, &keystates, sizeof(keystates), NULL);

	// pause toggle button is Q and button_edges[0]
	if (keystates[81] & 0x80)
		button_edges[0] = true;
	else
	{
		if (button_edges[0])
		{
			ReadProcessMemory(proc, (void *)GAME_SPEED, &tmp, 4, NULL);
			if (tmp > 0xf0000000)
				tmp = game_speed;
			else
				tmp = 0xffffffff;
			WriteProcessMemory(proc, (void *)GAME_SPEED, &tmp, 4, NULL);
			FlushInstructionCache(proc, NULL, 0);
			current = tmp;
		}
		button_edges[0] = false;
	}
	
	// normal playback/speed reset button is W and button_edges[1]
	if (keystates[87] & 0x80)
		button_edges[1] = true;
	else
	{
		if (button_edges[1])
		{
			game_speed = default_speed;
			WriteProcessMemory(proc, (void *)GAME_SPEED, &game_speed, 4, NULL);
			FlushInstructionCache(proc, NULL, 0);
			current = game_speed;
		}
		button_edges[1] = false;
	}

	// slowdown button is E and button_edges[2]
	if (keystates[69] & 0x80)
		button_edges[2] = true;
	else
	{
		if (button_edges[2])
		{
			ReadProcessMemory(proc, (void *)GAME_SPEED, &tmp, 4, NULL);
			if (tmp <= 0xffffffff - SPEED_MOD)
				tmp += SPEED_MOD;
			else
				tmp = 0xffffffff;
			WriteProcessMemory(proc, (void *)GAME_SPEED, &tmp, 4, NULL);
			if (tmp < 0xf0000000)
				game_speed = tmp;
			FlushInstructionCache(proc, NULL, 0);
			if (tmp > 0xf0000000)
				current = 0xffffffff;
			else
				current = tmp;
		}
		button_edges[2] = false;
	}

	// speedup button is R and button_edges[3]
	if (keystates[82] & 0x80)
		button_edges[3] = true;
	else
	{
		if (button_edges[3])
		{
			ReadProcessMemory(proc, (void *)GAME_SPEED, &tmp, 4, NULL);
			if (tmp >= SPEED_MOD + 1)
				tmp -= SPEED_MOD;
			else
				tmp = 1;
			if (tmp < 0xf0000000)
				game_speed = tmp;
			WriteProcessMemory(proc, (void *)GAME_SPEED, &tmp, 4, NULL);
			FlushInstructionCache(proc, NULL, 0);
			if (tmp > 0xf0000000)
				current = 0xffffffff;
			else
				current = tmp;
		}
		button_edges[3] = false;
	}

	return current;
}


int run_game (int seed, int network, int record_replay, char *filename, int spectator)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DEBUG_EVENT de;
	CONTEXT c, saved_context;
	DWORD state;
	HANDLE proc_thread;
	DWORD proc_thread_id;
	FILE *replay = NULL;
	int dlls = 0;
	int remote_p = local_p ? 0 : 1;
	int single_player = 0;
	char path[_MAX_PATH*2];
	char *debugstring;
	unsigned char nop = 0x90;
	int zero = 0;
	void *address;
	DWORD frames = 0, now, last_sec = 0, inputs = 0, waits = 0;
	char title_base[1024], window_title[1280];
	DWORD speed_factor = 0xfffffffe;
	HANDLE snapshot;
	char speed_buf[34];
	DWORD profile_time = 0, profile_addr = 0;
	// stats gathering vars
	char int3 = '\xcc';
	int i;
	char stage_name[GAME_STRING_BUFFER], p1_char[GAME_STRING_BUFFER], p2_char[GAME_STRING_BUFFER];
	DWORD stage_num, p1_char_num, p2_char_num;

	rnd_seed = seed;
	SetEvent(event_running);

	if (record_replay == 1)
		replay = create_replay_file();
	else if (record_replay == -1)
	{
		replay = open_replay_file(filename, &seed);
		speed_factor = replay_control(1);
	}

	if (record_replay && replay == NULL)
	{
		printf("Cannot open replay file. Aborting.\n");
		return 0;
	}

	if (DEBUG){ l(); fprintf(logfile, "Seed: %08x\n", seed); u(); }

	ZeroMemory(&title_base, sizeof(title_base));
	ZeroMemory(&window_title, sizeof(window_title));
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	if(strlen(game_exe) > 1 && game_exe[1] == ':') {
		strcpy(path, game_exe);
	}
	else {
		strcpy(path, dir_prefix);
		strcat(path, "\\");
		strcat(path, game_exe);
	}
	l(); printf("Running: %s\n", path); u();

	char prev_cd[_MAX_PATH];
	_getcwd(prev_cd, _MAX_PATH);
	{
		char _path[_MAX_PATH];
		strcpy(_path, path);
		for (i = strlen(_path) - 1; i >= 0; i--)
			if (_path[i] == '\\' || _path[i] == '/')
			{
				_path[i] = 0;
				break;
			}
		_chdir(_path);
	}
	if(proc) {
		CloseHandle(proc);
		proc = NULL;
	}
	if (!CreateProcess(path, NULL, NULL, NULL, FALSE, DEBUG_PROCESS, NULL, NULL, &si, &pi))
	{
		l(); printf("Couldn't find game exe. Trying alternative.\n"); u();
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		strcpy(path, dir_prefix);
		strcat(path, "\\");
		strcat(path, ALTGAME);
		l(); printf("Running: %s\n", path); u();
		if (!CreateProcess(path, NULL, NULL, NULL, FALSE, DEBUG_PROCESS, NULL, NULL, &si, &pi))
		{
			l(); printf("Couldn't find alternative game exe. Giving up.\n"); u();
			return 0;
		}
	}
#ifdef _MSC_VER
	proc = pi.hProcess;
#else
	proc = OpenProcess(PROCESS_ALL_ACCESS, false, pi.dwProcessId);
#endif

	if (!spectator)
		update_history(seed); // first entry of the game history is the seed

	if (record_replay != -1 && !spectator)
		session.start(p1_name, p2_name, local_p, network ? remote_player : 0);
	else
		session.set_names(p1_name, p2_name);
	stage_name[GAME_STRING_LENGTH] = 0;
	p1_char[GAME_STRING_LENGTH] = 0;
	p2_char[GAME_STRING_LENGTH] = 0;

	if (lobby_flag)
		lobby.run(p1_char, p2_char);

	while (WaitForDebugEvent(&de, INFINITE))
	{
		state = DBG_CONTINUE;
		switch (de.dwDebugEventCode)
		{
		case EXCEPTION_DEBUG_EVENT:
			if (DEBUG) { l(); fprintf(logfile, "EXCEPTION_DEBUG_EVENT: %08x@%08x\n", de.u.Exception.ExceptionRecord.ExceptionCode, de.u.Exception.ExceptionRecord.ExceptionAddress); u(); }
			if (de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
			{
				l(); printf("Access Violation %08x at %08x: %s@%08x\n", de.u.Exception.ExceptionRecord.ExceptionCode, de.u.Exception.ExceptionRecord.ExceptionAddress, de.u.Exception.ExceptionRecord.ExceptionInformation[0] ? "read" : "write", de.u.Exception.ExceptionRecord.ExceptionInformation[1]);
				if (DEBUG) fprintf(logfile, "Access Violation %08x at %08x: %s@%08x\n", de.u.Exception.ExceptionRecord.ExceptionCode, de.u.Exception.ExceptionRecord.ExceptionAddress, de.u.Exception.ExceptionRecord.ExceptionInformation[0] ? "read" : "write", de.u.Exception.ExceptionRecord.ExceptionInformation[1]); u();
				snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, de.dwProcessId);

				MODULEENTRY32 ep;
				DWORD e_addr = (DWORD)de.u.Exception.ExceptionRecord.ExceptionAddress;
				ep.dwSize = sizeof(ep);
				char module[MAX_MODULE_NAME32 + 1];
				DWORD base, size;
				module[0] = 0;

				if (Module32First(snapshot, &ep))
				{
					if ((DWORD)ep.modBaseAddr <= e_addr && (DWORD)ep.modBaseAddr + ep.modBaseSize >= e_addr)
					{
						strcpy(module, ep.szModule);
						base = (DWORD)ep.modBaseAddr;
						size = ep.modBaseSize;
					}
					while (Module32Next(snapshot, &ep))
					{
						if ((DWORD)ep.modBaseAddr <= e_addr && (DWORD)ep.modBaseAddr + ep.modBaseSize >= e_addr)
						{
							strcpy(module, ep.szModule);
							base = (DWORD)ep.modBaseAddr;
							size = ep.modBaseSize;
						}
					}
				}
				if (strlen(module))
				{
					l(); printf("Module: %s (%08x + %08x).\n", module, base, size);
					if (DEBUG) fprintf(logfile, "Module: %s (%08x + %08x).\n", module, base, size); u();
					// follow PE header in module to get approx function name
					// http://win32assembly.online.fr/pe-tut7.html
				}

				CloseHandle(snapshot);
				state = DBG_EXCEPTION_NOT_HANDLED;
			}
			if (de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
			{
				if (de.dwThreadId != proc_thread_id)
					break;
				address = de.u.Exception.ExceptionRecord.ExceptionAddress;

				if (BP_PROFILE)
				{
					if (profile_addr)
					{
						fwrite(&profile_time, 4, 1, bpprof);
						fwrite(&profile_addr, 4, 1, bpprof);
					}
					profile_addr = (DWORD)address;
					timeBeginPeriod(1);
					profile_time = timeGetTime();
				}

				switch ((unsigned int)address)
				{
				case CONTROL_CHANGE:
					ReadProcessMemory(proc, (void *)P1_KBD_CONTROLS, kbd_control_buffer, sizeof(kbd_control_buffer), NULL); // get p1 keyboard controls
					WriteProcessMemory(proc, (void *)P2_KBD_CONTROLS, kbd_control_buffer, sizeof(kbd_control_buffer), NULL); // set p2 keyboard controls as those of p1
					ReadProcessMemory(proc, (void *)P1_JOY_CONTROLS, joy_control_buffer, sizeof(joy_control_buffer), NULL); // get p1 joystick controls
					WriteProcessMemory(proc, (void *)P2_JOY_CONTROLS, joy_control_buffer, sizeof(joy_control_buffer), NULL); // set p2 joystick controls as those of p1
					WriteProcessMemory(proc, (void *)STICK_SELECTION, stick_selection, sizeof(stick_selection), NULL); // always read input from first joystick

					// disable control config writeback
					WriteProcessMemory(proc, (void *)KBD_WRITEBACK, kbd_writeback, sizeof(kbd_writeback), NULL); // don't write changes to keyboard control config
					WriteProcessMemory(proc, (void *)JOY_WRITEBACK, joy_writeback, sizeof(joy_writeback), NULL); // don't write changes to joystick control config

					// disable control change installation
					WriteProcessMemory(proc, (void *)CONTROL_CHANGE, &nop, 1, NULL);
					break;


				case STAGE_SELECT_BREAK:
					c.ContextFlags = CONTEXT_INTEGER;
					GetThreadContext(proc_thread, &c);
					c.Eax = stagemanager.map(c.Eax);
					SetThreadContext(proc_thread, &c);

					FlushInstructionCache(proc, NULL, 0); // shouldn't hurt
					break;


				case LOCAL_P_BREAK:
					inputs++;
					c.ContextFlags = CONTEXT_INTEGER;
					GetThreadContext(proc_thread, &c);
					if (DEBUG) { l(); fprintf(logfile, "Got LPB (%08x).\n", c.Eax); u(); }
					c.Eax = local_handler(c.Eax);
					update_history(c.Eax);
					SetThreadContext(proc_thread, &c);

					FlushInstructionCache(proc, NULL, 0); // shouldn't hurt
					break;


				case REMOTE_P_BREAK:
					inputs++;
					if (DEBUG) { l(); fprintf(logfile, "Got RPB.\n"); u(); }
					c.ContextFlags = CONTEXT_INTEGER;
					GetThreadContext(proc_thread, &c);
					c.Eax = remote_handler();
					update_history(c.Eax);
					SetThreadContext(proc_thread, &c);
					if (DEBUG) { l(); fprintf(logfile, "Put RPB (%08x).\n", c.Eax); u(); }

					FlushInstructionCache(proc, NULL, 0); // shouldn't hurt
					break;


				case REPLAY_P1_BREAK: // we can use the same handler in both cases, since we do the same thing
				case REPLAY_P2_BREAK:
					if (record_replay == -1)
						speed_factor = replay_control(0);
					inputs++;
					if (DEBUG) { l(); fprintf(logfile, "Entering input handler.\n"); u(); }
					if (record_replay || spectator)
					{
						if(replay_input_handler(address, proc_thread, replay, record_replay, 0, 0, spectator))
						{
							quit_game();
							fclose(replay);
							replay = NULL;
						}
					}
					if (DEBUG) { l(); fprintf(logfile, "Leaving input handler.\n"); u(); }
					break;


				case SINGLE_BREAK_CONTROL:
					c.ContextFlags = CONTEXT_INTEGER;
					GetThreadContext(proc_thread, &c);
					single_player = c.Ecx & 1;
					break;


				case SINGLE_BREAK_INPUT:
					inputs += 2;
					if (record_replay || spectator)
					{
						if(replay_input_handler(address, proc_thread, replay, record_replay, network, single_player, spectator))
						{
							quit_game();
							fclose(replay);
							replay = NULL;
						}
						break;
					}

					c.ContextFlags = CONTEXT_INTEGER;
					GetThreadContext(proc_thread, &c);

					// while in single player mode, we are basically just streaming one-sidedly
					// the keep_alive thread should keep us going in the other direction
					// this should work, in theory
					if (local_p == single_player)
						c.Eax = local_handler(c.Eax);
					else
						c.Eax = remote_handler();

					update_history(c.Eax);
					SetThreadContext(proc_thread, &c);
					FlushInstructionCache(proc, NULL, 0); // shouldn't hurt
					break;


				case TITLE_BREAK:
					if (DEBUG) { l(); fprintf(logfile, "Entering title handler.\n"); u(); }
					c.ContextFlags = CONTEXT_FULL;
					GetThreadContext(proc_thread, &c);
					c.Eip--;
					c.EFlags |= 0x0100; // single step for reinstallation of breakpoint
					SetThreadContext(proc_thread, &c);
					ZeroMemory(title_base, sizeof(title_base));
					ReadProcessMemory(proc, (void *)c.Edx, title_base, sizeof(title_base) / 2, NULL); // read into title buffer, actually read too much, but never mind
					title_base[sizeof(title_base)-1] = 0;
					if (display_names && strlen(p1_name) && strlen(p2_name))
					{
						strcat(title_base, " - ");
						strcat(title_base, p1_name);
						strcat(title_base, " vs ");
						strcat(title_base, p2_name);
						strcpy(window_title, title_base);
						//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)set_caption, (void *)window_title, 0, NULL);
						PostThreadMessage(small_task_thread_id, STM_SETCAPTION, (WPARAM)window_title, 0);
					}
					WriteProcessMemory(proc, (void *)TITLE_BREAK, title_break_bak, sizeof(title_break_bak), NULL); // reset old code
					FlushInstructionCache(proc, NULL, 0); // shouldn't hurt
					if (DEBUG) { l(); fprintf(logfile, "Leaving title handler.\n"); u(); }
					break;


				case FRAME_BREAK:
					frames++;
					if (DEBUG) { l(); fprintf(logfile, "Entering frame handler.\n"); u(); }
					if (!title_base[0])
						break;
					if (DEBUG) { l(); fprintf(logfile, "For real.\n"); u(); }
					now = GetTickCount();
					if (now - last_sec >= 1000)
					{
						if (DEBUG) { l(); fprintf(logfile, "Second started.\n"); u(); }
						strcpy(window_title, title_base);
						if (display_framerate)
						{
							strcat(window_title, " - FPS: ");
							sprintf(window_title + strlen(window_title), "%d", frames);
						}
						if (display_inputrate)
						{
							strcat(window_title, " - Inputs: ");
							sprintf(window_title + strlen(window_title), "%d", inputs / 2); // counted inputs for both players
						}
						if (record_replay == -1 && speed_factor != 0xfffffffe)
						{
							strcat(window_title, " - Speed: ");
							if (speed_factor > 0xf0000000)
								strcat(window_title, "Paused");
							else
							{
								strcat(window_title, ltoa(speed_factor, speed_buf, 10));
								strcat(window_title, "ms delay");
							}
						}
						if (display_framerate || display_inputrate || record_replay == -1)
							//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)set_caption, (void *)window_title, 0, NULL);
							PostThreadMessage(small_task_thread_id, STM_SETCAPTION, (WPARAM)window_title, 0);
						frames = 0;
						inputs = 0;
						last_sec = now;
						if (DEBUG) { l(); fprintf(logfile, "Done.\n"); u(); }
					}
					if (DEBUG) { l(); fprintf(logfile, "Leaving frame handler.\n"); u(); }
					break;

				case EXTRA_INPUT_WAIT_BREAK:
					waits = (waits + 1) % WAITSKIP;
					if (!waits)
					{
						c.ContextFlags = CONTEXT_FULL;
						GetThreadContext(proc_thread, &c);
						memcpy(&saved_context, &c, sizeof(c));
						saved_context.Eip = EXTRA_INPUT_BACK;
						c.Eip = EXTRA_INPUT_GO;
						SetThreadContext(proc_thread, &c);
					}
					break;
				case EXTRA_INPUT_BREAK:
					SetThreadContext(proc_thread, &saved_context);
					replay_control(0);
					break;

				case LOAD_STAGE_BREAK:
					c.ContextFlags = CONTEXT_FULL;
					GetThreadContext(proc_thread, &c);
					c.Eip--;
					c.EFlags |= 0x0100; // single step for reinstallation of breakpoint
					SetThreadContext(proc_thread, &c);
					WriteProcessMemory(proc, address, &(simple_int3_backups[(DWORD)address]), 1, NULL);
					ReadProcessMemory(proc, (void *)P1_CHAR_NUM, &p1_char_num, sizeof(p1_char_num), NULL);
					ReadProcessMemory(proc, (void *)P2_CHAR_NUM, &p2_char_num, sizeof(p2_char_num), NULL);
					ReadProcessMemory(proc, (void *)STAGE_NUM, &stage_num, sizeof(stage_num), NULL);
					ReadProcessMemory(proc, (void *)(CHAR_NAMES + GAME_STRING_LENGTH * p1_char_num), &p1_char, sizeof(p1_char), NULL);
					ReadProcessMemory(proc, (void *)(CHAR_NAMES + GAME_STRING_LENGTH * p2_char_num), &p2_char, sizeof(p2_char), NULL);
					ReadProcessMemory(proc, (void *)(STAGE_NAMES + GAME_STRING_LENGTH * stage_num), &stage_name, sizeof(stage_name), NULL);
					session.match_start(stage_name, p1_char, p2_char);
					simple_int3_reset.push_back((DWORD)address);
					break;

				case DOUBLE_KO_BREAK:
					c.ContextFlags = CONTEXT_FULL;
					GetThreadContext(proc_thread, &c);
					c.Eip--;
					c.EFlags |= 0x0100; // single step for reinstallation of breakpoint
					SetThreadContext(proc_thread, &c);
					WriteProcessMemory(proc, address, &(simple_int3_backups[(DWORD)address]), 1, NULL);
					session.double_ko();
					simple_int3_reset.push_back((DWORD)address);
					break;

				case DRAW_BREAK:
					c.ContextFlags = CONTEXT_FULL;
					GetThreadContext(proc_thread, &c);
					c.Eip--;
					c.EFlags |= 0x0100; // single step for reinstallation of breakpoint
					SetThreadContext(proc_thread, &c);
					WriteProcessMemory(proc, address, &(simple_int3_backups[(DWORD)address]), 1, NULL);
					session.draw();
					simple_int3_reset.push_back((DWORD)address);
					break;

				case P1_WIN_BREAK:
					c.ContextFlags = CONTEXT_FULL;
					GetThreadContext(proc_thread, &c);
					c.Eip--;
					c.EFlags |= 0x0100; // single step for reinstallation of breakpoint
					SetThreadContext(proc_thread, &c);
					WriteProcessMemory(proc, address, &(simple_int3_backups[(DWORD)address]), 1, NULL);
					session.p1_win();
					simple_int3_reset.push_back((DWORD)address);
					break;

				case P2_WIN_BREAK:
					c.ContextFlags = CONTEXT_FULL;
					GetThreadContext(proc_thread, &c);
					c.Eip--;
					c.EFlags |= 0x0100; // single step for reinstallation of breakpoint
					SetThreadContext(proc_thread, &c);
					WriteProcessMemory(proc, address, &(simple_int3_backups[(DWORD)address]), 1, NULL);
					session.p2_win();
					simple_int3_reset.push_back((DWORD)address);
					break;

				default:
					// this breakpoint wasn't set by LunaPort, replace it with a nop
					// this might actually solve some speed problems
					c.ContextFlags = CONTEXT_FULL;
					GetThreadContext(proc_thread, &c);
					c.Eip--;
					WriteProcessMemory(proc, (void *)c.Eip, &nop, 1, NULL);
					FlushInstructionCache(proc, NULL, 0);
					break;
				}

				if (BP_PROFILE)
				{
					profile_time = timeGetTime() - profile_time;
					timeEndPeriod(1);
				}
			}
			else if (de.u.Exception.ExceptionRecord.ExceptionCode == STATUS_SINGLE_STEP)
			{
				if (DEBUG) { l(); fprintf(logfile, "Entering single step handler.\n"); u(); }
				WriteProcessMemory(proc, (void *)TITLE_BREAK, title_break, sizeof(title_break), NULL); // reset breakpoint

				// reset enqueued breakpoints
				for (std::list<DWORD>::iterator r_i = simple_int3_reset.begin(); r_i != simple_int3_reset.end(); )
				{
					WriteProcessMemory(proc, (void *)*r_i, &int3, 1, NULL);
					r_i = simple_int3_reset.erase(r_i);
				}

				FlushInstructionCache(proc, NULL, 0); // shouldn't hurt
				if (DEBUG) { l(); fprintf(logfile, "Leaving single step handler.\n"); u(); }
			}
			break;


		case CREATE_THREAD_DEBUG_EVENT:
			if (DEBUG) { l(); fprintf(logfile, "CREATE_THREAD_DEBUG_EVENT\n"); u(); }
			break;
		case CREATE_PROCESS_DEBUG_EVENT:
			proc_thread = de.u.CreateProcessInfo.hThread;
			proc_thread_id = de.dwThreadId;
			if (DEBUG) { l(); fprintf(logfile, "CREATE_PROCESS_DEBUG_EVENT\n"); u(); }
			break;
		case EXIT_THREAD_DEBUG_EVENT:
			if (DEBUG) { l(); fprintf(logfile, "EXIT_THREAD_DEBUG_EVENT\n"); u(); }
			break;
		case EXIT_PROCESS_DEBUG_EVENT:
			if (DEBUG) { l(); fprintf(logfile, "EXIT_PROCESS_DEBUG_EVENT\n"); u(); }
			goto DONE;
			break;


		case LOAD_DLL_DEBUG_EVENT:
			dlls++;
			if (DEBUG) { l(); fprintf(logfile, "LOAD_DLL_DEBUG_EVENT(%d)\n", dlls); u(); }
			if (dlls == 17) // all we need is loaded into mem now
			{
				WriteProcessMemory(proc, (void *)RANDOM_SEED, &seed, 4, NULL); // initialize seed (they never even call srand)
				//WriteProcessMemory(proc, (void *)0x76af4e5d, &zero, 4, NULL); // provoke access violation for testing purposes

				WriteProcessMemory(proc, (void *)STAGE_SELECT, stage_select, sizeof(stage_select), NULL); // set stage selection code
				WriteProcessMemory(proc, (void *)STAGE_SELECT_FUNC, stage_select_func, sizeof(stage_select_func), NULL); // set stage selection code

				WriteProcessMemory(proc, (void *)LOCAL_P_FUNC, local_p_func, sizeof(local_p_func), NULL); // local player input grabber
				WriteProcessMemory(proc, (void *)LOCAL_P_JMPBACK, local_p_jmpback[local_p], sizeof(local_p_jmpback[local_p]), NULL); // local player input grabber jump back

				WriteProcessMemory(proc, (void *)REMOTE_P_FUNC, remote_p_func, sizeof(remote_p_func), NULL); // remote player input grabber
				WriteProcessMemory(proc, (void *)REMOTE_P_JMPBACK, remote_p_jmpback[remote_p], sizeof(remote_p_jmpback[remote_p]), NULL); // remote player input grabber jump back

				if (display_framerate || display_inputrate)
				{
					WriteProcessMemory(proc, (void *)FRAME_FUNC, frame_func, sizeof(frame_func), NULL); // frame rate counter hook
					WriteProcessMemory(proc, (void *)FRAME_JUMP, frame_jump, sizeof(frame_jump), NULL); // frame rate counter jump
					WriteProcessMemory(proc, (void *)TITLE_BREAK, title_break, sizeof(title_break), NULL); // install breakpoint where window title is set
				}

				if (network || record_replay || spectator)
				{
					WriteProcessMemory(proc, (void *)SINGLE_FUNC, single_func, sizeof(single_func), NULL); // single player input grabber
					WriteProcessMemory(proc, (void *)SINGLE_JUMP, single_jump, sizeof(single_jump), NULL); // single player input grabber jump
				}

				if (network)
				{
					WriteProcessMemory(proc, (void *)P1_JUMP, p1_jump[local_p], sizeof(p1_jump[local_p]), NULL); // hook up input for p1
					WriteProcessMemory(proc, (void *)P2_JUMP, p2_jump[remote_p], sizeof(p2_jump[remote_p]), NULL); // hook up input for p2
				}
				else /*if (record_replay || spectator)*/ // local replay recording or replay playback
				{
					WriteProcessMemory(proc, (void *)REPLAY_HOOKS, replay_hooks, sizeof(replay_hooks), NULL); // hooks for replay recording
					WriteProcessMemory(proc, (void *)REPLAY_P1_JUMP, replay_p1_jump, sizeof(replay_p1_jump), NULL); // install for p1
					WriteProcessMemory(proc, (void *)REPLAY_P2_JUMP, replay_p2_jump, sizeof(replay_p2_jump), NULL); // install for p2
				}

				// set simple int3/single-step breakpoints and save original data
				i = 0;
				simple_int3_backups.clear();
				simple_int3_reset.clear();
				while (simple_int3[i])
				{
					char read_buf;
					ReadProcessMemory(proc, (void *)simple_int3[i], &read_buf, 1, NULL);
					simple_int3_backups[simple_int3[i]] = read_buf;
					WriteProcessMemory(proc, (void *)simple_int3[i], &int3, 1, NULL);
					i++;
				}

				FlushInstructionCache(proc, NULL, 0); // shouldn't hurt

				// now we are basically done with debug stuff, all necessary code has been injected
			}
			if (dlls == 35)
			{
				if (record_replay == -1)
				{
					ReadProcessMemory(proc, (void *)IMPORT_KBD_CALL, import_kbd_call, sizeof(import_kbd_call), NULL); // read GetKeyboardState call
					WriteProcessMemory(proc, (void *)EXTRA_INPUT_JUMP, extra_input_jump, sizeof(extra_input_jump), NULL); // jump injection for replay playback input grabbing
					WriteProcessMemory(proc, (void *)EXTRA_INPUT_FUNC, extra_input_func, sizeof(extra_input_func), NULL); // function injection for replay playback input grabbing
					WriteProcessMemory(proc, (void *)EXPORT_KBD_CALL, import_kbd_call, sizeof(import_kbd_call), NULL); // write GetKeyboardState call
					FlushInstructionCache(proc, NULL, 0);
				}

				//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)send_foreground, (void *)NULL, 0, NULL);
				PostThreadMessage(small_task_thread_id, STM_SENDFOREGROUND, 0, 0);
			}
			break;


		case UNLOAD_DLL_DEBUG_EVENT:
			if (DEBUG) { l(); fprintf(logfile, "UNLOAD_DLL_DEBUG_EVENT\n"); u(); }
			break;
		case OUTPUT_DEBUG_STRING_EVENT:
			if (DEBUG) {
				debugstring = (char*)malloc(de.u.DebugString.nDebugStringLength);
				ReadProcessMemory(proc, de.u.DebugString.lpDebugStringData, debugstring, de.u.DebugString.nDebugStringLength, NULL);
				l(); fprintf(logfile, "OUTPUT_DEBUG_STRING_EVENT(%s)\n", debugstring); u();
				free(debugstring);
			}
			break;
		case RIP_EVENT:
			if (DEBUG) { l(); fprintf(logfile, "RIP_EVENT\n"); u(); }
			break;
		}

		ContinueDebugEvent(de.dwProcessId, de.dwThreadId, state);
	}

DONE:
	l(); printf("Done\n"); u();
	if (replay != NULL)
	{
		fclose(replay);
		replay = NULL;
	}
	session.end();
	quit_game_sync();
	_chdir(prev_cd);

	return 0;
}

int create_sems ()
{
	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&sa, sizeof(sa));
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof(sa);

	sem_recvd_input = CreateSemaphore(&sa, 0, LONG_MAX, NULL);
	sem_ping_end = CreateSemaphore(&sa, 0, 1, NULL);
	mutex_history = CreateMutex(&sa, FALSE, NULL);

	return 0;
}

int close_sems ()
{
	CloseHandle(sem_recvd_input);
	CloseHandle(sem_ping_end);
	CloseHandle(mutex_history);

	return 0;
}

void set_delay (int d)
{
	delay = d;

	// create/initialize semaphores
	if (delay) ReleaseSemaphore(sem_recvd_input, delay, NULL);

	// initialize input history tables
	local_history.init(d, SENT);
	remote_history.init(d, RECEIVED);
}

int get_ping (unsigned long peer)
{
	TIMECAPS tc;
	DWORD start, end;
	luna_packet lp;
	simple_luna_packet(&lp, PACKET_TYPE_PING, 0);

	timeGetDevCaps(&tc, sizeof(tc));
	timeBeginPeriod(tc.wPeriodMin);
	start = timeGetTime();

	if (!conmanager.send(&lp, peer))
		return -1;

	end = timeGetTime();
	timeEndPeriod(tc.wPeriodMin);
	return (end - start);
}

void serve_spectators (void *p)
{
	DWORD now;

	if (allow_spectators)
		for (;;)
		{
			Sleep(JUMBO_SLEEP);
			now = GetTickCount();

			WaitForSingleObject(mutex_history, INFINITE);
			for (spec_map::iterator i = spectators.begin(); i != spectators.end(); )
			{
				spectator s = i->second;
				if (s.phase == 1 && WaitForSingleObject(event_running, 0) == WAIT_TIMEOUT)
				{
					luna_packet lp;
					simple_luna_packet(&lp, PACKET_TYPE_ALIVE, 0);
					if (!conmanager.send_raw((void *)&lp, s.peer, sizeof(lp)))
					{
						l(); printf("Dropping spectator %s.\n", ip2str(s.peer));
						if (DEBUG) fprintf(logfile, "Dropping spectator %s.\n", ip2str(s.peer));
						u();
						conmanager.disconnect_nobye(s.peer);
						spectators.erase(i++);
						continue;
					}
				}
				else if (s.phase == 3 && s.position < game_history_pos - JUMBO_VALUES)
				{
					luna_jumbo_packet ljp;
					ljp.header = PACKET_HEADER(PACKET_TYPE_JUMBO, 0);
					ljp.position = s.position;
					i->second.position += JUMBO_VALUES;
					for (int j = 0; j < JUMBO_VALUES; j++)
						ljp.values[j] = game_history[s.position + j];
					if (!conmanager.send_jumbo(&ljp, s.peer))
					{
						l(); printf("Dropping spectator %s.\n", ip2str(s.peer));
						if (DEBUG) fprintf(logfile, "Dropping spectator %s.\n", ip2str(s.peer));
						u();
						conmanager.disconnect_nobye(s.peer);
						spectators.erase(i++);
						continue;
					}
				}
				i++;
			}
			ReleaseMutex(mutex_history);
		}
}

void keep_alive (unsigned long peer)
{
	luna_packet lp;
	for (;;)
	{
		lp = local_history.resend();
		if (DEBUG)
		{
			l(); fprintf(logfile, "KASend: %08x %08x %02x %02x %08x ", lp.low_id, lp.high_id, PACKET_TYPE_LOW(lp), PACKET_VERSION(lp), lp.value);
			for (int i = 0; i < BACKUP_VALUES; i++)
				fprintf(logfile, "%04x ", lp.backup[i]);
			fprintf(logfile, "\n");
			u();
		}
		conmanager.send(&lp, peer);
		Sleep(ALIVE_INTERVAL);
	}
}

void bye_spectators ()
{
	for (spec_map::iterator i = spectators.begin(); i != spectators.end(); i++)
	{
		spectator s = i->second;
		conmanager.disconnect(s.peer);
	}
}

void spec_keep_alive (unsigned long peer)
{
	luna_packet lp;
	for (;;)
	{
		if (WaitForSingleObject(event_running, 0) != WAIT_OBJECT_0)
		{
			simple_luna_packet(&lp, PACKET_TYPE_IGNOR, 0);
			conmanager.send_raw(&lp, peer, sizeof(luna_packet));
		}
		else
		{
			WaitForSingleObject(mutex_history, INFINITE);
			simple_luna_packet(&lp, PACKET_TYPE_SPECT, 0);
			lp.value = game_history_pos;
			conmanager.send_raw(&lp, peer, sizeof(luna_packet));
			ReleaseMutex(mutex_history);
		}
		Sleep(ALIVE_INTERVAL);
	}
}

void spec_receiver (AsyncPacket *packet)
{
	luna_jumbo_packet *ljp = (luna_jumbo_packet *)(packet->packet);
	unsigned long peer = packet->peer;
	long left = 0;

	switch (PACKET_TYPE(*ljp))
	{
	case PACKET_TYPE_TOUT:
		l();
		printf("Connection lost. No packets received for 10s.\n");
		u();
		if (proc != NULL)
		{
			remote_history.fake();
			ReleaseSemaphore(sem_recvd_input, 2, NULL);
			quit_game();
		}
		break;
	case PACKET_TYPE_ERROR:
	case PACKET_TYPE_BYE:
		if (proc != NULL)
		{
			l();
			printf("Connection closed.\n");
			u();
			remote_history.fake();
			ReleaseSemaphore(sem_recvd_input, 2, NULL);
			quit_game();
		}
		break;
	case PACKET_TYPE_JUMBO:
		WaitForSingleObject(mutex_history, INFINITE);
		if (game_history_pos != ljp->position)
		{
			ReleaseMutex(mutex_history);
			break;
		}
		ReleaseMutex(mutex_history);
		for (int i = 0; i < JUMBO_VALUES; i++)
			update_history(ljp->values[i]);
		ReleaseSemaphore(sem_recvd_input, JUMBO_VALUES, &left);
		if (DEBUG) { l(); fprintf(logfile, "Spec recvd semaphore now contains %u.\n", left); u(); }
		break;
	}
}

void peer_receiver (AsyncPacket *packet)
{
	unsigned long peer = packet->peer;
	luna_packet *lp = (luna_packet *)(packet->packet);
	luna_packet *resends;
	unsigned int old_recvd, i;
	int n;
	int gained;
	spectator s;

	switch (PACKET_TYPE(*lp))
	{
	case PACKET_TYPE_INPUT:
		if (peer != remote_player)
			break;

		gained = remote_history.put(*lp);

		if (gained)
		{
			if (DEBUG) { l(); fprintf(logfile, "Releasing semaphore recvd_input (%04x).\n", gained); u(); }
			ReleaseSemaphore(sem_recvd_input, gained, (long *)&old_recvd);
			if (DEBUG) { l(); fprintf(logfile, "Now recvd_input is %08x.\n", old_recvd); u(); }
		}
		else
			if (DEBUG) { l(); fprintf(logfile, "Semaphore recvd_input not released (%04x).\n", gained); u(); }
		break;
	case PACKET_TYPE_ERROR:
	case PACKET_TYPE_BYE:
		if (peer != remote_player)
			break;

		if (proc != NULL)
		{
			l();
			printf("Connection closed.\n");
			u();
			remote_history.fake();
			ReleaseSemaphore(sem_recvd_input, 2, NULL);
			quit_game();
		}
		break;
	case PACKET_TYPE_AGAIN:
		if (peer != remote_player)
			break;

		if (DEBUG) { l(); fprintf(logfile, "Got resend request from %08x to %08x.\n", lp->low_id, lp->high_id); u(); }
		resends = local_history.resend(lp->low_id, lp->high_id, &n);
		for (i = 0; i < n; i++)
		{
			if (DEBUG)
			{
				l(); fprintf(logfile, "AgainSend: %08x %08x %02x %02x %08x ", resends[i].low_id, resends[i].high_id, PACKET_TYPE_LOW(resends[i]), PACKET_VERSION(resends[i]), resends[i].value);
				for (int i = 0; i < BACKUP_VALUES; i++)
					fprintf(logfile, "%04x ", resends[i].backup[i]);
				fprintf(logfile, "\n");
				u();
			}

			conmanager.send(&(resends[i]), peer);
		}
		if (resends != NULL)
			free(resends);
		break;
	case PACKET_TYPE_SPECT:
		if (!spectators.count(peer))
			break;

		WaitForSingleObject(mutex_history, INFINITE);
		s = spectators[peer];
		s.position = lp->value;
		spectators[peer] = s;
		ReleaseMutex(mutex_history);
		break;
	case PACKET_TYPE_TOUT:
		if (peer != remote_player && spectators.count(peer))
		{
			l(); printf("Dropping spectator %s.\n", ip2str(peer));
			if (DEBUG) fprintf(logfile, "Dropping spectator %s.\n", ip2str(peer));
			u();

			WaitForSingleObject(mutex_history, INFINITE);
			spectators.erase(peer);
			ReleaseMutex(mutex_history);
			if (DEBUG) { l(); fprintf(logfile, "Dropped spectator %s.\n", ip2str(peer)); u(); }
		}
		else if (peer == remote_player)
		{
			l();
			printf("Connection lost. No packets received for 10s.\n");
			u();
			if (proc != NULL)
			{
				remote_history.fake();
				ReleaseSemaphore(sem_recvd_input, 2, NULL);
				quit_game();
			}
		}
		break;
	}
}

void clear_socket ()
{
	int r;
	char buf[1024];
	Sleep(500);
	do
	{
		r = recv(sock, buf, 1024, 0);
	} while (r != 0 && r != SOCKET_ERROR);
}

bool receive_string (char *buf, unsigned long peer)
{
	luna_packet lp;
	char *old_buf = buf;
	
	ZeroMemory(buf, NET_STRING_BUFFER);

	do
	{
		if (!conmanager.receive(&lp, peer))
			return false;

		if (PACKET_IS(lp, PACKET_TYPE_TEXT) && buf - old_buf <= NET_STRING_LENGTH - NET_STRING_PERPACKET)
		{
			memcpy(buf, &(lp.value), NET_STRING_PERPACKET);
			buf += MIN(lp.low_id, NET_STRING_PERPACKET);
		}
		else if (!PACKET_IS(lp, PACKET_TYPE_DONE))
			return false;
	} while (buf - old_buf <= NET_STRING_LENGTH && !PACKET_IS(lp, PACKET_TYPE_DONE));

	if (!PACKET_IS(lp, PACKET_TYPE_DONE))
		return false;

	return true;
}

bool send_string (char *buf, unsigned long peer)
{
	luna_packet lp;
	char *old_buf = buf;
	int len = MAX(0, MIN(strlen(buf), NET_STRING_LENGTH));
	int packets = (int)ceil(((double)len) / ((double)NET_STRING_PERPACKET));
	int size = NET_STRING_PERPACKET;

	ZeroMemory(&lp, sizeof(lp));
	lp.header = PACKET_HEADER(PACKET_TYPE_TEXT, 0);
	for (int i = 0; i < packets; i++)
	{
		if (i == packets - 1)
			size = len - (buf - old_buf);
		lp.low_id = size;
		memcpy(&(lp.value), buf, size);
		buf += NET_STRING_PERPACKET;
		if (!conmanager.send(&lp, peer))
			return false;
	}

	ZeroMemory(&lp, sizeof(lp));
	lp.header = PACKET_HEADER(PACKET_TYPE_DONE, 0);
	if (!conmanager.send(&lp, peer))
		return false;
	return true;
}

void read_int (int *i)
{
	char str[256];
	char *end;
	long n;
	l(); printf("[%d]> ", *i); u();
	cin.getline(str, 256, '\n');
	n = strtol(str, &end, 10);
	if (str != end)
		*i = n;
}

void spec_handshake (unsigned long peer)
{
	spectator s;
	luna_packet lp;
	char *dummy = "";
	bool ok = true;

	if (!spectators.count(peer))
	{
		l();
		printf("Spectator connecting from %s.\n", ip2str(peer));
		if (DEBUG) fprintf(logfile, "Spectator connecting from %s.\n", ip2str(peer));
		u();
	}
	else
		return;

	WaitForSingleObject(mutex_history, INFINITE);
	s.position = 1;
	s.peer = peer;
	s.phase = 0;
	spectators[peer] = s;
	ReleaseMutex(mutex_history);

	simple_luna_packet(&lp, PACKET_TYPE_KGTCH, 0);
	lp.value = kgt_crc;
	if (!conmanager.send(&lp, peer))
	{
		l();
		printf("Connection to spectator %s lost.\n", ip2str(peer));
		u();
		WaitForSingleObject(mutex_history, INFINITE);
		spectators.erase(peer);
		ReleaseMutex(mutex_history);
		conmanager.disconnect(peer);
		return;
	}
	simple_luna_packet(&lp, PACKET_TYPE_KGTLN, 0);
	lp.value = kgt_size;
	if (!conmanager.send(&lp, peer))
	{
		l();
		printf("Connection to spectator %s lost.\n", ip2str(peer));
		u();
		WaitForSingleObject(mutex_history, INFINITE);
		spectators.erase(peer);
		ReleaseMutex(mutex_history);
		conmanager.disconnect(peer);
		return;
	}

	WaitForSingleObject(mutex_history, INFINITE);
	s.phase = 1;
	spectators[peer] = s;
	ReleaseMutex(mutex_history);

	// wait until all information can be transmitted
	while (WaitForSingleObject(event_running, 500) == WAIT_TIMEOUT)
	{
		if (WaitForSingleObject(event_waiting, 0) != WAIT_OBJECT_0)
			// sorry, spectators
			return;
	}

	WaitForSingleObject(mutex_history, INFINITE);
	s.phase = 2;
	spectators[peer] = s;
	ReleaseMutex(mutex_history);

	Sleep(100);

	simple_luna_packet(&lp, PACKET_TYPE_RUN, 0);
	ok = ok && conmanager.send(&lp, peer);

	if (strlen(p1_name) && strlen(p2_name))
	{
		ok = ok && send_string(p1_name, peer);
		ok = ok && send_string(p2_name, peer);
	}
	else
	{
		ok = ok && send_string(dummy, peer);
		ok = ok && send_string(dummy, peer);
	}

	simple_luna_packet(&lp, PACKET_TYPE_STAGE, 0);
	lp.value = max_stages;
	ok = ok && conmanager.send(&lp, peer);
	ok = ok && send_string(blacklist1, peer);
	ok = ok && send_string(blacklist2, peer);

	simple_luna_packet(&lp, PACKET_TYPE_SEED, 0);
	lp.value = rnd_seed;
	ok = ok && conmanager.send(&lp, peer);

	if (!ok)
	{
		l();
		printf("Connection to spectator %s lost.\n", ip2str(peer));
		u();
		WaitForSingleObject(mutex_history, INFINITE);
		spectators.erase(peer);
		ReleaseMutex(mutex_history);
		conmanager.disconnect(peer);
		return;
	}

	WaitForSingleObject(mutex_history, INFINITE);
	s.phase = 3;
	spectators[peer] = s;
	ReleaseMutex(mutex_history);

	conmanager.set_async(peer);
}

bool spec_accept_callback (SOCKADDR_IN peer, luna_packet *packet)
{
	luna_packet lp;

	conmanager.register_connection(peer, peer_receiver);
	if (!PACKET_IS(*packet, PACKET_TYPE_SPECT) || PACKET_ID(*packet) != 1 || !allow_spectators)
	{
		simple_luna_packet(&lp, PACKET_TYPE_OK, PACKET_ID(*packet));
		conmanager.send_raw(&lp, peer.sin_addr.s_addr, sizeof(lp));
		conmanager.send_raw(&lp, peer.sin_addr.s_addr, sizeof(lp));
		conmanager.send_raw(&lp, peer.sin_addr.s_addr, sizeof(lp));
		simple_luna_packet(&lp, PACKET_TYPE_DENY, 2);
		conmanager.send_raw(&lp, peer.sin_addr.s_addr, sizeof(lp));
		conmanager.send_raw(&lp, peer.sin_addr.s_addr, sizeof(lp));
		conmanager.send_raw(&lp, peer.sin_addr.s_addr, sizeof(lp));
		conmanager.disconnect_nobye(peer.sin_addr.s_addr);
		return false;
	}

	conmanager.rereceive(peer.sin_addr.s_addr, packet);
	//CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)spec_handshake, (void *)peer.sin_addr.s_addr, 0, NULL);
	PostThreadMessage(small_task_thread_id, STM_SPECHANDSHAKE, (WPARAM)peer.sin_addr.s_addr, 0);
	return true;
}

#define CLEANUP_EARLY { if (serve_specs != NULL) { TerminateThread(serve_specs, 0); CloseHandle(serve_specs); } if (alive != NULL) { TerminateThread(alive, 0); CloseHandle(alive); } bye_spectators(); conmanager.clear(); /*clear_socket();*/ close_sems(); closesocket(sock); }
#define TERMINATE_EARLY { CLEANUP_EARLY ; return; }

void spectate_game (char *ip_str, int port, int record_replay)
{
	HANDLE alive = NULL, serve_specs = NULL;
	SOCKADDR_IN sa;
	unsigned long ip = get_ip_from_ipstr(ip_str);
	port = get_port_from_ipstr(ip_str, port);
	int seed;
	DWORD other_crc;
	long other_size;
	int stages;
	luna_packet lp;
	bool ok = true;

	ZeroMemory(&sa, sizeof(sa));
	ZeroMemory(&lp, sizeof(lp));

	if (ip == INADDR_NONE)
	{
		l();
		printf("Invalid IP address %s.\n", ip_str);
		u();
		return;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = ip;
	sa.sin_port = htons(port);
	conmanager.init(&sock, port, (DefaultCallback)spec_accept_callback);
	conmanager.register_connection(sa, spec_receiver);

	l(); printf("Connecting to LunaPort on %s.\n", ip_str); u();

	create_sems();

	if (DEBUG) { l(); fprintf(logfile, "Sending SPECT.\n"); u(); }
	simple_luna_packet(&lp, PACKET_TYPE_SPECT, 0);
	if (!conmanager.send(&lp, ip))
	{
		l(); printf("Connection to %s failed.\n", ip_str); u();
		TERMINATE_EARLY
	}

	if (PACKET_IS(lp, PACKET_TYPE_DENY))
	{
		l(); printf("LunaPort at %s does not accept spectators.\n", ip_str); u();
		TERMINATE_EARLY
	}

	if (!conmanager.receive(&lp, ip))
	{
		l(); printf("Connection to %s lost.\n", ip_str); u();
		TERMINATE_EARLY
	}
	if (!PACKET_IS(lp, PACKET_TYPE_KGTCH))
	{
		l(); printf("Expected KGT checksum, but got %02u.\n", PACKET_TYPE_LOW(lp)); u();
		conmanager.disconnect(ip);
		TERMINATE_EARLY
	}
	other_crc =lp.value;
	if (!conmanager.receive(&lp, ip))
	{
		l(); printf("Connection to %s lost.\n", ip_str); u();
		TERMINATE_EARLY
	}
	if (!PACKET_IS(lp, PACKET_TYPE_KGTLN))
	{
		l(); printf("Expected KGT size, but got %02u.\n", PACKET_TYPE_LOW(lp)); u();
		conmanager.disconnect(ip);
		TERMINATE_EARLY
	}
	other_size = lp.value;
	if (kgt_crc != other_crc)
	{
		l(); printf("KGT checksum mismatch, expected %08X but got %08X.\nThis means that an incompatible game version is used.\n", kgt_crc, other_crc); u();
		conmanager.disconnect(ip);
		TERMINATE_EARLY
	}
	if (kgt_size != other_size)
	{
		l(); printf("KGT size mismatch, expected %d but got %d.\nThis means that an incompatible game version is used.\n", kgt_size, other_size); u();
		conmanager.disconnect(ip);
		TERMINATE_EARLY
	}

	alive = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)spec_keep_alive, (void *)ip, 0, NULL);
	serve_specs = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)serve_spectators, NULL, 0, NULL);

	if (!conmanager.receive(&lp, ip))
		TERMINATE_EARLY

	if (PACKET_IS(lp, PACKET_TYPE_ALIVE))
	{
		l(); printf("Waiting for game on %s to start.\n", ip_str); u();
	}

	while (PACKET_IS(lp, PACKET_TYPE_ALIVE))
	{
		if (!conmanager.receive(&lp, ip))
			TERMINATE_EARLY
	}

	if (!PACKET_IS(lp, PACKET_TYPE_RUN))
	{
		l(); printf("Connection to %s terminated.\n", ip_str); u();
		TERMINATE_EARLY
	}

	l(); printf("Game on started. Recieving game data.\n"); u();

	if (!receive_string(p1_name, ip) || !receive_string(p2_name, ip))
	{
		l(); printf("Connection to %s failed.\n", ip_str); u();
		TERMINATE_EARLY
	}

	if (strlen(p1_name) && strlen(p2_name))
	{
		l(); printf("Match: %s vs %s\n", p1_name, p2_name); u();
	}

	set_delay(0);

	ok = ok && conmanager.receive(&lp, ip);
	if (!PACKET_IS(lp, PACKET_TYPE_STAGE))
	{
		l(); printf("Connection to %s failed. Expected stages, got %02x.\n", ip_str, PACKET_TYPE_LOW(lp)); u();
		TERMINATE_EARLY
	}
	stages = lp.value;
	ok = ok && receive_string(blacklist1, ip);
	ok = ok && receive_string(blacklist2, ip);
	if (!ok)
	{
		l(); printf("Connection to %s failed.\n", ip_str); u();
		TERMINATE_EARLY
	}
	if (max_stages < stages)
	{
		l(); printf("Less stages available (%u) than required to spectate (%u).\n", max_stages, stages); u();
		TERMINATE_EARLY
	}
	max_stages = stages;
	stagemanager.init(max_stages);
	stagemanager.blacklist(blacklist1);
	stagemanager.blacklist(blacklist2);
	if (!stagemanager.ok())
	{
		l(); printf("After applying blacklists, no stages (of %u) are left.\nBlacklist 1: %s\nBlacklist 2: %s\n", max_stages, blacklist1, blacklist2); u();
		TERMINATE_EARLY
	}

	if (!conmanager.receive(&lp, ip) || !PACKET_IS(lp, PACKET_TYPE_SEED))
	{
		l(); printf("Connection to %s failed.\n", ip_str); u();
		TERMINATE_EARLY
	}

	seed = lp.value;
	update_history(seed);
	WaitForSingleObject(mutex_history, INFINITE);
	spec_pos = 1;
	ReleaseMutex(mutex_history);

	if (DEBUG) { l(); fprintf(logfile, "Spectate set up.\n"); u(); }
	l(); printf("Successfully connected to %s. Spectating.\n", ip_str); u();
	remote_player = ip;
	proc = NULL;
	conmanager.set_async(ip);
	if (DEBUG) { l(); fprintf(logfile, "Created spectator threads.\n"); u(); }

	l(); printf("Receiving initial input buffer.\n"); u();
	do
	{
		if (DEBUG) { l(); fprintf(logfile, "Waiting for buffer.\n"); u(); }
		Sleep(126);
	} while (game_history_pos < SPECTATOR_INITIAL);
	l(); printf("Done. Starting game.\n"); u();
	if (DEBUG) { l(); fprintf(logfile, "Running game.\n"); u(); }

	run_game(seed, 0, record_replay, NULL, 1);
	proc = NULL;
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
}

void join_game (char *ip_str, int port, int record_replay)
{
	HANDLE alive = NULL, serve_specs = NULL;
	SOCKADDR_IN sa;
	int ip = get_ip_from_ipstr(ip_str);
	port = get_port_from_ipstr(ip_str, port);
	int seed, d;
	DWORD other_crc;
	long other_size;
	unsigned int other_stages;
	luna_packet lp, lp2;

	if (DEBUG) { l(); fprintf(logfile, "Joining\n"); u(); }

	ZeroMemory(&sa, sizeof(sa));
	ZeroMemory(&lp, sizeof(lp));

	if (ip == INADDR_NONE)
	{
		l();
		printf("Invalid IP address %s.\n", ip_str);
		u();
		return;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = ip;
	sa.sin_port = htons(port);
	conmanager.init(&sock, port, (DefaultCallback)spec_accept_callback);
	conmanager.register_connection(sa, peer_receiver);

	l(); printf("Connecting to LunaPort on %s.\n", ip_str); u();

	create_sems();
	simple_luna_packet(&lp, PACKET_TYPE_HELLO, 0);
	if (!conmanager.send(&lp, ip))
	{
		l(); printf("Connection to %s failed.\n", ip_str); u();
		TERMINATE_EARLY
	}

	conmanager.receive(&lp, ip);
	if (PACKET_IS(lp, PACKET_TYPE_PNUM))
	{
		local_p = lp.value ? 1 : 0;
		l(); printf("Establishing connection...\nLocal player %u.\n", local_p+1); u();
	}
	else if (PACKET_IS(lp, PACKET_TYPE_DENY))
	{
		int spec = 0;
		l(); printf("Host is busy. Connection denied.\n"); u();
		CLEANUP_EARLY
		if (ask_spectate)
		{
			do
			{
				l(); printf("Do you want to spectate?\n0: Do not spectate\n1: Spectate\n"); u();
				read_int(&spec);
			} while (spec != 0 && spec != 1);
			if (spec)
			{
				l(); printf("Ok. Entering spectator mode.\n"); u();
				spectate_game(ip_str, port, record_replay);
			}
			else
			{
				l(); printf("Aborting.\n"); u();
			}
		}
		return;
	}
	else
	{
		l(); printf("Invalid handshake sequence from %s. Expected player number, got %02x.\n", ip2str(ip), PACKET_TYPE_LOW(lp)); u();
		TERMINATE_EARLY
	}

		if (!conmanager.receive(&lp, ip))
	{
		l(); printf("Connection to %s lost.\n", ip_str); u();
		TERMINATE_EARLY
	}
	if (!PACKET_IS(lp, PACKET_TYPE_KGTCH))
	{
		l(); printf("Expected KGT checksum, but got %02u.\n", PACKET_TYPE_LOW(lp)); u();
		conmanager.disconnect(ip);
		TERMINATE_EARLY
	}
	other_crc =lp.value;
	if (!conmanager.receive(&lp, ip))
	{
		l(); printf("Connection to %s lost.\n", ip_str); u();
		TERMINATE_EARLY
	}
	if (!PACKET_IS(lp, PACKET_TYPE_KGTLN))
	{
		l(); printf("Expected KGT size, but got %02u.\n", PACKET_TYPE_LOW(lp)); u();
		conmanager.disconnect(ip);
		TERMINATE_EARLY
	}
	other_size = lp.value;
	simple_luna_packet(&lp, PACKET_TYPE_KGTST, 0);
	if (kgt_crc != other_crc)
	{
		l(); printf("KGT checksum mismatch, expected %08X but got %08X.\nThis means that an incompatible game version is used.\n", kgt_crc, other_crc); u();
		lp.value = 0;
		conmanager.send(&lp, ip);
		conmanager.disconnect(ip);
		TERMINATE_EARLY
	}
	if (kgt_size != other_size)
	{
		l(); printf("KGT size mismatch, expected %d but got %d.\nThis means that an incompatible game version is used.\n", kgt_size, other_size); u();
		lp.value = 0;
		conmanager.send(&lp, ip);
		conmanager.disconnect(ip);
		TERMINATE_EARLY
	}
	lp.value = 1;

	if (!conmanager.send(&lp, ip) || !receive_string(local_p ? p1_name : p2_name, ip) || !send_string(own_name, ip))
	{
		l(); printf("Failed to transmit names.\n"); u();
		TERMINATE_EARLY
	}
	strcpy(local_p ? p2_name : p1_name, own_name);
	l(); printf("Remote player: %s\n", local_p ? p1_name : p2_name); u();

	simple_luna_packet(&lp2, PACKET_TYPE_STAGE, 0);
	lp2.value = max_stages;
	if (!conmanager.receive(&lp, ip) || !PACKET_IS(lp, PACKET_TYPE_STAGE) || !conmanager.send(&lp2, ip) || !receive_string(local_p ? blacklist1 : blacklist2, ip) || !send_string(set_blacklist, ip))
	{
		l(); printf("Failed to transmit stage information.\n"); u();
		TERMINATE_EARLY
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
		TERMINATE_EARLY
	}

	conmanager.receive(&lp, ip);
	if (PACKET_IS(lp, PACKET_TYPE_SEED))
		seed = lp.value;
	else
	{
		l(); printf("Invalid handshake sequence from %s. Expected random seed, got %02x.\n", ip2str(ip), PACKET_TYPE_LOW(lp)); u();
		TERMINATE_EARLY
	}

	l(); printf("Waiting for delay setting.\n"); u();

	for (;;)
	{
		conmanager.receive(&lp, ip);
		if (PACKET_IS(lp, PACKET_TYPE_PING))
			continue;
		if (!PACKET_IS(lp, PACKET_TYPE_DELAY))
		{
			l(); printf("Invalid handshake sequence from %s. Expected ping or delay, got %02x.\n", ip2str(ip), PACKET_TYPE_LOW(lp)); u();
			TERMINATE_EARLY
		}
		else
			break;
	}

	if (PACKET_IS(lp, PACKET_TYPE_DELAY))
	{
		d = (int)(((float)PACKET_PING(lp) + MS_PER_INPUT) / (2.0 * MS_PER_INPUT) + SAFETY_DELAY);
		if (PACKET_DELAY(lp) == d)
		{
			l(); printf("Determined ping of %ums. This corresponds to an input delay of %u input requests.\nGame setup complete.\n", PACKET_PING(lp), PACKET_DELAY(lp)); u();
		}
		else
		{
			l(); printf("Determined ping of %ums. This corresponds to an input delay of %u input requests.\nDelay set to %u.\nGame setup complete.\n", PACKET_PING(lp), d, PACKET_DELAY(lp)); u();
		}
		set_delay(PACKET_DELAY(lp));
	}
	else
	{
		l(); printf("Invalid handshake sequence from %s. Expected ping or delay, got %02x.\n", ip2str(ip), PACKET_TYPE_LOW(lp)); u();
		TERMINATE_EARLY
	}

	l(); printf("Connected. Starting game.\n"); u();
	remote_player = ip;
	proc = NULL;
	conmanager.set_async(ip);
	alive = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)keep_alive, (void *)remote_player, 0, NULL);
	serve_specs = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)serve_spectators, NULL, 0, NULL);
	run_game(seed, 1, record_replay, NULL, 0);
	proc = NULL;
	conmanager.disconnect(ip);
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
}

void ping_thread_loop (unsigned long peer)
{
	luna_packet lp;
	simple_luna_packet(&lp, PACKET_TYPE_PING, 0);
	while (!terminate_ping_thread)
	{
		Sleep(ALIVE_INTERVAL);
		conmanager.send(&lp, peer);
	}
	ReleaseSemaphore(sem_ping_end, 1, NULL);
}

void host_game (int seed, int record_replay, int ask_delay)
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
		d = (int)(((float)pings + MS_PER_INPUT) / (2.0 * MS_PER_INPUT) + SAFETY_DELAY);
		if (d > 0xff)
			d = 0xff;
		if (play_host_sound)
			play_sound();
		if (ask_delay)
		{
			terminate_ping_thread = 0;
			ping_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ping_thread_loop, (void *)peer, 0, NULL);
			l(); printf("\nDetermined ping of %ums. This corresponds to an input delay of %u input requests.\nIf you experience heavy lag, try increasing the delay.\nPlease enter delay (0 = ping again) ", pings, d); u();
			SetForegroundWindow(FindWindow(NULL, "LunaPort " VERSION));
			read_int(&d);
			terminate_ping_thread = 1;
			WaitForSingleObject(sem_ping_end, INFINITE);
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

void print_menu (int record_replay)
{
	l();
	printf("LunaPort " VERSION "\n"
		   "====================\n"
		   "1: Host game (port %u UDP)\n"
		   "2: Join game (port %u UDP)\n"
		   "3: Host game on lobby (port %u UDP)\n"
		   "4: List lobby games\n"
		   "5: Local game with random stages in vs mode\n"
		   "6: Toggle replay recording (%s)\n"
		   "7: Watch replay\n"
		   "8: Toggle spectators (%s)\n"
		   "9: Spectate\n"
		   "10: Display this menu\n"
		   "0: Exit\n"
		   "====================\n\n", port, port, port, record_replay ? "currently enabled" : "currently disabled", allow_spectators ? "currently allowed" : "currently disabled");
	u();
}

UINT CALLBACK ofn_hook (HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_INITDIALOG)
	{
		SetForegroundWindow(hdlg);
		return TRUE;
	}

	return FALSE;
}

void read_config (unsigned int *port, int *record_replay, int *allow_spectators, unsigned int *max_stages, int *ask_delay,
				  int *ask_spectate, int *display_framerate, int *display_inputrate, int *display_names, char *game_exe,
				  char *own_name, char *set_blacklist, int *blacklist_local, int *check_exe, int *max_points,
				  int *keep_session_log, char *session_log, char *lobby_url, char *lobby_comment, int *display_lobby_comments,
				  int *keep_hosting, char *sound, int *play_host_sound, int *play_lobby_sound, char *replays_dir)
{
	char config_filename[_MAX_PATH];
	strcpy(config_filename, dir_prefix);
	strcat(config_filename, "\\"INIFILE);
	*port = GetPrivateProfileInt("LunaPort", "Port", PORT, config_filename);
	*record_replay = GetPrivateProfileInt("LunaPort", "RecordReplayDefault", 0, config_filename);
	*allow_spectators = GetPrivateProfileInt("LunaPort", "AllowSpectatorsDefault", 1, config_filename);
	*max_stages = GetPrivateProfileInt("LunaPort", "Stages", MAX_STAGES, config_filename);
	*ask_delay = GetPrivateProfileInt("LunaPort", "AskDelay", 1, config_filename);
	*ask_spectate = GetPrivateProfileInt("LunaPort", "AskSpectate", 1, config_filename);
	*keep_hosting = GetPrivateProfileInt("LunaPort", "KeepHosting", 0, config_filename);
	*display_framerate = GetPrivateProfileInt("LunaPort", "DisplayFramerate", 1, config_filename);
	*display_inputrate = GetPrivateProfileInt("LunaPort", "DisplayInputrate", 1, config_filename);
	*display_names = GetPrivateProfileInt("LunaPort", "DisplayNames", 1, config_filename);
	*display_lobby_comments = GetPrivateProfileInt("LunaPort", "DisplayLobbyComments", 1, config_filename);
	*play_host_sound = GetPrivateProfileInt("LunaPort", "PlayHostConnect", 1, config_filename);
	*play_lobby_sound = GetPrivateProfileInt("LunaPort", "PlayLobbyChange", 1, config_filename);
	*blacklist_local = GetPrivateProfileInt("LunaPort", "UseBlacklistLocal", 0, config_filename);
	*max_points = GetPrivateProfileInt("LunaPort", "MaxPoints", 2, config_filename);
	*keep_session_log = GetPrivateProfileInt("LunaPort", "KeepSessionLog", 1, config_filename);
	*check_exe = GetPrivateProfileInt("LunaPort", "CheckExeCRC", 1, config_filename);
	GetPrivateProfileString("LunaPort", "GameExe", GAME, game_exe, _MAX_PATH-1, config_filename);
	GetPrivateProfileString("LunaPort", "Replays", REPLAYDIR, replays_dir, _MAX_PATH-1, config_filename);
	GetPrivateProfileString("LunaPort", "PlayerName", "Unknown", own_name, NET_STRING_BUFFER-1, config_filename);
	GetPrivateProfileString("LunaPort", "StageBlacklist", "", set_blacklist, NET_STRING_BUFFER-1, config_filename);
	GetPrivateProfileString("LunaPort", "SessionLog", "session.log", session_log, NET_STRING_BUFFER-1, config_filename);
	GetPrivateProfileString("LunaPort", "Lobby", DEFAULT_LOBBY, lobby_url, NET_STRING_BUFFER-1, config_filename);
	GetPrivateProfileString("LunaPort", "LobbyComment", "", lobby_comment, NET_STRING_BUFFER-1, config_filename);
	GetPrivateProfileString("LunaPort", "Sound", "notify.wav", sound, _MAX_PATH-1, config_filename);
}

void calc_crcs ()
{
	char path[_MAX_PATH];
	HWND hwnd = GetForegroundWindow();
	DWORD crc;
	long cnt;

	if(strlen(game_exe) > 1 && game_exe[1] == ':') {
		strcpy(path, game_exe);
	}
	else {
		strcpy(path, dir_prefix);
		strcat(path, "\\");
		strcat(path, game_exe);
	}

	if (!crc32file(path, &crc, &cnt))
	{
		MessageBox(hwnd, "Cannot open game exe for CRC32 calculation.\nPlease check the filename and GameExe setting in \""INIFILE"\".", "LunaPort Error", MB_OK | MB_ICONERROR);
		exit(1);
	}
	if (check_exe && (crc != ENGINE_CRC || cnt != ENGINE_LEN))
	{
		MessageBox(hwnd, "CRC32 or size mismatch.\nPlease make sure that your game exe is not corrupted.", "LunaPort Error", MB_OK | MB_ICONERROR);
		exit(1);
	}
	cnt = strlen(path);
	if (cnt < 4)
	{
		MessageBox(hwnd, "Invalid KGT file path.", "LunaPort Error", MB_OK | MB_ICONERROR);
		exit(1);
	}
	strcpy(path + cnt - 4, ".kgt");
	if (!crc32file(path, &kgt_crc, &kgt_size))
	{
		MessageBox(hwnd, "Cannot open KGT file for CRC32 calculation.\nPlease check the filename of your KGT file,\nit should match that of your game exe.", "LunaPort Error", MB_OK | MB_ICONERROR);
		exit(1);
	}
	//l(); printf("KGT CRC32: %08X Size: %d\n", kgt_crc, kgt_size); u();
}

void unregister_lobby ()
{
	int refresh = 0;
	if (lobby_flag)
	{
		data_del(lobby_url, kgt_crc, kgt_size, port, &refresh);
		lobby.disconnect();
	}
}

bool inifile_exists(const char *argv0)
{
	char _path[_MAX_PATH] = "";
	strcpy(_path, argv0);
	for (int i = strlen(_path) - 1; i >= 0; i--)
		if (_path[i] == '\\' || _path[i] == '/')
		{
			_path[i] = 0;
			break;
		}
	strcat(_path, "\\"INIFILE);
	return ::PathFileExists(_path) ? true : false;
}

int main(int argc, char* argv[])
{
	WSADATA wsa;
	char ip_str[NET_STRING_BUFFER], tmp[256];
	char filename[_MAX_PATH];
	SECURITY_ATTRIBUTES sa;
	OPENFILENAME ofn;
	int in = 10, old_in = 10;
	int record_replay, ask_delay;
	int i;
	int max_points = 2;
	int keep_session_log = 1;
	char session_log[_MAX_PATH];
	bool exit_lobby = false;
	int lobby_port, lobby_spec;

	SetConsoleTitle("LunaPort " VERSION);

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

	this_proc = GetModuleHandle(NULL);
	local_p = 0;
	srand(time(NULL));
	game_history = NULL;
	game_history_pos = 0;
	ip_str[0] = 0;
	tmp[0] = 0;
	write_replay = NULL;
	recording_ended = false;
	ZeroMemory(game_exe, sizeof(game_exe));
	ZeroMemory(own_name, sizeof(own_name));
	ZeroMemory(p1_name, sizeof(p1_name));
	ZeroMemory(p2_name, sizeof(p2_name));
	ZeroMemory(set_blacklist, sizeof(set_blacklist));
	ZeroMemory(blacklist1, sizeof(blacklist1));
	ZeroMemory(blacklist2, sizeof(blacklist2));
	ZeroMemory(session_log, sizeof(session_log));

	ZeroMemory(&sa, sizeof(sa));
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof(sa);
	mutex_print = CreateMutex(&sa, FALSE, NULL);
	event_running = CreateEvent(&sa, TRUE, FALSE, "");
	event_waiting = CreateEvent(&sa, TRUE, FALSE, "");

	if(!::PathFileExists(INIFILE) && inifile_exists(argv[0])) {
		strcpy(dir_prefix, argv[0]);
		for (i = strlen(dir_prefix) - 1; i >= 0; i--)
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

	max_stages = set_max_stages;
	calc_crcs();
	stagemanager.init(max_stages);

	if (!keep_session_log)
		session_log[0] = 0;
	session.set_options(session_log, max_points, kgt_crc, kgt_size);

	if (WSAStartup(MAKEWORD(2, 0), &wsa))
	{
		l(); printf("Couldn't initialize winsocks.\n"); u();
		return 1;
	}
	curl_global_init(CURL_GLOBAL_ALL);
	lobby.init(lobby_url, kgt_crc, kgt_size, own_name, lobby_comment, port, &lobby_flag, display_lobby_comments, play_lobby_sound);
	atexit(unregister_lobby);

	CreateDirectory(replays_dir, NULL);

	if (argc == 2)
	{
		if (!strcmp("--local", argv[1]))
		{
			if (blacklist_local)
				stagemanager.blacklist(set_blacklist);
			if (!stagemanager.ok())
			{
				l(); printf("After applying blacklists, no stages (of %u) are left.\nBlacklist: %s\n", max_stages, set_blacklist); u();
				return 1;
			}
			SetEvent(event_waiting);
			run_game((rand()<<16) + rand(), 0, record_replay, NULL, 0);
			printf("\n\nLunaPort done.\n");
			Sleep(1500);
			return 0;
		}
		else
		{
			SetEvent(event_waiting);
			run_game(0, 0, -1, argv[1], 0);
			printf("\n\nLunaPort done.\n");
			Sleep(1500);
			return 0;
		}
	}

	timeBeginPeriod(1);
	small_task_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)small_task_thread_proc, NULL, 0, &small_task_thread_id);
	print_menu(record_replay);
	do
	{
		read_int(&in);
		switch (in)
		{
		case 1:
			SetEvent(event_waiting);
			host_game((rand()<<16) + rand(), record_replay, ask_delay);
			break;
		case 2:
			l(); printf("IP[%s]> ", ip_str); u();
			cin.getline(tmp, 256, '\n');
			if (strlen(tmp))
			{
				if (get_ip_from_ipstr(tmp) == INADDR_NONE || get_ip_from_ipstr(tmp) == 0)
				{
					l(); printf("Invalid IP: %s\n", tmp); u();
					break;
				}
				strcpy(ip_str, tmp);
			}
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
					if (get_ip_from_ipstr(ip_str) == INADDR_NONE || get_ip_from_ipstr(ip_str) == 0)
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
			if (GetOpenFileName(&ofn))
			{
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
			l(); printf("IP[%s]> ", ip_str); u();
			cin.getline(tmp, 256, '\n');
			if (strlen(tmp))
			{
				if (get_ip_from_ipstr(tmp) == INADDR_NONE || get_ip_from_ipstr(tmp) == 0)
				{
					l(); printf("Invalid IP: %s\n", tmp); u();
					break;
				}
				strcpy(ip_str, tmp);
			}
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
		old_in = in;

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
	} while (in != 0);

	timeEndPeriod(1);
	PostThreadMessage(small_task_thread_id, WM_QUIT, 0, 0);
	WSACleanup();

	printf("\n\nLunaPort done.\n");
	Sleep(300);

	return 0;
}
