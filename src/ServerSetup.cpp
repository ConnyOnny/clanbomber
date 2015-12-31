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

#include "ServerSetup.h"

#include "ClanBomber.h"
#include "Server.h"
#include "Client.h"
#include "Mutex.h"
#include "Timer.h"
#include "Controller.h"
#include "Observer.h"
#include "GameConfig.h"
#include "Chat.h"
#include "Menu.h"

#include "Utils.h"

#include <stdio.h>

static ServerSetup* server_setup=NULL;
static float req_alpha=0;
static bool req_forward=true;
static Timer req_timer;

ServerSetup::ServerSetup(ClanBomberApplication* _app, Server* _server, Client* _client)
{
    app=_app;
    cur_row=0;
    cur_col=0;
    cb_server=_server;
    cb_client=_client;
    for(int i=0;i<8;i++) {
        Config::bomber[i].disable();
        Config::bomber[i].set_client_ip(cb_server->get_ip());
        Config::bomber[i].set_server_bomber(true);
        Config::bomber[i].set_highlight_maptile(false);
        Config::bomber[i].set_config_index(i);
        unselected_players.push_back(&Config::bomber[i]);
    }
    Config::save();
    Chat::hide();
    exit_setup=false;
    server_setup=this;
}

ServerSetup::~ServerSetup()
{
}

void ServerSetup::select_player(int index)
{
  //CL_Iterator<BomberConfig> players(server_setup->unselected_players);
  for(std::list<BomberConfig*>::iterator players
	= server_setup->unselected_players.begin();
      players != server_setup->unselected_players.end();
      players++) {
    if ((*players)->get_config_index() == index) {
      BomberConfig* bc = *players;
      server_setup->selected_players.push_back(bc);
      server_setup->unselected_players.erase(players);
      break;
    }
  }
}

void ServerSetup::unselect_player(int index)
{
  //CL_Iterator<BomberConfig> players(server_setup->selected_players);
  for(std::list<BomberConfig*>::iterator players
	= server_setup->selected_players.begin();
      players != server_setup->selected_players.end();
      players++) {
    if ((*players)->get_config_index() == index) {
      BomberConfig* bc = *players;
      server_setup->unselected_players.push_back(bc);
      server_setup->selected_players.erase(players);
      break;
    }
  }
}

