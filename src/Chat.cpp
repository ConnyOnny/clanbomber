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

#include "Chat.h"

#include "ClanBomber.h"
#include "Client.h"
#include "Mutex.h"
#include "Utils.h"

#include <stdio.h>

static Chat* chat=NULL;

static int get_msg_parts (char *msg, msgparts parts);

Chat::Chat(ClanBomberApplication* app)
{
    cb_app=app;
    exclusion=new Mutex();
    is_enabled=false;
    num_messages=0;
    cb_client=NULL;
    chat=this;
/*
    msgparts parts;
    char msg[6666]="das sollte jetzt zu testzwecken eine echt krass lange message sein, oder...? na hoffentlich lang genug. funktioniert der automatische zeilenumbruch mit maximaler stringzeilenlaenge jetzt endlich oder wie? na wie sieht es denn nun aus? na das werden wir ja gleich mal sehen, bin jedenfalls schon ziemlich gespannt drauf, weil ich echt _fucking_ lange damit rumspielen musste eher es funktionierte ;) problem waren vor allem einzelne worte die die definierte maximalzeilenlaenge ueberschritten haben, so wie jetzt... nun denn, die routine scheint zu funktionieren, jetzt muss es nur noch im chat in echtzeit beim tippen getestet werden...";
    int num=get_msg_parts(msg, parts);
    for (int i = 0; i < num; i++) {
        printf("msgpart(%02d): <%s>\n", i, parts[i]);
    }
*/
}

Chat::~Chat()
{
}

void Chat::set_client(Client* cl)
{
    chat->cb_client=cl;
}

void Chat::reset()
{
    chat->exclusion->lock();
    chat->is_enabled=false;
    chat->num_messages=0;
    chat->cb_client=NULL;
    while(chat->messages.size()>0) {
        char* del_msg=chat->messages.front();
        chat->messages.pop_front();
        delete del_msg;
    }
    chat->exclusion->unlock();
}

bool Chat::enabled()
{
    chat->exclusion->lock();
    bool ret=chat->is_enabled;
    chat->exclusion->unlock();
    return ret;
}

void Chat::draw()
{
    chat->exclusion->lock();
    if(chat->is_enabled) {
        ///primary->SetDrawingFlags(primary, DSDRAW_NOFX);
        ///primary->SetColor(primary, 111, 111, 111, 222);
        ///primary->FillRectangle(primary, 0, 0, 800, 600);
        CB_FillScreen(0, 0, 0);

        ///primary->SetColor(primary, 70, 100, 100, 70);
        ///primary->FillRectangle(primary, 250, 0, 300, MSG_Y_OFF);
        ///primary->SetFont(primary, Resources::Font_big());
        ///primary->SetColor(primary, 255, 255, 255, 255);
        ///primary->DrawString(primary, "~ clanbomber chat ~", -1, 400, 0, DSTF_TOPCENTER);

        int i;
        for(i=0;i<CHAT_MESSAGES_COUNT;i++) {
            if (i%2) {
              ///primary->SetColor(primary, 50, 100, 130, 70);
            }
            else {
              ///primary->SetColor(primary, 50, 105, 130, 70);
            }
            ///primary->FillRectangle(primary, 1, i*30+MSG_Y_OFF+1, 798, 29);
        }
        ///primary->SetColor(primary, 111, 111, 111, 222);
        ///primary->FillRectangle(primary, MSG_X_OFF, MSG_Y_OFF, 1, (CHAT_MESSAGES_COUNT+1)*30+1);
        ///primary->SetColor(primary, 90, 100, 130, 100);
        ///primary->FillRectangle(primary, 1, i*30+MSG_Y_OFF+1, MSG_X_OFF-1, 29);
        ///primary->SetColor(primary, 255, 255, 255, 255);
        ///primary->SetFont(primary, Resources::Font_small());
        ///primary->DrawString(primary, "> > >", -1, 11, i*30+MSG_Y_OFF+3, DSTF_TOPLEFT);
        ///primary->DrawString(primary, "PRESS ESCAPE TO QUIT", -1, 400, 578, DSTF_TOPCENTER);

        /**primary->SetColor(primary,
                          Client::get_client_color_r_by_index(chat->cb_client->get_my_index()),
                          Client::get_client_color_g_by_index(chat->cb_client->get_my_index()),
                          Client::get_client_color_b_by_index(chat->cb_client->get_my_index()),
                          255);**/
        ///primary->FillRectangle(primary, MSG_X_OFF, CHAT_MESSAGES_COUNT*30+MSG_Y_OFF+1, 800-MSG_X_OFF-1, 29);

        ///primary->SetFont(primary, Resources::Font_small());
	std::list<char*>::iterator message;
        i=0;
        for(message=chat->messages.begin();message!=chat->messages.end();message++) {
            int client_index=(*message)[0];
            int autom=(*message)[1];
            int j=0;
            if(autom) {
                for (j=0;j<5;j++){
		    ///primary->SetColor(primary, 20+j*7, 20+j*7, 20+j*7, 255);
                    ///primary->DrawRectangle(primary, 1+j, i*30+MSG_Y_OFF+1+j, MSG_X_OFF-1-j*2, 29-j*2);
                }
            }
            /**primary->SetColor(primary,
                              Client::get_client_color_r_by_index(client_index),
                              Client::get_client_color_g_by_index(client_index),
                              Client::get_client_color_b_by_index(client_index),
                              255);**/
            ///primary->SetDrawingFlags(primary, DSDRAW_NOFX);
            ///primary->FillRectangle(primary, 1+j, i*30+MSG_Y_OFF+1+j, MSG_X_OFF-1-j*2, 29-j*2);
            ///primary->SetColor(primary, 255, 255, 255, 255);
            ///primary->DrawString(primary, (*message) + 2, -1, MSG_X_OFF+4, i*30+MSG_Y_OFF+4, DSTF_TOPLEFT);
            i++;
        }
    }
    chat->exclusion->unlock();
}

