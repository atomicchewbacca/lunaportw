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

#include "stdafx.h"
#ifdef _MSC_VER
#pragma warning (disable : 4786)
#endif

#include <algorithm>
#ifdef DEBUG
# undef DEBUG
#endif
#include "HTTP.h"
#include "Lobby.h"

extern void l ();
extern void u ();
extern void read_int (int *i);
extern bool get_esc ();
extern void play_sound ();
extern int __log_printf(const char *fmt, ...);

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

bool Lobby::loadgamelist()
{
	int err = 0;
	lplobby_head *result = NULL;

	// receive initial game list
	WaitForSingleObject(mutex, INFINITE);
	games.clear();
	strcpy(msg, "Not connected.");
	err = data_get(lobby_url, kgt_crc, kgt_size, &refresh, &result);
	if (!err)
	{
		update_menu(result);
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
		__log_printf("%s\n", http_error(err));
		u();
		return false;
	}

	ReleaseMutex(mutex);
	return true;
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
			__log_printf("Lobby disabled: %s\n", http_error(err));
			u();
			refresher = NULL;
			ReleaseMutex(mutex);
			return;
		}
		ReleaseMutex(mutex);
	}
}