void ServerSetup::show_player_selected(int index)
{
    int cg[5]={180, 230, 405, 500, 640};
    unsigned char cl_col_r=0;
    unsigned char cl_col_g=0;
    unsigned char cl_col_b=0;
    bool fick=false;
    ///primary->SetFont(primary, Resources::Font_small());
    int cl_index=Config::bomber[index].get_client_index();
    if(cl_index>=0) {
        cl_col_r=Client::get_client_color_r_by_index(cl_index);
        cl_col_g=Client::get_client_color_g_by_index(cl_index);
        cl_col_b=Client::get_client_color_b_by_index(cl_index);
    }
    if(Config::bomber[index].is_local()) {
        if(cl_index>=0) {
	  ///primary->SetColor(primary, cl_col_r, cl_col_g, cl_col_b, 255);
        }
        else {
	  ///primary->SetColor(primary, 70, 70, 240, 120);
        }
        ///primary->DrawString(primary, "* local *", -1, cg[4]+50, 200 + index*50, DSTF_CENTER);
	Resources::Font_small()->render(_("* local *"), cg[4]+50,
                                        200 + index*50, cbe::FontAlignment_0center);
        ///primary->SetColor(primary, 50, 50, 140, 120);
    }
    else {
        if(Config::bomber[index].is_server_bomber()) {
            if(cl_index>=0) {
	      ///primary->SetColor(primary, cl_col_r, cl_col_g, cl_col_b, 255);
            }    
            else {
	      ///primary->SetColor(primary, 90, 90, 200, 120);
            }
            ///primary->DrawString(primary, "* server *", -1, cg[4]+50, 200 + index*50, DSTF_CENTER);
	    Resources::Font_small()->render(_("* server *"), cg[4]+50,
                                            200 + index*50, cbe::FontAlignment_0center);
        }
        ///primary->SetColor(primary, 40, 40, 40, 120);
    }
    if(cl_index>=0) {
      ///primary->SetColor(primary, cl_col_r, cl_col_g, cl_col_b, 255);
    }
    if(Config::bomber[index].is_local()) {
      ///primary->DrawString(primary, server_setup->cb_client->get_ip(), -1, cg[1]-130, 205+index*50, DSTF_CENTER);
      Resources::Font_small()->render(server_setup->cb_client->get_ip(),
                                      cg[1]-130, 205+index*50,
                                      cbe::FontAlignment_0center);
    }
    else {
      ///primary->DrawString(primary, Config::bomber[index].get_client_ip(), -1, cg[1]-130, 205+index*50, DSTF_CENTER);
      Resources::Font_small()->render(Config::bomber[index].get_client_ip(),
                                      cg[1]-130, 205+index*50,
                                      cbe::FontAlignment_0center);
    }
    if(Config::bomber[index].get_highlight_maptile()) {
      ///primary->SetColor(primary, 0, 0, 255, 50); 
      ///primary->FillRectangle(primary, cg[0], 170+index*50, 50, 50);
      CB_FillRect(cg[0], 170+index*50, 50, 50, 0, 0, 255, 50);
    }
    Resources::Bombers(Config::bomber[index].get_skin())->put_screen( cg[0]+12, 230+index*50 - Resources::Bombers(Config::bomber[index].get_skin())->get_height(), 0.7, 0.7, 0);
    if(!fick || server_setup->cur_row!=index) {
      ///primary->SetFont(primary, Resources::Font_small());
      ///primary->SetColor(primary, 0x20, 0x90, 0xF0, 0xFF);
      ///primary->DrawString(primary, Config::bomber[index].get_name(), -1, cg[1]+20, 190+index*50, DSTF_TOPLEFT);
      Resources::Font_small()->render(Config::bomber[index].get_name(),
                                      cg[1]+20, 190+index*50,
                                      cbe::FontAlignment_0topleft);
    }       
    Resources::Playersetup_teams()->put_screen(cg[2]+5, 172+index*50, 0.7, 0.7, Config::bomber[index].get_team());
    ///primary->SetFont(primary, Resources::Font_small());
    ///primary->SetColor(primary, 255, 255, 255, 255);
    switch(Config::bomber[index].get_controller()) {   
        case Controller::AI:
            Resources::Playersetup_controls()->put_screen(cg[3]+20, 172+index*50, 0.7, 0.7, 0);
            ///primary->DrawString(primary, "DOK", -1, cg[3]+120, 182+index*50, DSTF_TOPRIGHT);   
	    Resources::Font_small()->render("DOK", cg[3]+120, 182+index*50,
                                            cbe::FontAlignment_0topright);
            break;
        case Controller::AI_mass:
            Resources::Playersetup_controls()->put_screen(cg[3]+20, 172+index*50, 0.7, 0.7, 0);
            ///primary->DrawString(primary, "MASS", -1, cg[3]+120, 182+index*50, DSTF_TOPRIGHT);   
	    Resources::Font_small()->render("MASS", cg[3]+120, 182+index*50,
                                            cbe::FontAlignment_0topright);
            break;
        case Controller::KEYMAP_1:
        case Controller::KEYMAP_2:
        case Controller::KEYMAP_3:
            Resources::Playersetup_controls()->put_screen(cg[3]+20, 172+index*50, 0.7, 0.7, Config::bomber[index].get_controller()-Controller::KEYMAP_1+1);
            break;
        case Controller::JOYSTICK_1:
        case Controller::JOYSTICK_2:
        case Controller::JOYSTICK_3:
        case Controller::JOYSTICK_4:
        case Controller::JOYSTICK_5:
        case Controller::JOYSTICK_6:
        case Controller::JOYSTICK_7:
            Resources::Playersetup_controls()->put_screen(cg[3]+20, 172+index*50, 0.7, 0.7, 4);
            break;
    }
}

