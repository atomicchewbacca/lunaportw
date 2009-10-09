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

#ifndef STAGEMANAGER_H
#define STAGEMANAGER_H

#include <map>
#include <vector>

// not thread safe, but it doesn't need to be
class StageManager
{
	private:
		int stages;
		std::map<const unsigned int, bool> blacklist_map;
		std::vector<unsigned int> stage_map;

		void fill_stage_map ();
		void skip_nondigit (char **str_ptr);
		int next_int (char **str_ptr);

	public:
		StageManager ();
		void init (int max_stages);           // clear blacklist_map, set stages, initialize vector with 0..stages
		void blacklist (char *blacklist);     // parse blacklist string, fill blacklist_map, reinitialize vector with 0..stages, skipping blacklist_map entries
		bool ok ();                           // check if stage_map still contains stages
		unsigned int map (unsigned int rnd);  // look up stage in stage_map, mod stage_map.size()
};

#endif