void Chat::show()
{
    chat->exclusion->lock();
    chat->is_enabled=true;
    chat->exclusion->unlock();
}

void Chat::hide()
{
    chat->exclusion->lock();
    chat->is_enabled=false;
    chat->exclusion->unlock();
}

void Chat::enter()
{
    chat->exclusion->lock();
    char msg[256] = "";
    strcat (msg, "'");
    strcat (msg, chat->cb_client->get_name());
    strcat (msg, "' entered the chat.");
    chat->cb_client->current_chat_message=std::string(msg);
    chat->cb_client->send_CLIENT_CHAT_MESSAGE(1);
    chat->cb_client->current_chat_message="";
    chat->cb_client->reset_new_chat_message_arrived();
    chat->exclusion->unlock();
}
                                                                                                                                                                                                                                
void Chat::leave()
{
    chat->exclusion->lock();
    char msg[256] = "";
    strcat (msg, "'");
    strcat (msg, chat->cb_client->get_name());
    strcat (msg, "' left the chat.");
    chat->cb_client->current_chat_message=std::string(msg);
    chat->cb_client->send_CLIENT_CHAT_MESSAGE(1);
    chat->cb_client->current_chat_message=std::string("");
    chat->cb_client->reset_new_chat_message_arrived();
    chat->exclusion->unlock();
}

static int get_msg_parts (char *msg, msgparts parts)
{
    char  sep  = ' ';
    char *tok  = msg;
    char *last = 0;
    char *tmsg = msg;
    int   nr   = 0;
    ///DFBRectangle lrect;
    SDL_Rect lrect;

    if (!tmsg) {
        return -1;
    }

    /**if (Resources::Font_small()->GetStringExtents (Resources::Font_small(), tmsg, -1, &lrect, NULL)) {
        return -1;
    }**/

    if (lrect.w <= CHAT_MESSAGE_WIDTH) {
        return 0;
    }

    while (1) {
        last = tok;
        tok = strchr(tok, sep);
        if (!tok) {
            break;
        }
        else {
            if (abs(last-tok) < 1) {
                tok++;
            }
        }

        /**if (Resources::Font_small()->GetStringExtents (Resources::Font_small(), tmsg, abs(tmsg-tok), &lrect, NULL)) {
            return -1;
	}**/

        if (lrect.w > CHAT_MESSAGE_WIDTH) {
            if (last == tmsg) {
                last = tok;
            }

            //printf("tmsg <%s> tok <%s> last <%s>\n", tmsg, tok, last);
            strncpy (parts[nr], tmsg, abs(tmsg-last));
            parts[nr][abs(tmsg-last)] = 0;
            //printf("copied part <%s>\n", parts[nr]);
            tmsg = last;
            tok  = last;
            nr++;
        }
    }

    strcpy (parts[nr++], tmsg);
    //printf("copied last <%s>\n", parts[nr-1]);

    return nr;
}

void Chat::add(char* message)
{
    chat->exclusion->lock();
    while(chat->messages.size()>=CHAT_MESSAGES_COUNT) {
        char* del_msg=chat->messages.front();
        chat->messages.pop_front();
        delete del_msg;
    }
    chat->messages.push_back(message);
    chat->num_messages++;
    chat->exclusion->unlock();
}