void ServerSetup::show_player_unselected(int index)
{
    int cg[5]={180, 230, 405, 500, 640};
    bool fick=false;
    if(Config::bomber[index].get_highlight_maptile()) {
      ///primary->SetColor(primary, 0, 0, 255, 50);
      ///primary->FillRectangle(primary, cg[0], 170+index*50, 50, 50);
      CB_FillRect(cg[0], 170+index*50, 50, 50, 0, 0, 255, 50);
    }
    Resources::Bombers(Config::bomber[index].get_skin())->put_screen( cg[0]+12, 230+index*50 - Resources::Bombers(Config::bomber[index].get_skin())->get_height(), 0.7, 0.7, 0);
    if(!fick || server_setup->cur_row!=index) {
      ///primary->SetFont(primary, Resources::Font_small());
      ///primary->SetColor(primary, 0x20, 0x90, 0xF0, 0xFF);
      ///primary->DrawString(primary, Config::bomber[index].get_name(), -1, cg[1]+20, 190+index*50, DSTF_TOPLEFT);
      Resources::Font_small()->render(Config::bomber[index].get_name(),
                                      cg[1]+20, 190+index*50,
                                      cbe::FontAlignment_0topleft);
    }
    Resources::Playersetup_teams()->put_screen(cg[2]+5, 172+index*50, 0.7, 0.7, Config::bomber[index].get_team());
    ///primary->SetFont(primary, Resources::Font_small());
    ///primary->SetColor(primary, 255, 255, 255, 255);
    switch(Config::bomber[index].get_controller()) {
        case Controller::AI:
            Resources::Playersetup_controls()->put_screen(cg[3]+20, 172+index*50, 0.7, 0.7, 0);
            ///primary->DrawString(primary, "DOK", -1, cg[3]+120, 182+index*50, DSTF_TOPRIGHT);
	    Resources::Font_small()->render("DOK", cg[3]+120, 182+index*50,
                                            cbe::FontAlignment_0topright);
            break;
        case Controller::AI_mass:
            Resources::Playersetup_controls()->put_screen(cg[3]+20, 172+index*50, 0.7, 0.7, 0);
            ///primary->DrawString(primary, "MASS", -1, cg[3]+120, 182+index*50, DSTF_TOPRIGHT);
	    Resources::Font_small()->render("MASS", cg[3]+120, 182+index*50,
                                            cbe::FontAlignment_0topright);
            break;
        case Controller::KEYMAP_1:
        case Controller::KEYMAP_2:
        case Controller::KEYMAP_3:
            Resources::Playersetup_controls()->put_screen(cg[3]+20, 172+index*50, 0.7, 0.7, Config::bomber[index].get_controller()-Controller::KEYMAP_1+1);
            break;
        case Controller::JOYSTICK_1:
        case Controller::JOYSTICK_2:
        case Controller::JOYSTICK_3:
        case Controller::JOYSTICK_4:
        case Controller::JOYSTICK_5:
        case Controller::JOYSTICK_6:
        case Controller::JOYSTICK_7:
            Resources::Playersetup_controls()->put_screen(cg[3]+20, 172+index*50, 0.7, 0.7, 4);
            break;
    }
    ///primary->SetColor(primary, 0, 0, 0, 170);
    ///primary->SetDrawingFlags(primary, DSDRAW_BLEND);
    ///primary->FillRectangle(primary, 0, 170+index*50, 800, 50);
    CB_FillRect(0, 170+index*50, 800, 50, 0, 0, 0, 170);
}

