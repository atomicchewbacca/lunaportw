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

#include <windows.h>
#include <string.h>
#include "StageManager.h"
#include "LunaPort.h"

StageManager::StageManager ()
{
	this->stages = 0;
}

void StageManager::fill_stage_map ()
{
	stage_map.clear();

	for (unsigned int i = 0; i < stages; i++)
	{
		if (!blacklist_map.count(i))
			stage_map.push_back(i);
	}
}

void StageManager::init (int max_stages)
{
	stages = max_stages;
	blacklist_map.clear();
	fill_stage_map();
}

// i lied, it's a nondigit separated list
void StageManager::skip_nondigit (char **str_ptr)
{
	char *str = *str_ptr;
	while (*str && !isdigit(*str))
		str++;
	*str_ptr = str;
}

int StageManager::next_int (char **str_ptr)
{
	char *start, *end;

	skip_nondigit(str_ptr);
	start = *str_ptr;
	end = start;
	while(*end && isdigit(*end))
		end++;
	if (*end)
	{
		*str_ptr = end + 1;
		*end = 0;
	}
	else
		*str_ptr = end;
	if (start == end)
		return 0;
	else
		return atoi(start);
}

void StageManager::blacklist (char *blacklist)
{
	int stage;
	char *blacklist_copy = (char *)malloc(NET_STRING_BUFFER);
	char *tmp = blacklist_copy;
	ZeroMemory(blacklist_copy, sizeof(blacklist_copy));
	strncpy(blacklist_copy, blacklist, NET_STRING_LENGTH);
	while (*blacklist_copy)
	{
		stage = next_int(&blacklist_copy);
		if (stage > 0)
			blacklist_map[stage - 1] = true;
	}
	free(tmp);
	fill_stage_map();
}

bool StageManager::ok ()
{
	return !stage_map.empty();
}

unsigned int StageManager::map (unsigned int rnd)
{
	if (stage_map.empty())
		return 0;
	return stage_map[rnd % stage_map.size()];
}
