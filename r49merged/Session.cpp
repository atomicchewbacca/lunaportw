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

#include <string.h>
#include <time.h>
#include <string>
#include <sstream>
#include <iostream>
#include "Session.h"
using namespace std;

extern void l ();
extern void u ();
extern char *ip2str(unsigned long ip);

Session::Session ()
{
	enable_log = false;
	log_name[0] = 0;
	log = NULL;
	max_points = 2;
	match_p1 = 0;
	match_p2 = 0;
	matches_p1 = 0;
	matches_p2 = 0;
	matches = 0;
	total_p1 = 0;
	total_p2 = 0;
	total = 0;
	local_p = -1;
	p1_name[0] = 0;
	p2_name[0] = 0;
	stage[0] = 0;
	p1_char[0] = 0;
	p2_char[0] = 0;
	p1_name[NET_STRING_LENGTH] = 0;
	p2_name[NET_STRING_LENGTH] = 0;
	stage[GAME_STRING_LENGTH] = 0;
	p1_char[GAME_STRING_LENGTH] = 0;
	p2_char[GAME_STRING_LENGTH] = 0;
	active = false;
	strcpy(this->p1_name, "Unknown 1");
	strcpy(this->p2_name, "Unknown 2");
}

void Session::log_line (std::string line)
{
	char date[200];
	time_t ltime;
	struct tm *tm;

	if (!enable_log || log == NULL)
		return;
	if (!active)
		return;

	_tzset();
	time(&ltime);
	tm = localtime(&ltime);
	strftime(date, 200, "%Y-%m-%d %H:%M:%S", tm);
	fprintf(log, "%s %s\n", date, line.c_str());
}

void Session::write_line (std::string line)
{
	l();
	printf("%s\n", line.c_str());
	u();
	log_line(line);
}

void Session::check_win ()
{
	std::ostringstream line;
	int win = -1;

	if (match_p1 == max_points && match_p2 == max_points)
	{
		matches_p1++;
		matches_p2++;
		matches++;
		line << "Match ended as draw (" << match_p1 << ":" << match_p2 << ")";
		match_p1 = 0;
		match_p2 = 0;
		write_line(line.str());
		return;
	}
	if (match_p1 == max_points)
	{
		matches_p1++;
		win = 0;
	}
	else if (match_p2 == max_points)
	{
		matches_p2++;
		win = 1;
	}
	if (win != -1)
	{
		matches++;
		line << "Match won by " << (win ? p2_name : p1_name) << " (P" << (win + 1) << ") with: " << match_p1 << ":" << match_p2;
		match_p1 = 0;
		match_p2 = 0;
		write_line(line.str());
	}
}

void Session::set_options (char *filename, int max_points, DWORD kgt_crc, int kgt_size)
{
	std::ostringstream line;

	if (filename[0])
	{
		log = fopen(filename, "a");
		setvbuf(log, NULL, _IONBF, 0);
		enable_log = true;
		line << "LunaPort session initialized (MaxPoints: " << max_points << " KGTCRC: ";
		line.setf(ios::hex, ios::basefield);
		line << kgt_crc;
		line.setf(ios::dec, ios::basefield);
		line << " KGTSize: " << kgt_size << ")";
		active = true;
		log_line(line.str());
		active = false;
	}
	this->max_points = max_points;
}

void Session::set_names (char *p1_name, char *p2_name)
{
	strncpy(this->p1_name, p1_name, NET_STRING_LENGTH);
	strncpy(this->p2_name, p2_name, NET_STRING_LENGTH);

	if (!this->p1_name[0])
		strcpy(this->p1_name, "Unknown 1");

	if (!this->p2_name[0])
		strcpy(this->p2_name, "Unknown 2");
}

void Session::start (char *p1_name, char *p2_name, int local_p, unsigned long peer)
{
	std::ostringstream line;

	match_p1 = 0;
	match_p2 = 0;
	matches_p1 = 0;
	matches_p2 = 0;
	matches = 0;
	total_p1 = 0;
	total_p2 = 0;
	total = 0;
	this->local_p = local_p;
	stage[0] = 0;
	p1_char[0] = 0;
	p2_char[0] = 0;
	active = true;
	set_names(p1_name, p2_name);

	line << "Starting game session as " << (local_p ? this->p2_name : this->p1_name) << " (Player " << (local_p + 1) << ") vs " << this->p2_name;
	if (peer)
		line << " (" << ip2str(peer) << ")";
	log_line(line.str());
}

void Session::match_start (char *stage, char *p1_char, char *p2_char)
{
	std::ostringstream line;
	match_p1 = 0;
	match_p2 = 0;

	line << "Starting match on " << stage << ", between " << p1_name << " (" << p1_char << ") vs " << p2_name << " (" << p2_char << ")";
	log_line(line.str());
}

void Session::p1_win ()
{
	std::ostringstream line;
	match_p1++;
	total_p1++;
	total++;

	line << "Round won by " << p1_name << " as " << p1_char << " (Player 1)";
	log_line(line.str());

	check_win();
}

void Session::p2_win ()
{
	std::ostringstream line;
	match_p2++;
	total_p2++;
	total++;

	line << "Round won by " << p2_name << " as " << p2_char  << " (Player 2)";
	log_line(line.str());

	check_win();
}

void Session::double_ko ()
{
	match_p1++;
	match_p2++;
	total_p1++;
	total_p2++;
	total++;

	log_line("Round ended in double K.O. (point for both)");

	check_win();
}

void Session::draw ()
{
	match_p1++;
	match_p2++;
	total_p1++;
	total_p2++;
	total++;

	log_line("Round ended in draw (point for both)");

	check_win();
}

void Session::end ()
{
	std::ostringstream line;

	line << "Session summary" << (match_p1 || match_p2 ? " (there was still a match in progress)" : "") << ":";
	write_line(line.str()); line.str("");
	line << "Player 1" << (local_p == 0 ? " (local)" : "") << ": " << p1_name << " won " << matches_p1 << " matches (" << total_p1 << " rounds)";
	write_line(line.str()); line.str("");
	line << "Player 2" << (local_p == 1 ? " (local)" : "") << ": " << p2_name << " won " << matches_p2 << " matches (" << total_p2 << " rounds)";
	write_line(line.str()); line.str("");
	line << "Total: " << matches << " matches with " << total << " rounds";
	write_line(line.str());

	active = false;
	matches_p1 = 0;
	matches_p2 = 0;
	matches = 0;
	total_p1 = 0;
	total_p2 = 0;
	total = 0;
	local_p = -1;
	strcpy(this->p1_name, "Unknown 1");
	strcpy(this->p1_name, "Unknown 1");
}