void ServerSetup::exec()
{
    draw();
    ///primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC);
    CB_Flip();
    ///DFBInputEvent event;
    SDL_Event event;
    ///keybuffer->Reset(keybuffer);
    while(1) {
        cb_server->disconnect_dead_clients();
        cb_server->send_SERVER_KEEP_ALIVE();
        cb_server->send_update_messages_to_clients(ClanBomberApplication::get_server_frame_counter());
        if(!Chat::enabled() && cb_client->new_chat_message_arrived()) {
            draw();
            ///primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC);
	    CB_Flip();
        }
        ///if(keybuffer->GetEvent(keybuffer,DFB_EVENT(&event))==DFB_OK) {
	if(SDL_PollEvent(&event)) {
	  ///if(event.type!=DIET_KEYPRESS) {
	  if(event.type != SDL_KEYDOWN) {
	    continue;
	  }
        }
        else if(cb_client->is_updated()) {
            if(!Config::bomber[cur_row].is_local() && cur_col!=2) {
                cur_col=2;
            }
            draw();
            ///primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC);
	    CB_Flip();

            if(ClanBomberApplication::run_server_with_players()) {
                if(selected_players.size() >= ClanBomberApplication::run_server_with_players()) {
                    if(cb_server->get_nr_of_clients()>0 && Config::get_number_of_opponents()>1) {
                        cb_server->send_SERVER_CLIENT_INFO();
                        cb_server->send_SERVER_GAME_START();
                        cb_server->send_update_messages_to_clients(ClanBomberApplication::get_server_frame_counter());
                        cb_server->analyze_game_mode();
                    }
                }
            }

            continue;
        }
        else if(cb_client->server_started_game()) {
            return;
        }
        else {
            draw();
            ///primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC);
	    CB_Flip();
            continue;
        }

        switch(event.key.keysym.sym) {
	  ///case DIKI_ESCAPE:
	case SDLK_ESCAPE:
                cb_server->send_SERVER_END_OF_GAME();
                exit_setup=true;
                cb_server->send_SERVER_END_GAME_SESSION();
                cb_server->send_update_messages_to_clients(ClanBomberApplication::get_server_frame_counter());
                return;
		//case DIKI_BACKSPACE:
	case SDLK_BACKSPACE:
                Chat::enter();
                Chat::show();
                enter_chat_message(true);
                cb_client->reset_new_chat_message_arrived();
                Chat::hide();
                Chat::leave();
                break;
		//case DIKI_ENTER:
	case SDLK_RETURN:
          Resources::Menu_clear()->play();
                handle_enter();
                if(Config::bomber[cur_row].is_enabled()) {
                    cb_client->send_SERVER_BOMBER_CONFIG(true,
                                                         Config::bomber[cur_row].get_skin(),
                                                         Config::bomber[cur_row].get_team()+78,
                                                         Config::bomber[cur_row].get_controller(),
                                                         cur_row,
                                                         Config::bomber[cur_row].get_name().c_str());
                }
                break;
		///case DIKI_SPACE:
	case SDLK_SPACE:
                if(Config::bomber[cur_row].is_enabled()) {
                    cb_client->send_SERVER_BOMBER_CONFIG(false,
                                                         Config::bomber[cur_row].get_skin(),
                                                         Config::bomber[cur_row].get_team(),
                                                         Config::bomber[cur_row].get_controller(),
                                                         cur_row,
                                                         Config::bomber[cur_row].get_name().c_str());
                }
                else {
                    cb_client->send_SERVER_BOMBER_CONFIG(true,
                                                         Config::bomber[cur_row].get_skin(),
                                                         Config::bomber[cur_row].get_team(),
                                                         Config::bomber[cur_row].get_controller(),
                                                         cur_row,
                                                         Config::bomber[cur_row].get_name().c_str());
                }
                Resources::Menu_clear()->play();
                break;
		///case DIKI_O:
	case SDLK_o:
                ClanBomberApplication::get_menu()->execute_options_menu_hack();
                Resources::Menu_clear()->play();
                break;
		///case DIKI_H:
	case SDLK_h:
                if(Config::bomber[cur_row].is_local()) {
                    Config::bomber[cur_row].set_highlight_maptile(!Config::bomber[cur_row].get_highlight_maptile());
                    if(Config::bomber[cur_row].is_enabled()) {
                        cb_client->send_CLIENT_BOMBER_CONFIG(true,
                                                             Config::bomber[cur_row].get_skin(),
                                                             Config::bomber[cur_row].get_controller(),
                                                             cur_row,
                                                             Config::bomber[cur_row].get_name().c_str());
                    }
                }
                Resources::Menu_clear()->play();
                break;
		///case DIKI_S:
	case SDLK_s:
                if(cb_server->get_nr_of_clients()>0 && Config::get_number_of_opponents()>1) {
		    cb_server->send_SERVER_CLIENT_INFO();
                    cb_server->send_SERVER_GAME_START();
                    cb_server->send_update_messages_to_clients(ClanBomberApplication::get_server_frame_counter());
                    cb_server->analyze_game_mode();
                }
                break;
		///case DIKI_DOWN:
	case SDLK_DOWN:
                cur_row += (cur_row<7) ? 1 : 0;
                if(!Config::bomber[cur_row].is_local() && Config::bomber[cur_row].is_enabled()) {
                    cur_col=2;
                }
                Resources::Menu_break()->play();
                break;
		///case DIKI_UP:
	case SDLK_UP:
                cur_row -= (cur_row>0) ? 1 : 0;
                if(!Config::bomber[cur_row].is_local() && Config::bomber[cur_row].is_enabled()) {
                    cur_col=2;
                }
                Resources::Menu_break()->play();
                break;
		///case DIKI_LEFT:
	case SDLK_LEFT:
                if(Config::bomber[cur_row].is_local() || !Config::bomber[cur_row].is_enabled()) {
                    cur_col -= (cur_col>0) ? 1 : 0;
                }
                else {
                    cur_col=2;
                }
                Resources::Menu_break()->play();
                break;
		///case DIKI_RIGHT:
	case SDLK_RIGHT:
                if(Config::bomber[cur_row].is_local() || !Config::bomber[cur_row].is_enabled()) {
                    cur_col += (cur_col<3) ? 1 : 0;
                }
                else {
                    cur_col=2;
                }
                Resources::Menu_break()->play();
                break;
	default:
                break;
        }
        draw();
        ///primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC);
	CB_Flip();
    }
}

