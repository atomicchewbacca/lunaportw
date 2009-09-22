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

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include <string.h>
#include "HTTP.h"
#include "LunaPort.h"

#define TERMINATE(x) {ret=(x);goto end;}

struct received_data
{
	size_t size;
	char *data;
};

size_t receive (void *ptr, size_t size, size_t nmemb, void *stream)
{
	struct received_data *rd = (struct received_data *)stream;
	size_t len = size * nmemb;
	char *data;

	rd->data = (char *)realloc(rd->data, rd->size + len);
	data = rd->data + rd->size;
	rd->size += len;
	memcpy(data, ptr, len);

	return len;
}

static const char *http_err_curl = "Could not initialize cURL.";
static const char *http_err_unkn = "Unknown error.";
static const char *http_err_http = "Could not establish HTTP connection.";
static const char *http_err_bad  = "Received bad or non-lobby data.";
static const char *http_err_prot = "Incompatible lobby protocol version.";
static const char *http_err_zero = "Did not receive data.";
static const char *lobb_err_ok   = "No error.";
static const char *lobb_err_vers = "Incompatible lobby protocol version.";
static const char *lobb_err_inpt = "Invalid lobby data input.";
static const char *lobb_err_db   = "Lobby database error.";
static const char *lobb_err_enc  = "Lobby requires client to accept gzip compressed data.";

const char *http_error (int e)
{
	switch (e)
	{
		case 0:
			return lobb_err_ok;
		case 1:
			return lobb_err_vers;
		case 2:
			return lobb_err_inpt;
		case 3:
			return lobb_err_db;
		case 4:
			return lobb_err_enc;
		case -1:
			return http_err_curl;
		case -2:
			return http_err_http;
		case -3:
			return http_err_bad;
		case -4:
			return http_err_prot;
		case -5:
			return http_err_zero;
	}
	return http_err_unkn;
}

void clean_string (char *s)
{
	for (int i = 0; i < NET_STRING_LENGTH && s[i]; i++)
	{
		if (s[i] < 0x20)
			s[i] = ' ';
	}
}

// >0 = lobby error
//  0 = ok
// -1 = curl error
// -2 = HTTP error
// -3 = bad data
// -4 = bad lobby protocol version
int data_set (char *url, unsigned int kgtcrc, unsigned int kgtsize, char *name, char *name2, char *comment, unsigned int port, int state, int *refresh, char *msg)
{
	char kgtcrc_str[9], kgtsize_str[32], port_str[32], state_str[32], version_str[32], lunaport_str[32];
	int ret = 0;
	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	struct received_data rd = {0, NULL};
	lplobby_head *response;

	curl = curl_easy_init();
	if (!curl)
		TERMINATE(-1)

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "LunaPort " VERSION);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rd);

	sprintf(version_str, "%u", LOBBY_VERSION);
	sprintf(lunaport_str, "%u", PROTOCOL_VERSION);
	sprintf(kgtcrc_str, "%08X", kgtcrc);
	sprintf(kgtsize_str, "%u", kgtsize);
	sprintf(port_str, "%u", port);
	sprintf(state_str, "%d", state);

	// lobby settings
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "version", CURLFORM_COPYCONTENTS, version_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "type", CURLFORM_COPYCONTENTS, "1", CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "lunaport", CURLFORM_COPYCONTENTS, lunaport_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "kgtcrc", CURLFORM_COPYCONTENTS, kgtcrc_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "kgtsize", CURLFORM_COPYCONTENTS, kgtsize_str, CURLFORM_END);

	// type 1 settings
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "name", CURLFORM_COPYCONTENTS, name, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "name2", CURLFORM_COPYCONTENTS, name2, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "comment", CURLFORM_COPYCONTENTS, comment, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "port", CURLFORM_COPYCONTENTS, port_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "state", CURLFORM_COPYCONTENTS, state_str, CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	res = curl_easy_perform(curl);
	if (res)
		TERMINATE(-2);

	// handle received data
	response = (lplobby_head *)rd.data;
	if (!rd.size)
		TERMINATE(-5);
	if (!(response->header[0] == 'L' && response->header[1] == 'P'))
		TERMINATE(-3);
	if (response->version != 1)
		TERMINATE(-4);
	*refresh = response->refresh < 1 ? *refresh : response->refresh;
	if (msg != NULL)
	{
		memcpy(msg, response->msg, NET_STRING_LENGTH);
		msg[NET_STRING_LENGTH] = 0;
		clean_string(msg);
	}
	TERMINATE(response->state);

	// cleanup
end:
	if (curl)
		curl_easy_cleanup(curl);
	if (formpost != NULL)
		curl_formfree(formpost);
	if (rd.data != NULL)
		free(rd.data);

	return ret;
}

