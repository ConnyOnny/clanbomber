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
servers lifetime:
  -make 1 tcp-socket (to accept client connections)
  -make 1 udp-socket (to send to and receive from clients)
  -make 1 thread to accept connections on tcp-socket
      (stop thread when game has started)
  -make 1 thread to receive from clients on udp-socket
      (write received data to queue of received objects)
  -make 1 thread to asynchronously update clients bombers
      (read data from queue of received objects)
  -send updates of game objects to clients
      (first write data to send to send-queue, all messages in that queue
       can be send with send_update_messages_to_clients() then, every frame)
**/

#include "Server.h"

#include "Thread.h"
#include "Mutex.h"
#include "Event.h"
#include "ClanBomber.h"
#include "GameConfig.h"
#include "Bomber.h"
#include "Bomb.h"
#include "Map.h"
#include "MapTile.h"
#include "MapTile_Arrow.h"
#include "MapEntry.h"
#include "Controller.h"
#include "Disease.h"
#include "Client.h"
#include "ServerSetup.h"

#include <iostream>
#include <stdio.h>

#include <sys/time.h>

//#define SERVER_MESSAGES
//#define SHOW_SERVER_TRAFFIC_STATISTICS

#ifdef SERVER_MESSAGES
    #define SERVERMSG(x...) Client::printt(); printf(x)
#else
    #define SERVERMSG(x...)
#endif

Server::Server(ClanBomberApplication* app)
{
    cb_app=app;
    receiving_thread=new Thread();
    connection_thread=new Thread();
    thread_mutex=new Mutex();
    send_buffer_mutex=new Mutex();
    updating_thread=new Thread;
    update_event=new Event();
    nr_of_clients=0;
    in_demo_mode=true;
    was_client_before=false;
    current_game_time=0;
    total_bytes_sent=0;
    total_bytes_received=0;
    remember_total_bytes_sent=0;   
    remember_total_bytes_received=0;
    game_timer.reset();
    reset_traffic_statistics();
    maps_played=0;
    my_name=NULL;
    my_ip=NULL;
    my_tcp_acceptor = NULL;
    my_udp_socket = NULL;
#if __BYTE_ORDER == __BIG_ENDIAN
    big_endian=true;
#else
    big_endian=false;
#endif
    host_fqdn=Client::determine_fqdn();
    //printf("server determined fqdn <%s>.\n", host_fqdn);
    last_error=new char[100];
    for(int i=0;i<NET_SERVER_MAX_CLIENTS;i++) {
        client_endpoint[i]=NULL;
        client_ips[i]=NULL;
        client_names[i]=NULL;
        //client_tcp_sockets[i]=-1;
	client_tcp_sockets[i] = NULL;
        client_convert[i]=false;
    }
    reset_keep_alive_timer();
    bool no_error=true;
    if(!init_my_name()) {
      std::cout<<"(!) clanbomber: error while initializing server name"<<std::endl;
        no_error=false;
    }
    if(no_error && !init_my_ip()) {
      std::cout<<"(!) clanbomber: error while initializing server ip"<<std::endl;
        no_error=false;
    }
    if(no_error && !init_tcp_socket()) {
      std::cout<<"(!) clanbomber: error while initializing server tcp socket"<<std::endl;
        no_error=false;
    }
    if(no_error && !init_udp_socket()) {
      std::cout<<"(!) clanbomber: error while initializing server udp socket"<<std::endl;
        no_error=false;
    }
    if(no_error) {
        start_accepting_connections();
        delete last_error;
        last_error=NULL;
    }
    reset();
}

Server::~Server()
{
    delete connection_thread;
    delete receiving_thread;
    delete updating_thread;
    delete last_error;
    delete thread_mutex;
    delete send_buffer_mutex;
    delete update_event;
    delete my_name;
    delete my_ip;
    delete my_tcp_acceptor;
    delete my_udp_socket;
    int i=-1;
    while(true) {
      i=get_next_client_index(i);
      if(i<0) {
	break;
      }
      delete client_endpoint[i];
      delete client_ips[i];
      delete client_names[i];
      delete client_tcp_sockets[i];
      client_tcp_sockets[i] = NULL;
    }
}

void Server::reset()
{
    for(int i=0;i<NET_SERVER_MAX_CLIENTS;i++) {
        clients_ready_to_play[i]=false;
    }
}

bool Server::init_tcp_socket()
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
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server_addr.sin_port=htons(NET_SERVER_TCP_PORT);
    int bres=bind(my_tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    printf("bres = %d",  bres);
    if(bres<0) {
        sprintf(last_error, "!!! error while binding tcp socket at port %d !!!", NET_SERVER_TCP_PORT);
        return false;
    }
    int lres=listen(my_tcp_socket, NET_SERVER_MAX_CLIENTS);
    if(lres<0) {
        sprintf(last_error, "!!! error while listening at tcp port %d !!!", NET_SERVER_TCP_PORT);
        return false;
	}*/
  using boost::asio::ip::tcp;
  tcp::endpoint endpoint(tcp::v4(), NET_SERVER_TCP_PORT);
  my_tcp_acceptor = new tcp::acceptor(io_service, endpoint);
  //my_tcp_acceptor->listen(NET_SERVER_MAX_CLIENTS);
  return true;
}

bool Server::init_udp_socket()
{
  /*if(my_udp_socket<0) {
        my_udp_socket=socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        int setopt_val=1; 
        setsockopt(my_udp_socket, SOL_SOCKET, SO_REUSEADDR, &setopt_val, sizeof(setopt_val));
	}*/
  using boost::asio::ip::udp;
  if(my_udp_socket == NULL) {
    udp::endpoint endpoint(udp::v4(), NET_SERVER_UDP_PORT);
    my_udp_socket = new udp::socket(io_service, endpoint);
    //udp::socket::reuse_address reuse_address(true);
    //my_udp_socket->set_option(reuse_address);
  }
  /* if(my_udp_socket<0) {
        sprintf(last_error, "!!! creation of new udp socket failed !!!");
        return false;
	}*/
  /*sockaddr_in my_addr;
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family=PF_INET;
    my_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    my_addr.sin_port=htons(NET_SERVER_UDP_PORT);
    int bres=bind(my_udp_socket, (struct sockaddr*)&my_addr, sizeof(struct sockaddr));
    if(bres<0) {
        sprintf(last_error, "!!! error while binding udp socket at port %d !!!", NET_SERVER_UDP_PORT);
        return false;
	}*/
    return true;
}

bool Server::init_my_name()
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

bool Server::init_my_ip()
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

int Server::accept_connection_thread(void* param)
{
  //pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  CB_ThreadSetInstantKill();
    while(true) {
        ((Server*)param)->accept_connection();
    }
    return 0;
}

void Server::accept_connection()
{
  //struct sockaddr_in client_addr;
  //socklen_t client_len=sizeof(client_addr);
  //int new_socket=accept(my_tcp_socket, (struct sockaddr*)&client_addr, &client_len);
  using boost::asio::ip::tcp;
  tcp::socket *new_socket = new tcp::socket(io_service);
  my_tcp_acceptor->accept(*new_socket);
  ClanBomberApplication::lock();
  if (new_socket != NULL) {
    bool ok=add_client(new_socket);
    if(ok) {
      send_SERVER_FULL_BOMBER_CONFIG();
      send_SERVER_CONFIG();
    }
  }
  ClanBomberApplication::unlock();
}

void Server::start_accepting_connections()
{
  //connection_thread->run((THREADPROC)&Server::accept_connection_thread, this);
  connection_thread->run((THREADFUNCTION)&Server::accept_connection_thread, this);
}

void Server::stop_accepting_connections()
{
    if(connection_thread!=NULL) {
        connection_thread->stop();
        delete connection_thread;
        connection_thread=NULL;
    }
}

int Server::receive_from_client_thread(void* param)
{
  //pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  CB_ThreadSetInstantKill();
  while(true) {
    ((Server*) param)->receive_from_client();
  }
  return 0;
}