void ServerSetup::draw(bool fick)
{
    int cg[5]={180, 230, 405, 500, 640};
    ///primary->SetColor(primary, 0, 0, 0, 255);
    ///primary->SetDrawingFlags(primary, DSDRAW_NOFX);
    ///primary->FillRectangle(primary, 0, 0, 800, 600);
    CB_FillScreen(0, 0, 0);
    ///primary->SetFont(primary, Resources::Font_big());
    ///primary->SetColor(primary, 55, 110, 220, 255);
    ///primary->DrawString(primary, "SERVER MENU", -1, 400, 1, DSTF_TOPCENTER);
    Resources::Font_big()->render("SERVER MENU", 400, 1,
                                  cbe::FontAlignment_0topcenter);
    ///primary->SetFont(primary, Resources::Font_small());
    ///primary->SetColor(primary, 155, 155, 155, 255);
    ///primary->DrawString(primary, "NAME:", -1, 55, 1, DSTF_TOPRIGHT);
    Resources::Font_small()->render(_("NAME:"), 55, 1,
                                    cbe::FontAlignment_0topright);
    /**primary->SetColor(primary,
                      Client::get_client_color_r_by_index(server_setup->cb_client->get_my_index()),
                      Client::get_client_color_g_by_index(server_setup->cb_client->get_my_index()),
                      Client::get_client_color_b_by_index(server_setup->cb_client->get_my_index()),
                      255);**/
    ///primary->DrawString(primary, server_setup->cb_client->get_name(), -1, 65, 1, DSTF_TOPLEFT);
    Resources::Font_small()->render(server_setup->cb_client->get_name(), 65, 1,
                                    cbe::FontAlignment_0topleft);
    ///primary->SetColor(primary, 155, 155, 155, 255);
    ///primary->DrawString(primary, "IP:", -1, 55, 20, DSTF_TOPRIGHT);
    Resources::Font_small()->render(_("IP:"), 55, 20,
                                    cbe::FontAlignment_0topright);
    /**primary->SetColor(primary,
                      Client::get_client_color_r_by_index(server_setup->cb_client->get_my_index()),
                      Client::get_client_color_g_by_index(server_setup->cb_client->get_my_index()),
                      Client::get_client_color_b_by_index(server_setup->cb_client->get_my_index()),
                      255);**/
    ///primary->DrawString(primary, server_setup->cb_client->get_ip(), -1, 65, 20, DSTF_TOPLEFT);
    Resources::Font_small()->render(server_setup->cb_client->get_ip(), 65, 20,
                                    cbe::FontAlignment_0topleft);
    ///primary->SetColor(primary, 255, 255, 255, 255);
    ///primary->DrawString(primary, "SKIN", -1, cg[0]+(cg[1]-cg[0])/2, 150, DSTF_CENTER);
    Resources::Font_small()->render(_("SKIN"), cg[0]+(cg[1]-cg[0])/2, 150,
                                    cbe::FontAlignment_0center);
    ///primary->DrawString(primary, "NAME", -1, cg[1]+(cg[2]-cg[1])/2, 150, DSTF_CENTER);
    Resources::Font_small()->render(_("NAME"), cg[1]+(cg[2]-cg[1])/2, 150,
                                    cbe::FontAlignment_0center);
    ///primary->DrawString(primary, "TEAM", -1, cg[2]+(cg[3]-cg[2])/2, 150, DSTF_CENTER);
    Resources::Font_small()->render(_("TEAM"), cg[2]+(cg[3]-cg[2])/2, 150,
                                    cbe::FontAlignment_0center);
    ///primary->DrawString(primary, "CONTROLLER", -1, cg[3]+(cg[4]-cg[3])/2, 150, DSTF_CENTER);
    Resources::Font_small()->render(_("CONTROLLER"), cg[3]+(cg[4]-cg[3])/2,
                                    150, cbe::FontAlignment_0center);
    if(!Chat::enabled()) {
      /**primary->SetColor(primary, 255, 255, 255, 255);
        primary->DrawString(primary, "SPACE:", -1, 12, 580, DSTF_TOPLEFT);
        primary->SetColor(primary, 177, 177, 177, 255);
        primary->DrawString(primary, "SELECT PLAYER", -1, 73, 580, DSTF_TOPLEFT);
        primary->SetColor(primary, 255, 255, 255, 255);
        primary->DrawString(primary, "H:", -1, 240, 580, DSTF_TOPLEFT);
        primary->SetColor(primary, 177, 177, 177, 255);
        primary->DrawString(primary, "TOGGLE HIGHLIGHTING", -1, 260, 580, DSTF_TOPLEFT);
        primary->SetColor(primary, 255, 255, 255, 255);
        primary->DrawString(primary, "O:", -1, 495, 580, DSTF_TOPLEFT);
        primary->SetColor(primary, 177, 177, 177, 255);
        primary->DrawString(primary, "OPTIONS", -1, 517, 580, DSTF_TOPLEFT);
        primary->SetColor(primary, 255, 255, 255, 255);
        primary->DrawString(primary, "BACKSPACE:", -1, 640, 580, DSTF_TOPLEFT);
        primary->SetColor(primary, 177, 177, 177, 255);
        primary->DrawString(primary, "CHAT", -1, 745, 580, DSTF_TOPLEFT);**/
        show_chat_request ();
    }
    ///primary->SetColor(primary, 55, 110, 220, 70);
    ///primary->SetDrawingFlags(primary, DSDRAW_BLEND);
    ///primary->FillRectangle(primary, 0, 170+server_setup->cur_row*50, 800, 50);
    CB_FillRect(0, 170+server_setup->cur_row*50, 800, 50, 55, 110, 220, 70);
    if(!Chat::enabled() && server_setup->cb_server->get_nr_of_clients()>0 && Config::get_number_of_opponents()>1) {
      ///primary->SetColor(primary, 155, 110, 220, 255);
      ///primary->SetFont(primary, Resources::Font_big());
      ///primary->DrawString(primary, "press S to start the game!", -1, 400, 80, DSTF_CENTER);
      Resources::Font_big()->render("press S to start the game", 400, 80,
                                    cbe::FontAlignment_0center);
    }
    if(!fick) {
      ///primary->SetColor(primary, 40, 40, 240, 120);
      ///primary->FillRectangle(primary, cg[server_setup->cur_col], 170+server_setup->cur_row*50, cg[server_setup->cur_col+1]-cg[server_setup->cur_col], 50);
      CB_FillRect(cg[server_setup->cur_col], 170+server_setup->cur_row*50, cg[server_setup->cur_col+1]-cg[server_setup->cur_col], 50, 40, 40, 240, 120);
    }
    ClanBomberApplication::lock();
    //CL_Iterator<BomberConfig> sel_players(server_setup->selected_players);
    for(std::list<BomberConfig*>::iterator sel_players
	  = server_setup->selected_players.begin();
	sel_players != server_setup->selected_players.end();
	sel_players++) {
      show_player_selected((*sel_players)->get_config_index());
    }
    //CL_Iterator<BomberConfig> unsel_players(server_setup->unselected_players);
    for(std::list<BomberConfig*>::iterator unsel_players
	  = server_setup->unselected_players.begin();
	unsel_players != server_setup->unselected_players.end();
	unsel_players++) {
      show_player_unselected((*unsel_players)->get_config_index());
    }
    ClanBomberApplication::unlock();
    Chat::draw();
}

