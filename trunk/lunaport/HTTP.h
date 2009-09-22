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

#ifndef HTTP_H
#define HTTP_H

#include "LunaPort.h"

#ifdef _MSC_VER
#pragma warning (disable : 4200)
#pragma pack (push, 1)
#endif

typedef struct _lplobby_record
{
	char name[NET_STRING_BUFFER];
	char name2[NET_STRING_BUFFER];
	char comment[NET_STRING_BUFFER];
	char ip[NET_STRING_BUFFER];
	unsigned short port;
	unsigned char state;
	unsigned int refresh;
} PACKED_STRUCT lplobby_record;

typedef struct _lplobby_head
{
	char header[2];              // {'L', 'P'}
	unsigned short version;      // 1
	unsigned char state;         // response state or error code
	unsigned int n;              // number of records
	unsigned int refresh;        // send request every refresh seconds
	char msg[NET_STRING_BUFFER]; // greeting sent by lobby
	lplobby_record records[];    // records sent as response to request
} PACKED_STRUCT lplobby_head;

#ifdef _MSC_VER
#pragma pack (pop)
#endif

// call curl_global_init(CURL_GLOBAL_ALL); at startup
const char *http_error (int e);
int data_set (char *url, unsigned int kgtcrc, unsigned int kgtsize, char *name, char *name2, char *comment, unsigned int port, int state, int *refresh, char *msg);
int data_get (char *url, unsigned int kgtcrc, unsigned int kgtsize, int *refresh, lplobby_head **result); // result is a pointer to a pointer, should be NULL, will be alloced, must be freed
int data_del (char *url, unsigned int kgtcrc, unsigned int kgtsize, unsigned int port, int *refresh);

#endif

