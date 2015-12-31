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

#ifndef _header_clanbomber_serversetup_
#define _header_clanbomber_serversetup_

#include "ClientSetup.h"

class ClanBomberApplication;
class Server;
class Client;
class BomberConfig;

class ServerSetup {
public: 
    ServerSetup(ClanBomberApplication* _app, Server* _server, Client* _client);
    ~ServerSetup();

    void exec();
    bool end_setup();

    static bool enter_chat_message(bool fick);
    static void show_chat_request();
    static void draw(bool fick=false);

    static void select_player(int index);  
    static void unselect_player(int index);
protected:
    ClanBomberApplication* app;
    int cur_row;
    int cur_col;
    Server* cb_server;
    Client* cb_client;
    void enter_name();
    void handle_enter();
    bool exit_setup;

    static void show_client_messages();
    static void show_player_selected(int index);
    static void show_player_unselected(int index);

    std::list<BomberConfig*> selected_players;
    std::list<BomberConfig*> unselected_players;
};

#endif