void ServerSetup::handle_enter()
{
    switch(cur_col) {
        case 0:
            Config::bomber[cur_row].set_skin(Config::bomber[cur_row].get_skin()+1);
            break;
        case 1:
            enter_name();
            Resources::Menu_clear()->play();
            break;
        case 2:
            Config::bomber[cur_row].set_team(Config::bomber[cur_row].get_team()+1);
            break;
        case 3:
            Config::bomber[cur_row].set_controller(Config::bomber[cur_row].get_controller()+1);

#ifndef NET_SERVER_ALLOW_SERVER_AI
            while(Config::bomber[cur_row].get_controller()==Controller::AI ||
                  Config::bomber[cur_row].get_controller()==Controller::AI_mass) {
                Config::bomber[cur_row].set_controller(Config::bomber[cur_row].get_controller()+1);
            }
#endif

            break;
    }
}

void ServerSetup::enter_name()
{
    float alpha=0;
    ///keybuffer->Reset(keybuffer);
    std::string new_string=Config::bomber[cur_row].get_name();
    Timer::reset();
    /**primary->SetDrawingFlags(primary, DSDRAW_BLEND);
    while(1) {
        cb_server->disconnect_dead_clients();
        cb_server->send_SERVER_KEEP_ALIVE();
        alpha+=Timer::time_elapsed(true);
        while(alpha>0.5f) {
            alpha-=0.5f;
        }
        draw(true);
        primary->SetColor(primary, 50, 220, 128, (alpha>0.25f) ? (unsigned char)((0.5f-alpha)*3.0f*255) : (unsigned char)(alpha*3.0f*255));
        primary->FillRectangle(primary, 230, 170+cur_row*50, 175, 50);
        primary->SetFont(primary, Resources::Font_small());
        primary->SetColor(primary, 255, 255, 255, 255);
        primary->DrawString(primary, new_string, -1, 250, 190+cur_row*50, DSTF_TOPLEFT);
        primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC);
        if(enter_string(new_string)) {
            Config::bomber[cur_row].set_name(new_string);
            cb_server->send_update_messages_to_clients(ClanBomberApplication::get_server_frame_counter());
            return;
        }
        cb_server->send_update_messages_to_clients(ClanBomberApplication::get_server_frame_counter());
    }**/
}

