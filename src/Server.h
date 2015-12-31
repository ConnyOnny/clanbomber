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

#ifndef _header_clanbomber_server_
#define _header_clanbomber_server_

#include <list>

#include <boost/asio.hpp>

class Mutex;
class Event;
class Thread;
class ClanBomberApplication;
class Bomber;
class GameObject;
class MapEntry;

#define NET_PROTOCOL_VERSION 8

#define NET_SERVER_ALLOW_SERVER_AI
#define NET_SERVER_ALLOW_CLIENT_AI

#define NET_SERVER_TCP_PORT 27316
#define NET_SERVER_UDP_PORT 27317
#define NET_CLIENT_UDP_PORT 27318

#define NET_SERVER_MAX_CLIENTS 23

#define NET_SERVER_BOMBER_CONFIG_ID 0x71987198
#define NET_SERVER_FULL_BOMBER_CONFIG_ID 0x71997199
#define NET_SERVER_GAME_START_ID 0x19293293
#define NET_SERVER_CONFIG_ID 0x31701701
#define NET_SERVER_ADD_BOMBER_ID 0x16123123
#define NET_SERVER_UPDATE_BOMBER_ID 0x16004004
#define NET_SERVER_OBSERVER_FLY_ID 0x29288928
#define NET_SERVER_OBJECT_FLY_ID 0x26188188
#define NET_SERVER_ADD_BOMB_ID 0x22149149
#define NET_SERVER_ADD_EXPLOSION_ID 0x20187187
#define NET_SERVER_BOMBER_DIE_ID 0x90029002
#define NET_SERVER_GAIN_EXTRA_ID 0x65416541
#define NET_SERVER_DELETE_OBJECTS_ID 0x11787787
#define NET_SERVER_ADD_BOMBER_CORPSE_ID 0x25190190
#define NET_SERVER_EXPLOSION_SPLATTERED_CORPSE_ID 0x56325632
#define NET_SERVER_MAPTILE_VANISH_ID 0x75317531
#define NET_SERVER_MAPTILE_REFRESH_ID 0x91729172
#define NET_SERVER_ADD_EXTRA_ID 0x17991991
#define NET_SERVER_LOOSE_DISEASE_ID 0x19292292
#define NET_SERVER_INFECT_WITH_DISEASE_ID 0x82348234
#define NET_SERVER_OBJECT_FALL_ID 0x75497549
#define NET_SERVER_UPDATE_MAP_ID 0x25301301
#define NET_SERVER_END_OF_GAME_ID 0x50075007
#define NET_SERVER_PAUSE_GAME_ID 0x78467846
#define NET_SERVER_END_OF_LEVEL_ID 0x60176017
#define NET_SERVER_END_GAME_SESSION_ID 0x87328732
#define NET_SERVER_START_NEW_LEVEL_ID 0x56395639
#define NET_SERVER_UPDATE_BOMBER_SKILLS_ID 0x77147714
#define NET_SERVER_CHAT_MESSAGE_ID 0x51245124
#define NET_SERVER_DISCONNECT_CLIENT_ID 0x65496549
#define NET_SERVER_MAP_REQUEST_ID 0x72877287
#define NET_SERVER_HANDSHAKE_ID 0x87618761
#define NET_SERVER_CLIENT_INFO_ID 0x00910091
#define NET_SERVER_KEEP_ALIVE_ID 0x07522077
#define NET_SERVER_PING_ID 0x77222037
#define NET_SERVER_CONNECT_CLIENT_ID 0x78673478
#define NET_SERVER_BOMB_KICK_START_ID 0x76539833
#define NET_SERVER_BOMB_KICK_STOP_ID 0x75901783
#define NET_SERVER_CLIENT_READY_TO_PLAY_ID 0x87612439

#define NET_CLIENT_BOMBER_CONFIG_ID 0x71977197
#define NET_CLIENT_BOMBER_DIR_ID 0x10291029
#define NET_CLIENT_BOMBER_BOMB_ID 0x22742274
#define NET_CLIENT_CHAT_MESSAGE_ID 0x31001001
#define NET_CLIENT_DISCONNECT_ID 0x75627562
#define NET_CLIENT_KEEP_ALIVE_ID 0x07620762
#define NET_CLIENT_MAP_CHECKSUMS_ID 0x87308730
#define NET_CLIENT_MAP_EXCHANGE_ID 0x87468746
#define NET_CLIENT_HANDSHAKE_ID 0x29862986
#define NET_CLIENT_READY_TO_PLAY_ID 0x98632482

