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

/**
clients lifetime:
  -make 1 tcp-socket (to connect to server)
  -make 1 udp-socket (to send to and receive from server)
  -make 1 thread to receive from server on udp-socket
      (write received data to queue of received objects)
  -make 1 thread to asynchronously update all game-objects
      (synchronize all objects with server)
  -send direction updates (if this has changed only)
  -send putbomb requests for local bombers
**/

#include "Client.h"

#include "Thread.h"
#include "Mutex.h"
#include "Event.h"
#include "GameConfig.h"
#include "Controller.h"
#include "Bomber.h"
#include "Map.h"
#include "MapTile.h"
#include "MapTile_Box.h"
#include "MapTile_Arrow.h"
#include "Observer.h"
#include "Bomb.h"
#include "Chat.h"
#include "Explosion.h"
#include "Bomber_Corpse.h"
#include "Extra.h"
#include "Disease.h"
#include "ClientSetup.h"
#include "ServerSetup.h"
#include "Timer.h"
#include "Menu.h"
#include "Utils.h"

#include <iostream>
#include <stdio.h>
#include <sstream>

#include <boost/asio.hpp>
#include <sys/time.h>

unsigned char client_colors[8][3]={{128, 0, 0},
                                   {0, 128, 0},
                                   {0, 0, 128},
                                   {128, 128, 0},
                                   {128, 0, 128},
                                   {0, 128, 128},
                                   {128, 128, 128},
                                   {255, 255, 255}};

//#define CLIENT_MESSAGES
//#define SHOW_CLIENT_TRAFFIC_STATISTICS

#ifdef CLIENT_MESSAGES
    #define CLIENTMSG(x...) Client::printt(); printf(x)
#else
    #define CLIENTMSG(x...)
#endif

Client::Client(ClanBomberApplication* app, const std::string &srv_name)
{
    cb_app=app;
    bombers_updated=false;
    receiving_thread=new Thread();
    updating_thread=new Thread;
    thread_mutex=new Mutex();
    update_event=new Event();
    last_error=new char[100];
    my_name=NULL;
    my_ip=NULL;
    my_tcp_socket = NULL;
    my_udp_socket = NULL;
    nr_of_clients=0;
    my_index=0;
    my_previous_client_index=0;
    server_name=strdup(srv_name.c_str());
    server_ip=NULL;
    start_game=false;
    current_client_info_index=0;
    bool no_error=true;
    new_map_started=false;
    end_current_game=false;
    server_frame_count=0;
    current_chat_message="";
    chat_message_arrived=false;
    last_chat_message_client_index=0;
    current_game_time=0;
    total_bytes_sent=0;
    total_bytes_received=0;
    remember_total_bytes_sent=0;
    remember_total_bytes_received=0;
    game_timer.reset();
    reset_traffic_statistics();
    maps_played=0;
    server_ping=0;
    server_ping_curr_request_nr=0;
#if __BYTE_ORDER == __BIG_ENDIAN
    big_endian=true;
#else
    big_endian=false;
#endif
    server_convert=false;
    host_fqdn=determine_fqdn();
    //printf("client determined fqdn <%s>.\n", host_fqdn);
    if(!init_my_name()) {
      std::cout<<"(!) clanbomber: error while initializing client name"<<std::endl;
        no_error=false;
    }
    if(no_error && !init_my_ip()) {
      std::cout<<"(!) clanbomber: error while initializing client ip"<<std::endl;
        no_error=false;
    }
    if(no_error && !init_tcp_socket()) {
      std::cout<<"(!) clanbomber: error while initializing client tcp socket"<<std::endl;
        no_error=false;
    }
    if(no_error && !init_udp_socket()) {
      std::cout<<"(!) clanbomber: error while initializing client udp socket"<<std::endl;
        no_error=false;
    }
    if(no_error && !connect_to_server()) {
      std::cout<<"(!) clanbomber: client error while connecting to server <"<<srv_name<<">"<<std::endl;
         no_error=false;
    }
    if(no_error) {
      std::cout<<"(+) clanbomber: client connected to server <"<<server_ip<<">"<<std::endl;
        delete last_error;
        last_error=NULL;
    }
    for(int i=0;i<NET_SERVER_MAX_CLIENTS;i++) {
        client_ips[i]=NULL;
        client_names[i]=NULL;
        client_alive[i]=false;
        client_convert[i]=false;
    }

    for(int i=0;i<PING_RESPONSE_QUEUE_LENGTH;i++) {
        server_ping_request_nr[i]=0;
        server_ping_timer[i].reset();
    }

    make_server_timer.reset();
    reset_keep_alive_timer();
    reset();
}

Client::~Client()
{
    delete receiving_thread;
    delete updating_thread;
    delete thread_mutex;
    delete update_event;
    delete last_error;
    delete my_name;
    delete my_ip;
    delete server_name;
    delete server_ip;
    delete my_tcp_socket;
    delete my_udp_socket;
    Chat::reset();
}

void Client::reset()
{
    bombers_updated=false;
    start_game=false;
    new_map_started=false;
    end_current_game=false;
    for(int i=0;i<NET_SERVER_MAX_CLIENTS;i++) {
        clients_ready_to_play[i]=false;
    }
}

int Client::receive_from_server_thread(void* param)
{
  //pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  CB_ThreadSetInstantKill();
    while(true) {
        ((Client*)param)->receive_from_server();
    }
    return 0;
}

void Client::receive_from_server()
{
    int buf[4096];
    int bufsize=sizeof(buf);
    boost::asio::ip::udp::endpoint sender_endpoint;
    size_t bytes_received = my_udp_socket
      ->receive_from(boost::asio::buffer(buf, bufsize), sender_endpoint);
    if(bytes_received>4 && bytes_received<bufsize) {
        for(int j=0;j<bytes_received/4;j++) {
            buf[j]=ntohl(buf[j]);
        }
        total_bytes_received+=bytes_received;
        keep_server_alive();
        //CLIENTMSG("--- client received bytes <%d> cframe <%d> fid <%04x> sframe <%d>\n",
        //          bytes_received, buf[0], buf[1], cb_app->get_server_frame_counter());

        update_event->lock();
        thread_mutex->lock();
        add_received_objects(bytes_received, buf);
        thread_mutex->unlock();
        update_event->signal();
        update_event->unlock();
    }
}

void Client::start_receiving_from_server()
{
  //receiving_thread->run((THREADPROC)&receive_from_server_thread, this);
  receiving_thread->run((THREADFUNCTION)&receive_from_server_thread, (void *)this);
}

int Client::update_objects_thread(void* param)
{
  //pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  CB_ThreadSetInstantKill();
    while(true) {
        ((Client*)param)->update_objects();
    }
    return 0;
}

