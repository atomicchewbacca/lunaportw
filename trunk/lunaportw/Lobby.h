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

#ifndef LOBBY_H
#define LOBBY_H

#include <vector>
#include <windows.h>
#include "HTTP.h"
#include "LunaPort.h"

class Lobby
{
	private:
		char msg[NET_STRING_BUFFER];
		char *lobby_url, *own_name, *name, *name2, *comment;
		HANDLE refresher;                  // resend requests to lobby server 
		HANDLE mutex;
		HANDLE sem_refresh;
		int type;                          // -1 = none, 0 = host, 1 = client
		unsigned int kgt_crc;
		unsigned int kgt_size;
		unsigned int port;
		int state;                         // -1 = none, 0 = waiting, 1 = playing
		int refresh;
		int last_input;
		int display_comments;
		int play_sounds;
		int *lobby;                        // pointer to lobby flag, should be updated as necessary, 0 = no connection, 1 = connected as host

		void update_menu (lplobby_head *result);

	public:
		std::vector<lplobby_record> games; // ordered list of games received from the server

		Lobby ();
		void init (char *url, int crc, int size, char *name, char *comment, unsigned short port, int *lobby, int display_comments, int play_sounds);
		bool loadgamelist ();
		void host ();
		void run (char *name, char *name2);
		void disconnect ();                 // data_del and terminate refresher
		void refresher_thread ();
};

#endif
