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

#include <algorithm>
#include "HTTP.h"
#include "Lobby.h"

extern void l ();
extern void u ();
extern void read_int (int *i);
extern bool get_esc ();
extern void play_sound ();

inline bool lt_record(lplobby_record r1, lplobby_record r2)
{
	if (r1.state < r2.state)
		return true;
	if (r2.state < r1.state)
		return false;
	return r1.refresh < r2.refresh;
}

static int run_refresher (Lobby *l)
{
	l->refresher_thread();
	return 0;
}

Lobby::Lobby ()
{
	lobby_url = NULL;
	own_name = NULL;
	name = NULL;
	name2 = NULL;
	comment = NULL;
	games.clear();
	refresher = NULL;
	mutex = NULL;
	sem_refresh = NULL;
	type = -1;
	kgt_crc = 0;
	kgt_size = 0;
	port = 0;
	state = -1;
	refresh = 4 * 60;
	last_input = 1;
	lobby = NULL;
	display_comments = 1;
	play_sounds = 1;
}

void Lobby::init (char *url, int crc, int size, char *name, char *comment, unsigned short port, int *lobby, int display_comments, int play_sounds)
{
	SECURITY_ATTRIBUTES sa;

	this->lobby_url = url;
	this->kgt_crc = crc;
	this->kgt_size = size;
	this->own_name = name;
	this->name = "Unknown";
	this->name2 = "Unknown";
	this->comment = comment;
	this->port = port;
	this->lobby = lobby;
	this->type = -1;
	this->refresh = INFINITE;
	this->last_input = 1;
	this->state = -1;
	this->display_comments = display_comments;
	this->play_sounds = play_sounds;

	ZeroMemory(&sa, sizeof(sa));
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;
	sa.nLength = sizeof(sa);
	mutex = CreateMutex(&sa, FALSE, NULL);
	sem_refresh = CreateSemaphore(&sa, 0, 1, NULL);
	refresher = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)run_refresher, (void *)this, 0, NULL);
	strcpy(msg, "Not connected.");

	if (lobby != NULL)
		*lobby = 0;
}

void Lobby::show_menu ()
{
	lplobby_record r;
	l();
	printf("Lobby: %s\n"
	       "====================\n", msg);
	for (int i = games.size() - 1; i >= 0; i--)
	{
		r = games[i];
		if (r.state == 0)
		{
			printf("%d: '%s' waiting on %s:%u ", i+4, r.name, r.ip, r.port);
		}
		else
		{
			printf("%d: '%s' vs '%s' on %s:%u ", i+4, r.name, r.name2, r.ip, r.port);
		}
		if (r.comment[0] && display_comments)
			printf("[%s] ", r.comment);
		printf("(last refresh: %ds)\n", r.refresh);
	}
	printf("3: Watch lobby\n"
	       "2: Refresh game list\n"
		   "1: Display this menu\n"
		   "0: Exit lobby\n"
		   "====================\n");
	u();
}

void Lobby::update_menu (lplobby_head *result)
{
	lplobby_record r;
	memcpy(msg, result->msg, NET_STRING_LENGTH);
	games.clear();
	if (result == NULL)
		return;
	for (unsigned int i = 0; i < result->n; i++)
	{
		r = result->records[i];
		games.push_back(r);
	}
	std::stable_sort(games.begin(), games.end(), lt_record);
}