void Client::update_objects()
{
    update_event->lock();
    update_event->wait();
    update_event->unlock();
    while(1) {
        thread_mutex->lock();
        if(received_objects.empty()) {
            thread_mutex->unlock();
            return;
        }
        int *recv_buf = received_objects.front();
        received_objects.pop_front();
        thread_mutex->unlock();
        int recv_buf_len=recv_buf[0];
        int frame_count=recv_buf[1];
        //CLIENTMSG("--- client recv bytes <%d> sframe <%d> cframe <%d>\n",
        //          recv_buf_len, frame_count, cb_app->get_server_frame_counter());
        if(server_frame_count<=frame_count) {
            server_frame_count=frame_count;
        }
        else {
            CLIENTMSG("!!!!!!! message received from server is too old !!!!!!!\n");
            delete recv_buf;
            return;
        }
        int bytes_read=8;
        int pos=0;
        ClanBomberApplication::lock();
        while((bytes_read-4)<recv_buf_len) {
            pos=0;
            int *tmp = &recv_buf[bytes_read/4];
            unsigned int server_message_id = tmp[pos++];
            if(server_message_id==NET_SERVER_GAME_START_ID) {
                recv_SERVER_GAME_START(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_END_OF_GAME_ID) {
                recv_SERVER_END_OF_GAME();
            }
            else if(server_message_id==NET_SERVER_PAUSE_GAME_ID) {
                recv_SERVER_PAUSE_GAME(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_END_OF_LEVEL_ID) {
                recv_SERVER_END_OF_LEVEL();
            }
            else if(server_message_id==NET_SERVER_END_GAME_SESSION_ID) {
                recv_SERVER_END_GAME_SESSION();
            }
            else if(server_message_id==NET_SERVER_START_NEW_LEVEL_ID) {
                recv_SERVER_START_NEW_LEVEL();
            }
            else if(server_message_id==NET_SERVER_FULL_BOMBER_CONFIG_ID) {
                recv_SERVER_FULL_BOMBER_CONFIG(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_CONFIG_ID) {
                recv_SERVER_CONFIG(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_ADD_BOMBER_ID) {
                recv_SERVER_ADD_BOMBER(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_UPDATE_BOMBER_ID) {
                recv_SERVER_UPDATE_BOMBER(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_OBSERVER_FLY_ID) {
                recv_SERVER_OBSERVER_FLY(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_OBJECT_FLY_ID) {
                recv_SERVER_OBJECT_FLY(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_ADD_BOMB_ID) {
                recv_SERVER_ADD_BOMB(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_BOMBER_DIE_ID) {
                recv_SERVER_BOMBER_DIE(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_UPDATE_MAP_ID) {
                recv_SERVER_UPDATE_MAP(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_ADD_EXPLOSION_ID) {
                recv_SERVER_ADD_EXPLOSION(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_DELETE_OBJECTS_ID) {
                recv_SERVER_DELETE_OBJECTS(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_UPDATE_BOMBER_SKILLS_ID) {
                recv_SERVER_UPDATE_BOMBER_SKILLS(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_ADD_BOMBER_CORPSE_ID) {
                recv_SERVER_ADD_BOMBER_CORPSE(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_EXPLOSION_SPLATTERED_CORPSE_ID) {
                recv_SERVER_EXPLOSION_SPLATTERED_CORPSE(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_MAPTILE_VANISH_ID) {
                recv_SERVER_MAPTILE_VANISH(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_MAPTILE_REFRESH_ID) {
                recv_SERVER_MAPTILE_REFRESH(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_ADD_EXTRA_ID) {
                recv_SERVER_ADD_EXTRA(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_LOOSE_DISEASE_ID) {
                recv_SERVER_LOOSE_DISEASE(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_INFECT_WITH_DISEASE_ID) {
                recv_SERVER_INFECT_WITH_DISEASE(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_CHAT_MESSAGE_ID) {
                recv_SERVER_CHAT_MESSAGE(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_OBJECT_FALL_ID) {
                recv_SERVER_OBJECT_FALL(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_GAIN_EXTRA_ID) {
                recv_SERVER_GAIN_EXTRA(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_DISCONNECT_CLIENT_ID) {
                recv_SERVER_DISCONNECT_CLIENT(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_MAP_REQUEST_ID) {
                recv_SERVER_MAP_REQUEST(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_HANDSHAKE_ID) {
                recv_SERVER_HANDSHAKE(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_CLIENT_INFO_ID) {
                recv_SERVER_CLIENT_INFO(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_KEEP_ALIVE_ID) {
                recv_SERVER_KEEP_ALIVE();
            }
            else if(server_message_id==NET_SERVER_PING_ID) {
                recv_SERVER_PING(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_CONNECT_CLIENT_ID) {
                recv_SERVER_CONNECT_CLIENT(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_BOMB_KICK_START_ID) {
                recv_SERVER_BOMB_KICK_START(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_BOMB_KICK_STOP_ID) {
                recv_SERVER_BOMB_KICK_STOP(tmp, &pos);
            }
            else if(server_message_id==NET_SERVER_CLIENT_READY_TO_PLAY_ID) {
                recv_SERVER_CLIENT_READY_TO_PLAY(tmp, &pos);
            }
            else {
                CLIENTMSG("------ client received unknown message_id !!!\n");
            }
            bytes_read+=(pos*4);
        }
        delete recv_buf;
        ClanBomberApplication::unlock();
    }
}

bool Client::server_started_new_map()
{
    bool tmp=new_map_started;
    new_map_started=false;
    return tmp;
}

void Client::start_updating_objects()
{
  //updating_thread->run((THREADPROC)&update_objects_thread, this);
  updating_thread->run((THREADFUNCTION)&update_objects_thread, this);
}

void Client::add_received_objects(int bytes_received, int* buf)
{
    int* tmp=new int[(bytes_received/4)+1];
    tmp[0]=bytes_received;
    memcpy(&(tmp[1]), buf, bytes_received);
    received_objects.push_back(tmp);
}

bool Client::init_tcp_socket()
{
  /*if(my_tcp_socket<0) {
    my_tcp_socket=socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
    int setopt_val=1; 
    setsockopt(my_tcp_socket, SOL_SOCKET, SO_REUSEADDR, &setopt_val, sizeof(setopt_val));
    setsockopt(my_tcp_socket, IPPROTO_TCP, TCP_NODELAY, &setopt_val, sizeof(setopt_val));
  }
  if(my_tcp_socket<0) {
    sprintf(last_error, "!!! creation of new tcp socket failed !!!");
    return false;
  }*/
  using boost::asio::ip::tcp;

  std::ostringstream net_server_tcp_port;
  net_server_tcp_port << NET_SERVER_TCP_PORT;

  tcp::resolver resolver(io_service);
  tcp::resolver::query query(tcp::v4(), server_name, net_server_tcp_port.str());
  tcp::endpoint endpoint = *resolver.resolve(query);

  my_tcp_socket = new tcp::socket(io_service);

  my_tcp_socket->connect(endpoint);

  //tcp::socket::reuse_address reuse_address(true);
  //my_tcp_socket->set_option(reuse_address);
  tcp::no_delay no_delay(true);
  my_tcp_socket->set_option(no_delay);

  return true;
}

//TODO remove this method
bool Client::connect_to_server()
{
  return true;
}

bool Client::init_udp_socket()
{
  /*if(my_udp_socket<0) {
        my_udp_socket=socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int setopt_val=1; 
        setsockopt(my_udp_socket, SOL_SOCKET, SO_REUSEADDR, &setopt_val, sizeof(setopt_val));
    }
    if(my_udp_socket<0) {
        sprintf(last_error, "!!! creation of new udp socket failed !!!");
        return false;
    }
    struct sockaddr_in my_addr;
    memset(&my_addr,0,sizeof(my_addr));
    my_addr.sin_family=PF_INET;
    my_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    my_addr.sin_port=htons(NET_CLIENT_UDP_PORT);
    int bres=bind(my_udp_socket, (struct sockaddr*)&my_addr, sizeof(struct sockaddr));
    if(bres<0) {
        sprintf(last_error, "!!! error while binding udp socket at port %d !!!", NET_CLIENT_UDP_PORT);
        return false;
    }
    struct hostent *he=NULL;
    he=gethostbyname(server_name);
    if(he==NULL) {
        sprintf(last_error, "!!! could not find host entity for '%s' !!!", server_name);
        return false;
    }
    memset((char*)&server_address, 0, sizeof(server_address));
    server_address.sin_family=PF_INET;
    server_address.sin_addr=*((struct in_addr *)he->h_addr);
    server_address.sin_port=htons(NET_SERVER_UDP_PORT);
    server_ip=strdup(inet_ntoa(server_address.sin_addr));*/
  using boost::asio::ip::udp;

  std::ostringstream net_server_udp_port;
  net_server_udp_port << NET_SERVER_UDP_PORT;

  udp::resolver resolver(io_service);
  udp::resolver::query query(udp::v4(), server_name, net_server_udp_port.str());
  receiver_endpoint = *resolver.resolve(query);

  udp::endpoint bind_endpoint;
  bind_endpoint.port(NET_CLIENT_UDP_PORT);

  server_ip = strdup(receiver_endpoint.address().to_string().c_str());

  my_udp_socket = new udp::socket(io_service);
  my_udp_socket->open(udp::v4());
  //udp::socket::reuse_address reuse_address(true);
  //my_udp_socket->set_option(reuse_address);
  my_udp_socket->bind(bind_endpoint);
  return true;
}

bool Client::init_my_name()
{
    char name[256]="";
    int hres=gethostname(name, 256);
    if(hres<0) {
        sprintf(last_error, "!!! could not get host name of localhost !!!");
        return false;
    }
    my_name=strdup(name);
    return true;
}

bool Client::init_my_ip()
{
    struct hostent *he=NULL;
    he=gethostbyname(my_name);
    if(he==NULL) {
        sprintf(last_error, "!!! could not find host entity for '%s' !!!", my_name);
        return false;
    }
    my_ip=strdup(inet_ntoa(*((struct in_addr*)he->h_addr)));
    return true;
}

int Client::get_my_index()
{
    return my_index;
}

char* Client::get_name()
{
    return my_name;
}

char* Client::get_ip()
{
    return my_ip;
}

char* Client::get_server_name()
{
    return server_name;
}

char* Client::get_server_ip()
{
    return server_ip;
}

char* Client::get_last_error()
{
    return last_error;
}

bool Client::server_started_game()
{
    thread_mutex->lock();
    bool sg=start_game;
    thread_mutex->unlock();
    return sg;
}

bool Client::is_updated()
{
    if(bombers_updated) {
        bombers_updated=false;
        return true;
    }
    return false;
}

void Client::end_the_game()
{
    end_current_game=true;
}

void Client::send_SERVER_BOMBER_CONFIG(bool enable, int skin, int team, int controller, int rel_pos, const char* name)
{
  CLIENTMSG("--- client sent SERVER_BOMBER_CONFIG\n");
  reset_keep_alive_timer();
  int namelen=strlen(name);
  int intnamelen=0;
  int* namebuf=pack_string(name, namelen, &intnamelen);
  int bufsize=4+intnamelen;
  int buf[bufsize];
  buf[0]=NET_SERVER_BOMBER_CONFIG_ID;
  buf[1]=((enable << 16) | skin);
  buf[2]=((team << 16) | controller);
  buf[3]=((rel_pos << 16) | namelen);
  memcpy(&buf[4], namebuf, intnamelen*4);
  delete namebuf;
  for(int j=0;j<bufsize;j++) {
    buf[j]=htonl(buf[j]);
  }
  size_t bytes_sent = my_udp_socket
    ->send_to(boost::asio::buffer(buf, bufsize*4), receiver_endpoint);
  total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_BOMBER_CONFIG(bool enable, int skin, int controller, int rel_pos, const char* name)
{
    CLIENTMSG("--- client sent CLIENT_BOMBER_CONFIG\n");
    reset_keep_alive_timer();
    int namelen=strlen(name);
    int intnamelen=0;
    int* namebuf=pack_string(name, namelen, &intnamelen);
    int bufsize=4+intnamelen;
    int buf[bufsize];
    buf[0]=NET_CLIENT_BOMBER_CONFIG_ID;
    buf[1]=((enable << 16) | skin);
    buf[2]=((controller << 16) | rel_pos);
    buf[3]=strlen(name); 
    memcpy(&buf[4], namebuf, intnamelen*4);
    delete namebuf;
    int addr_len=sizeof(struct sockaddr);
    for(int j=0;j<bufsize;j++) {
        buf[j]=htonl(buf[j]);
    }
    int bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(buf, bufsize*4), receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_BOMBER_DIR(int bomber_number, Direction updir)
{
    CLIENTMSG("--- client sent CLIENT_BOMBER_DIR\n");
    reset_keep_alive_timer();
    int bufsize=2;
    int buf[bufsize];
    buf[0]=NET_CLIENT_BOMBER_DIR_ID;
    buf[1]=((bomber_number << 16) | (updir+100));
    int addr_len=sizeof(struct sockaddr);
    for(int j=0;j<bufsize;j++) {
        buf[j]=htonl(buf[j]);            
    }
    int bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(buf, bufsize*4), receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_BOMBER_BOMB(int pwr, int obj_id) {
    CLIENTMSG("--- client sent CLIENT_BOMBER_BOMB\n");
    reset_keep_alive_timer();
    int bufsize=2;
    int addr_len=sizeof(struct sockaddr);
    int tmp[bufsize];
    tmp[0]=NET_CLIENT_BOMBER_BOMB_ID;
    tmp[1]=((pwr << 16) | obj_id);
    for(int j=0;j<bufsize;j++) {
        tmp[j]=htonl(tmp[j]);
    }
    int bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(tmp, bufsize*4), receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_CHAT_MESSAGE(int autom)
{
    CLIENTMSG("--- client sent CLIENT_CHAT_MESSAGE\n");
    /*int msglen=current_chat_message.length();
    if(msglen<1) {
        return;
	}*/
    if (current_chat_message.length() == 0) {
      return;
    }
    if (current_chat_message.find(" ", 0) != std::string::npos) {
      return;
    }

    /*bool sendit=false;
    
    char* msg=current_chat_message.c_str();
    for(int i=0;i<strlen(msg);i++) {
        if(msg[i]!=' ') {
            sendit=true;
            break;
        }
    }
    if(!sendit) {
        return;
	}*/
    reset_keep_alive_timer();
    int intmsglen=0;
    //int* msgbuf=pack_string(msg, msglen, &intmsglen);
    int* msgbuf=pack_string(current_chat_message.c_str(),
			    current_chat_message.length(),
			    &intmsglen);
    int addr_len=sizeof(struct sockaddr);
    int bufsize=intmsglen+3;
    int buf[bufsize];
    buf[0]=NET_CLIENT_CHAT_MESSAGE_ID;
    buf[1]=autom;
    //buf[2]=((msglen << 16) | intmsglen);
    buf[2]=((current_chat_message.length() << 16) | intmsglen);
    memcpy(&buf[3], msgbuf, intmsglen*4);
    delete msgbuf;
    for(int j=0;j<bufsize;j++) {
        buf[j]=htonl(buf[j]);
    }
    int bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(buf, bufsize*4), receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_DISCONNECT()
{
    CLIENTMSG("--- client sent CLIENT_DISCONNECT\n");
	int bufsize=1;
    int addr_len=sizeof(struct sockaddr);
    int tmp[bufsize];
    tmp[0]=NET_CLIENT_DISCONNECT_ID;
    for(int j=0;j<bufsize;j++) {
        tmp[j]=htonl(tmp[j]);
    }
    int bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(tmp, bufsize*4), receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_KEEP_ALIVE()
{
    if(send_keep_alive_timer.elapsed()<NET_CLIENT_MILLISECONDS_SEND_KEEP_ALIVE_PERIOD) {
        return;
    }
    reset_keep_alive_timer();
    CLIENTMSG("--- client sent CLIENT_KEEP_ALIVE\n");
    int bufsize=2;
    int addr_len=sizeof(struct sockaddr);
    int tmp[bufsize];
    tmp[0]=NET_CLIENT_KEEP_ALIVE_ID;
    thread_mutex->lock();
    server_ping_curr_request_nr++;
    cleanup_server_ping_requests();
    add_server_ping_request(server_ping_curr_request_nr);
    tmp[1]=server_ping_curr_request_nr;
    thread_mutex->unlock();
    for(int j=0;j<bufsize;j++) {
        tmp[j]=htonl(tmp[j]);
    }
    int bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(tmp, bufsize*4), receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_MAP_CHECKSUMS()
{
    CLIENTMSG("--- client sent CLIENT_MAP_CHECKSUMS\n");
    reset_keep_alive_timer();
    unsigned int* map_checksums=NULL;
    int map_nr=cb_app->map->get_map_data_checksums(&map_checksums);
	int bufsize=map_nr+2;
    int addr_len=sizeof(struct sockaddr);
    int tmp[bufsize];
    tmp[0]=NET_CLIENT_MAP_CHECKSUMS_ID;
    tmp[1]=map_nr;
    memcpy(&tmp[2], map_checksums, map_nr*4);
    for(int j=0;j<bufsize;j++) {
        tmp[j]=htonl(tmp[j]);
    }
    int bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(tmp, bufsize*4), receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_MAP_EXCHANGE(unsigned int map_checksum)
{
    CLIENTMSG("--- client sent CLIENT_MAP_EXCHANGE\n");
    reset_keep_alive_timer();
    int pos=0;
    int tmp[1024];
    int addr_len=sizeof(struct sockaddr);
    tmp[pos++]=NET_CLIENT_MAP_EXCHANGE_ID;
    cb_app->map->load_by_data_checksum(map_checksum);
    int intmaplen=0;
    int* mapbuf=pack_map(MAP_WIDTH, MAP_HEIGHT, &intmaplen, true);
    memcpy(&tmp[pos], mapbuf, intmaplen*4); 
    pos+=intmaplen;
    delete mapbuf;
    int namelen=cb_app->map->get_current_map()->get_name().length();  
    tmp[pos++]=namelen;
    int intnamelen=0;
    int* namebuf=pack_string(cb_app->map->get_current_map()->get_name().c_str(), namelen, &intnamelen);
    memcpy(&tmp[pos], namebuf, intnamelen*4);
    delete namebuf;
    pos+=intnamelen;
    int authorlen=cb_app->map->get_current_map()->get_author().length();
    tmp[pos++]=authorlen;
    int intauthorlen=0;
    int* authorbuf=pack_string(cb_app->map->get_current_map()->get_author().c_str(), authorlen, &intauthorlen);
    memcpy(&tmp[pos], authorbuf, intauthorlen*4);
    delete authorbuf;
    pos+=intauthorlen;
    int max_players=cb_app->map->get_current_map()->get_max_players();
    tmp[pos++]=max_players;
    cb_app->map->get_current_map()->read_bomber_positions();
    for(int k=0;k<max_players;k++) {
        CL_Vector bomber_pos=cb_app->map->get_bomber_pos(k);
        tmp[pos++]=(((int)(bomber_pos.x) << 16) | (int)(bomber_pos.y));
    }
    for(int j=0;j<pos;j++) {
        tmp[j]=htonl(tmp[j]);
    }
    int bytes_sent = my_udp_socket->send_to(boost::asio::buffer(tmp, pos*4),
                                            receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_HANDSHAKE()
{
    CLIENTMSG("--- client sent CLIENT_HANDSHAKE\n");
    reset_keep_alive_timer();
    int bufsize=3;
    int buf[bufsize];
    buf[0]=NET_CLIENT_HANDSHAKE_ID;
    buf[1]=NET_PROTOCOL_VERSION;
    if(big_endian) {
        buf[2]=1;
    }
    else {
        buf[2]=0;
    }
    int addr_len=sizeof(struct sockaddr);
    for(int j=0;j<bufsize;j++) {
        buf[j]=htonl(buf[j]);
    }
    int bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(buf, bufsize*4), receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::send_CLIENT_READY_TO_PLAY(bool ready)
{
    CLIENTMSG("--- client sent CLIENT_READY_TO_PLAY\n");
    reset_keep_alive_timer();
    int bufsize=2;   
    int buf[bufsize];
    buf[0]=NET_CLIENT_READY_TO_PLAY_ID;
    buf[1]=(int)ready;
    int addr_len=sizeof(struct sockaddr);
    for(int j=0;j<bufsize;j++) {
        buf[j]=htonl(buf[j]);
    }
    int bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(buf, bufsize*4), receiver_endpoint);
    total_bytes_sent+=bytes_sent;
}

void Client::reset_keep_alive_timer()
{
    send_keep_alive_timer.reset();
}

void Client::stop_receiving_from_server()
{
    receiving_thread->stop();
}

void Client::stop_updating_objects()
{
    updating_thread->stop();
}

void Client::refresh_maptiles()
{
    ClanBomberApplication::lock();
    while(!refreshed_maptiles.empty()) {
        MapTile* m=refreshed_maptiles.front();
        refreshed_maptiles.pop_front();
        if(m!=NULL) {
            cb_app->map->set_maptile(m->get_x()/40, m->get_y()/40, m);
        }
    }
    ClanBomberApplication::unlock();
}

bool Client::end_game()
{
    return end_current_game;
}

int Client::get_server_frame_counter()
{
    return server_frame_count;
}

void Client::reset_new_chat_message_arrived()
{
    chat_message_arrived=false;
}

bool Client::new_chat_message_arrived()
{
    return chat_message_arrived;
}

int Client::get_last_chat_message_client_index()
{
    return last_chat_message_client_index;
}

int* Client::pack_string(const char* name, int bytelen, int* intlen)
{
    int* trans=new int[(bytelen/4)+1];
    int int_nr=0;
    for(int i=0;i<bytelen;i+=4) {
        if((i+1)>=bytelen) {
            trans[int_nr]=((name[i]) << 24);
        }
        else if((i+2)>=bytelen) {
            trans[int_nr]=(((name[i]) << 24) | ((name[i+1]) << 16));
        }
        else if((i+3)>=bytelen) {
            trans[int_nr]=(((name[i]) << 24) | ((name[i+1]) << 16) | ((name[i+2]) << 8));
        }
        else {
            trans[int_nr]=(((name[i]) << 24) | ((name[i+1]) << 16) | ((name[i+2]) << 8) | (name[i+3]));
        }
        int_nr++;
    }
    (*intlen)=int_nr;
    return trans;
}

char* Client::unpack_string(int* namebuf, int bytelen, int* intlen)
{
    char* trans=new char[bytelen+1];
    int int_nr=0;
    for(int i=0;i<bytelen;i+=4) {
        if((i+1)>=bytelen) {
            trans[i]=((namebuf[int_nr]) >> 24);
        }
        else if((i+2)>=bytelen) {
            trans[i]=((namebuf[int_nr]) >> 24);
            trans[i+1]=((namebuf[int_nr] & 0x00FF0000) >> 16);
        }
        else if((i+3)>=bytelen) {
            trans[i]=((namebuf[int_nr]) >> 24);
            trans[i+1]=((namebuf[int_nr] & 0x00FF0000) >> 16);
            trans[i+2]=((namebuf[int_nr] & 0x0000FF00) >> 8);
        }
        else {
            trans[i]=((namebuf[int_nr]) >> 24);
            trans[i+1]=((namebuf[int_nr] & 0x00FF0000) >> 16);
            trans[i+2]=((namebuf[int_nr] & 0x0000FF00) >> 8);
            trans[i+3]=(namebuf[int_nr] & 0x000000FF);
        }
        int_nr++;
    }
    (*intlen)=int_nr;
    trans[bytelen]='\0';
    return trans;
}

char Client::get_maptile_type(int i, bool with_random)
{
    char ret_type=0;
    if(with_random && cb_app->map->get_current_map()->is_random(i % MAP_WIDTH, i / MAP_WIDTH)) {
        ret_type=(77+100);
    }
    else {
        ret_type=(char)(cb_app->map->get_maptile(i % MAP_WIDTH, i / MAP_WIDTH)->get_type()+100);
    }
    return ret_type;
}

char Client::get_maptile_dir(int i)
{
    int j=(i % MAP_WIDTH);
    int k=(i / MAP_WIDTH);
    if(cb_app->map->get_maptile(j, k)->get_type()==MapTile::ARROW) {
        return (char)(((MapTile_Arrow*)(cb_app->map->get_maptile(j, k)))->get_direction()+100);
    }
    else {
        return (char)(DIR_NONE+100);
    }
}
 
int* Client::pack_map(int width, int height, int* intlen, bool with_random)
{
    int bytelen=width*height;
    int* trans=new int[(bytelen/2)+1];
 	int int_nr=0;
    for(int i=0;i<bytelen;i+=2) {
        if((i+1)>=bytelen) {
            trans[int_nr]=((get_maptile_type(i, with_random) << 24) | (get_maptile_dir(i) << 16));
        }
        else {
            trans[int_nr]=((get_maptile_type(i, with_random) << 24) | (get_maptile_dir(i) << 16) | (get_maptile_type(i+1, with_random) << 8) | get_maptile_dir(i+1));
        }
        int_nr++;
    }
    (*intlen)=int_nr;
    return trans;
}

void Client::goto_next_client_info_index()
{
    int i;
    for(i=0;i<(NET_SERVER_MAX_CLIENTS-1);i++) {
        if(client_alive[i]) {
            break;
        }
    }
    current_client_info_index=i;
}

bool Client::current_client_info_index_is_mine()
{
    return (current_client_info_index==my_index);
}

void Client::delete_current_client_info_index()
{
    client_alive[current_client_info_index]=false;
}

void Client::disconnect_from_server()
{
    if(server_keep_alive_timer.elapsed()>NET_CLIENT_MILLISECONDS_KEEP_SERVER_ALIVE)
    {
        server_frame_count=0;
        if(current_client_info_index_is_mine()) {
            cb_app->set_pause_game(false);
	    std::cout<<"(!) clanbomber: server has disconnected, i am the server now..."<<std::endl;
            ClanBomberApplication::lock();
            stop_receiving_from_server();
            stop_updating_objects();
            ClanBomberApplication::unlock();
            cb_app->set_client_disconnected_from_server(true);
            cb_app->set_client_connecting_to_new_server(false);
            keep_server_alive();
            end_the_game();
            ClientSetup::end_session();
        }
        else {
            delete_current_client_info_index();
            goto_next_client_info_index();
            keep_server_alive();
        }
    }
}

void Client::keep_server_alive()
{
    server_keep_alive_timer.reset();
}

void Client::init_server_from_client_info()
{
    for(int i=0;i<NET_SERVER_MAX_CLIENTS;i++) {
        if(client_names[i]!=NULL && client_ips[i]!=NULL) {
            ClanBomberApplication::get_server()->add_client(client_names[i], client_ips[i]);
        }
    }
}

void Client::reset_traffic_statistics()
{
    remember_total_bytes_sent=total_bytes_sent;
    remember_total_bytes_received=total_bytes_received;
    level_timer.reset(); 
    game_timer.reset();
}

void Client::update_traffic_statistics()
{
    // ?
}

void Client::output_traffic_statistics()
{
#ifdef SHOW_CLIENT_TRAFFIC_STATISTICS
    if(!ClanBomberApplication::is_server()) {
        current_game_time+=(int)(game_timer.elapsed()/1000);
        int map_time_seconds=(int)(level_timer.elapsed()/1000);
        int map_bytes_sent=total_bytes_sent-remember_total_bytes_sent;
        int map_bytes_received=total_bytes_received-remember_total_bytes_received;
        cout<<"*** client: traffic statistics (map nr."<<maps_played<<") ***"<<endl;
        cout<<"    client: total game time     : "<<current_game_time<<" s ("<<(int)(current_game_time/60)<<" min.)"<<endl;
        cout<<"    client: total bytes sent    : "<<total_bytes_sent<<" ("<<(int)(total_bytes_sent/1024)<<" kb)"<<endl;
        cout<<"    client: total bytes received: "<<total_bytes_received<<" ("<<(int)(total_bytes_received/1024)<<" kb)"<<endl;
        cout<<"    client: map time            : "<<map_time_seconds<<" s"<<endl;
        cout<<"    client: map bytes sent      : "<<map_bytes_sent<<" (avg. "<<(int)(map_bytes_sent/map_time_seconds)<<" b/s)"<<endl;
        cout<<"    client: map bytes received  : "<<map_bytes_received<<" (avg. "<<(int)(map_bytes_received/map_time_seconds)<<" b/s)"<<endl;
    }
#endif
}

void Client::increase_maps_played() 
{
    maps_played++;
}

char* Client::get_new_server_name()
{
    return client_names[current_client_info_index];
}

char* Client::get_new_server_ip()
{
    return client_ips[current_client_info_index];
}

int Client::get_previous_client_index()
{
    return my_previous_client_index;
}

void Client::send_old_bomber_config_to_new_server(int previous_client_index)
{
    for(int i=0;i<8;i++) {
        if(Config::bomber[i].get_client_index()==previous_client_index) {
            if(ClanBomberApplication::is_server()) {
                send_SERVER_BOMBER_CONFIG(true,
                                          Config::bomber[i].get_skin(),
                                          Config::bomber[i].get_team(),
                                          Config::bomber[i].get_controller(),
                                          i,
                                          Config::bomber[i].get_name().c_str());
            }
            else if(ClanBomberApplication::is_client()) {
                send_CLIENT_BOMBER_CONFIG(true,
                                          Config::bomber[i].get_skin(),
                                          Config::bomber[i].get_controller(),
                                          i,
                                          Config::bomber[i].get_name().c_str());
            }
        }
    }
}

int Client::get_client_color_r_by_index(int client_index)
{
    return client_colors[client_index % 8][0];
}

int Client::get_client_color_g_by_index(int client_index)
{
    return client_colors[client_index % 8][1];
}

int Client::get_client_color_b_by_index(int client_index)
{
    return client_colors[client_index % 8][2];
}

int Client::get_server_ping()
{
    return server_ping;
}

void Client::recv_SERVER_GAME_START(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_GAME_START\n");
    my_index=data[(*pos)++];
    start_game=true;
    if(!ClanBomberApplication::is_server()) {
	cb_app->delete_all_game_objects();
        if(cb_app->get_observer()==NULL) {   
            cb_app->make_observer();
        }
        Chat::hide();
    }
    std::cout<<"(+) clanbomber: server started game.."<<std::endl;
}

void Client::recv_SERVER_END_OF_GAME()
{
    CLIENTMSG("------ client received SERVER_END_OF_GAME\n");
    end_current_game=true;
    if(!ClanBomberApplication::is_server()) {
        Chat::hide();
    }
}

void Client::recv_SERVER_PAUSE_GAME(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_PAUSE_GAME\n");
    bool paused=(bool)data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        cb_app->set_pause_game(paused);
    }
}

void Client::recv_SERVER_END_OF_LEVEL()
{
    CLIENTMSG("------ client received SERVER_END_OF_LEVEL\n");
    if(!ClanBomberApplication::is_server()) {
        cb_app->get_observer()->set_client_game_runs(false);
        increase_maps_played();
        output_traffic_statistics();
    }
}

void Client::recv_SERVER_END_GAME_SESSION()
{
    CLIENTMSG("------ client received SERVER_END_GAME_SESSION\n");
    if(!ClanBomberApplication::is_server()) {
        ClientSetup::end_session();
    }
}

void Client::recv_SERVER_START_NEW_LEVEL()
{
    CLIENTMSG("------ client received SERVER_START_NEW_LEVEL\n");
    if(!ClanBomberApplication::is_server()) {
        if(cb_app->get_observer()==NULL) {   
            cb_app->make_observer();
        }
        Chat::hide();
        cb_app->get_observer()->set_client_game_runs(true);
        cb_app->map->is_received_by_client=true;
        new_map_started=true;
        cb_app->get_observer()->set_offset(10, 2);
        cb_app->get_observer()->set_pos(0, 0);
        Timer::reset();
        cb_app->get_observer()->reset_round_time();
        //CL_Iterator<Bomber> bomber_object_counter(cb_app->bomber_objects);
	for(std::list<Bomber*>::iterator bomber_object_iter = cb_app->bomber_objects.begin();
	    bomber_object_iter != cb_app->bomber_objects.end();
	    bomber_object_iter++) {
	  (*bomber_object_iter)->reset();
	  (*bomber_object_iter)->set_pos(350, 270);
	   (*bomber_object_iter)->controller->deactivate();
	}
        if(start_game) {
            ClanBomberApplication::signal();
            start_game=false;
        }
        reset_traffic_statistics();
    }
}

void Client::recv_SERVER_FULL_BOMBER_CONFIG(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_FULL_BOMBER_CONFIG\n");
    my_index=data[(*pos)++];
    for(int i=0;i<8;i++) {
        int enableindex=data[(*pos)++];
        int enable=(enableindex >> 16);
        int index=(enableindex & 0x0000FFFF)-100;
        int skinteam=data[(*pos)++];
        int skin=(skinteam >> 16);
        int team=(skinteam & 0x0000FFFF);
        int controllerserverbomber=data[(*pos)++];
        int controller=(controllerserverbomber >> 16);
        int server_bomber=(controllerserverbomber & 0x0000FFFF);
        int name_len=data[(*pos)++];
        int intnamelen=0;
        char* name=unpack_string(&data[(*pos)], name_len, &intnamelen);
        (*pos)+=intnamelen;
        std::string cl_name=name;
        int ip_len=data[(*pos)++];
        int intiplen=0;
        char* ip=unpack_string(&data[(*pos)], ip_len, &intiplen);
        (*pos)+=intiplen;
        Config::bomber[i].set_client_index(index);
        Config::bomber[i].set_skin(skin);
        Config::bomber[i].set_team(team);
        Config::bomber[i].set_server_bomber(server_bomber);
        Config::bomber[i].set_name(cl_name);
        if(server_bomber) {
            Config::bomber[i].set_client_ip(get_server_ip());
        }
        else {
            Config::bomber[i].set_client_ip(ip);
        }
        if(enable && !Config::bomber[i].is_enabled()) {
            if(ClanBomberApplication::is_server()) {
                ServerSetup::select_player(i);
            }
            else {
                ClientSetup::select_player(i);
            }
        }
        else if(!enable && Config::bomber[i].is_enabled()) {
            if(ClanBomberApplication::is_server()) {
                ServerSetup::unselect_player(i);
            }
            else {
                ClientSetup::unselect_player(i);
            }
        }
        Config::bomber[i].set_enabled(enable);
        Config::bomber[i].set_local(my_index==index);
        Config::bomber[i].set_controller(controller);
        bombers_updated=true;
    }
}

void Client::recv_SERVER_CONFIG(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_CONFIG\n");
    int roundtimemaxskateboards=data[(*pos)++];
    int round_time=(roundtimemaxskateboards >> 16);
    int max_skateboards=(roundtimemaxskateboards & 0x0000FFFF);
    int maxpowermaxbombs=data[(*pos)++];
    int max_power=(maxpowermaxbombs >> 16);
    int max_bombs=(maxpowermaxbombs & 0x0000FFFF);
    int startbombsstartpower=data[(*pos)++];
    int start_bombs=(startbombsstartpower >> 16);
    int start_power=(startbombsstartpower & 0x0000FFFF);
    int startskateboardsstartkick=data[(*pos)++];
    int start_skateboards=(startskateboardsstartkick >> 16);
    int start_kick=(startskateboardsstartkick & 0x0000FFFF);
    int startglovebombs=data[(*pos)++];
    int start_glove=(startglovebombs >> 16); 
    int bombs=(startglovebombs & 0x0000FFFF);
    int powerskateboards=data[(*pos)++];
    int power=(powerskateboards >> 16);
    int skateboards=(powerskateboards & 0x0000FFFF);
    int kickglove=data[(*pos)++];
    int kick=(kickglove >> 16);
    int glove=(kickglove & 0x0000FFFF);
    int jointviagra=data[(*pos)++];
    int joint=(jointviagra >> 16);
    int viagra=(jointviagra & 0x0000FFFF);
    int koksbombcountdown=data[(*pos)++];
    int koks=(koksbombcountdown >> 16);
    int bomb_countdown=(koksbombcountdown & 0x0000FFFF);
    int bombdelaypointstowin=data[(*pos)++];
    int bomb_delay=(bombdelaypointstowin >> 16);
    int bomb_speed=(bombdelaypointstowin & 0x0000FFFF);
    int randommaprandompos=data[(*pos)++];
    int random_map=(randommaprandompos >> 16);
    int random_pos=(randommaprandompos & 0x0000FFFF);
    int points_to_win=data[(*pos)++];
    Config::set_round_time(round_time);
    Config::set_max_skateboards(max_skateboards); 
    Config::set_max_power(max_power);   
    Config::set_max_bombs(max_bombs);
    Config::set_start_bombs(start_bombs);
    Config::set_start_power(start_power);
    Config::set_start_skateboards(start_skateboards);
    Config::set_start_kick(start_kick);
    Config::set_start_glove(start_glove);
    Config::set_bombs(bombs);
    Config::set_power(power);
    Config::set_skateboards(skateboards);
    Config::set_kick(kick);
    Config::set_glove(glove);
    Config::set_joint(joint);
    Config::set_viagra(viagra);
    Config::set_koks(koks); 
    Config::set_bomb_countdown(bomb_countdown);
    Config::set_bomb_delay(bomb_delay);
    Config::set_bomb_speed(bomb_speed);   
    Config::set_random_map_order(random_map);
    Config::set_random_positions(random_pos);
    Config::set_points_to_win(points_to_win);
    Config::save();
    ClanBomberApplication::get_menu()->restore_options_values();
    ClanBomberApplication::get_menu()->save_common_options (0, true, true);
}

void Client::recv_SERVER_ADD_BOMBER(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_ADD_BOMBER\n");
    int bomber_to_add=data[(*pos)++];
    for(int j=0;j<bomber_to_add;j++) {
        int object_id=data[(*pos)++];
        int xy=data[(*pos)++];
        int colteam=data[(*pos)++];
        int numpoints=data[(*pos)++];
        if(!ClanBomberApplication::is_server()) {
            int posx=(xy >> 16);
            int posy=(xy & 0x0000FFFF);
            int col=(colteam >> 16);
            int team=(colteam & 0x0000FFFF);
            int number=(numpoints >> 16);
            int points=(numpoints & 0x0000FFFF);
            bool found=false;
            //CL_Iterator<Bomber> bomber_object_counter(cb_app->bomber_objects);
	    for(std::list<Bomber*>::iterator bomber_object_iter = cb_app->bomber_objects.begin();
		bomber_object_iter != cb_app->bomber_objects.end();
		bomber_object_iter++) {
	      if((*bomber_object_iter)->get_object_id()==object_id) {
		(*bomber_object_iter)->set_points(points);
		(*bomber_object_iter)->controller->deactivate();
		found=true;
		break;
	      }
            }
            if(!found) {
                CLIENTMSG("--------- client: adding bomber with id <%d>\n", object_id);
                Controller* con=Controller::create((Controller::CONTROLLER_TYPE)Config::bomber[number].get_controller());
                Bomber* b=new Bomber(posx, posy, (Bomber::COLOR)col, con, Config::bomber[number].get_name(), team, number, cb_app);
                b->set_object_id(object_id);
                b->set_points(points);
                cb_app->bomber_objects.push_back(b);
                b->controller->deactivate();
            }
        }
    }
    cb_app->bombers_received_by_client=true;
    ClanBomberApplication::signal();
}

void Client::recv_SERVER_UPDATE_BOMBER(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_UPDATE_BOMBER\n");
    int bomber_to_update=data[(*pos)++];
    for(int j=0;j<bomber_to_update;j++) {
        int xy=data[(*pos)++];
        int diroid=data[(*pos)++];
        if(!ClanBomberApplication::is_server()) {
            int px=(xy >> 16);
            int py=(xy & 0x0000FFFF);
            int dir_tmp=(diroid >> 16)-100;
            int object_id=(diroid & 0x0000FFFF);
            GameObject* obj=cb_app->get_object_by_id(object_id);  
            if(obj!=NULL && obj->get_type()==GameObject::BOMBER) {
                Bomber* b=(Bomber*)obj;
                if(!b->is_flying() && !b->is_falling()) {
                    b->set_pos(px, py);
                }
                b->set_server_dir((Direction)dir_tmp);
                if(dir_tmp!=DIR_NONE) {
                    b->set_dir((Direction)dir_tmp);
                }
            }
        }
    }
}

void Client::recv_SERVER_OBSERVER_FLY(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_OBSERVER_FLY\n");
    int xy=data[(*pos)++];
    int speed=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int px=(xy >> 16)-10000;
        int py=(xy & 0x0000FFFF)-10000;
        if(cb_app->get_observer()!=NULL) {
            if(cb_app->get_observer()->is_flying()) {
                cb_app->get_observer()->set_next_fly_job(px, py, speed);
            }
            else {
                cb_app->get_observer()->fly_to(px, py, speed);
            }
        }
    }
}

void Client::recv_SERVER_OBJECT_FLY(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_OBJECT_FLY\n");
    int xy=data[(*pos)++];
    int owoid=data[(*pos)++];
    int speed=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {  
        int px=(xy >> 16)-10000;
        int py=(xy & 0x0000FFFF)-10000;
        bool overwalls=(owoid >> 16);
        int object_id=(owoid & 0x0000FFFF);
        GameObject* obj=cb_app->get_object_by_id(object_id);
        if(obj!=NULL) {
            obj->set_fly_over_walls(overwalls);  
            if(obj->get_type()==GameObject::BOMBER) {
                obj->fly_to(px, py, speed);
                Bomber* b=(Bomber*)obj;
                b->controller->deactivate();
            }
            else {
                if(obj->is_flying()) {
                    obj->set_next_fly_job(px, py, speed);
                }
                else {
                    obj->fly_to(px, py, speed);
                }
            }
        }
        else {
            Bomb* b=cb_app->activate_suspended_client_bomb_by_id(object_id);
            if(b!=NULL) {
                b->set_fly_over_walls(overwalls);
                b->set_reactivation_frame_counter(server_frame_count);
                if(b->is_flying()) {
                    b->set_next_fly_job(px, px, speed);
                }
                else {
                    b->fly_to(px, py, speed);
                }
            }
        }
    }
}

void Client::recv_SERVER_ADD_BOMB(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_ADD_BOMB\n");
    int bomberxy=data[(*pos)++];
    int xy=data[(*pos)++];
    int pwrbid=data[(*pos)++];
    int bomb_id=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int bomber_x=(bomberxy >> 16);
        int bomber_y=(bomberxy & 0x0000FFFF);
        int bx=(xy >> 16);
        int by=(xy & 0x0000FFFF);  
        int pwr=(pwrbid >> 16);
        int object_id=(pwrbid & 0x0000FFFF);
        GameObject* bomber=cb_app->get_object_by_id(object_id);
        if(bomber!=NULL) {
            if(bomber_x>0 || bomber_y>0) {
                bomber->set_pos(bomber_x, bomber_y);
            }
            cb_app->map->get_maptile(bx, by)->bomb=NULL;
            cb_app->map->add_bomb(bx, by, pwr, (Bomber*)bomber);
            if(cb_app->map->get_maptile(bx, by)!=NULL &&
               cb_app->map->get_maptile(bx, by)->bomb!=NULL) {
                cb_app->map->get_maptile(bx, by)->bomb->set_object_id(bomb_id);
                cb_app->objects.push_back(cb_app->map->get_maptile(bx, by)->bomb);
                int x=bx;
                Resources::Game_putbomb()->play();
            }
        }
    }
}

void Client::recv_SERVER_BOMBER_DIE(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_BOMBER_DIE\n");
    int oidsnr=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int object_id=(oidsnr >> 16);
        int sprite_nr=(oidsnr & 0x0000FFFF);
        GameObject* obj=cb_app->get_object_by_id(object_id);
        if(obj!=NULL) {   
            if(obj->get_type()==GameObject::BOMBER) {
                Bomber* b=(Bomber*)obj;
                int x=b->get_x();
                b->set_pos(0, 0);
                b->set_dir(DIR_NONE);
                b->set_dead(); 
                b->set_sprite_nr(sprite_nr);
                Resources::Game_die()->play();
            }
        }
    }
}

void Client::recv_SERVER_UPDATE_MAP(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_UPDATE_MAP\n");
    int heightwidth=data[(*pos)++];
    int map_height=(heightwidth >> 16);
    int map_width=(heightwidth & 0x0000FFFF);
    if(!ClanBomberApplication::is_server()) {
        cb_app->objects.clear();
    }
    int maplen=map_width*map_height;
    int k=0;
    int l=0;
    for(int j=0;j<maplen;j+=2) {
        int m12td=data[(*pos)++];
        if(!ClanBomberApplication::is_server()) {
            if(j+1>=maplen) {
                int m1t=((m12td & 0xFF000000) >> 24)-100;
                int m1d=((m12td & 0x00FF0000) >> 16)-100;
                cb_app->map->set_maptile(k, l, MapTile::create((MapTile::TYPE)m1t, 40*k, 40*l, (Direction)m1d, cb_app));
            }
            else {
                int m1t=((m12td & 0xFF000000) >> 24)-100;
                int m1d=((m12td & 0x00FF0000) >> 16)-100;
                cb_app->map->set_maptile(k, l, MapTile::create((MapTile::TYPE)m1t, 40*k, 40*l, (Direction)m1d, cb_app));
                k++;
                if(k>=map_width) {
                    k=0;
                    l++;
                }
                int m2t=((m12td & 0x0000FF00) >> 8)-100;
                int m2d=(m12td & 0x000000FF)-100;
                cb_app->map->set_maptile(k, l, MapTile::create((MapTile::TYPE)m2t, 40*k, 40*l, (Direction)m2d, cb_app));
                k++;
                if(k>=map_width) {
                    k=0;
                    l++;
                }
            }
        }
    }
    int map_name_length=data[(*pos)++];
    int intnamelen=0;
    char* map_name=unpack_string(&data[(*pos)], map_name_length, &intnamelen);
    (*pos)+=intnamelen;
    if(!ClanBomberApplication::is_server()) {
        cb_app->map->current_server_map_name=map_name;
    }
    int bomber_to_update=data[(*pos)++];
    for(int j=0;j<bomber_to_update;j++) {
        int xy=data[(*pos)++];
        int pointsoid=data[(*pos)++];
        if(!ClanBomberApplication::is_server()) {
            int px=(xy >> 16);
            int py=(xy & 0x0000FFFF);
            int points=(pointsoid >> 16);
            int obj_id=(pointsoid & 0x0000FFFF);
            CLIENTMSG("--------- client: updating bomber with id <%d>\n", obj_id);
            GameObject* go=cb_app->get_object_by_id(obj_id);
            if(go!=NULL && go->get_type()==GameObject::BOMBER) {
                ((Bomber*)go)->set_orig(px*40, py*40);
                ((Bomber*)go)->set_points(points);
            }
            else CLIENTMSG("--------- client: bomber with id <%d> not found for update!\n", obj_id);
        }
    }
}

void Client::recv_SERVER_ADD_EXPLOSION(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_ADD_EXPLOSION\n");
    int xy=data[(*pos)++];
    int lengths=data[(*pos)++];
    int pwrbid=data[(*pos)++];
    int explosion_id=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int bx=(xy >> 16);
        int by=(xy & 0x0000FFFF);
        int leftlen=((lengths & 0xFF000000) >> 24);
        int rightlen=((lengths & 0x00FF0000) >> 16);
        int uplen=((lengths & 0x0000FF00) >> 8);
        int downlen=(lengths & 0x000000FF);
        int pwr=(pwrbid >> 16);
        int bomber_id=(pwrbid & 0x0000FFFF);
        GameObject* obj=cb_app->get_object_by_id(bomber_id);
        if(obj!=NULL) {
            Bomber* b=(Bomber*)obj;
            Explosion* expl=new Explosion(bx, by, pwr, b, cb_app);
            expl->set_lengths(leftlen, rightlen, uplen, downlen);
            expl->set_object_id(explosion_id);
            cb_app->objects.push_back(expl);
            cb_app->map->shake((int)(0.25f/Timer::time_elapsed()));
        }
    }
}

void Client::recv_SERVER_DELETE_OBJECTS(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_DELETE_OBJECTS\n");
    int nr_obj_ids=data[(*pos)++];
    for(int i=0;i<nr_obj_ids;i++) {
        int an_obj_id=data[(*pos)++];
        if(!ClanBomberApplication::is_server()) {
            CLIENTMSG("--------- client: deleting object id <%d>", an_obj_id);
            bool found=false;
            //CL_Iterator<GameObject> game_object_counter(cb_app->objects);
	    for(std::list<GameObject*>::iterator game_object_iter = cb_app->objects.begin();
		game_object_iter != cb_app->objects.end();
		game_object_iter++) {
	      if((*game_object_iter)->get_object_id()==an_obj_id) {
		CLIENTMSG(" (%p) ===> %s", (*game_object_iter), GameObject::objecttype2string((*game_object_iter)->get_type()));
		(*game_object_iter)->delete_me=true;
		if((*game_object_iter)->get_type()==GameObject::BOMB) {
		  (*game_object_iter)->get_maptile()->bomb=NULL;
		}
		found=true;
		break;
	      }
            }
            if(!found) {
	      //CL_Iterator<Bomber> bomber_object_counter(cb_app->bomber_objects);
	      for(std::list<Bomber*>::iterator bomber_object_iter = cb_app->bomber_objects.begin();
		  bomber_object_iter != cb_app->bomber_objects.end();
		  bomber_object_iter++) {
		if((*bomber_object_iter)->get_object_id()==an_obj_id) {
		  CLIENTMSG(" (%p) ===> %s", *bomber_object_iter, GameObject::objecttype2string((*bomber_object_iter)->get_type()));
		  (*bomber_object_iter)->delete_me=true;
		  found=true;
		  break;
		}
	      }
            }
            CLIENTMSG("\n");
        }
    }
}

void Client::recv_SERVER_UPDATE_BOMBER_SKILLS(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_UPDATE_BOMBER_SKILLS\n");
    int bomber_id=data[(*pos)++];
    int pointsdeaths=data[(*pos)++];
    int killscurbombs=data[(*pos)++];
    int bombsextrabombs=data[(*pos)++];
    int powerextrapower=data[(*pos)++];
    int skateboardsextraskateboards=data[(*pos)++];
    int speedabletokick=data[(*pos)++];
    int extrakickabletothrow=data[(*pos)++];
    int glovesextragloves=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int points=(pointsdeaths >> 16);
        int deaths=(pointsdeaths & 0x0000FFFF);
        int kills=(killscurbombs >> 16);
        int cur_bombs=(killscurbombs & 0x0000FFFF);
        int bombs=(bombsextrabombs >> 16);
        int extra_bombs=(bombsextrabombs & 0x0000FFFF);
        int power=(powerextrapower >> 16);
        int extra_power=(powerextrapower & 0x0000FFFF);
        int skateboards=(skateboardsextraskateboards >> 16);
        int extra_skateboards=(skateboardsextraskateboards & 0x0000FFFF);
        int speed=(speedabletokick >> 16);
        int able_to_kick=(speedabletokick & 0x0000FFFF);
        int extra_kick=(extrakickabletothrow >> 16);
        int able_to_throw=(extrakickabletothrow & 0x0000FFFF);
        int gloves=(glovesextragloves >> 16);
        int extra_gloves=(glovesextragloves & 0x0000FFFF);
        GameObject* obj=cb_app->get_object_by_id(bomber_id);
        if(obj!=NULL) {
            Bomber* bomber=(Bomber*)obj;
            bomber->set_points(points); 
            bomber->set_deaths(deaths); 
            bomber->set_kills(kills);   
            bomber->set_cur_bombs(cur_bombs);
            bomber->set_bombs(bombs);
            bomber->set_extra_bombs(extra_bombs);
            bomber->set_power(power);
            bomber->set_extra_power(extra_power);
            bomber->set_skateboards(skateboards);
            bomber->set_extra_skateboards(extra_skateboards);
            bomber->set_speed(speed);
            bomber->set_is_able_to_kick(able_to_kick);
            bomber->set_extra_kick(extra_kick); 
            bomber->set_is_able_to_throw(able_to_throw);
            bomber->set_gloves(gloves);
            bomber->set_extra_gloves(extra_gloves);
        }
    }
}

void Client::recv_SERVER_ADD_BOMBER_CORPSE(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_ADD_BOMBER_CORPSE\n");
    int bc_id=data[(*pos)++];
    int xy=data[(*pos)++];
    int colsprnr=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int px=(xy >> 16);
        int py=(xy & 0x0000FFFF);
        int col=(colsprnr >> 16);
        int spr=(colsprnr & 0x0000FFFF);
        Bomber_Corpse* bc=new Bomber_Corpse(px, py, col, spr, cb_app);
        bc->set_object_id(bc_id);
        cb_app->objects.push_back(bc);
    }
}

void Client::recv_SERVER_EXPLOSION_SPLATTERED_CORPSE(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_EXPLOSION_SPLATTERED_CORPSE\n");
    int object_id=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        GameObject* obj=cb_app->get_object_by_id(object_id);
        if(obj!=NULL) {
            if(obj->get_type()==GameObject::BOMBER_CORPSE) {
                Bomber_Corpse* bc=(Bomber_Corpse*)obj;
                bc->explode();
            }
        }
    }
}

void Client::recv_SERVER_MAPTILE_VANISH(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_MAPTILE_VANISH\n");
    int xy=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int x=(xy >> 16);
        int y=(xy & 0x0000FFFF);
        if(cb_app->map!=NULL) {
            if(cb_app->map->get_maptile(x, y)!=NULL) {
                cb_app->map->get_maptile(x, y)->vanish();
            }
        }
    }
}

void Client::recv_SERVER_MAPTILE_REFRESH(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_MAPTILE_REFRESH\n");
    int xy=data[(*pos)++];
    int td=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int x=(xy >> 16); 
        int y=(xy & 0x0000FFFF);
        MapTile::TYPE t=(MapTile::TYPE)((td >> 16)-100);
        Direction d=(Direction)((td & 0x0000FFFF)-100);
        if(cb_app->map!=NULL) {
            MapTile* m=MapTile::create(t, 40*x, 40*y, d, cb_app);
            if(m!=NULL) {
                //refreshed_maptiles.push_back(m);
                cb_app->map->set_maptile(m->get_x()/40, m->get_y()/40, m);
            }
        }
    }
}

void Client::recv_SERVER_ADD_EXTRA(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_ADD_EXTRA\n");
    int xy=data[(*pos)++];
    int etoid=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int px=(xy >> 16); 
        int py=(xy & 0x0000FFFF);
        Extra::ExtraType extra_type=(Extra::ExtraType)((etoid >> 16)-100);
        int object_id=(etoid & 0x0000FFFF);
        Extra* e=Extra::create(extra_type, px, py, cb_app);
        if(e!=NULL) {
            e->set_object_id(object_id);
            cb_app->objects.push_back(e);
        }
    }
}

void Client::recv_SERVER_LOOSE_DISEASE(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_LOOSE_DISEASE\n");
    int bomber_object_id=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        GameObject* obj=cb_app->get_object_by_id(bomber_object_id);
        if(obj!=NULL) {
            Bomber* b=(Bomber*)obj;
            b->delete_disease();
        }
    }
}

void Client::recv_SERVER_INFECT_WITH_DISEASE(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_INFECT_WITH_DISEASE\n");
    int biddt=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int bomber_object_id=(biddt >> 16);  
        int disease_type=(biddt & 0x0000FFFF)-100;
        GameObject* obj=cb_app->get_object_by_id(bomber_object_id);
        if(obj!=NULL) {
            Bomber* b=(Bomber*)obj;
            Disease::DiseaseType dt=(Disease::DiseaseType)disease_type;
            bool play_sound=(b->get_disease()==NULL || (b->get_disease()!=NULL && b->get_disease()->get_DiseaseType()!=dt));
            b->set_disease(Disease::create(dt, b), play_sound);
        }
    }
}

void Client::recv_SERVER_CHAT_MESSAGE(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_CHAT_MESSAGE\n");
    last_chat_message_client_index=data[(*pos)++];
    int autom=data[(*pos)++];
    int intlen_bytelen=data[(*pos)++];
    int chat_byte_msg_len=(intlen_bytelen >> 16);
    char* msg=new char[chat_byte_msg_len+3];
    if(!autom) {
        chat_message_arrived=true;
    }
    msg[0]=last_chat_message_client_index;
    msg[1]=autom;
    int intmsglen=0;
    char* name=unpack_string(&data[(*pos)], chat_byte_msg_len, &intmsglen);
    memcpy(&msg[2], name, chat_byte_msg_len);
    msg[chat_byte_msg_len+2]='\0';
    (*pos)+=intmsglen;
    Chat::add(msg);
}

void Client::recv_SERVER_OBJECT_FALL(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_OBJECT_FALL\n");
    int object_id=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        GameObject* obj=cb_app->get_object_by_id(object_id);
        if(obj!=NULL) { 
            obj->fall();
        }
    }
}

void Client::recv_SERVER_GAIN_EXTRA(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_GAIN_EXTRA\n");
    int bomber_object_id=data[(*pos)++];
    int etx=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int extra_type=(etx >> 16)-100;
        int x=(etx & 0x0000FFFF);   
        Resources::Extras_wow()->play();
        GameObject* obj=cb_app->get_object_by_id(bomber_object_id);
        if(obj!=NULL) {
            Bomber* b=(Bomber*)obj;
            if(extra_type==Extra::SKATEBOARD) {
                b->set_skateboards(b->get_skateboards()+1);
                b->set_extra_skateboards(b->get_extra_skateboards()+1);
                b->set_speed(b->get_speed()+40);
            }
            else if(extra_type==Extra::BOMB) { 
                b->set_bombs(b->get_bombs()+1);
                b->set_extra_bombs(b->get_extra_bombs()+1);
                b->set_cur_bombs(b->get_cur_bombs()+1);
            }
            else if(extra_type==Extra::GLOVE) {  
                b->set_gloves(b->get_gloves()+1);
                b->set_extra_gloves(b->get_extra_gloves()+1);
            }
            else if(extra_type==Extra::POWER) {
                b->set_power(b->get_power()+1);
                b->set_extra_power(b->get_extra_power()+1);
            }
            else if(extra_type==Extra::KICK) {
                b->set_is_able_to_kick(true);
                b->set_extra_kick(b->get_extra_kick()+1);
            }
        }
    }
}

void Client::recv_SERVER_DISCONNECT_CLIENT(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_DISCONNECT_CLIENT\n");
    int bomber_object_id=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        GameObject* obj=cb_app->get_object_by_id(bomber_object_id);
        if(obj!=NULL) {
            Bomber* b=(Bomber*)obj;
            b->set_disconnected();
        }
    }
}

void Client::recv_SERVER_MAP_REQUEST(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_MAP_REQUEST\n");
    int maps_nr=data[(*pos)++];
    for(int k=0;k<maps_nr;k++) {
        int map_checksum=data[(*pos)++];
        if(!ClanBomberApplication::is_server()) {
            send_CLIENT_MAP_EXCHANGE(map_checksum);
        }
    }
}

void Client::recv_SERVER_HANDSHAKE(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_HANDSHAKE\n");
    int server_protocol_version=data[(*pos)++];
    int server_big_endian=data[(*pos)++];
    if(big_endian!=server_big_endian) {
        server_convert=false;
    }
    if(!ClanBomberApplication::is_server()) {
        if(server_protocol_version!=NET_PROTOCOL_VERSION) {
	  std::cout<<"(!) clanbomber: connection to <"<<server_name<<"> refused, protocol version "<<server_protocol_version<<" required (have "<<NET_PROTOCOL_VERSION<<")."<<std::endl;
            ClientSetup::set_disconnected();
            ClientSetup::end_session();
        }
    }
    CLIENTMSG("------ SERVER_HANDSHAKE - proto(%d) convert(%d).\n", server_protocol_version, server_convert);
}

void Client::recv_SERVER_CLIENT_INFO(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_CLIENT_INFO\n");
    int ips_nr=data[(*pos)++];
    for(int i=0;i<ips_nr;i++) {
        int client_index=data[(*pos)++];
        int iplen=data[(*pos)++];
        int intiplen=0;
        char* an_ip=unpack_string(&data[(*pos)], iplen, &intiplen);
        (*pos)+=intiplen;
        int namelen=data[(*pos)++];
        int intnamelen=0;
        char* an_name=unpack_string(&data[(*pos)], namelen, &intnamelen);
        (*pos)+=intnamelen;
        if(!ClanBomberApplication::is_server()) {
            client_ips[client_index]=an_ip;
            client_names[client_index]=an_name;
            client_alive[client_index]=true;
        }
    }
    goto_next_client_info_index();
}

void Client::recv_SERVER_KEEP_ALIVE()
{
    CLIENTMSG("------ client received SERVER_KEEP_ALIVE\n");
}

void Client::recv_SERVER_PING(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_PING\n");
    int request_nr=data[(*pos)++];
    thread_mutex->lock();
    process_server_ping_response(request_nr);
    cleanup_server_ping_requests();
    thread_mutex->unlock();
}

void Client::recv_SERVER_CONNECT_CLIENT(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_CONNECT_CLIENT\n");
    int servers_previous_client_index=data[(*pos)++];
    int my_new_client_index=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
		stop_receiving_from_server();
        my_previous_client_index=my_index;
        my_index=my_new_client_index;
        current_client_info_index=servers_previous_client_index;
        cb_app->set_client_connecting_to_new_server(true);
        cb_app->set_client_disconnected_from_server(false);
        cb_app->set_pause_game(false);
        end_the_game();
        ClientSetup::end_session();
    }
}

void Client::recv_SERVER_BOMB_KICK_START(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_BOMB_KICK_START\n");
    int biddir=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int bomb_id=(biddir >> 16);
        Direction bomb_dir=(Direction)((biddir & 0x0000FFFF)-100);
        GameObject* obj=cb_app->get_object_by_id(bomb_id);
        if(obj!=NULL) {
            Bomb* b=(Bomb*)obj;
            b->set_dir(bomb_dir);
        }
    }
}

void Client::recv_SERVER_BOMB_KICK_STOP(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_BOMB_KICK_STOP\n");
    int bomb_id=data[(*pos)++];
    int xy=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int bx=(xy >> 16);
        int by=(xy & 0x0000FFFF);
        GameObject* obj=cb_app->get_object_by_id(bomb_id);
        if(obj!=NULL) {
            Bomb* b=(Bomb*)obj;
            b->set_stopped();
            b->set_pos(bx, by);
            b->set_dir(DIR_NONE);
        }
    }
}

void Client::recv_SERVER_CLIENT_READY_TO_PLAY(int* data, int* pos)
{
    CLIENTMSG("------ client received SERVER_CLIENT_READY_TO_PLAY\n");
    int indexready=data[(*pos)++];
    if(!ClanBomberApplication::is_server()) {
        int client_index=(indexready >> 16);
        bool is_ready=(bool)(indexready & 0x0000FFFF);
        clients_ready_to_play[client_index]=is_ready;
    }
}

void Client::cleanup_server_ping_requests()
{
    for(int i=0;i<PING_RESPONSE_QUEUE_LENGTH;i++) {
        if(server_ping_request_nr[i] > 0 &&
           (server_ping_request_nr[i] > server_ping_curr_request_nr ||
            (server_ping_request_nr[i] + PING_RESPONSE_QUEUE_LENGTH) <
             server_ping_curr_request_nr))  {
            server_ping_request_nr[i]=0;
        }
    }
}

void Client::add_server_ping_request(int nr)
{
    for(int i=0;i<PING_RESPONSE_QUEUE_LENGTH;i++) {
        if(!server_ping_request_nr[i]) {
            server_ping_request_nr[i] = nr;
            server_ping_timer[i].reset();
            break;
        }
    }
}

void Client::process_server_ping_response(int nr)
{
    int queued=0;
    int ok=0;
    for(int i=0;i<PING_RESPONSE_QUEUE_LENGTH;i++) {
        if(server_ping_request_nr[i] == nr) {
            server_ping=(int)server_ping_timer[i].elapsedn();
            server_ping_request_nr[i]=0;
            //printf("found ping request nr.%d ---> ping <%f> <%d>\n", server_ping);
            bombers_updated = true;
            ok++;  
        }
        else if(server_ping_request_nr[i] != 0) {
            //printf("ping nr.%d remained\n", server_ping_request_nr[i]);
            queued++;  
        }
    }
    //printf("processed %d pings, %d remained - nr %d ping %d (frame=%d)\n",
    //       ok, queued, nr, server_ping, ClanBomberApplication::get_server_frame_counter());
}

void Client::printt(void)
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    int sec = (tv.tv_sec % 1000);
    int msec = (tv.tv_usec / 1000);
    int usec = (tv.tv_usec - (msec * 1000));
    printf ("%03d:%03d:%03d ", sec, msec, usec);
}


static char* check_host_entity (struct hostent *he)
{
  int             nr     = 0;
  struct hostent *he_tmp = 0;

  if (strchr (he->h_name, '.'))
    {
      return strdup (he->h_name);
    }

  nr = 0;
  while (he->h_aliases[nr])
    {
      if (strchr (he->h_aliases[nr], '.'))
        {
          return strdup (he->h_aliases[nr]);
        }

      nr++;
    }

  nr = 0;
  while (he->h_addr_list[nr])
    {
      he_tmp = gethostbyaddr (he->h_addr_list[nr], he->h_length, he->h_addrtype);
      if (!he_tmp)
        {
          nr++;
          continue;
        }

      if (strchr (he_tmp->h_name, '.'))
        {
          return strdup (he_tmp->h_name);
        }

      nr++;
    }

  return 0;
}


char* Client::determine_fqdn (void)
{
  char            tmp[256] = "";
  int             ret      = 0;
  struct hostent *he       = 0;
  struct hostent *he_tmp   = 0;
  char           *fqdn     = 0;

  ret = gethostname (tmp, 256);
  if (ret < 0)
    {
      return 0;
    }

  he = gethostbyname (tmp);
  if (!he)
    {
      /* no fqdn found, return the hostname */
      return strdup (tmp);
    }

  if (strchr (he->h_name, '.'))
    {
      return strdup (he->h_name);
    }

  ret = 0;
  while (he->h_aliases[ret])
    {
      if (strchr (he->h_aliases[ret], '.'))
        {
          return strdup (he->h_aliases[ret]);
        }

      ret++;
    }

  ret = 0;
  while (he->h_addr_list[ret])
    {
      he_tmp = gethostbyaddr (he->h_addr_list[ret], he->h_length, he->h_addrtype);
      if (!he_tmp)
        {
          ret++;
          continue;
        }

      fqdn = check_host_entity (he_tmp);
      if (fqdn)
        {
          return fqdn;
        }

      ret++;
    }

  /* no fqdn found, return the hostname */
  return strdup (tmp);
}