#define NET_SERVER_PAUSE_MILLISECONDS_BETWEEN_MAPS 4777

#define NET_CLIENT_MILLISECONDS_SEND_KEEP_ALIVE_PERIOD 1777
#define NET_SERVER_MILLISECONDS_SEND_KEEP_ALIVE_PERIOD 1777
#define NET_SERVER_MILLISECONDS_KEEP_CLIENT_ALIVE 7777
#define NET_CLIENT_MILLISECONDS_KEEP_SERVER_ALIVE 7777

class SimpleTimer
{
    public: 
        SimpleTimer();
        ~SimpleTimer();  
        void reset();
        float elapsed();
        float elapsedn(); 
    private:
        struct timeval* start_time;
        struct timeval* current_time;
};

class Server
{
    public:
        Server(ClanBomberApplication* app);
        ~Server();
        void reset();
        bool init_tcp_socket();
        bool init_udp_socket();
        bool init_my_name();
        bool init_my_ip();
        static int accept_connection_thread(void* param);
        void accept_connection();
        void start_accepting_connections();
        void stop_accepting_connections();
        static int receive_from_client_thread(void* param);
        void receive_from_client();
        void start_receiving_from_clients();
        static int update_objects_thread(void* param);
        void update_objects();
        void start_updating_objects();
       	void add_received_objects(int client_nr, int bytes_received, int* buf);
       	bool add_client(char* client_name, char* client_ip);
        bool add_client(boost::asio::ip::tcp::socket *socket);
        char* get_name();
        char* get_ip();
        char* get_last_error();
        int get_nr_of_clients();
        char* get_client_location(int client_nr);
        //int get_client_index_by_ip(in_addr ip);
	int get_client_index_by_ip(const char *ip);
        char* get_client_ip_by_index(int index);
        void send_update_messages_to_clients(int current_frame_nr);
        void analyze_game_mode();
        bool is_in_demo_mode();
        void keep_client_alive(int client_index);
        void disconnect_dead_clients();
        void reset_traffic_statistics();
        void update_traffic_statistics();
        void output_traffic_statistics();
        void increase_maps_played();
        std::list<Bomber*> bomber_objects;
    private:
        void delete_client(int client_index);
        bool is_free_client_index(int client_index);
        int get_next_client_index(int last_index);
        int get_next_free_client_index();
        bool map_already_exists(const char* map_name);
        unsigned int merge_maps(int client_maps_nr, unsigned int* client_maps, unsigned int** new_maps);
        void set_mapentry_data(MapEntry* me, int x, int y, int type, int dir);
        void reset_keep_alive_timer(int client_index);
        void reset_keep_alive_timer();
    public:
        void send_SERVER_GAME_START();
        void send_SERVER_FULL_BOMBER_CONFIG();
        void send_SERVER_CONFIG();
        void send_SERVER_ADD_BOMBER();
        void send_SERVER_OBSERVER_FLY(int px, int py, int speed);
        void send_SERVER_OBJECT_FLY(int px, int py, int speed, bool overwalls, int object_id);
        void send_SERVER_UPDATE_BOMBER();
        void send_SERVER_UPDATE_BOMBER(int bx, int by, int bdir, int bomber_id);
        void send_SERVER_ADD_BOMB(int bomber_x, int bomber_y, int bx, int by, int pwr, int bomber_id, int bomb_id);
        void send_SERVER_BOMBER_DIE(int obj_id, int sprite_nr);
        void send_SERVER_UPDATE_MAP();
        void send_SERVER_CHAT_MESSAGE(int* msg_buf);
        void send_SERVER_END_OF_GAME();
        void send_SERVER_PAUSE_GAME(bool paused);
        void send_SERVER_END_OF_LEVEL();
        void send_SERVER_END_GAME_SESSION();
        void send_SERVER_START_NEW_LEVEL();
        void send_SERVER_ADD_EXPLOSION(int x, int y, int leftlen, int rightlen, int uplen, int downlen, int power, int bomber_id, int explosion_id);
        void send_SERVER_DELETE_OBJECTS(int nr, int* obj_ids);
        void send_SERVER_UPDATE_BOMBER_SKILLS(Bomber* bomber);
        void send_SERVER_ADD_BOMBER_CORPSE(int object_id, int x, int y, int color, int sprite_nr);
        void send_SERVER_EXPLOSION_SPLATTERED_CORPSE(int object_id);
        void send_SERVER_MAPTILE_VANISH(int x, int y);
        void send_SERVER_MAPTILE_REFRESH(int x, int y, int maptile_type, int dir);
        void send_SERVER_ADD_EXTRA(int object_id, int x, int y, int extra_type);
        void send_SERVER_LOOSE_DISEASE(int bomber_object_id);
        void send_SERVER_INFECT_WITH_DISEASE(int bomber_object_id, int disease_type);
        void send_SERVER_OBJECT_FALL(int obj_id);
        void send_SERVER_GAIN_EXTRA(int bomber_object_id, int extra_type, int x_coord);
        void send_SERVER_DISCONNECT_CLIENT(int bomber_object_id);
        void send_SERVER_MAP_REQUEST(int client_index, int new_maps_nr, unsigned int* new_maps);
        void send_SERVER_HANDSHAKE(int client_index);
        void send_SERVER_CLIENT_INFO();
        void send_SERVER_KEEP_ALIVE();
        void send_SERVER_PING(int client_index, int request_nr);
        void send_SERVER_CONNECT_CLIENT(int previous_client_index);
        void send_SERVER_BOMB_KICK_START(int bomb_id, int dir);
        void send_SERVER_BOMB_KICK_STOP(int bomb_id, int bx, int by);
        void send_SERVER_CLIENT_READY_TO_PLAY(int client_index, bool ready);
    private:
        void recv_SERVER_BOMBER_CONFIG(int* data, int* pos, int from_client_index);
        void recv_CLIENT_BOMBER_CONFIG(int* data, int* pos, int from_client_index);
        void recv_CLIENT_BOMBER_DIR(int* data, int* pos);
        void recv_CLIENT_BOMBER_BOMB(int* data, int* pos);
        void recv_CLIENT_CHAT_MESSAGE(int* data, int* pos, int from_client_index);
        void recv_CLIENT_DISCONNECT(int* data, int* pos, int from_client_index);
        void recv_CLIENT_KEEP_ALIVE(int* data, int* pos, int from_client_index);
        void recv_CLIENT_MAP_CHECKSUMS(int* data, int* pos, int from_client_index);
        void recv_CLIENT_MAP_EXCHANGE(int* data, int* pos, int from_client_index);
        void recv_CLIENT_HANDSHAKE(int* data, int* pos, int from_client_index);
        void recv_CLIENT_READY_TO_PLAY(int* data, int* pos, int from_client_index);
    private:
        ClanBomberApplication* cb_app;
        Thread* connection_thread;
        Thread* receiving_thread;
        Mutex* thread_mutex;
        Mutex* send_buffer_mutex;
	std::list<int*> received_objects[NET_SERVER_MAX_CLIENTS];
	std::list<int*> update_messages_to_send;
        Event* update_event;
        Thread* updating_thread;
        char* my_name;
        char* my_ip;
        char* client_names[NET_SERVER_MAX_CLIENTS];
        char* client_ips[NET_SERVER_MAX_CLIENTS];
        bool big_endian;
        bool client_convert[NET_SERVER_MAX_CLIENTS];
        boost::asio::io_service io_service;
	boost::asio::ip::tcp::acceptor *my_tcp_acceptor;
        boost::asio::ip::udp::socket *my_udp_socket;
        int nr_of_clients;
        //int client_tcp_sockets[NET_SERVER_MAX_CLIENTS];
        boost::asio::ip::tcp::socket *client_tcp_sockets[NET_SERVER_MAX_CLIENTS];
        //sockaddr* client_addresses[NET_SERVER_MAX_CLIENTS];
        boost::asio::ip::udp::endpoint *client_endpoint[NET_SERVER_MAX_CLIENTS];
        char* last_error;
        bool in_demo_mode;
        SimpleTimer client_keep_alive_timer[NET_SERVER_MAX_CLIENTS];
        SimpleTimer send_keep_alive_timer[NET_SERVER_MAX_CLIENTS];
        bool clients_ready_to_play[NET_SERVER_MAX_CLIENTS];
        SimpleTimer game_timer;
        SimpleTimer level_timer;
        unsigned int total_bytes_sent;
        unsigned int total_bytes_received;
        unsigned int remember_total_bytes_sent;
       	unsigned int remember_total_bytes_received;
        int maps_played;
        char* host_fqdn;
        int current_game_time;
        bool was_client_before;
};

#endif
