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

#ifndef SESSION_H
#define SESSION_H

#include <windows.h>
#include <string>
#include "LunaPort.h"

// not thread safe, but it doesn't need to be
class Session
{
	private:
		bool active;                        // only keep session log for games we actively play, not spectator or replay

		bool enable_log;                    // do we keep a log?
		char log_name[_MAX_PATH];           // filename for logfile
		FILE *log;                          // file handle to logfile

		int max_points;                     // max points in match for any player (2 in best-of-3 match)
		int match_p1;                       // rounds won in this match by p1
		int match_p2;                       // rounds won in this match by p2
		int matches_p1;                     // total matches won by p1
		int matches_p2;                     // total matches won by p2
		int matches;                        // total matches
		int total_p1;                       // total rounds won by p1
		int total_p2;                       // total rounds won by p2
		int total;                          // total rounds

		int local_p;                        // 0 = local is P1, 1 = local is P2
		char p1_name[NET_STRING_BUFFER];    // name of first player
		char p2_name[NET_STRING_BUFFER];    // name of second player

		char stage[GAME_STRING_BUFFER];     // name of stage
		char p1_char[GAME_STRING_BUFFER];   // name of first player's character
		char p2_char[GAME_STRING_BUFFER];   // name of second player's character

		void log_line (std::string line);   // prefix timestamp and and write to logfile
		void write_line (std::string line); // write string to stdout and log line
		void check_win ();                  // check win conditions

	public:
		Session ();                                                                     // initialize values
		void set_options (char *filename, int max_points, DWORD kgt_crc, int kgt_size); // set options from lunaport.ini and write info to log
		void start (char *p1_name, char *p2_name, int local_p, unsigned long peer);     // start new session
		void set_names (char *p1_name, char *p2_name);                                  // set names
		void match_start (char *stage, char *p1_char, char *p2_char);                   // start a match
		void p1_win ();                                                                 // add point for player 1, check win conditions
		void p2_win ();                                                                 // add point for player 2, check win conditions
		void double_ko () ;                                                             // add point for both players, check win conditions
		void draw ();                                                                   // add point for both players, check win conditions
		void end ();                                                                    // disconnected/game ended, give summary
};

#endif