void Server::receive_from_client()
{
  //sockaddr_in client_addr;
  //int bytes_received=-1;
    int buf[4096];
    int bufsize=sizeof(buf);
    //size_t addr_len=sizeof(struct sockaddr);
    //memset((char*)&client_addr, 0, addr_len);
    //int bytes_received=recvfrom(my_udp_socket, buf, bufsize, 0, (sockaddr*)&client_addr, &addr_len);
    boost::asio::ip::udp::endpoint remote_endpoint;
    //int bytes_received = my_udp_socket->receive(packet);
    size_t bytes_received = my_udp_socket
      ->receive_from(boost::asio::buffer(buf, bufsize), remote_endpoint);
    //if(bytes_received>0 && bytes_received<bufsize) {
    if(bytes_received > 0) {
      for(int j=0; j < bytes_received/4; j++) {
	buf[j]=ntohl(buf[j]);
      }
      //SERVERMSG("+++ server recv bytes <%d> id <%08x> sframe <%d>\n",
      //          bytes_received, buf[0], cb_app->get_server_frame_counter());
      //int client_index=get_client_index_by_ip(client_addr.sin_addr);
      //char *address_tmp = strdup(packet.getAddress().getStringAddress());
      //std::string address_tmp = packet.getAddress().getStringAddress();
      //int client_index = get_client_index_by_ip(address_tmp.c_str());
      int client_index = get_client_index_by_ip(remote_endpoint.address()
                                                .to_string().c_str());
      if(client_index>=0) {
	if(client_index!=0) {
	  total_bytes_received += bytes_received;
	}
	keep_client_alive(client_index);
	update_event->lock();
	thread_mutex->lock();
	add_received_objects(client_index, bytes_received, buf);
	thread_mutex->unlock();
	update_event->signal();
	update_event->unlock();
      }
    }
}

void Server::start_receiving_from_clients()
{
  //receiving_thread->run((THREADPROC)&Server::receive_from_client_thread, this);
  receiving_thread->run((THREADFUNCTION)&Server::receive_from_client_thread, this);
}

int Server::update_objects_thread(void* param)
{
  //pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  CB_ThreadSetInstantKill();
    while(true) {
        ((Server*)param)->update_objects();
    }
    return 0;
}

void Server::update_objects()
{
    update_event->lock();
    update_event->wait();
    update_event->unlock();
    int client_index=-1;
    int msg_len=0;
    while(true) {
        client_index=get_next_client_index(client_index);
        if(client_index<0) {
            break;
        }
        thread_mutex->lock();
        while (!received_objects[client_index].empty()) {
            int* tmp=received_objects[client_index].front();
            received_objects[client_index].pop_front();
            int pos=0;
            msg_len=tmp[pos++];
            //SERVERMSG("--- server recv bytes <%d> sframe <%d>\n",
            //          len, cb_app->get_server_frame_counter());
            unsigned int client_message_id=tmp[pos++];
            if(client_message_id==NET_SERVER_BOMBER_CONFIG_ID) {
                recv_SERVER_BOMBER_CONFIG(tmp, &pos, client_index);
            }
            else if(client_message_id==NET_CLIENT_BOMBER_CONFIG_ID) {
                recv_CLIENT_BOMBER_CONFIG(tmp, &pos, client_index);
            }
            else if(client_message_id==NET_CLIENT_BOMBER_DIR_ID) {
                recv_CLIENT_BOMBER_DIR(tmp, &pos);
            }
            else if(client_message_id==NET_CLIENT_BOMBER_BOMB_ID) {
                recv_CLIENT_BOMBER_BOMB(tmp, &pos);
            }
            else if(client_message_id==NET_CLIENT_CHAT_MESSAGE_ID) {
                recv_CLIENT_CHAT_MESSAGE(tmp, &pos, client_index);
            }
            else if(client_message_id==NET_CLIENT_DISCONNECT_ID) {
                recv_CLIENT_DISCONNECT(tmp, &pos, client_index);
            }
            else if(client_message_id==NET_CLIENT_KEEP_ALIVE_ID) {
                recv_CLIENT_KEEP_ALIVE(tmp, &pos, client_index);
            }
            else if(client_message_id==NET_CLIENT_MAP_CHECKSUMS_ID) {
                recv_CLIENT_MAP_CHECKSUMS(tmp, &pos, client_index);
            }
            else if(client_message_id==NET_CLIENT_MAP_EXCHANGE_ID) {
                recv_CLIENT_MAP_EXCHANGE(tmp, &pos, client_index);
            }
            else if(client_message_id==NET_CLIENT_HANDSHAKE_ID) {
                recv_CLIENT_HANDSHAKE(tmp, &pos, client_index);
            }
            else if(client_message_id==NET_CLIENT_READY_TO_PLAY_ID) {
                recv_CLIENT_READY_TO_PLAY(tmp, &pos, client_index);
            }
            else {
                SERVERMSG("++++++ server received unknown message_id !!!\n");
            }
            delete tmp;
        }

        thread_mutex->unlock();
    }
}

void Server::start_updating_objects()
{
  //updating_thread->run((THREADPROC)&Server::update_objects_thread, this);
  updating_thread->run((THREADFUNCTION)&Server::update_objects_thread, this);
}

void Server::add_received_objects(int client_nr, int bytes_received, int* buf)
{
    int* tmp=new int[(bytes_received/4)+1];
    tmp[0]=bytes_received;
    memcpy(&(tmp[1]), buf, bytes_received);
    received_objects[client_nr].push_back(tmp);
}

bool Server::add_client(char* client_name, char* client_ip)
{
  using boost::asio::ip::udp;
  int free_index = get_next_free_client_index();
  keep_client_alive(free_index);
    ///socklen_t sockaddr_size=sizeof(struct sockaddr);
  ///in_addr_t iat=inet_addr(client_ip);
  //if(iat==INADDR_NONE) {
  //printf("(!) clanbomber: server error while readding client <%s>.\n", client_name);
  //return false;
  //}
  //TODO delete this code
  ///sockaddr_in* act_addr_in=new sockaddr_in;
  ///memset((char*)act_addr_in, 0, sockaddr_size);
  ///act_addr_in->sin_addr.s_addr=iat;
  ///act_addr_in->sin_family=PF_INET;
  ///act_addr_in->sin_port=htons(NET_CLIENT_UDP_PORT);
  ///client_addresses[free_index]=(sockaddr*)act_addr_in;
  //udp::endpoint *endpoint = new udp::endpoint(udp::v4(), NET_CLIENT_UDP_PORT);
  udp::endpoint *endpoint = new udp::endpoint;
  boost::asio::ip::address address;
  address.from_string(client_ip);
  endpoint->address(address);
  endpoint->port(NET_CLIENT_UDP_PORT);
  client_endpoint[free_index] = endpoint;
  /* understand what you have done here! why setting client_tcp_socket=1 ?? */
  /* answer: this is used if the server shuts down for some reason, any     */
  /* client will become the new server then, no tcp sockets are used then;  */
  /* nevertheless, the socket has to be marked as 'accepted connection' ... */
  //client_tcp_sockets[free_index]=1;
  client_tcp_sockets[free_index] = new boost::asio::ip::tcp::socket(io_service);
  ///client_ips[free_index] = client_ip;
  client_ips[free_index] = strdup(client_ip);
  ///client_names[free_index] = client_name;
  client_names[free_index] = strdup(client_name);
  nr_of_clients++;
  printf("(+) clanbomber: server readded client <%s> (ip: %s).\n", endpoint->address().to_string().c_str() , client_name);
  return true;
}

bool Server::add_client(boost::asio::ip::tcp::socket *socket)
{
  using boost::asio::ip::udp;
    int free_index = get_next_free_client_index();
    keep_client_alive(free_index);
    client_tcp_sockets[free_index] = socket;
    //int setopt_val=1;
    //setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &setopt_val, sizeof(setopt_val));
    udp::endpoint *endpoint = new udp::endpoint;
    endpoint->address(socket->remote_endpoint().address());
    endpoint->port(NET_CLIENT_UDP_PORT);
    client_endpoint[free_index] = endpoint;
    //printf("(!) clanbomber: server error while adding client <%s>.\n", client_names[free_index]);
    client_ips[free_index] = strdup(socket->local_endpoint().address().to_string().c_str());
    ///act_addr_in->sin_family=PF_INET;
    ///act_addr_in->sin_port=htons(NET_CLIENT_UDP_PORT);
    ///client_addresses[free_index]=(sockaddr*)act_addr_in;
    //struct hostent *he=NULL;
    ///he=gethostbyaddr((char*)&(act_addr_in->sin_addr.s_addr), 4, PF_INET);
    ///if(he==NULL) {
    ///    printf("(!) clanbomber: server error while adding client <%s>.\n", client_names[free_index]);
    ///    return false;
    ///}
    ///client_names[free_index]=strdup(he->h_name);
    client_names[free_index] = strdup("FIXME");
    nr_of_clients++;
    //printf("(+) clanbomber: server added client <%s> (ip: %s)\n", client_names[free_index], client_ips[free_index]);
    return true;
}

