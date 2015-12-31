/*
 * This file is part of ClanBomber;
 * you can get it at "http://www.nongnu.org/clanbomber".
 *
 * Copyright (C) 2001-2004, 2007 mass
 * Copyright (C) 2009, 2010 Rene Lopez <rsl@members.fsf.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef _header_clanbomber_client_
#define _header_clanbomber_client_

#include <boost/asio.hpp>

#include "ClanBomber.h"
#include "Server.h"

#include <string>
#include <list>

class Mutex;
class Event;
class Thread;
class MapTile;

#define PING_RESPONSE_QUEUE_LENGTH  7
#define MENU_OPTIONS_NUMBER        30

typedef enum
{
    I_START_BOMBS    = 0,
    I_START_POWER    = 1,
    I_START_SKATES   = 2,
    I_START_KICK     = 3,
    I_START_GLOVE    = 4,
    I_BOMBS          = 5,
    I_POWER          = 6,
    I_SKATES         = 7,
    I_KICK           = 8,
    I_GLOVE          = 9,
    I_JOINT          = 10,
    I_VIAGRA         = 11,
    I_KOKS           = 12,
    I_MAX_BOMBS      = 13,
    I_MAX_POWER      = 14,
    I_MAX_SKATES     = 15,
    I_KIDS_MODE      = 16,
    I_CORPSE_PARTS   = 17,
    I_SHAKE          = 18,
    I_BOMB_COUNTDOWN = 19,
    I_BOMB_DELAY     = 20,
    I_BOMB_SPEED     = 21,
    I_RANDOM_POS     = 22,
    I_RANDOM_MAP     = 23,
    I_POINTS         = 24,
    I_ROUND_TIME     = 25,
    I_RANDOM_POS_NG  = 26,
    I_RANDOM_MAP_NG  = 27,
    I_POINTS_NG      = 28,
    I_ROUND_TIME_NG  = 29
} MenuOptionsIndex;

class Client
{
    public:
  Client(ClanBomberApplication* app, const std::string &server_name);
        ~Client();
        static int receive_from_server_thread(void* param);
        static void printt(void);
        void receive_from_server();
        void start_receiving_from_server();
        static int update_objects_thread(void* param);
        void update_objects();
        void start_updating_objects();
        void add_received_objects(int bytes_received, int* buf);
        bool init_tcp_socket();
        bool connect_to_server();
        bool init_udp_socket();
        bool init_my_name();
        bool init_my_ip();
        int get_my_index();
        char* get_name();
        char* get_ip();
        char* get_server_name();
        char* get_server_ip();
        char* get_last_error();
        void reset();
        bool server_started_game();
        bool is_updated();
        bool server_started_new_map();
        void reset_keep_alive_timer();
        void stop_receiving_from_server(); 
        void stop_updating_objects();
        void refresh_maptiles();
        bool end_game();
        void end_the_game();
        void reset_new_chat_message_arrived();
        bool new_chat_message_arrived();
        int get_server_frame_counter();
        int get_last_chat_message_client_index();
        std::string current_chat_message;
        static int* pack_string(const char* name, int bytelen, int* intlen);
        static char* unpack_string(int* namebuf, int bytelen, int* intlen);
        static char* determine_fqdn (void);
        int* pack_map(int width, int height, int* intlen, bool with_random=false);
        void disconnect_from_server();
        void keep_server_alive();
        void goto_next_client_info_index();
        bool current_client_info_index_is_mine();
        void delete_current_client_info_index();
        void init_server_from_client_info();
        void reset_traffic_statistics();
        void update_traffic_statistics();
        void output_traffic_statistics();
        void increase_maps_played();
        char* get_new_server_name();
        char* get_new_server_ip();
        int get_previous_client_index();
        void send_old_bomber_config_to_new_server(int previous_client_index);
        static int get_client_color_r_by_index(int client_index);
        static int get_client_color_g_by_index(int client_index);
        static int get_client_color_b_by_index(int client_index);
        int get_server_ping();
    public:
        void send_SERVER_BOMBER_CONFIG(bool enable, int skin, int team, int controller, int rel_pos, const char* name);
        void send_CLIENT_BOMBER_CONFIG(bool enable, int skin, int controller, int rel_pos, const char* name);
        void send_CLIENT_BOMBER_DIR(int bomber_number, Direction updir);
        void send_CLIENT_BOMBER_BOMB(int pwr, int obj_id);
        void send_CLIENT_CHAT_MESSAGE(int autom);
        void send_CLIENT_DISCONNECT();
        void send_CLIENT_KEEP_ALIVE();
        void send_CLIENT_MAP_CHECKSUMS();  
        void send_CLIENT_MAP_EXCHANGE(unsigned int map_checksum);
        void send_CLIENT_HANDSHAKE();
        void send_CLIENT_READY_TO_PLAY(bool ready);
    private:
        void recv_SERVER_GAME_START(int* data, int* pos);
        void recv_SERVER_END_OF_GAME();
        void recv_SERVER_PAUSE_GAME(int* data, int* pos);
        void recv_SERVER_END_OF_LEVEL();
        void recv_SERVER_END_GAME_SESSION();
        void recv_SERVER_START_NEW_LEVEL();
        void recv_SERVER_FULL_BOMBER_CONFIG(int* data, int* pos);
        void recv_SERVER_CONFIG(int* data, int* pos);
        void recv_SERVER_ADD_BOMBER(int* data, int* pos);
        void recv_SERVER_UPDATE_BOMBER(int* data, int* pos);
        void recv_SERVER_OBSERVER_FLY(int* data, int* pos);
        void recv_SERVER_OBJECT_FLY(int* data, int* pos);
        void recv_SERVER_ADD_BOMB(int* data, int* pos);
        void recv_SERVER_BOMBER_DIE(int* data, int* pos);
        void recv_SERVER_UPDATE_MAP(int* data, int* pos);
        void recv_SERVER_ADD_EXPLOSION(int* data, int* pos);
        void recv_SERVER_DELETE_OBJECTS(int* data, int* pos);
        void recv_SERVER_UPDATE_BOMBER_SKILLS(int* data, int* pos);
        void recv_SERVER_ADD_BOMBER_CORPSE(int* data, int* pos);
        void recv_SERVER_EXPLOSION_SPLATTERED_CORPSE(int* data, int* pos);
        void recv_SERVER_MAPTILE_VANISH(int* data, int* pos);
        void recv_SERVER_MAPTILE_REFRESH(int* data, int* pos);
        void recv_SERVER_ADD_EXTRA(int* data, int* pos);
        void recv_SERVER_LOOSE_DISEASE(int* data, int* pos);
        void recv_SERVER_INFECT_WITH_DISEASE(int* data, int* pos);
        void recv_SERVER_CHAT_MESSAGE(int* data, int* pos);
        void recv_SERVER_OBJECT_FALL(int* data, int* pos);
        void recv_SERVER_GAIN_EXTRA(int* data, int* pos);
        void recv_SERVER_DISCONNECT_CLIENT(int* data, int* pos);
        void recv_SERVER_MAP_REQUEST(int* data, int* pos);
        void recv_SERVER_HANDSHAKE(int* data, int* pos);
        void recv_SERVER_CLIENT_INFO(int* data, int* pos);
        void recv_SERVER_KEEP_ALIVE();
        void recv_SERVER_PING(int* data, int* pos);
        void recv_SERVER_CONNECT_CLIENT(int* data, int* pos);
        void recv_SERVER_BOMB_KICK_START(int* data, int* pos);
        void recv_SERVER_BOMB_KICK_STOP(int* data, int* pos);
        void recv_SERVER_CLIENT_READY_TO_PLAY(int* data, int* pos);
    private:
        char get_maptile_type(int i, bool with_random=false);
        char get_maptile_dir(int i);
        void cleanup_server_ping_requests();
        void add_server_ping_request(int nr);
        void process_server_ping_response(int nr);
        ClanBomberApplication* cb_app;
	std::list<int*> received_objects;
	std::list<MapTile*> refreshed_maptiles;
        Event* update_event;
        Thread* receiving_thread;
        Thread* updating_thread;
        Mutex* thread_mutex;
        int nr_of_clients;
        char* my_name;
        char* my_ip;
        char* server_name;
        char* server_ip;
        boost::asio::io_service io_service;
        boost::asio::ip::udp::endpoint receiver_endpoint;
        boost::asio::ip::tcp::socket *my_tcp_socket;
        boost::asio::ip::udp::socket *my_udp_socket;
        sockaddr_in server_address;
        char* last_error;
        int my_index;
        int my_previous_client_index;
        bool start_game;
        bool bombers_updated;
        bool new_map_started;
        bool end_current_game;
        int server_frame_count;
        bool chat_message_arrived;
        int last_chat_message_client_index;
        SimpleTimer send_keep_alive_timer;
        SimpleTimer server_keep_alive_timer;
        char* client_names[NET_SERVER_MAX_CLIENTS];
        char* client_ips[NET_SERVER_MAX_CLIENTS];
        bool client_alive[NET_SERVER_MAX_CLIENTS];
        bool clients_ready_to_play[NET_SERVER_MAX_CLIENTS];
        SimpleTimer make_server_timer;
        SimpleTimer game_timer;
        SimpleTimer level_timer;
        unsigned int total_bytes_sent;
        unsigned int total_bytes_received;
        unsigned int remember_total_bytes_sent;
        unsigned int remember_total_bytes_received;
        int maps_played;
        int server_ping;
        int server_ping_curr_request_nr;
        int server_ping_request_nr[PING_RESPONSE_QUEUE_LENGTH];
        SimpleTimer server_ping_timer[PING_RESPONSE_QUEUE_LENGTH];
        int current_game_time;
        int current_client_info_index;
        char* host_fqdn;
        bool big_endian;
        bool server_convert;
        bool client_convert[NET_SERVER_MAX_CLIENTS];
};

#endif
