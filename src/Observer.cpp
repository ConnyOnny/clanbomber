/*
 * This file is part of ClanBomber;
 * you can get it at "http://www.nongnu.org/clanbomber".
 *
 * Copyright (C) 1999-2004, 2007 Andreas Hundt, Denis Oliver Kropp
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

#include "ClanBomber.h"
#include "Observer.h"

#include "GameConfig.h"
#include "Timer.h"
#include "GameStatus_Team.h"
#include "Map.h"
#include "MapTile.h"
#include "Controller.h"
#include "Server.h"
#include "Client.h"
#include "Mutex.h"
#include "Extra.h"

#include <stdio.h>
#include <math.h>
#include <boost/format.hpp>

#include "Utils.h"

Observer::Observer( int _x, int _y, ClanBomberApplication *_app ) : GameObject( _x, _y, _app )
{
	x = 0;
	y = 0;
	z = Z_OBSERVER;
	end_of_game = false;
	client_game_runs = false;
	surface = Resources::Observer_observer();
	sprite_nr = 0;
	offset_x = 10;
	offset_y = 2;
	speed = 300;
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
	  //CL_Iterator<Bomber> bomber_object_counter(app->bomber_objects);
	  mode = NORMAL;
	  for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
	      bomber_object_iter != app->bomber_objects.end();
	      bomber_object_iter++) {
	    if ((*bomber_object_iter)->get_team() != 0) {
	      mode = TEAM;
	    }
	  }	
	  if (mode == TEAM) {
	    game_status = new GameStatus_Team (_app);
	  }
	  else {
	    game_status = new GameStatus (_app);
	  }
	}
	reset_round_time();
	first_destruction = true;
	repaint_hack = 0;
}

Observer::~Observer()
{
	delete game_status;
}

bool Observer::end_of_game_requested()
{
	return end_of_game;
}

int Observer::active_players()	
{
	int c = 0;
	int t = -1;

	//CL_Iterator<Bomber> bomber_object_counter(app->bomber_objects);
	for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
	    bomber_object_iter != app->bomber_objects.end();
	    bomber_object_iter++) {
	  if (!(*bomber_object_iter)->is_dead()) {
	    if (((*bomber_object_iter)->get_team() != t) || ((*bomber_object_iter)->get_team() == 0)) {
	      t = (*bomber_object_iter)->get_team();
	      c++;
	    }
	  }
	}
	return c;
}

float Observer::get_round_time() const
{
	return round_time;
}

void Observer::reset_round_time()
{
	round_time=Config::get_round_time();
	play_hurryup = true;
	first_destruction = true;
	repaint_hack = 0;
	x = 0;
	y = 0;
	flying = false;
}

void Observer::act()
{
	GameObject::act();
	if (!ClanBomberApplication::is_server() && ClanBomberApplication::is_client()) {
		if (client_game_runs) {
			ClanBomberApplication::lock();
			//CL_Iterator<Bomber> bomber_object_counter(app->bomber_objects);
			for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
			    bomber_object_iter != app->bomber_objects.end();
			    bomber_object_iter++) {
			  if ((*bomber_object_iter)->is_flying()) {
			    ClanBomberApplication::unlock();
			    return;
			  }
			}
			//CL_Iterator<Bomber> bomber_object_counter2(app->bomber_objects);
			for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
			    bomber_object_iter != app->bomber_objects.end();
			    bomber_object_iter++) {
			  (*bomber_object_iter)->controller->activate();
			}
			ClanBomberApplication::unlock();
			round_time -= Timer::time_elapsed();
			if (round_time < 0) {
				if (first_destruction) {
					offset_x = 60;
					offset_y = 40;
					Resources::Observer_time_over()->play();
					first_destruction=false;
				}
				if (repaint_hack < 2 && round_time < -1) {
					repaint_hack++;
					///primary->SetColor( primary, 0, 0, 0, 0xFF );
					///primary->FillRectangle( primary, 0, 0, 60, 200 );
				}
			}
		}
		else {
			reset_next_fly_job();
			ClanBomberApplication::lock();
                        for (std::list<GameObject*>::iterator object_iter = app->objects.begin();
                             object_iter != app->objects.end();
                             object_iter++) {
                          delete *object_iter;
                        }
			//CL_Iterator<Bomber> bomber_object_counter(app->bomber_objects);
			for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
			    bomber_object_iter != app->bomber_objects.end();
			    bomber_object_iter++) {
			  if ((*bomber_object_iter)->is_disconnected()) {
			    delete (*bomber_object_iter);
			    bomber_object_iter = app->bomber_objects.erase(bomber_object_iter);
			  }
			}
			reset_round_time();
			game_status->analyze_game();
			ClanBomberApplication::unlock();
 	 	 	game_status->show();
			Timer::reset();
		}
	}
	else {
		ClanBomberApplication::lock();
		if (round_time == Config::get_round_time()) {
		  //CL_Iterator<Bomber> bomber_object_counter(app->bomber_objects);
		  for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
		      bomber_object_iter != app->bomber_objects.end();
		      bomber_object_iter++) {
		    if ((*bomber_object_iter)->is_flying()) {
		      ClanBomberApplication::unlock();
		      return;
		    }
		  }
		  //CL_Iterator<Bomber> bomber_object_counter2(app->bomber_objects);
		  for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
		      bomber_object_iter != app->bomber_objects.end();
		      bomber_object_iter++) {
		    (*bomber_object_iter)->controller->activate();
		  }
		}
		round_time -= Timer::time_elapsed();
		if (round_time < 0) {
			offset_x = 60;
			offset_y = 40;
			if (!flying) {
				if (first_destruction) {
                                  Resources::Observer_time_over()->play();
					first_destruction = false;
				}
				else {
					MapTile* vanish_maptile=get_maptile();
					if (ClanBomberApplication::is_server() && app->map->is_dummy_maptile(vanish_maptile)) {
						kill_all_bombers();
					}
					vanish_maptile->vanish();
					Resources::Observer_crunch()->play();
				}
				MapTile* maptile = app->map->get_random();
				if (maptile) {
					fly_to(maptile, 500);
				}
				else if (ClanBomberApplication::is_server()) {
					kill_all_bombers();
				}
			}
		}
		if (active_players() < 2) {
		  //CL_Iterator<GameObject> object_counter(app->objects);
		  for(std::list<GameObject*>::iterator object_iter = app->objects.begin();
		      object_iter != app->objects.end();
		      object_iter++) {
		    if ((*object_iter)->get_type() == GameObject::EXPLOSION) {
		      ClanBomberApplication::unlock();
		      return;
		    }
		    if ((*object_iter)->get_type() == GameObject::EXTRA && (*object_iter)->is_flying()) {
		      (*object_iter)->show();
		      ClanBomberApplication::unlock();
		      return;
		    }
		  }
		  //CL_Iterator<Bomber> bomber_object_counter(app->bomber_objects);
		  for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
		      bomber_object_iter != app->bomber_objects.end();
		      bomber_object_iter++) {
		    if (!(*bomber_object_iter)->is_dead() && (*bomber_object_iter)->is_falling()) {
		      ClanBomberApplication::unlock();
		      return;
		    }
		  }
		  offset_x = 10;
		  offset_y = 2;
		  //CL_Iterator<Bomber> bomber_object_counter2(app->bomber_objects);
		  for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
		      bomber_object_iter != app->bomber_objects.end();
		      bomber_object_iter++) {
		    if (ClanBomberApplication::is_server() && (*bomber_object_iter)->is_disconnected()) {
		      delete (*bomber_object_iter);
		      bomber_object_iter = app->bomber_objects.erase(bomber_object_iter);
		    }
		    else if (!(*bomber_object_iter)->is_dead()) {
		      (*bomber_object_iter)->inc_points();
		    }
		  }
		  if (Config::get_random_map_order()) {
		    app->map->load_random_valid();
		  }
		  else {
		    app->map->load_next_valid();
		  }
		  if (ClanBomberApplication::is_server()) {
		    ClanBomberApplication::get_server()->increase_maps_played();
		    ClanBomberApplication::get_server()->output_traffic_statistics();
		    ClanBomberApplication::get_server()->send_SERVER_END_OF_LEVEL();
		    ClanBomberApplication::get_server()->send_SERVER_UPDATE_MAP();
		    ClanBomberApplication::get_server()->send_update_messages_to_clients(app->get_server_frame_counter());
		  }
		  reset_round_time();
		  game_status->analyze_game();
		  ClanBomberApplication::unlock();
		  game_status->show();
		  ClanBomberApplication::lock();
		  Timer::reset();
		  end_of_game = game_status->get_end_of_game(); //FIXME
                  for (std::list<GameObject*>::iterator object_iter = app->objects.begin();
                       object_iter != app->objects.end();
                       object_iter++) {
                    delete *object_iter;
                  }
                  app->objects.clear();
		  CL_Vector pos;
		  if (Config::get_random_positions()) {
		    app->map->randomize_bomber_positions();
		  }
		  if(ClanBomberApplication::is_server() && !end_of_game) {
		    ClanBomberApplication::get_server()->send_SERVER_START_NEW_LEVEL();
		    ClanBomberApplication::get_server()->reset_traffic_statistics();
		    ClanBomberApplication::get_server()->send_update_messages_to_clients(app->get_server_frame_counter());
		  }
		  int c = 0;
		  //CL_Iterator<Bomber> bomber_object_counter3(app->bomber_objects);
		  for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
		      bomber_object_iter != app->bomber_objects.end();
		      bomber_object_iter++) {
		    pos = app->map->get_bomber_pos(c++);
		    (*bomber_object_iter)->set_orig( (int)(pos.x*40), (int)(pos.y*40) );
		    (*bomber_object_iter)->reset();
		    (*bomber_object_iter)->set_pos( 350, 270 );
		    (*bomber_object_iter)->fly_to( (int)(pos.x*40), (int)(pos.y*40), 200 );
		    (*bomber_object_iter)->controller->deactivate();
		  }
		}
		ClanBomberApplication::unlock();
	}
}

void Observer::kill_all_bombers()
{
  //CL_Iterator<Bomber> bomber_object_counter(app->bomber_objects);
  for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
      bomber_object_iter != app->bomber_objects.end();
      bomber_object_iter++) {
    (*bomber_object_iter)->die();
  }
}

void Observer::show()
{
  ///primary->SetBlittingFlags( primary, DSBLIT_NOFX );
  ///primary->Blit( primary, Resources::Game_cb_logo_small(), NULL, 325, 0 );
  Resources::Game_cb_logo_small()->blit(325, 0);
  GameObject::show();
  ///primary->SetColor( primary, 0xFF, 0xFF, 0xFF, 0xFF );
  ///primary->SetFont( primary, Resources::Font_big() );
  if (round_time > 0) {
    Resources::Observer_observer()->put_screen (600,1,1);
    ///primary->DrawString( primary, CL_String((int)round_time), -1, 650, 3, DSTF_TOPLEFT );
    Resources::Font_big()->render(str(boost::format("%d") % (int)round_time),
                                  650, 3, cbe::FontAlignment_0topleft);
  }
  else {
    Resources::Observer_observer()->put_screen (600,1,2);
  }		
  
  ///primary->SetFont( primary, Resources::Font_small() );
  if (app->is_server() || !app->is_client()) {
    ///primary->DrawString( primary, app->map->get_name(), -1, 80, 5, DSTF_TOPLEFT );		
    Resources::Font_small()->render(app->map->get_name(), 80, 5,
                                    cbe::FontAlignment_0topleft);
  }
  else {
    ///primary->DrawString( primary, app->map->current_server_map_name, -1, 80, 5, DSTF_TOPLEFT );
    Resources::Font_small()->render(app->map->current_server_map_name, 80, 5,
                                    cbe::FontAlignment_0topleft);
  }
  if ((round_time < 21) && (round_time > 18)) {
    if (fmod(round_time,0.3f) > 0.15f) {
      ///primary->SetDrawingFlags( primary, DSDRAW_BLEND);
      ///primary->SetColor( primary, 0, 0, 0, 128 );
      ///primary->FillRectangle( primary, 300, 250, 200, 30 );
      CB_FillRect(300, 250, 200, 30, 0, 0, 0, 128);
      ///primary->SetColor( primary, 0xFF, 0x20, 0x20, 0xFF );
      ///primary->DrawString( primary, "HURRY UP !!!!", -1, 400, 255, DSTF_TOPCENTER );
      Resources::Font_small()->render(_("HURRY UP !!!!"), 400, 255,
                                      cbe::FontAlignment_0topcenter);
    }
    if (play_hurryup) {
      Resources::Observer_hurry_up()->play();
      play_hurryup = false;
    }
  }
}

void Observer::set_client_game_runs(bool is_running)
{
	client_game_runs = is_running;
}

bool Observer::client_game_is_running()
{
	return client_game_runs;
}

void Observer::make_client_game_status()
{
  //CL_Iterator<Bomber> bomber_object_counter(app->bomber_objects);
  for(std::list<Bomber*>::iterator bomber_object_iter = app->bomber_objects.begin();
      bomber_object_iter != app->bomber_objects.end();
      bomber_object_iter++) {
    mode = NORMAL;
    if ((*bomber_object_iter)->get_team() != 0) {   
      mode = TEAM;
    }
  }
  if (mode == TEAM) {
    game_status = new GameStatus_Team (app);
  }
  else {
    game_status = new GameStatus (app);
  }
}

GameStatus* Observer::get_game_status()
{
	return game_status;
}
