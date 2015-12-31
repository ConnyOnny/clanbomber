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

#ifndef _header_clanbomber_clientsetup_
#define _header_clanbomber_clientsetup_

#include <list>

class ClanBomberApplication;
class Client;
class BomberConfig;

class ClientSetup {
public: 
    ClientSetup(ClanBomberApplication* _app, Client* _client);
    ~ClientSetup();
    void exec();
    bool end_setup();
    bool is_disconnected();

    static void set_disconnected();
    static void end_session();
    static bool enter_chat_message(bool fick);
    static void show_chat_request();
    static void draw(bool fick=false);

    static void select_player(int index);  
    static void unselect_player(int index);
protected:
    ClanBomberApplication* app;
    int cur_row;
    int cur_col;
    Client* cb_client;
    void enter_name();
    void handle_enter();
    bool server_is_full;
    bool exit_setup;
    bool disconnected;

    static void show_player_selected(int index);
    static void show_player_unselected(int index);
        
    std::list<BomberConfig*> selected_players;
    std::list<BomberConfig*> unselected_players;
};

#endif