char* Server::get_name()
{
    return my_name;
}

char* Server::get_ip()
{
    return my_ip;
}

char* Server::get_last_error()
{
    return last_error;
}

int Server::get_nr_of_clients()
{
    return nr_of_clients;
}

char* Server::get_client_location(int client_nr)
{
    char* tmp=new char[100];
    tmp[0]='\0';
    strcat(tmp, client_names[client_nr]);
    strcat(tmp, " (");
    strcat(tmp, client_ips[client_nr]);
    strcat(tmp, ")");
    return tmp;
}

//int Server::get_client_index_by_ip(in_addr ip)
int Server::get_client_index_by_ip(const char *ip)
{
  //char* ip_str=inet_ntoa(ip);
  int i = -1;
  while(true) {
    i = get_next_client_index(i);
    if (i < 0) {
      return -1;
    }
    fflush(stdout);
    if(strcmp(client_ips[i], ip) == 0) {
      return i;
    }
  }
  return -1;
}

char* Server::get_client_ip_by_index(int index)
{
    return client_ips[index];
}

void Server::send_SERVER_GAME_START()
{
    SERVERMSG("+++ server sent SERVER_GAME_START\n");
    int buflen=3*4;
    int* trans=new int[buflen/4];
    trans[0]=buflen-4;
    trans[1]=NET_SERVER_GAME_START_ID;
    trans[2]=0;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(trans);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_FULL_BOMBER_CONFIG()
{
    SERVERMSG("+++ server sent SERVER_FULL_BOMBER_CONFIG\n");
    int pos=1;
    int* tmp=new int[1024];
    tmp[pos++]=NET_SERVER_FULL_BOMBER_CONFIG_ID;
    tmp[pos++]=0;
    for(int i=0;i<8;i++) {
        tmp[pos++]=((Config::bomber[i].is_enabled() << 16) | (Config::bomber[i].get_client_index()+100));
        tmp[pos++]=((Config::bomber[i].get_skin() << 16) | Config::bomber[i].get_team());
        tmp[pos++]=((Config::bomber[i].get_controller() << 16) | Config::bomber[i].is_server_bomber());
        tmp[pos++]=Config::bomber[i].get_name().length();
        int namelen=Config::bomber[i].get_name().length();
        int intnamelen=0;
        int* namebuf = Client::pack_string(Config::bomber[i].get_name().c_str(), namelen, &intnamelen);
        memcpy(&tmp[pos], namebuf, intnamelen*4);
        delete namebuf;
        pos+=intnamelen;
        int intiplen=0;
        int ip_len=0;
        int* ipbuf=0;
        if(Config::bomber[i].get_client_ip()==NULL) {
            ip_len=strlen(get_ip());
            ipbuf=Client::pack_string(get_ip(), ip_len, &intiplen);
        }
        else {
            ip_len=strlen(Config::bomber[i].get_client_ip());
            ipbuf=Client::pack_string(Config::bomber[i].get_client_ip(), ip_len, &intiplen);                                      
        }
        tmp[pos++]=ip_len;
        memcpy(&tmp[pos], ipbuf, intiplen*4);    
        delete ipbuf;  
        pos+=intiplen;
    }
    tmp[0]=pos*4-4;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_CONFIG()
{
    SERVERMSG("+++ server sent SERVER_CONFIG\n");
    int pos=0;
    int buflen=14*4;
    int* tmp=new int[buflen/4];
    tmp[pos++]=buflen-4;
    tmp[pos++]=NET_SERVER_CONFIG_ID;
    tmp[pos++]=((Config::get_round_time() << 16) | Config::get_max_skateboards());
    tmp[pos++]=((Config::get_max_power() << 16) | Config::get_max_bombs());
    tmp[pos++]=((Config::get_start_bombs() << 16) | Config::get_start_power());
    tmp[pos++]=((Config::get_start_skateboards() << 16) | Config::get_start_kick());
    tmp[pos++]=((Config::get_start_glove() << 16) | Config::get_bombs());
    tmp[pos++]=((Config::get_power() << 16) | Config::get_skateboards());
    tmp[pos++]=((Config::get_kick() << 16) | Config::get_glove());
    tmp[pos++]=((Config::get_joint() << 16) | Config::get_viagra());
    tmp[pos++]=((Config::get_koks() << 16) | Config::get_bomb_countdown());
    tmp[pos++]=((Config::get_bomb_delay() << 16) | Config::get_bomb_speed());
    tmp[pos++]=((Config::get_random_map_order() << 16) | Config::get_random_positions());
    tmp[pos++]=Config::get_points_to_win();
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_ADD_BOMBER()
{
    SERVERMSG("+++ server sent SERVER_ADD_BOMBER\n");
    int pos=1;
    int* tmp=new int[256];
    tmp[pos++]=NET_SERVER_ADD_BOMBER_ID;
    pos++;
    int bomber_to_add=0;
    //CL_Iterator<Bomber> bomber_object_counter(cb_app->bomber_objects);
    for(std::list<Bomber*>::iterator bomber_object_iter = cb_app->bomber_objects.begin();
	bomber_object_iter != cb_app->bomber_objects.end();
	bomber_object_iter++) {
      tmp[pos++]= (*bomber_object_iter)->get_object_id();
      tmp[pos++]=(((*bomber_object_iter)->get_x() << 16) | (*bomber_object_iter)->get_y());
      tmp[pos++]=(((*bomber_object_iter)->get_color() << 16) | (*bomber_object_iter)->get_team());
      tmp[pos++]=(((*bomber_object_iter)->get_number() << 16) | (*bomber_object_iter)->get_points());
      bomber_to_add++;
    }
    tmp[2]=bomber_to_add;
    tmp[0]=pos*4-4;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_OBSERVER_FLY(int px, int py, int speed)
{
    SERVERMSG("+++ server sent SERVER_OBSERVER_FLY\n");
    int pos=0;
    int buflen=4*4;
    int* tmp=new int[buflen/4];
    tmp[pos++]=buflen-4;
    tmp[pos++]=NET_SERVER_OBSERVER_FLY_ID;
    tmp[pos++]=(((px+10000) << 16) | (py+10000));
    tmp[pos++]=speed;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_OBJECT_FLY(int px, int py, int speed, bool overwalls, int object_id)
{
    SERVERMSG("+++ server sent SERVER_OBJECT_FLY\n");
    int pos=0;
    int buflen=5*4;
    int* tmp=new int[buflen/4];
    tmp[pos++]=buflen-4;
    tmp[pos++]=NET_SERVER_OBJECT_FLY_ID;
    tmp[pos++]=(((px+10000) << 16) | (py+10000));
    tmp[pos++]=((overwalls << 16) | object_id);
    tmp[pos++]=speed;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_UPDATE_BOMBER()
{
    SERVERMSG("+++ server sent SERVER_UPDATE_BOMBER\n");
    int pos=1;
    int* tmp=new int[256];
    tmp[pos++]=NET_SERVER_UPDATE_BOMBER_ID;
    pos++;
    int bomber_to_update=0;
    ClanBomberApplication::lock();
    //CL_Iterator<Bomber> bomber_object_counter(cb_app->bomber_objects);
    for(std::list<Bomber*>::iterator bomber_object_iter = cb_app->bomber_objects.begin();
	bomber_object_iter != cb_app->bomber_objects.end();
	bomber_object_iter++) {
      tmp[pos++]=((*bomber_object_iter)->get_x() << 16) | (*bomber_object_iter)->get_y();
      tmp[pos++]=(((*bomber_object_iter)->get_server_send_dir()+100) << 16) | (*bomber_object_iter)->get_object_id();
      bomber_to_update++;
    }
    ClanBomberApplication::unlock();
    tmp[2]=bomber_to_update;
    tmp[0]=pos*4-4;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_UPDATE_BOMBER(int bx, int by, int bdir, int bomber_id)
{
    SERVERMSG("+++ server sent SERVER_UPDATE_BOMBER\n");
    int bufsize=5;
    int* tmp=new int[bufsize];
    tmp[0]=bufsize*4-4;
    tmp[1]=NET_SERVER_UPDATE_BOMBER_ID;
    tmp[2]=1;
    tmp[3]=((bx << 16) | by);
    tmp[4]=(((bdir+100) << 16) | bomber_id);
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_ADD_BOMB(int bomber_x, int bomber_y, int bx, int by, int pwr, int bomber_id, int bomb_id)
{
    SERVERMSG("+++ server sent SERVER_ADD_BOMB\n");
    int pos=0;
    int buflen=6*4;
    int* tmp=new int[buflen/4];
    tmp[pos++]=buflen-4;
    tmp[pos++]=NET_SERVER_ADD_BOMB_ID;
    tmp[pos++]=((bomber_x << 16) | bomber_y);
    tmp[pos++]=((bx << 16) | by);
    tmp[pos++]=((pwr << 16) | bomber_id);
    tmp[pos++]=bomb_id;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_BOMBER_DIE(int obj_id, int sprite_nr)
{
    SERVERMSG("+++ server sent SERVER_BOMBER_DIE\n");
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_BOMBER_DIE_ID;
    tmp[2]=((obj_id << 16) | sprite_nr);
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_UPDATE_MAP()
{
    SERVERMSG("+++ server sent SERVER_UPDATE_MAP\n");
    int* tmp=new int[1024];
    int pos=1;
    tmp[pos++]=NET_SERVER_UPDATE_MAP_ID;
    tmp[pos++]=((MAP_HEIGHT << 16) | MAP_WIDTH);
    int intmaplen=0;
    int* mapbuf=cb_app->get_client()->pack_map(MAP_WIDTH, MAP_HEIGHT, &intmaplen);
    memcpy(&tmp[pos], mapbuf, intmaplen*4);
    pos+=intmaplen;
    delete mapbuf;
    int namelen=cb_app->map->get_name().length();
    tmp[pos++]=namelen;
    int intnamelen=0;
    int* namebuf=Client::pack_string(cb_app->map->get_name().c_str(), namelen, &intnamelen);
    memcpy(&tmp[pos], namebuf, intnamelen*4);   
    delete namebuf;
    pos+=intnamelen;
    int c=0;
    int bomber_to_update=0;
    int remember_pos=pos++;
    //CL_Iterator<Bomber> bomber_object_counter(cb_app->bomber_objects);
    for(std::list<Bomber*>::iterator bomber_object_iter = cb_app->bomber_objects.begin();
	bomber_object_iter != cb_app->bomber_objects.end();
	bomber_object_iter++) {
      bomber_to_update++;
      CL_Vector p=cb_app->map->get_bomber_pos(c++);
      tmp[pos++]=((((int)p.x) << 16) | ((int)p.y));
      tmp[pos++]=(((*bomber_object_iter)->get_points() << 16) | (*bomber_object_iter)->get_object_id());
    }
    tmp[remember_pos]=bomber_to_update;
    tmp[0]=pos*4-4;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_CHAT_MESSAGE(int* msg_buf)
{
    SERVERMSG("+++ server sent SERVER_CHAT_MESSAGE\n");
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(msg_buf);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_END_OF_GAME()
{
    SERVERMSG("+++ server sent SERVER_END_OF_GAME\n");
    int buflen=2*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_END_OF_GAME_ID;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_PAUSE_GAME(bool paused)
{
    SERVERMSG("+++ server sent SERVER_PAUSE_GAME\n");
    int pos=0;
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[pos++]=buflen-4;
    tmp[pos++]=NET_SERVER_PAUSE_GAME_ID;
    tmp[pos++]=cb_app->paused_game();
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_END_OF_LEVEL()
{
    SERVERMSG("+++ server sent SERVER_END_OF_LEVEL\n");
    int buflen=2*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_END_OF_LEVEL_ID;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_END_GAME_SESSION()
{
    SERVERMSG("+++ server sent SERVER_END_GAME_SESSION\n");
    int buflen=2*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_END_GAME_SESSION_ID;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_START_NEW_LEVEL()
{
    SERVERMSG("+++ server sent SERVER_START_NEW_LEVEL\n");
    int buflen=2*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_START_NEW_LEVEL_ID;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_ADD_EXPLOSION(int x, int y, int leftlen, int rightlen, int uplen, int downlen, int power, int bomber_id, int explosion_id)
{
    SERVERMSG("+++ server sent SERVER_ADD_EXPLOSION\n");
    int pos=0;
    int buflen=6*4;
    int* tmp=new int[buflen/4];
    tmp[pos++]=buflen-4;
    tmp[pos++]=NET_SERVER_ADD_EXPLOSION_ID;
    tmp[pos++]=((x << 16) | y);
    tmp[pos++]=((((char)leftlen) << 24) | (((char)rightlen) << 16) | (((char)uplen) << 8) | ((char)downlen));
    tmp[pos++]=((power << 16) | bomber_id);
    tmp[pos++]=explosion_id;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_DELETE_OBJECTS(int nr, int* obj_ids)
{
    SERVERMSG("+++ server sent SERVER_DELETE_OBJECTS\n");
    int pos=0;
    int buflen=3*4+nr*4;
    int* tmp=new int[buflen/4];
    tmp[pos++]=buflen-4;
    tmp[pos++]=NET_SERVER_DELETE_OBJECTS_ID;
    tmp[pos++]=nr;
    for(int i=0;i<nr;i++) {
        tmp[pos++]=obj_ids[i];
    }
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_UPDATE_BOMBER_SKILLS(Bomber* bomber)
{
    SERVERMSG("+++ server sent SERVER_UPDATE_BOMBER_SKILLS\n");
    int pos=0;
    int buflen=11*4;
    int* tmp=new int[buflen/4];
    tmp[pos++]=buflen-4;
    tmp[pos++]=NET_SERVER_UPDATE_BOMBER_SKILLS_ID;
    tmp[pos++]=bomber->get_object_id();
    tmp[pos++]=((bomber->get_points() << 16) | bomber->get_deaths());
    tmp[pos++]=((bomber->get_kills() << 16) | bomber->get_cur_bombs());
    tmp[pos++]=((bomber->get_bombs() << 16) | bomber->get_extra_bombs());
    tmp[pos++]=((bomber->get_power() << 16) | bomber->get_extra_power());
    tmp[pos++]=((bomber->get_skateboards() << 16) | bomber->get_extra_skateboards());
    tmp[pos++]=((bomber->get_speed() << 16) | bomber->is_able_to_kick());
    tmp[pos++]=((bomber->get_extra_kick() << 16) | bomber->is_able_to_throw());
    tmp[pos++]=((bomber->get_gloves() << 16) | bomber->get_extra_gloves());
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_ADD_BOMBER_CORPSE(int object_id, int x, int y, int color, int sprite_nr)
{
    SERVERMSG("+++ server sent SERVER_ADD_BOMBER_CORPSE\n");
    int pos=0;
    int buflen=5*4;
    int* tmp=new int[buflen/4];
    tmp[pos++]=buflen-4;
    tmp[pos++]=NET_SERVER_ADD_BOMBER_CORPSE_ID;
    tmp[pos++]=object_id;
    tmp[pos++]=((x << 16) | y);
    tmp[pos++]=((color << 16) | sprite_nr);
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_EXPLOSION_SPLATTERED_CORPSE(int object_id)
{
    SERVERMSG("+++ server sent SERVER_EXPLOSION_SPLATTERED_CORPSE\n");
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_EXPLOSION_SPLATTERED_CORPSE_ID;
    tmp[2]=object_id;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_MAPTILE_VANISH(int x, int y)
{
    SERVERMSG("+++ server sent SERVER_MAPTILE_VANISH\n");
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_MAPTILE_VANISH_ID;
    tmp[2]=(((x/40) << 16) | (y/40));
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_MAPTILE_REFRESH(int x, int y, int maptile_type, int dir)
{
    SERVERMSG("+++ server sent SERVER_MAPTILE_REFRESH\n");
    int buflen=4*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_MAPTILE_REFRESH_ID;
    tmp[2]=((x << 16) | y);
    tmp[3]=(((maptile_type+100) << 16) | (dir+100));
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_ADD_EXTRA(int object_id, int x, int y, int extra_type)
{
    SERVERMSG("+++ server sent SERVER_ADD_EXTRA\n");
    int buflen=4*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_ADD_EXTRA_ID;
    tmp[2]=((x << 16) | y);
    tmp[3]=(((extra_type+100) << 16) | object_id);
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_LOOSE_DISEASE(int bomber_object_id)
{
    SERVERMSG("+++ server sent SERVER_LOOSE_DISEASE\n");
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_LOOSE_DISEASE_ID;
    tmp[2]=bomber_object_id;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_INFECT_WITH_DISEASE(int bomber_object_id, int disease_type)
{
    SERVERMSG("+++ server sent SERVER_INFECT_WITH_DISEASE\n");
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_INFECT_WITH_DISEASE_ID;
    tmp[2]=((bomber_object_id << 16) | (disease_type+100));
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_OBJECT_FALL(int obj_id)
{
    SERVERMSG("+++ server sent SERVER_OBJECT_FALL\n");
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_OBJECT_FALL_ID;
    tmp[2]=obj_id;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_GAIN_EXTRA(int bomber_object_id, int extra_type, int x_coord)
{
    SERVERMSG("+++ server sent SERVER_GAIN_EXTRA\n");
    int buflen=4*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_GAIN_EXTRA_ID;
    tmp[2]=bomber_object_id;
    tmp[3]=(((extra_type+100) << 16) | x_coord);
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_DISCONNECT_CLIENT(int bomber_object_id)
{
    SERVERMSG("+++ server sent SERVER_DISCONNECT_CLIENT\n");
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_DISCONNECT_CLIENT_ID;
    tmp[2]=bomber_object_id;
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_MAP_REQUEST(int client_index, int new_maps_nr, unsigned int* new_maps)
{
    SERVERMSG("+++ server sent SERVER_MAP_REQUEST\n");
    int buflen=new_maps_nr+3;
    int tmp[buflen];
    int pos=0;
    tmp[pos++]=cb_app->get_server_frame_counter();
    tmp[pos++]=NET_SERVER_MAP_REQUEST_ID;
    tmp[pos++]=new_maps_nr;
    for(int i=0;i<new_maps_nr;i++) {
        tmp[pos++]=new_maps[i];
    }
    for(int j=0;j<buflen;j++) {
        tmp[j]=htonl(tmp[j]);
    }
    //int bytes_sent=-1;
    //int addr_len=sizeof(struct sockaddr);
    //bytes_sent=sendto(my_udp_socket, tmp, buflen*4, 0, client_addresses[client_index], addr_len);
    size_t bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(tmp, buflen*4),
                *client_endpoint[client_index]);
    total_bytes_sent += bytes_sent;
    reset_keep_alive_timer(client_index);
}

void Server::send_SERVER_HANDSHAKE(int client_index)
{
    SERVERMSG("+++ server sent SERVER_HANDSHAKE\n");
    int buflen=4;
    int tmp[buflen];
    tmp[0]=cb_app->get_server_frame_counter();
    tmp[1]=NET_SERVER_HANDSHAKE_ID;
    tmp[2]=NET_PROTOCOL_VERSION;
    if(big_endian) {
        tmp[3]=1;
    }
    else {
        tmp[3]=0;
    }
    for(int j=0;j<buflen;j++) {
        tmp[j]=htonl(tmp[j]);  
    }
    //int bytes_sent=-1;
    //int addr_len=sizeof(struct sockaddr);
    //bytes_sent=sendto(my_udp_socket, tmp, buflen*4, 0, client_addresses[client_index], addr_len);
    size_t bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(tmp, buflen*4),
                *client_endpoint[client_index]);
    total_bytes_sent += bytes_sent;
    reset_keep_alive_timer(client_index);
}

void Server::send_SERVER_CLIENT_INFO()
{
    SERVERMSG("+++ server sent SERVER_CLIENT_INFO\n");
    int tmp[1024];
    int pos=0;
    tmp[pos++]=cb_app->get_server_frame_counter();
    tmp[pos++]=NET_SERVER_CLIENT_INFO_ID;
    tmp[pos++]=0;
    int i=0;
    int nr=0;
    while(true) {
        i=get_next_client_index(i);
        if(i<0) {  
             break;
        }
        tmp[pos++]=i;
        int iplen=strlen(client_ips[i]);
        tmp[pos++]=iplen;
        int intiplen=0;
        int* ipbuf=Client::pack_string(client_ips[i], iplen, &intiplen);
        memcpy(&tmp[pos], ipbuf, intiplen*4);
        delete ipbuf;
        pos+=intiplen;
        int namelen=strlen(client_names[i]);
        tmp[pos++]=namelen;
        int intnamelen=0;
        int* namebuf=Client::pack_string(client_names[i], namelen, &intnamelen);
        memcpy(&tmp[pos], namebuf, intnamelen*4);
        delete namebuf; 
        pos+=intnamelen;
        nr++;
    }
    tmp[2]=nr;
    //int bytes_sent=-1;
    //int addr_len=sizeof(struct sockaddr);
    for(int j=0;j<pos;j++) {    
        tmp[j]=htonl(tmp[j]);      
    }
    i=0;
    while(true) {
        i=get_next_client_index(i);
        if(i<0) {
             break;
        }
        //bytes_sent=sendto(my_udp_socket, tmp, pos*4, 0, client_addresses[i], addr_len);
	size_t bytes_sent = my_udp_socket
          ->send_to(boost::asio::buffer(tmp, pos*4), *client_endpoint[i]);
        total_bytes_sent += bytes_sent;
    }
    reset_keep_alive_timer();
}

void Server::send_SERVER_KEEP_ALIVE()
{
    int client_index=0;
    while(true) {
        client_index=get_next_client_index(client_index);
        if(client_index<0) {  
            break;
        }
        if(send_keep_alive_timer[client_index].elapsed()>=NET_SERVER_MILLISECONDS_SEND_KEEP_ALIVE_PERIOD) {
            reset_keep_alive_timer(client_index);
            SERVERMSG("+++ server sent SERVER_KEEP_ALIVE\n");
            int buflen=2;
            int tmp[buflen];
            tmp[0]=cb_app->get_server_frame_counter();
            tmp[1]=NET_SERVER_KEEP_ALIVE_ID;
            for(int j=0;j<buflen;j++) {    
                tmp[j]=htonl(tmp[j]);      
            }
            //int bytes_sent=-1;
            //int addr_len=sizeof(struct sockaddr);
            //bytes_sent=sendto(my_udp_socket, tmp, buflen*4, 0, client_addresses[client_index], addr_len);
	    size_t bytes_sent = my_udp_socket
              ->send_to(boost::asio::buffer(tmp, buflen*4),
                        *client_endpoint[client_index]);
            total_bytes_sent += bytes_sent;
        }
    }
}

void Server::send_SERVER_PING(int client_index, int request_nr)
{
    SERVERMSG("+++ server sent SERVER_PING\n");  
    int buflen=3;
    int tmp[buflen];
    tmp[0]=cb_app->get_server_frame_counter();
    tmp[1]=NET_SERVER_PING_ID;
    tmp[2]=request_nr;
    for(int j=0;j<buflen;j++) {
        tmp[j]=htonl(tmp[j]);  
    }
    //int bytes_sent=-1;
    //int addr_len=sizeof(struct sockaddr);
    //bytes_sent=sendto(my_udp_socket, tmp, buflen*4, 0, client_addresses[client_index], addr_len);
    size_t bytes_sent = my_udp_socket
      ->send_to(boost::asio::buffer(tmp, buflen*4),
                *client_endpoint[client_index]);
    total_bytes_sent += bytes_sent;
    reset_keep_alive_timer(client_index);
}

void Server::send_SERVER_CONNECT_CLIENT(int previous_client_index)
{
    SERVERMSG("+++ server sent SERVER_CONNECT_CLIENT\n");
    int buflen=4;
    int tmp[buflen];
    tmp[0]=cb_app->get_server_frame_counter();
    tmp[1]=NET_SERVER_CONNECT_CLIENT_ID;
    tmp[2]=previous_client_index;
    tmp[3]=0;
    for(int j=0;j<buflen;j++) {
        tmp[j]=htonl(tmp[j]);
    }
    //int bytes_sent=-1;
    //int addr_len=sizeof(struct sockaddr);
    int client_index=0;
    while(true) {
        client_index=get_next_client_index(client_index);
        if(client_index<0) {
            break;
        }
        tmp[3]=htonl(client_index);
        //bytes_sent=sendto(my_udp_socket, tmp, buflen*4, 0, client_addresses[client_index], addr_len);
        size_t bytes_sent = my_udp_socket
          ->send_to(boost::asio::buffer(tmp, buflen*4),
                    *client_endpoint[client_index]);
        total_bytes_sent += bytes_sent;
        reset_keep_alive_timer(client_index);
    }
    for(int i=0;i<NET_SERVER_MAX_CLIENTS;i++) {
        client_endpoint[i]=NULL;
        client_ips[i]=NULL;
        client_names[i]=NULL;
        //client_tcp_sockets[i]=-1;
	client_tcp_sockets[i] = NULL;
    }
    nr_of_clients=0;
}

void Server::send_SERVER_BOMB_KICK_START(int bomb_id, int dir)
{
    SERVERMSG("+++ server sent SERVER_BOMB_KICK_START\n");
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_BOMB_KICK_START_ID;
    tmp[2]=((bomb_id << 16) | (dir+100));
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_BOMB_KICK_STOP(int bomb_id, int bx, int by)
{
    SERVERMSG("+++ server sent SERVER_BOMB_KICK_STOP\n");
    int buflen=4*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_BOMB_KICK_STOP_ID;
    tmp[2]=bomb_id;
    tmp[3]=((bx << 16) | by);
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_SERVER_CLIENT_READY_TO_PLAY(int client_index, bool ready)
{
    SERVERMSG("+++ server sent SERVER_CLIENT_READY_TO_PLAY\n");
    int buflen=3*4;
    int* tmp=new int[buflen/4];
    tmp[0]=buflen-4;
    tmp[1]=NET_SERVER_CLIENT_READY_TO_PLAY_ID;
    tmp[2]=((client_index << 16) | (int)ready);
    send_buffer_mutex->lock();
    update_messages_to_send.push_back(tmp);
    send_buffer_mutex->unlock();
}

void Server::send_update_messages_to_clients(int current_frame_nr)
{
    send_buffer_mutex->lock();
    if(update_messages_to_send.empty()) {
        send_buffer_mutex->unlock();
        return;
    }
    int positions_for_client_index[4]={-1,-1,-1,-1};
    //int bytes_sent=-1;
    //int addr_len=sizeof(struct sockaddr);
    int buflen=1;
    int* trans_tmp=new int[4096];
    trans_tmp[0]=current_frame_nr;
    int cur_pos=0;
    while(!update_messages_to_send.empty()) {
        int* tmp=update_messages_to_send.front();
        update_messages_to_send.pop_front();
        int tmp_len=tmp[0];
        if(tmp[1]==NET_SERVER_FULL_BOMBER_CONFIG_ID || tmp[1]==NET_SERVER_GAME_START_ID) {
            positions_for_client_index[cur_pos++]=buflen+1;
        }
        memcpy(&trans_tmp[buflen], &tmp[1], tmp_len);
        buflen+=(tmp_len/4);
        delete tmp;
    }
    send_buffer_mutex->unlock();
    for(int j=0;j<buflen;j++) {
        trans_tmp[j]=htonl(trans_tmp[j]);
    }
    if(buflen>1) {
        //SERVERMSG("+++ server sent buffered messages to clients! frame=%d\n", current_frame_nr);
        int i=-1;
        while(true) {
            i=get_next_client_index(i);
            if(i<0) {
                break;
            }
            cur_pos=0;
            while(positions_for_client_index[cur_pos++]>0) {
                trans_tmp[positions_for_client_index[cur_pos-1]]=htonl(i);
            }
            //bytes_sent=sendto(my_udp_socket, trans_tmp, buflen*4, 0, client_addresses[i], addr_len);
            size_t bytes_sent = my_udp_socket
              ->send_to(boost::asio::buffer(trans_tmp, buflen*4),
                        *client_endpoint[i]);
            if(i>0) {
	      total_bytes_sent += bytes_sent;
            }
        }
        reset_keep_alive_timer();
    }
    delete trans_tmp;
}

void Server::analyze_game_mode()
{
    in_demo_mode=true;
    int i=0;
    while(i<8) {
        if(Config::bomber[i].is_enabled() &&
           Config::bomber[i].get_controller()!=Controller::AI &&
           Config::bomber[i].get_controller()!=Controller::AI_mass) {
            in_demo_mode=false;
            break;
        } 
        i++;
    }
    if(in_demo_mode) {
      std::cout<<"(+) clanbomber: server is in demo mode.. (no human players)"<<std::endl;
    }
}

bool Server::is_in_demo_mode()
{
    return in_demo_mode;
}

void Server::keep_client_alive(int client_index)
{
    client_keep_alive_timer[client_index].reset();
}

void Server::disconnect_dead_clients()
{
    int i=0;
    while(true) {
        i=get_next_client_index(i);
        if(i<0) {
            break;
        }
        if(client_keep_alive_timer[i].elapsed()>NET_SERVER_MILLISECONDS_KEEP_CLIENT_ALIVE) {
            delete_client(i);
            break;
        }
    }
}

void Server::delete_client(int client_index)
{
    if(client_index<0 || client_index>=NET_SERVER_MAX_CLIENTS) {
        return;
    }
    delete client_endpoint[client_index];
    client_endpoint[client_index] = NULL;
    delete client_ips[client_index];
    client_ips[client_index]=NULL;
    delete client_names[client_index];
    client_names[client_index]=NULL;
    //shutdown(client_tcp_sockets[client_index], SHUT_RDWR);
    //close(client_tcp_sockets[client_index]); 
    delete client_tcp_sockets[client_index];
    //client_tcp_sockets[client_index]=-1;
    client_tcp_sockets[client_index] = NULL;
    nr_of_clients--;
    ClanBomberApplication::lock();
    for(int i=0;i<8;i++) {
        if(Config::bomber[i].get_client_index()==client_index) {
            Config::bomber[i].disable();
            Config::bomber[i].set_local(true);
            Config::bomber[i].set_client_index(-1);
            free(Config::bomber[i].get_client_ip());
            Config::bomber[i].set_client_ip(NULL);
            Config::bomber[i].set_server_bomber(false);
            ServerSetup::unselect_player(i);
            //CL_Iterator<Bomber> my_bomber_object_counter(cb_app->bomber_objects);
	    for(std::list<Bomber*>::iterator bomber_object_iter = cb_app->bomber_objects.begin();
		bomber_object_iter != cb_app->bomber_objects.end();
		bomber_object_iter++) {
	      (*bomber_object_iter)->set_disconnected();
	      (*bomber_object_iter)->die();
	      send_SERVER_DISCONNECT_CLIENT((*bomber_object_iter)->get_object_id());
	      bomber_object_iter = cb_app->bomber_objects.erase(bomber_object_iter);
	    }
        }
    }
    ClanBomberApplication::unlock();
    send_SERVER_FULL_BOMBER_CONFIG();
}

bool Server::is_free_client_index(int client_index)
{
    return (client_ips[client_index]==NULL &&
            client_names[client_index]==NULL &&
            client_endpoint[client_index]==NULL &&
            client_tcp_sockets[client_index] == NULL);
}

int Server::get_next_client_index(int last_index)
{
    for(int i=last_index+1;i<NET_SERVER_MAX_CLIENTS;i++) {
        if(!is_free_client_index(i)) {
            return i;
        }
    }
    return -1;
}

int Server::get_next_free_client_index()
{
    for(int i=0;i<NET_SERVER_MAX_CLIENTS;i++) {
        if(is_free_client_index(i)) {
            return i;
        }
    }
    return -1;
}

bool Server::map_already_exists(const char* map_name)
{
    unsigned int map_name_checksum = cb_app->map->get_current_map()->get_name_checksum(map_name);
    unsigned int* my_map_name_checksums=NULL;
    int my_map_name_nr=cb_app->map->get_map_name_checksums(&my_map_name_checksums);
    bool found=false;
    for(int i=0;i<my_map_name_nr;i++) {
        if(map_name_checksum==my_map_name_checksums[i]) {
            found=true;
        }
    }
    return found;
}

unsigned int Server::merge_maps(int client_maps_nr, unsigned int* client_maps, unsigned int** new_maps)
{
    unsigned int tmp[client_maps_nr];
    int new_maps_nr=0;
    unsigned int* my_maps=NULL;
    int my_map_nr=cb_app->map->get_map_data_checksums(&my_maps);
    for(int j=0;j<client_maps_nr;j++) {
        bool found=false;
        for(int i=0;i<my_map_nr;i++) {
            if(client_maps[j]==my_maps[i]) {
                found=true;
            }
        }
        if(!found) {
            tmp[new_maps_nr++]=client_maps[j];
        }
    }
    if(new_maps_nr>0) {
        unsigned int* nm=new unsigned int[new_maps_nr];
        for(int i=0;i<new_maps_nr;i++) {
            nm[i]=tmp[i];
        }
        (*new_maps)=nm;
    }
    if(my_map_nr>0) {
        delete my_maps;
    }
    return new_maps_nr;
}

void Server::set_mapentry_data(MapEntry* me, int x, int y, int type, int dir)
{
    if(type==MapTile::NONE) {
        me->set_data(x, y, 'N');
    }
    else if(type==MapTile::ICE) {
        me->set_data(x, y, 'S');
    }
    else if(type==MapTile::TRAP) {
        me->set_data(x, y, 'o');
    }
    else if(type==MapTile::BOX) {
        me->set_data(x, y, '+');
    }
    else if(type==MapTile::WALL) {
        me->set_data(x, y, '*');
    }
    else if(type==MapTile::ARROW){
        if(dir==DIR_UP) {
            me->set_data(x, y, '^');
        }
        else if(dir==DIR_DOWN) {
            me->set_data(x, y, 'v');
        }
        else if(dir==DIR_LEFT) {
            me->set_data(x, y, '<');
        }
        else if(dir==DIR_RIGHT) {
            me->set_data(x, y, '>');
        }
        else {
            me->set_data(x, y, ' ');
        }
    }
    else if(type==77) {
        me->set_data(x, y, 'R');
    }
    else {
        me->set_data(x, y, ' ');
    }
}

void Server::reset_keep_alive_timer(int client_index)
{
    send_keep_alive_timer[client_index].reset();
}

void Server::reset_keep_alive_timer()
{
    int i=0;
    while(true) {
        i=get_next_client_index(i);
        if(i<0) {
     	    break;
	    }
        send_keep_alive_timer[i].reset();
    }
}

void Server::reset_traffic_statistics()
{
    remember_total_bytes_sent=total_bytes_sent;
    remember_total_bytes_received=total_bytes_received;
    level_timer.reset();
    game_timer.reset();
}

void Server::update_traffic_statistics()
{
    // ?
}

void Server::output_traffic_statistics()
{
#ifdef SHOW_SERVER_TRAFFIC_STATISTICS
    if(nr_of_clients>1) {
        current_game_time+=(int)(game_timer.elapsed()/1000);
        int map_time_seconds=(int)(level_timer.elapsed()/1000);
        int map_bytes_sent=total_bytes_sent-remember_total_bytes_sent;
        int map_bytes_received=total_bytes_received-remember_total_bytes_received;
        cout<<"*** server: traffic statistics ("<<(nr_of_clients-1)<<" connections) (map nr."<<maps_played<<") ***"<<endl;
        cout<<"    server: total game time     : "<<current_game_time<<" s ("<<(int)(current_game_time/60)<<" min.)"<<endl;
        cout<<"    server: total bytes sent    : "<<total_bytes_sent<<" ("<<(int)(total_bytes_sent/1024)<<" kb)"<<endl;
        cout<<"    server: total bytes received: "<<total_bytes_received<<" ("<<(int)(total_bytes_received/1024)<<" kb)"<<endl;
        cout<<"    server: map time            : "<<map_time_seconds<<" s"<<endl;
        cout<<"    server: map bytes sent      : "<<map_bytes_sent<<" (avg. "<<(int)(map_bytes_sent/map_time_seconds)<<" b/s)"<<endl;
        cout<<"    server: map bytes received  : "<<map_bytes_received<<" (avg. "<<(int)(map_bytes_received/map_time_seconds)<<" b/s)"<<endl;
    }
#endif
}

void Server::increase_maps_played()
{
    maps_played++;
}

void Server::recv_SERVER_BOMBER_CONFIG(int* data, int* pos, int from_client_index)
{
    SERVERMSG("++++++ server received SERVER_BOMBER_CONFIG\n");
    int enableskin=data[(*pos)++];
    int enable=(enableskin >> 16);
    int skin=(enableskin & 0x0000FFFF);
    int teamcontroller=data[(*pos)++];
    int team=(teamcontroller >> 16);
    int controller=(teamcontroller & 0x0000FFFF);
    int relposnamelen=data[(*pos)++];
    int rel_pos=(relposnamelen >> 16);
    int name_len=(relposnamelen & 0x0000FFFF);
    int intnamelen=0;
    char* name=Client::unpack_string(&data[(*pos)], name_len, &intnamelen);
    (*pos)+=intnamelen;
    std::string cl_name = name;
    ClanBomberApplication::lock();
    Config::bomber[rel_pos].set_skin(skin);
    Config::bomber[rel_pos].set_controller(controller);

#ifndef NET_SERVER_ALLOW_SERVER_AI
    while(Config::bomber[rel_pos].get_controller()==Controller::AI ||
          Config::bomber[rel_pos].get_controller()==Controller::AI_MIC) {
        Config::bomber[rel_pos].set_controller(Config::bomber[rel_pos].get_controller()+1);
    }
#endif

    Config::bomber[rel_pos].set_name(cl_name);
    if(enable && !Config::bomber[rel_pos].is_enabled()) {
        ServerSetup::select_player(rel_pos);
    }
    else if(!enable && Config::bomber[rel_pos].is_enabled()) {
        ServerSetup::unselect_player(rel_pos);
    }
    Config::bomber[rel_pos].set_enabled(enable);
    if(team>77) {
        Config::bomber[rel_pos].set_team(team-78);
    }
    else {
        Config::bomber[rel_pos].set_team(team);
        Config::bomber[rel_pos].set_client_ip(get_client_ip_by_index(from_client_index));
        Config::bomber[rel_pos].set_client_index(from_client_index);
        Config::bomber[rel_pos].set_server_bomber(true);
    }
    ClanBomberApplication::unlock();
    delete name;
    send_SERVER_FULL_BOMBER_CONFIG();
}

void Server::recv_CLIENT_BOMBER_CONFIG(int* data, int* pos, int from_client_index)
{
    SERVERMSG("++++++ server received CLIENT_BOMBER_CONFIG\n");
    int enableskin=data[(*pos)++];
    int enable=(enableskin >> 16);
    int skin=(enableskin & 0x0000FFFF);     
    int controllerrelpos=data[(*pos)++];    
    int controller=(controllerrelpos >> 16);
    int rel_pos=(controllerrelpos & 0x0000FFFF);
    int name_len=data[(*pos)++];
    int intnamelen=0;
    char* name=Client::unpack_string(&data[(*pos)], name_len, &intnamelen);
    (*pos)+=intnamelen;
    std::string cl_name = name;
    ClanBomberApplication::lock();
    Config::bomber[rel_pos].set_skin(skin);
    Config::bomber[rel_pos].set_controller(controller);

#ifndef NET_SERVER_ALLOW_CLIENT_AI
    while(Config::bomber[rel_pos].get_controller()==Controller::AI ||
          Config::bomber[rel_pos].get_controller()==Controller::AI_mass) {
        Config::bomber[rel_pos].set_controller(Config::bomber[rel_pos].get_controller()+1);
    } 
#endif

    Config::bomber[rel_pos].set_name(cl_name);
    Config::bomber[rel_pos].set_client_ip(get_client_ip_by_index(from_client_index));
    if(enable && !Config::bomber[rel_pos].is_enabled()) {
        ServerSetup::select_player(rel_pos);
    }
    else if(!enable && Config::bomber[rel_pos].is_enabled()) {
        ServerSetup::unselect_player(rel_pos);
    }
    Config::bomber[rel_pos].set_enabled(enable);
    Config::bomber[rel_pos].set_client_index(from_client_index);
    Config::bomber[rel_pos].set_server_bomber(false);
    ClanBomberApplication::unlock();
    delete name;
    send_SERVER_FULL_BOMBER_CONFIG();
}

void Server::recv_CLIENT_BOMBER_DIR(int* data, int* pos)
{
    SERVERMSG("++++++ server received CLIENT_BOMBER_DIR\n");
    int nrdir=data[(*pos)++];
    int cl_nr=(nrdir >> 16);
    int cl_dir=(nrdir & 0x0000FFFF)-100;
    ClanBomberApplication::lock();
    //CL_Iterator<Bomber> bomber_object_counter(bomber_objects);
    for(std::list<Bomber*>::iterator bomber_object_iter = bomber_objects.begin();
	bomber_object_iter != bomber_objects.end();
	bomber_object_iter++) {
      if((*bomber_object_iter)->get_number()==cl_nr) {
	(*bomber_object_iter)->set_server_dir(cl_dir);
	(*bomber_object_iter)->set_local_dir(cl_dir); 
	if(cl_dir!=DIR_NONE) {
	  (*bomber_object_iter)->set_cur_dir(cl_dir);
	}
	break;
      }
    }
    ClanBomberApplication::unlock();
}

void Server::recv_CLIENT_BOMBER_BOMB(int* data, int* pos)
{
    SERVERMSG("++++++ server received CLIENT_BOMBER_BOMB\n");
    int pwroid=data[(*pos)++];
    int pwr=(pwroid >> 16);
    int object_id=(pwroid & 0x0000FFFF);  
    ClanBomberApplication::lock();
    GameObject* obj=cb_app->get_object_by_id(object_id); 
    if(obj!=NULL) {
        Bomber* b=(Bomber*)obj;
        if(b->get_maptile()->bomb && b->get_gloves()) {
            b->get_maptile()->bomb->throww(b->get_cur_dir());
            b->set_anim_count(2);
        }
        else if(b->get_cur_bombs()>0) {
            cb_app->map->add_bomb((b->get_x()+20)/40, (b->get_y()+20)/40, pwr, b);
        }
    }
    ClanBomberApplication::unlock();
}

void Server::recv_CLIENT_CHAT_MESSAGE(int* data, int* pos, int from_client_index)
{
SERVERMSG("++++++ server received CLIENT_CHAT_MESSAGE\n");
    int autom=data[(*pos)++];
    int intlen_bytelen=data[(*pos)++];
    int chat_int_msg_len=(intlen_bytelen & 0x0000FFFF);
    int* msg_tmp=new int[chat_int_msg_len+5];
    msg_tmp[0]=(chat_int_msg_len+5)*4;
    msg_tmp[1]=NET_SERVER_CHAT_MESSAGE_ID;
    msg_tmp[2]=from_client_index;
    msg_tmp[3]=autom;
    msg_tmp[4]=intlen_bytelen;
    memcpy(&msg_tmp[5], &data[(*pos)], chat_int_msg_len*4);
    (*pos)+=chat_int_msg_len;
    send_SERVER_CHAT_MESSAGE(msg_tmp);
}

void Server::recv_CLIENT_DISCONNECT(int* data, int* pos, int from_client_index)
{
    SERVERMSG("++++++ server received CLIENT_DISCONNECT\n");
    delete_client(from_client_index);
}

void Server::recv_CLIENT_KEEP_ALIVE(int* data, int* pos, int from_client_index)
{
    SERVERMSG("++++++ server received CLIENT_KEEP_ALIVE\n");
    int request_nr=data[(*pos)++];
    send_SERVER_PING(from_client_index, request_nr);
    keep_client_alive(from_client_index);
}

void Server::recv_CLIENT_MAP_CHECKSUMS(int* data, int* pos, int from_client_index)
{
    SERVERMSG("++++++ server received CLIENT_MAP_CHECKSUMS\n");
    int client_map_nr=data[(*pos)++];
    unsigned int* new_maps=NULL;
    unsigned int map_tmp[client_map_nr];
    for(int k=0;k<client_map_nr;k++) {
        map_tmp[k]=data[(*pos)++];
    }
    ClanBomberApplication::lock();
    int new_maps_nr=merge_maps(client_map_nr, map_tmp, &new_maps);
    ClanBomberApplication::unlock();
    if(new_maps_nr>0) {
        send_SERVER_MAP_REQUEST(from_client_index, new_maps_nr, new_maps);
        delete new_maps;
    }
}

void Server::recv_CLIENT_MAP_EXCHANGE(int* data, int* pos, int from_client_index)
{
    MapEntry* me = new MapEntry("dummy", false);
    int maplen=MAP_WIDTH*MAP_HEIGHT;
    int k=0;
    int l=0;
    for(int j=0;j<maplen;j+=2) {
        int m12td=data[(*pos)++];
        if(j+1>=maplen) {
            int m1t=((m12td & 0xFF000000) >> 24)-100;
            int m1d=((m12td & 0x00FF0000) >> 16)-100;
            set_mapentry_data(me, k, l, m1t, m1d);   
        }
        else {
            int m1t=((m12td & 0xFF000000) >> 24)-100;
            int m1d=((m12td & 0x00FF0000) >> 16)-100;
            set_mapentry_data(me, k, l, m1t, m1d);   
            k++;
            if(k>=MAP_WIDTH) {
                k=0;
                l++;
            }
            int m2t=((m12td & 0x0000FF00) >> 8)-100;
            int m2d=(m12td & 0x000000FF)-100;
            set_mapentry_data(me, k, l, m2t, m2d);
            k++;
            if(k>=MAP_WIDTH) {
                k=0;
                l++;
            }
        }
    }
    int map_name_length=data[(*pos)++];
    int intnamelen=0; 
    char* map_name=Client::unpack_string(&data[(*pos)], map_name_length, &intnamelen);
    (*pos)+=intnamelen;
    int map_author_length=data[(*pos)++];
    int intauthorlen=0; 
    char* map_author=Client::unpack_string(&data[(*pos)], map_author_length, &intauthorlen);
    (*pos)+=intauthorlen;
    SERVERMSG("++++++ server received CLIENT_MAP_EXCHANGE <%s by %s>\n", map_name, map_author);
    int map_max_players=data[(*pos)++];
    me->set_name(map_name);
    me->set_author(map_author);
    me->set_max_players(map_max_players);
    for(int m=0;m<map_max_players;m++) {
        int bomberxy=data[(*pos)++];
        int bx=(bomberxy >> 16);
        int by=(bomberxy & 0x0000FFFF);
        me->set_bomber_pos(bx, by, m);
    }
    if(map_already_exists(map_name)) {
        me->reset_filename(client_names[from_client_index]);
        if(!map_already_exists(me->get_name().c_str())) {
            me->write_back();
        }
    }
    else {
        me->reset_filename(NULL);
        me->write_back();
    }
    delete map_name;
    delete map_author;
    delete me;
    cb_app->reload_map();
}

void Server::recv_CLIENT_HANDSHAKE(int* data, int* pos, int from_client_index)
{
    SERVERMSG("++++++ server received CLIENT_HANDSHAKE\n");
    int client_protocol_version=data[(*pos)++];
    int client_big_endian=data[(*pos)++];
    if(big_endian!=client_big_endian) {
        client_convert[from_client_index]=true;
    }
    send_SERVER_HANDSHAKE(from_client_index);
    if(client_protocol_version!=NET_PROTOCOL_VERSION) {
      std::cout<<"(!) clanbomber: connection from <"<<client_names[from_client_index]<<"> refused, protocol version "<<client_protocol_version<<" (required "<<NET_PROTOCOL_VERSION<<")."<<std::endl;
        delete_client(from_client_index);
        return;
    }
    SERVERMSG("++++++ CLIENT_HANDSHAKE - proto(%d) convert(%d).\n", client_protocol_version, client_convert[from_client_index]);
}

void Server::recv_CLIENT_READY_TO_PLAY(int* data, int* pos, int from_client_index)
{
    SERVERMSG("++++++ server received CLIENT_READY_TO_PLAY\n");
    bool client_ready_to_play=(bool)(data[(*pos)++]);
    clients_ready_to_play[from_client_index]=client_ready_to_play;
    send_SERVER_CLIENT_READY_TO_PLAY(from_client_index, client_ready_to_play);
}

SimpleTimer::SimpleTimer()
{
    start_time=new (struct timeval);
    gettimeofday(start_time, NULL);
    current_time=new (struct timeval);
    gettimeofday(current_time, NULL);
}

SimpleTimer::~SimpleTimer()
{
    delete start_time;
    delete current_time;
}

void SimpleTimer::reset()
{
    gettimeofday(start_time, NULL);
    gettimeofday(current_time, NULL);
}

float SimpleTimer::elapsed()
{
    gettimeofday(current_time, NULL);
    int time_start=start_time->tv_sec*1000000+start_time->tv_usec;
    int time_now=current_time->tv_sec*1000000+current_time->tv_usec;
    return (time_now-time_start)/1000.0f;
}

float SimpleTimer::elapsedn()
{
    gettimeofday(current_time, NULL);
    int time_start=start_time->tv_sec*1000000+start_time->tv_usec;
    int time_now=current_time->tv_sec*1000000+current_time->tv_usec;
    return (time_now-time_start);
}