bool Lobby::menu (char *ip, int *port, int *spec)
{
	DWORD last, now;
	bool exit = false, ok = false;
	int err = 0;
	int input = 1, old_in = 1;
	lplobby_head *result = NULL;

	// receive initial game list
	WaitForSingleObject(mutex, INFINITE);
	games.clear();
	strcpy(msg, "Not connected.");
	err = data_get(lobby_url, kgt_crc, kgt_size, &refresh, &result);
	if (!err)
	{
		update_menu(result);
		show_menu();
	}
	if (result != NULL)
	{
		result = NULL;
		free(result);
	}

	// handle errors
	if (err)
	{
		ReleaseMutex(mutex);
		l();
		printf("%s\n", http_error(err));
		u();
		return true;
	}

	// do not set up background receiver
	if (refresher == NULL)
		refresher = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)run_refresher, (void *)this, 0, NULL);
	type = -1;
	ReleaseSemaphore(sem_refresh, 1, NULL);
	if (lobby != NULL)
		*lobby = 0;
	ReleaseMutex(mutex);

	do
	{
		read_int(&input);
		switch (input)
		{
		case 0:
			exit = 1;
			old_in = 0;
			ok = true;
			break;
		case 1:
			show_menu();
			old_in = 1;
			break;
		case 2:
			WaitForSingleObject(mutex, INFINITE);
			err = data_get(lobby_url, kgt_crc, kgt_size, &refresh, &result);
			old_in = 2;
			if (!err)
			{
				update_menu(result);
				show_menu();
			}
			if (result != NULL)
			{
				result = NULL;
				free(result);
			}

			// handle errors
			if (err)
			{
				l();
				printf("%s\n", http_error(err));
				u();
				return true;
			}
			ReleaseMutex(mutex);
			break;
		case 3:
			old_in = 3;
			l(); printf("Watching lobby. Hit escape key to stop.\n"); u();
			last = 0;
			while (!get_esc())
			{
				now = GetTickCount();
				if (now - refresh * 1000 < last)
				{
					Sleep(75);
					continue;
				}
				last = now;

				WaitForSingleObject(mutex, INFINITE);
				err = data_get(lobby_url, kgt_crc, kgt_size, &refresh, &result);
				old_in = 2;
				if (!err)
				{
					update_menu(result);
					if (result->n)
					{
						show_menu();
						l(); printf("Watching lobby. Hit escape key to stop.\n"); u();
						SetForegroundWindow(FindWindow(NULL, "LunaPort " VERSION));
						if (play_sounds)
							play_sound();
					}
				}
				if (result != NULL)
				{
					result = NULL;
					free(result);
				}

				// handle errors
				if (err)
				{
					l();
					printf("%s\n", http_error(err));
					u();
					return true;
				}
				ReleaseMutex(mutex);
			}
			break;
		default:
			if (input >= 0 && input <= 3 + games.size())
			{
				WaitForSingleObject(mutex, INFINITE);
				old_in = input;
				memcpy(ip, games[input - 4].ip, NET_STRING_LENGTH);
				ip[NET_STRING_LENGTH] = 0;
				*port = games[input - 4].port;
				*spec = games[input - 4].state == 1;
				ok = true;
				ReleaseMutex(mutex);
			}
			else
			{
				l(); printf("Unknown menu item: %d\n", input); u();
				input = old_in;
			}

		}
	} while (!ok);

	return exit;
}

void Lobby::host ()
{
	WaitForSingleObject(mutex, INFINITE);
	if (refresher == NULL)
		refresher = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)run_refresher, (void *)this, 0, NULL);
	type = 0;
	state = 0;
	name = own_name;
	ReleaseSemaphore(sem_refresh, 1, NULL);
	if (lobby != NULL)
		*lobby = 1;
	ReleaseMutex(mutex);
}

void Lobby::run (char *name, char *name2)
{
	WaitForSingleObject(mutex, INFINITE);
	if (refresher == NULL)
		refresher = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)run_refresher, (void *)this, 0, NULL);
	type = 0;
	state = 1;
	this->name = name;
	this->name2 = name2;
	ReleaseSemaphore(sem_refresh, 1, NULL);
	if (lobby != NULL)
		*lobby = 1;
	ReleaseMutex(mutex);
}

void Lobby::disconnect ()
{
	WaitForSingleObject(mutex, INFINITE);
	if (refresher == NULL)
		refresher = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)run_refresher, (void *)this, 0, NULL);
	type = -1;
	ReleaseSemaphore(sem_refresh, 1, NULL);
	if (lobby != NULL)
		*lobby = 0;
	ReleaseMutex(mutex);
}

void Lobby::refresher_thread ()
{
	int err = 0;
	//lplobby_head *result;
	for (;;)
	{
		if (refresh == 0)
			refresh = 1;
		WaitForSingleObject(sem_refresh, refresh == INFINITE ? INFINITE : (refresh * 1000));
		WaitForSingleObject(mutex, INFINITE);
		switch (type)
		{
		case -1:
			err = data_del(lobby_url, kgt_crc, kgt_size, port, &refresh);
			refresh = INFINITE;
			break;
		case 0:
			err = data_set(lobby_url, kgt_crc, kgt_size, name, name2, comment, port, state, &refresh, msg);
			break;
		case 1:
			refresh = INFINITE;
			break;
			/*result = NULL;
			err = data_get(lobby_url, kgt_crc, kgt_size, &refresh, &result);
			if (!err)
				update_menu(result);
			if (result != NULL)
				free(result);
			break;*/
		}
		if (err)
		{
			l();
			printf("Lobby disabled: %s\n", http_error(err));
			u();
			refresher = NULL;
			ReleaseMutex(mutex);
			return;
		}
		ReleaseMutex(mutex);
	}
}