// >0 = lobby error
//  0 = ok
// -1 = curl error
// -2 = HTTP error
// -3 = bad data
// -4 = bad lobby protocol version
int data_del (char *url, unsigned int kgtcrc, unsigned int kgtsize, unsigned int port, int *refresh)
{
	char kgtcrc_str[9], kgtsize_str[32], version_str[32], lunaport_str[32], port_str[32];
	int ret = 0;
	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	struct received_data rd = {0, NULL};
	lplobby_head *response;

	curl = curl_easy_init();
	if (!curl)
		TERMINATE(-1)

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "LunaPort " VERSION);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rd);

	sprintf(version_str, "%u", LOBBY_VERSION);
	sprintf(lunaport_str, "%u", PROTOCOL_VERSION);
	sprintf(kgtcrc_str, "%08X", kgtcrc);
	sprintf(kgtsize_str, "%u", kgtsize);
	sprintf(port_str, "%u", port);

	// lobby settings
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "version", CURLFORM_COPYCONTENTS, version_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "type", CURLFORM_COPYCONTENTS, "2", CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "lunaport", CURLFORM_COPYCONTENTS, lunaport_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "kgtcrc", CURLFORM_COPYCONTENTS, kgtcrc_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "kgtsize", CURLFORM_COPYCONTENTS, kgtsize_str, CURLFORM_END);

	// type 2 settings
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "port", CURLFORM_COPYCONTENTS, port_str, CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	res = curl_easy_perform(curl);
	if (res)
		TERMINATE(-2);

	// handle received data
	response = (lplobby_head *)rd.data;
	if (!rd.size)
		TERMINATE(-5);
	if (!(response->header[0] == 'L' && response->header[1] == 'P'))
		TERMINATE(-3);
	if (response->version != 1)
		TERMINATE(-4);
	*refresh = response->refresh < 1 ? *refresh : response->refresh;
	TERMINATE(response->state);

	// cleanup
end:
	if (curl)
		curl_easy_cleanup(curl);
	if (formpost != NULL)
		curl_formfree(formpost);
	if (rd.data != NULL)
		free(rd.data);

	return ret;
}

// >0 = lobby error
//  0 = ok
// -1 = curl error
// -2 = HTTP error
// -3 = bad data
// -4 = bad lobby protocol version
int data_get (char *url, unsigned int kgtcrc, unsigned int kgtsize, int *refresh, lplobby_head **result)
{
	char kgtcrc_str[9], kgtsize_str[32], version_str[32], lunaport_str[32];
	int ret = 0;
	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	struct received_data rd = {0, NULL};
	lplobby_head *response;
	int i, records;

	*result = NULL;

	curl = curl_easy_init();
	if (!curl)
		TERMINATE(-1)

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "LunaPort " VERSION);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rd);

	sprintf(version_str, "%u", LOBBY_VERSION);
	sprintf(lunaport_str, "%u", PROTOCOL_VERSION);
	sprintf(kgtcrc_str, "%08X", kgtcrc);
	sprintf(kgtsize_str, "%u", kgtsize);

	// lobby settings
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "version", CURLFORM_COPYCONTENTS, version_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "type", CURLFORM_COPYCONTENTS, "0", CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "lunaport", CURLFORM_COPYCONTENTS, lunaport_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "kgtcrc", CURLFORM_COPYCONTENTS, kgtcrc_str, CURLFORM_END);
	curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "kgtsize", CURLFORM_COPYCONTENTS, kgtsize_str, CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
	res = curl_easy_perform(curl);
	if (res)
		TERMINATE(-2);

	// handle received data
	if (!rd.size)
		TERMINATE(-5);
	response = (lplobby_head *)rd.data;
	if (!(response->header[0] == 'L' && response->header[1] == 'P'))
		TERMINATE(-3);
	if (response->version != 1)
		TERMINATE(-4);

	*refresh = response->refresh < 1 ? *refresh : response->refresh;
	*result = response;
	records = rd.size > sizeof(lplobby_head) ? (rd.size - sizeof(lplobby_head)) / sizeof(lplobby_record) : 0;
	response->n = MIN(response->n, records);
	response->msg[NET_STRING_LENGTH] = 0;
	clean_string(response->msg);
	for (i = 0; i < response->n; i++)
	{
		response->records[i].name[NET_STRING_LENGTH] = 0;
		clean_string(response->records[i].name);
		response->records[i].name2[NET_STRING_LENGTH] = 0;
		clean_string(response->records[i].name2);
		response->records[i].comment[NET_STRING_LENGTH] = 0;
		clean_string(response->records[i].comment);
		response->records[i].ip[NET_STRING_LENGTH] = 0;
		clean_string(response->records[i].ip);
	}

	TERMINATE(response->state);

	// cleanup
end:
	if (curl)
		curl_easy_cleanup(curl);
	if (formpost != NULL)
		curl_formfree(formpost);

	return ret;
}
