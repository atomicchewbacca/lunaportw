<?php
/* lobby.php is a LunaPort lobby web application
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

// some settings
$header_magic = "LP";
$version = 1;
$dbfile = "lplobby.sqlt"; // see .htaccess
$netstringlength = 512;
$msg = "Welcome to the lobby. Only games matching your LunaPort and game version will be listed."; // message sent to users on connect
// timeout settings, lower values get better behaviour, but more traffic and cpu usage on the server
$refresh = 60 * 2; // hosts reenter their games every 4min, must be >0
$timeout = 60 * 4; // remove games that weren't refreshed for 5min, must be >refresh

// only run lobby code when called by LunaPort
if (preg_match("/^LunaPort /", $_ENV['HTTP_USER_AGENT'])) {

// warnings in binary output are bad and there shouldn't be any
error_reporting(0);

// enable gzip compression, should help with long padding in name fields
header("Content-Encoding: gzip");
header("Content-Type: application/x-lunaport-lobby");
//ini_set("zlib.output_compression", "on");
ini_set("zlib.output_compression_level", "5");
ob_start("ob_gzhandler");

// code
$header = $header_magic.pack("v", $version);

// name = string to 512B + \0, name2 = string to 512B + \0, comment = string to 512B + \0, ip = string to 512B + \0, port = 2B, state = int to 1B, refresh = int to 4B
function format_record ($name, $name2, $comment, $ip, $port, $state, $refresh)
{
	global $netstringlength;
	// you see why we want gzip compression? but it makes usage on the c side much easier
	$record = str_pad($name, $netstringlength, "\0");
	$record .= "\0";
	$record .= str_pad($name2, $netstringlength, "\0");
	$record .= "\0";
	$record .= str_pad($comment, $netstringlength, "\0");
	$record .= "\0";
	$record .= str_pad($ip, $netstringlength, "\0");
	$record .= "\0";
	$record .= pack("v", $port);
	$record .= pack("c", $state);
	$record .= pack("V", $refresh);
	return $record;
}

function truncate ($string)
{
	global $netstringlength;
	if (strlen($string) <= $netstringlength)
	{
		return $string;
	}
	return substr_replace($string, '', $netstringlength);
}

// states:
// 0 = ok
// 1 = invalid version
// 2 = invalid input
// 3 = db error
// 4 = accept-encoding header must contain deflate or gzip
function format_header ($state, $records)
{
	global $header;
	global $refresh;
	global $msg;
	global $netstringlength;
	$record = $header;
	$record .= pack("c", $state);
	$record .= pack("V", $records);
	$record .= pack("V", $refresh);
	$record .= str_pad(truncate($msg), $netstringlength, "\0");
	$record .= "\0";
	return $record;
}

if (strcmp($_POST['version'], "$version"))
{
	echo format_header(1, 0);
	exit(0);
}

if (!(preg_match("/gzip/", $_ENV['HTTP_ACCEPT_ENCODING']) || preg_match("/deflate/", $_ENV['HTTP_ACCEPT_ENCODING'])))
{
	echo format_header(4, 0);
	exit(0);
}

// request types:
// 0 = get data
// 1 = set data
// 2 = delete
$type = (int)$_POST['type'];
if (!($type >= 0 && $type <= 2))
{
	echo format_header(2, 0);
	exit(0);
}

// get current time
$now = time();

try {
	// create database if it doesn't exist
	if (!file_exists($dbfile))
	{
		$dbh = new PDO("sqlite:$dbfile");
		$dbh->exec("CREATE TABLE games (name TEXT, name2 TEXT, comment TEXT, ip TEXT NOT NULL, port INT, state INT, time INT, kgtcrc VARCHAR(8), kgtsize INT, lunaport INT)");
		$dbh->exec("CREATE UNIQUE INDEX host ON games (ip, port)");
		$dbh->exec("CREATE INDEX timeidx ON games (time)");
		$dbh->exec("CREATE INDEX game ON games (kgtcrc, kgtsize, lunaport)");
		$dbh = null;
	}

	// open database
	$dbh = new PDO("sqlite:$dbfile");
	$dbh->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

	// define variables for SQL use
	$sql_oldest_time = $now - $timeout;
	$sql_name = "";
	$sql_name2 = "";
	$sql_comment = "";
	$sql_ip = "";
	$sql_port = 0;
	$sql_state = 255;
	$sql_time = $now;
	$sql_kgtcrc = "00000000";
	$sql_kgtsize = 0;
	$sql_lunaport = 0;

	// prepare SQL statements
	$cleanup = $dbh->prepare("DELETE FROM games WHERE time < :sql_oldest_time");
	$cleanup->bindParam(":sql_oldest_time", $sql_oldest_time, PDO::PARAM_INT);
	$data_get = $dbh->prepare("SELECT name, name2, comment, ip, port, state, time FROM games WHERE kgtcrc = :sql_kgtcrc AND kgtsize = :sql_kgtsize AND lunaport = :sql_lunaport");
	$data_get->bindParam(":sql_kgtcrc", $sql_kgtcrc, PDO::PARAM_STR);
	$data_get->bindParam(":sql_kgtsize", $sql_kgtsize, PDO::PARAM_INT);
	$data_get->bindParam(":sql_lunaport", $sql_lunaport, PDO::PARAM_INT);
	$data_set = $dbh->prepare("REPLACE INTO games (name, name2, comment, ip, port, state, time, kgtcrc, kgtsize, lunaport) VALUES (:sql_name, :sql_name2, :sql_comment, :sql_ip, :sql_port, :sql_state, :sql_time, :sql_kgtcrc, :sql_kgtsize, :sql_lunaport)");
	$data_set->bindParam(":sql_name", $sql_name, PDO::PARAM_STR);
	$data_set->bindParam(":sql_name2", $sql_name2, PDO::PARAM_STR);
	$data_set->bindParam(":sql_comment", $sql_comment, PDO::PARAM_STR);
	$data_set->bindParam(":sql_ip", $sql_ip, PDO::PARAM_STR);
	$data_set->bindParam(":sql_port", $sql_port, PDO::PARAM_INT);
	$data_set->bindParam(":sql_state", $sql_state, PDO::PARAM_INT);
	$data_set->bindParam(":sql_time", $sql_time, PDO::PARAM_INT);
	$data_set->bindParam(":sql_kgtcrc", $sql_kgtcrc, PDO::PARAM_STR);
	$data_set->bindParam(":sql_kgtsize", $sql_kgtsize, PDO::PARAM_INT);
	$data_set->bindParam(":sql_lunaport", $sql_lunaport, PDO::PARAM_INT);
	$data_del = $dbh->prepare("DELETE FROM games WHERE ip = :sql_ip AND port = :sql_port");
	$data_del->bindParam(":sql_ip", $sql_ip, PDO::PARAM_STR);
	$data_del->bindParam(":sql_port", $sql_port, PDO::PARAM_INT);

	// cleanup db
	$cleanup->execute();

	// check inputs
	if (strlen($_POST['kgtcrc']) != 8 || preg_match("/[^0-9A-F]/", $_POST['kgtcrc']) || 0 == (int)$_POST['kgtsize'] || 0 == (int)$_POST['lunaport'])
	{
		echo format_header(2, 0);
		exit(0);
	}
	$sql_kgtcrc = $_POST['kgtcrc'];
	$sql_kgtsize = (int)$_POST['kgtsize'];
	$sql_lunaport = (int)$_POST['lunaport'];

	// handle request
	if ($type == 0)
	{
		// get data
		$data_get->execute();

		$records = 0;
		$output = "";
		while ($row = $data_get->fetch())
		{
			$output .= format_record(truncate($row['name']), truncate($row['name2']), truncate($row['comment']), truncate($row['ip']), $row['port'], $row['state'], $now - $row['time']);
			$records++;
		}

		$output = format_header(0, $records).$output;
		echo $output;
		exit(0);
	}
	if ($type == 1)
	{
		// set data
		$sql_name = truncate($_POST['name']);
		$sql_name2 = truncate($_POST['name2']);
		$sql_comment = truncate($_POST['comment']);
		$sql_ip = truncate($_ENV['REMOTE_ADDR']);
		$sql_port = (int)$_POST['port'];
		$sql_state = (int)$_POST['state'];
		if (0 == $sql_port || !($sql_state >= 0 && $sql_state <= 1) || 1 != preg_match("/^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$/", $sql_ip))
		{
			echo format_header(2, 0);
			exit(0);
		}

		if (strlen($sql_name) == 0)
		{
			$sql_name = "Unknown";
		}
		if (strlen($sql_name2) == 0)
		{
			$sql_name2 = "Unknown";
		}

		$data_set->execute();
		echo format_header(0, 0);
		exit(0);
	}
	if ($type == 2)
	{
		// delete data
		$sql_ip = truncate($_ENV['REMOTE_ADDR']);
		$sql_port = (int)$_POST['port'];
		if (0 == $sql_port || 1 != preg_match("/^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$/", $sql_ip))
		{
			echo format_header(2, 0);
			exit(0);
		}
		$data_del->execute();

		echo format_header(0, 0);
		exit(0);
	}

	echo format_header(2, 0);
	exit(0);
}
catch(PDOException $e)
{
	echo format_header(3, 0);
	exit(0);
}

exit(0);
}

// otherwise output html
?><html><head><title>LunaPort Lobby v<?php echo $version ?></title></head><body><h1>LunaPort Lobby v<?php echo $version ?></h1><p><b>Lobby message:</b> <?php echo $msg;?></p><p><small>To use the lobby, please use LunaPort r47 or higher. You can not use the lobby with a regular webbrowser, I am sorry.</small></p></body></html>
