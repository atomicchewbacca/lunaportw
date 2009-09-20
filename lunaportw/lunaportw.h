// lunaportw.h

extern FILE *logfile;                 // debug log file
extern FILE *bpprof;                  // breakpoint time profile file
extern HANDLE this_proc;              // LunaPort handle
extern HANDLE proc;                   // Vanguard Princess handle
extern HANDLE sem_recvd_input;        // new remote input has been received
extern HANDLE sem_ping_end;           // signal end of ping thread
extern HANDLE mutex_print;            // ensure prints are ordered/atomic
extern HANDLE mutex_history;          // lock changes to game	history
extern HANDLE event_running;          // game has started
extern HANDLE event_waiting;          // game is waiting
extern SOCKET sock;                   // socket used for communication
extern unsigned long remote_player;   // remote player
extern int local_p;                   // 0 = local p1, 1 = local p2
extern int delay;                     // delay of input requests
extern int lobby_flag;                // currently have registered game on lobby
extern InputHistory local_history;    // buffer for local input
extern InputHistory remote_history;   // buffer for remote input
extern char dir_prefix[_MAX_PATH];             // path prefix
extern unsigned int *game_history;             // history of the complete game session
extern unsigned int game_history_pos;          // current position in game history
extern unsigned int spec_pos;                  // maximum received position of this spectator
extern spec_map spectators;                    // map of spectators
extern int allow_spectators;                   // 0 = do not allow spectators
extern unsigned int port;                      // network port
extern unsigned int max_stages;                // number of stages
extern unsigned int set_max_stages;            // number of stages set in lunaport.ini
extern int rnd_seed;                           // seed of current game
extern FILE *write_replay;                     // write replay here
extern bool recording_ended;                   // set after faking a packet, this ensures it is not written to replay file
extern DWORD kgt_crc;                          // CRC32 of kgt file
extern long kgt_size;                          // size of kgt file
extern int blacklist_local;                    // 1 = use stage blacklist for local games too, 0 = don't
extern int terminate_ping_thread;
extern int check_exe;
extern int ask_spectate;
extern int keep_hosting;
extern int display_framerate;
extern int display_inputrate;
extern int display_names;
extern int display_lobby_comments;
extern char game_exe[_MAX_PATH], replays_dir[_MAX_PATH];
extern char own_name[NET_STRING_BUFFER], p1_name[NET_STRING_BUFFER], p2_name[NET_STRING_BUFFER];
extern char set_blacklist[NET_STRING_BUFFER], blacklist1[NET_STRING_BUFFER], blacklist2[NET_STRING_BUFFER];
extern char lobby_url[NET_STRING_BUFFER], lobby_comment[NET_STRING_BUFFER];
extern ConManager conmanager;
extern StageManager stagemanager;
extern Session session;
extern Lobby lobby;

extern int record_replay;
extern int ask_delay;

extern CLunaportwView *cur_logwin;

extern void l ();
extern void u ();
extern char *ip2str (unsigned long ip);
extern void update_history (unsigned int v);
extern void send_local_input (int v);
extern int local_handler (int v);
extern int remote_handler ();
extern void write_net_string (char *str, FILE *hnd);
extern void read_net_string (char *str, FILE *hnd);
extern void clean_filestring (char *str);
extern FILE *create_replay_file ();
extern FILE *open_replay_file (char *filename, int *seed);
extern bool replay_input_handler (void *address, HANDLE proc_thread, FILE *replay, int record_replay, int network, int single_player, int spectator);
extern void set_caption (void *p);
extern void send_foreground ();
extern int replay_control (int init);
extern int run_game (int seed, int network, int record_replay, char *filename, int spectator);
extern int create_sems ();
extern int close_sems ();
extern void set_delay (int d);
extern int get_ping (unsigned long peer);
extern void serve_spectators (void *p);
extern void keep_alive (unsigned long peer);
extern void bye_spectators ();
extern void spec_keep_alive (unsigned long peer);
extern void spec_receiver (AsyncPacket *packet);
extern void peer_receiver (AsyncPacket *packet);
extern void clear_socket ();
extern bool receive_string (char *buf, unsigned long peer);
extern bool send_string (char *buf, unsigned long peer);
extern void read_int (int *i);
extern void spec_handshake (unsigned long peer);
extern bool spec_accept_callback (SOCKADDR_IN peer, luna_packet *packet);
extern void spectate_game (char *ip_str, int port, int record_replay);
extern void join_game (char *ip_str, int port, int record_replay);
extern void ping_thread_loop (unsigned long peer);
extern void host_game (int seed, int record_replay, int ask_delay);
extern void read_config (unsigned int *port, int *record_replay, int *allow_spectators, unsigned int *max_stages, int *ask_delay,
	int *ask_spectate, int *display_framerate, int *display_inputrate, int *display_names, char *game_exe,
	char *own_name, char *set_blacklist, int *blacklist_local, int *check_exe, int *max_points,
	int *keep_session_log, char *session_log, char *lobby_url, char *lobby_comment, int *display_lobby_comments,
	int *keep_hosting, char *replays_dir);
extern void calc_crcs ();
extern void unregister_lobby ();
extern bool inifile_exists(const char *argv0);

extern unsigned int __stdcall lunaport_serve(void *_in);
