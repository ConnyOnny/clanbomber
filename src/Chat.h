/*
 * This file is part of ClanBomber;
 * you can get it at "http://www.nongnu.org/clanbomber".
 *
 * Copyright (C) 2003, 2004, 2007 mass
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

#ifndef _header_clanbomber_chat_
#define _header_clanbomber_chat_

#include "Client.h"

#define CHAT_MESSAGES_COUNT  17
#define CHAT_MESSAGE_WIDTH  222

#define MSG_Y_OFF 35
#define MSG_X_OFF 74

typedef char msgparts[256][1024];

class ClanBomberApplication;
class Mutex;
class Client;

class Chat
{
    public:
        Chat(ClanBomberApplication* app);
        ~Chat();
        static void set_client(Client* cl);
        static void reset();
        static bool enabled();
        static void draw();
        static void show();
        static void hide();
        static void add(char* message);
        static void enter();
        static void leave();
    public:
        ClanBomberApplication* cb_app;
        Client* cb_client;
	std::list<char*> messages;
        int num_messages;
        Mutex* exclusion;
        bool is_enabled;
};

#endif