bool ServerSetup::end_setup()
{
    return exit_setup;
}

void ServerSetup::show_chat_request()
{
    /**if(server_setup->cb_client->new_chat_message_arrived()) {
	if(req_forward) {
            req_alpha+=req_timer.time_elapsed(true);
 	} 
        else {
            req_alpha-=req_timer.time_elapsed(true);
	}
        if(req_alpha>1.0f) {  
 	    req_alpha=1.0f;   
            req_forward=false;
        }
        else if(req_alpha<0) {
            req_alpha=0;
            req_forward=true;
        }
        primary->SetDrawingFlags(primary, DSDRAW_BLEND);
        int cl_index=server_setup->cb_client->get_last_chat_message_client_index();
        primary->SetColor(primary,
                          Client::get_client_color_r_by_index(cl_index),
                          Client::get_client_color_g_by_index(cl_index),
                          Client::get_client_color_b_by_index(cl_index),
                          (unsigned char)(req_alpha*255)); 
        primary->FillRectangle(primary, 638, 582, 152, 20);
	}**/
}

bool ServerSetup::enter_chat_message(bool fick)
{
    /**keybuffer->Reset(keybuffer);
    CL_String new_string=server_setup->cb_client->current_chat_message;
    Timer::reset();
    while (1) {
        server_setup->cb_server->disconnect_dead_clients();
        server_setup->cb_server->send_SERVER_KEEP_ALIVE();
        Chat::draw();
        primary->SetFont(primary, Resources::Font_small());
        primary->SetColor(primary, 255, 255, 255, 255);
        primary->DrawString(primary, new_string, -1, MSG_X_OFF+4, CHAT_MESSAGES_COUNT*30+MSG_Y_OFF+4, DSTF_TOPLEFT);
        primary->Flip(primary, NULL, DSFLIP_WAITFORSYNC);
        int enter_status;
        if((enter_status=enter_string(new_string))) {
            if(enter_status>0) {
                server_setup->cb_client->current_chat_message=new_string;
                server_setup->cb_client->send_CLIENT_CHAT_MESSAGE(0);
                server_setup->cb_client->current_chat_message=CL_String("");
                new_string=CL_String("");
            }
            else {
                server_setup->cb_client->current_chat_message=new_string;
                return false;
            }
        }
        server_setup->cb_server->send_update_messages_to_clients(ClanBomberApplication::get_server_frame_counter());
	}**/
    return false;
}
