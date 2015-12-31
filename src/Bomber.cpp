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
#include "Resources.h"

#include "Bomber.h"

#include "Bomber_Corpse.h"
#include "Bomb.h"
#include "GameConfig.h"
#include "Timer.h"
#include "Controller.h"
#include "Disease.h"
#include "Map.h"
#include "MapTile.h"
#include "Client.h"
#include "Mutex.h"
#include "Observer.h"

#include "Extra_Bomb.h"
#include "Extra_Skateboard.h"
#include "Extra_Power.h"
#include "Extra_Kick.h"
#include "Extra_Glove.h"
#include "Utils.h"

#include <math.h>

Bomber::Bomber( int _x, int _y, COLOR _color, Controller* _controller, std::string _name, int _team, int _number, ClanBomberApplication *_app) : GameObject( _x, _y, _app )
{
	name = _name;
	team = _team;
	number = _number;

	surface = Resources::Bombers(_color);

	color = _color;
	points = 0;
	kills = 0;
	deaths = 0;
	disease = NULL;
	
	has_disconnected = false;

	switch (color) {
		case 0 :
		case 1 :
		case 3 :
			offset_x = 60;
			offset_y = 11;
			break;
		case 2 :
		case 4 :
		case 5 :
		case 6 :
		case 7 :
			offset_x = 60;
			offset_y = 40;	
	}
	controller = _controller;
	if (controller != NULL) {
		controller->attach(this);
		controller->reset();
		controller->deactivate();
	}
	reset();
	if (ClanBomberApplication::is_server()) {
	  ClanBomberApplication::get_server()->bomber_objects.push_back(this);
	  app->bomber_objects.push_back(this);
	}
	else if (!ClanBomberApplication::is_client()) {
		app->bomber_objects.push_back(this);
	}
}

void Bomber::reset()
{
	opacity_scaled = 255;

	if (orig_x > 300) {
		cur_dir = DIR_LEFT;
	}
	else {
		cur_dir = DIR_RIGHT;
	}
	sprite_nr = 0;

	can_pass_bomber = true;
	can_throw = true;

	if (disease) {
		disease->stop();
		delete disease;
		disease = NULL;
	}
	dead = false;
	falling = false;
	fallen_down = false;
	flying = false;
	extra_gloves = 0;
	extra_skateboards =0;
	extra_power = 0;
	extra_kick = 0;
	extra_bombs = 0;

	delete_me = false;
	server_dir = cur_dir;
	client_dir = cur_dir;
	local_dir = cur_dir; 
	server_x = (int)x;   
	server_y = (int)y;
	allow_putbomb_timer.reset();

	skateboards = Config::get_start_skateboards();
	speed = 90 + (skateboards*40);

	can_kick = Config::get_start_kick();
	cur_bombs = Config::get_start_bombs();
	bombs = Config::get_start_bombs();
	power = Config::get_start_power();
	gloves = Config::get_start_glove();
	
	x = orig_x;
	y = orig_y;
	anim_count = 0;
}

int Bomber::get_team() const
{
	return team;
}

int Bomber::get_number() const
{
	return number;
}

void Bomber::set_team(int _team)
{
	team = _team;
}

std::string Bomber::get_name() const
{
	return name;
}

bool Bomber::is_diseased()
{
	return (disease != NULL);
}

Disease* Bomber::get_disease()
{
	return disease;
}

Bomber::~Bomber()
{
	if (controller) {
		delete controller;
	}
}

void Bomber::show()
{
	if (disease && !dead && !falling) {
		GameObject::show((int)x+60,(int)y+40,sin( Timer::get_time() / 50.0f )  *  0.15f+1);
	}
	else if (!dead && cur_dir != DIR_NONE)
	{
		GameObject::show();
	}
}

Direction Bomber::get_server_send_dir()
{
    return server_send_dir;
}

bool Bomber::direction_has_changed(bool for_server)
{
    if (for_server) {
        if (Config::bomber[number].is_local()) {
            if (server_dir == DIR_NONE) {
                server_send_dir = DIR_NONE;
            }
            else {
                server_send_dir = cur_dir;
            }
            return (server_dir != client_dir);
        }
        else {
            if (server_dir == local_dir) {
                local_dir = DIR_NONE;
                server_send_dir = server_dir;
                server_dir = cur_dir;
                return true;
            }
            else {
                return false;
            }
        }
    }
    else {
        return (server_dir != client_dir && client_dir != local_dir);
    }
}

int Bomber::get_skateboards()
{
    return skateboards;
}

void Bomber::set_skateboards(int nr_skates)
{
    skateboards = nr_skates;
}

bool Bomber::allow_putbomb()
{
    if (allow_putbomb_timer.elapsed() > 177) {
        allow_putbomb_timer.reset();
        return true;
    }
    return false;
}

void Bomber::act_net()
{
    if (dead) {
        return;
    }
    bool server_acts = ClanBomberApplication::is_server();
    bool client_acts = ClanBomberApplication::is_client();
    bool local_bomber = Config::bomber[number].is_local();
    if (local_bomber) {
        controller->update();
    }
    if (disease && server_acts) {
        infect_others();
        disease->act();
        if (disease->get_countdown() == -1) {
            delete disease;
            disease=NULL;
            ClanBomberApplication::get_server()->send_SERVER_LOOSE_DISEASE(object_id);
        }
    }
    GameObject::act();
    bool moved = false;
    if (server_acts && fallen_down) {
        die();
        return;
    }
    if (local_bomber) {
        if (controller->is_left()) {
            if (server_acts) {
                client_dir = server_dir;
                server_dir = DIR_LEFT;
                cur_dir = DIR_LEFT;
                moved = true;
            }
            else if (client_acts) {
                client_dir = DIR_LEFT;
                if(server_dir != DIR_NONE) {
                    moved = true;
                }
            }
        }
        else if (controller->is_up()) {
            if (server_acts) {
                client_dir = server_dir;
                server_dir = DIR_UP;
                cur_dir = DIR_UP;
                moved = true;
            }
            else if (client_acts) {
                client_dir = DIR_UP;
                if(server_dir != DIR_NONE) {
                    moved = true;
                }
            }
        }
        else if (controller->is_right()) {
            if (server_acts) { 
                client_dir = server_dir;
                server_dir = DIR_RIGHT;
                cur_dir = DIR_RIGHT;
                moved = true;
            }
            else if (client_acts) {
                client_dir = DIR_RIGHT;
                if(server_dir != DIR_NONE) {
                    moved = true;
                }
            }
        }
        else if (controller->is_down()) {
            if (server_acts) {
                client_dir = server_dir;
                server_dir = DIR_DOWN;
                cur_dir = DIR_DOWN;
                moved = true;
            }
            else if (client_acts) {
                client_dir = DIR_DOWN;
                if(server_dir != DIR_NONE) {
                    moved = true;
                }
            }
        }
        else {
            moved = false;
            anim_count = 0;
            if (server_acts) {
                client_dir = server_dir;
                server_dir = DIR_NONE;
            } 
            else if (client_acts) {
                client_dir = DIR_NONE;
                if(server_dir != DIR_NONE) {
                    moved = true;
                }
            }
        }
    }
    else {
        moved = true;
        if (server_acts) {
            if (server_send_dir == DIR_NONE) {
                moved = false;
                anim_count = 0;
            }
        }
        else if (client_acts) {
            client_dir = server_dir;
            if(client_dir == DIR_NONE) {
                moved = false;
                anim_count = 0;
            }
        }
    }
    if (moved) {
        anim_count += Timer::time_elapsed()*20*(speed/90);
        move();
    }
    if (anim_count >= 9) {
        anim_count = 1;
    }
    sprite_nr = cur_dir*10+(int)anim_count;
    if (local_bomber && controller->is_bomb() && !flying) {
        if (get_maptile()->bomb) {
            if (gloves && !moved) {
                if (server_acts) {
                    get_maptile()->bomb->throww(cur_dir);
                    anim_count = 2;
                }
                else if (client_acts) {
                    if (controller->get_type() == Controller::AI ||
                        controller->get_type() == Controller::AI_mass) {
                        if (allow_putbomb()) {
                            ClanBomberApplication::get_client()->send_CLIENT_BOMBER_BOMB(power, object_id);
                        }
                    }
                    else {
                        ClanBomberApplication::get_client()->send_CLIENT_BOMBER_BOMB(power, object_id);
                    }
                }
            }
        }
        else {
            if (server_acts) {
                if (controller->get_type() == Controller::AI ||
                    controller->get_type() == Controller::AI_mass) {
                    if (allow_putbomb()) {
                        put_bomb();
                    }
                }
                else {
                    put_bomb();
                }
            }
            else if (client_acts) {
                if (controller->get_type() == Controller::AI ||
                    controller->get_type() == Controller::AI_mass) {
                   if (allow_putbomb()) {
                       ClanBomberApplication::get_client()->send_CLIENT_BOMBER_BOMB(power, object_id);
                   }
                }
                else {
                    ClanBomberApplication::get_client()->send_CLIENT_BOMBER_BOMB(power, object_id);
                }
            }
        }
    }
    if (!falling) {
        z = Z_BOMBER+get_y();
    }
}

void Bomber::act()
{
 	if (dead) {
 	 	return;
 	}
	controller->update();
	if (disease) {
 	 	infect_others();
	 	disease->act();
       	if (disease->get_countdown() == -1) {
       	 	delete disease;
 	 	 	disease = NULL;
		}
 	}
 	 
	GameObject::act();
    bool moved = false;

	if (fallen_down) {
	 	die();
	 	return;
	}

	if (controller->is_left()) {
	 	moved = true;
	 	cur_dir = DIR_LEFT;
	}
	else if (controller->is_up()) {
 	 	moved = true;
	 	cur_dir = DIR_UP;
	}
	else if (controller->is_right()) {
       	moved = true;
       	cur_dir = DIR_RIGHT;
 	}
	else if (controller->is_down()) {
 	 	moved = true;
	 	cur_dir = DIR_DOWN;
	}
	else {
 	 	anim_count = 0;
	}
 	if (moved) {
	 	anim_count += Timer::time_elapsed()*20*(speed/90);
       	move();
	}

	if (anim_count >= 9) {
		anim_count = 1;
	}

	sprite_nr = cur_dir*10 + (int)anim_count;
	        
	if ( controller->is_bomb() ) {
	 	if (get_maptile()->bomb) {
	 	 	if (gloves && !moved) {
	 	 	 	get_maptile()->bomb->throww(cur_dir);
	 	 	 	anim_count = 2;
	 	 	}
	 	} else {
 	        put_bomb();
	 	}
	}
	 
	if (!falling) {
	 	z = Z_BOMBER + get_y();
	}
}

void Bomber::infect_others()
{
  //CL_Iterator<GameObject> object_counter(get_maptile()->objects);
  std::list<GameObject*>::iterator object_iter;
  
  for (object_iter = get_maptile()->objects.begin();
       object_iter != get_maptile()->objects.end();
       object_iter++) {
    if ((*object_iter)->get_type() == GameObject::BOMBER && !(static_cast<Bomber*>(*object_iter)->is_diseased())) {
      static_cast<Bomber*>(*object_iter)->infect(disease->spawn(static_cast<Bomber*>(*object_iter)));
    }
  }
}

void Bomber::put_bomb()
{
	if (cur_bombs && !falling) {
		if (app->map->add_bomb( (int)((x+20)/40), (int)((y+20)/40), power, this, (int)x, (int)y )) {
                  Resources::Game_putbomb()->play();
		}
	}	
}

void Bomber::put_bomb(int mapx, int mapy)
{
	if (cur_bombs && !falling) {
       	if (app->map->add_bomb( mapx, mapy, power, this )) {
          Resources::Game_putbomb()->play();
        }
 	}
}

void Bomber::gain_extra_skateboard()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		if (Config::get_max_skateboards() > skateboards) {
			skateboards++;
			extra_skateboards++;
			speed += 40;
			if (ClanBomberApplication::is_server()) {
				ClanBomberApplication::get_server()->send_SERVER_GAIN_EXTRA(object_id, Extra::SKATEBOARD, (int)x);
			}
		}
	}
}


void Bomber::gain_extra_bomb()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		if (Config::get_max_bombs() > bombs) {
			bombs++;
			extra_bombs++;
			cur_bombs++;
			if (ClanBomberApplication::is_server()) {
				ClanBomberApplication::get_server()->send_SERVER_GAIN_EXTRA(object_id, Extra::BOMB, (int)x);
			}
		}
	}
}

void Bomber::gain_extra_glove()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		gloves++;
		extra_gloves++;
		if (ClanBomberApplication::is_server()) {
			ClanBomberApplication::get_server()->send_SERVER_GAIN_EXTRA(object_id, Extra::GLOVE, (int)x);
		}
	}
}

Bomber::COLOR Bomber::get_color() const
{
	return color;
}

void Bomber::gain_extra_power()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		if (Config::get_max_power() > power) {
			power++;
			extra_power++;
			if (ClanBomberApplication::is_server()) {
				ClanBomberApplication::get_server()->send_SERVER_GAIN_EXTRA(object_id, Extra::POWER, (int)x);
			}
		}
	}
}

void Bomber::loose_all_extras()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		int i;
		Extra* extra;
			
		for (i=0;i<extra_bombs;i++) {
			extra = new Extra_Bomb((int)x,(int)y,app);
			extra->fly_to(app->map->get_passable());
		}
		extra_bombs = 0;
		
		for (i=0;i<extra_power;i++) {
			extra = new Extra_Power((int)x,(int)y,app);
			extra->fly_to(app->map->get_passable());
		}
		extra_power = 0;
	
		for (i=0;i<extra_skateboards;i++) {		
			extra = new Extra_Skateboard((int)x,(int)y,app);
		     extra->fly_to(app->map->get_passable());
		}
		extra_skateboards = 0;
	
		for (i=0;i<extra_kick;i++) {		
			extra = new Extra_Kick((int)x,(int)y,app);
			extra->fly_to(app->map->get_passable());
		}
		extra_kick = 0;
	
		for (i=0;i<extra_gloves;i++) {				
			extra = new Extra_Glove((int)x,(int)y,app);
			extra->fly_to(app->map->get_passable());				
		}
		extra_gloves = 0;
	}
}

void Bomber::delete_disease()
{
	if (disease) {
		disease->stop();
		delete disease;
		disease = NULL;
	}
}

void Bomber::loose_disease()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		Extra* extra;
		if (disease) {
	
			disease->stop();
			if (ClanBomberApplication::is_server()) {
				ClanBomberApplication::get_server()->send_SERVER_LOOSE_DISEASE(object_id);
			}
			extra = disease->spawn_extra((int)x,(int)y);
			extra->fly_to(app->map->get_passable());
	
			delete disease;
			disease = NULL;		
		}
	}
}

void Bomber::set_disease(Disease* _disease, bool play_sound)
{
	delete_disease();
	disease = _disease;
	if (play_sound) {
		if (disease->get_DiseaseType()==Disease::FAST) {
                  Resources::Extras_schnief()->play();
		}
		else if (disease->get_DiseaseType()==Disease::STONED) {
                  Resources::Extras_joint()->play();
		}
		else if (disease->get_DiseaseType()==Disease::PUTBOMB) {
                  Resources::Extras_horny()->play();
		}
	}
}

void Bomber::infect(Disease* _disease)
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		if (disease) {
			disease->stop();
			delete disease;
		}
		disease = _disease;
		if (ClanBomberApplication::is_server()) {
			ClanBomberApplication::get_server()->send_SERVER_INFECT_WITH_DISEASE(object_id, _disease->get_DiseaseType());
		}
	}
}


void Bomber::gain_extra_kick()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		can_kick = true;
		extra_kick++;
		if (ClanBomberApplication::is_server()) {
			ClanBomberApplication::get_server()->send_SERVER_GAIN_EXTRA(object_id, Extra::KICK, (int)x);
		}
	}
}

void Bomber::inc_cur_bombs()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		cur_bombs++;
	}
}

void Bomber::dec_cur_bombs()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		if (cur_bombs) {
			cur_bombs--;
		}
	}
}

GameObject::ObjectType Bomber::get_type() const
{
	return BOMBER;
}

bool Bomber::die()
{
	if (dead || is_falling()) {
	 	return false;
	}
	dead = true;
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
  		deaths++;
		if (ClanBomberApplication::is_server()) {
			ClanBomberApplication::get_server()->send_SERVER_BOMBER_DIE(object_id, cur_dir*10+9);
			ClanBomberApplication::get_server()->send_SERVER_UPDATE_BOMBER_SKILLS(this);
		}
		Resources::Game_die()->play();
		if (!fallen_down) {
			loose_all_extras();
			if (!Config::get_kids_mode()) {
				Bomber_Corpse* tmp = new Bomber_Corpse((int)x, (int)y, color, cur_dir*10+9, app);
				if (ClanBomberApplication::is_server()) {
					ClanBomberApplication::get_server()->send_SERVER_ADD_BOMBER_CORPSE(tmp->get_object_id(), (int)x, (int)y, color, cur_dir*10+9);
				}
			}
		}
		x = 0;
		y = 0;
		sprite_nr = cur_dir*10+9;
	}
	return true;
}

void Bomber::inc_kills()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		kills++;
	}
}

int Bomber::get_cur_bombs() const
{
	return cur_bombs;
}

int Bomber::get_power() const
{
	return power;
}

int Bomber::get_bombs() const
{
	return bombs;
}

int Bomber::get_kills() const
{
	return kills;
}

int Bomber::get_points() const
{
	return points;
}

void Bomber::set_points( int _points )
{
	points = _points;
}

void Bomber::inc_points()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		points++;
	}
}

void Bomber::dec_points()
{
	if (ClanBomberApplication::is_server() || !ClanBomberApplication::is_client()) {
		points--;
	}
}

bool Bomber::is_dead() const
{
	return dead;
}

int Bomber::get_deaths() const
{
	return deaths;
}

int Bomber::get_gloves()
{
 	return gloves;
}

int Bomber::get_extra_gloves()
{
	return extra_gloves;
}

int Bomber::get_extra_skateboards()
{
	return extra_skateboards;
}

int Bomber::get_extra_power()
{
	return extra_power;
}

int Bomber::get_extra_kick()
{
	return extra_kick;
}

int Bomber::get_extra_bombs()
{
	return extra_bombs;
}

void Bomber::set_deaths(int d)
{
	deaths = d;
}

void Bomber::set_kills(int k)
{
	kills = k;
}

void Bomber::set_cur_bombs(int cb)
{
	cur_bombs = cb;
}

void Bomber::set_bombs(int b)
{
	bombs = b;
}

void Bomber::set_extra_bombs(int eb)
{
	extra_bombs = eb;
}

void Bomber::set_power(int p)
{
	power = p;
}

void Bomber::set_extra_power(int ep)
{
	extra_power = ep;
}

void Bomber::set_extra_skateboards(int es)
{
	extra_skateboards = es;
}

void Bomber::set_is_able_to_kick(bool k)
{
	can_kick = k;
}

void Bomber::set_extra_kick(int ek)
{
	extra_kick = ek;
}

void Bomber::set_is_able_to_throw(bool t)
{
	can_throw = t;
}

void Bomber::set_gloves(int g)
{
	gloves = g;
}

void Bomber::set_extra_gloves(int eg)
{
	extra_gloves = eg;
}

void Bomber::set_dead()
{
	dead = true;
}

void Bomber::set_sprite_nr(int snr)
{
 	sprite_nr = snr;
}

void Bomber::set_anim_count(float animcnt)
{
	anim_count = animcnt;
}

void Bomber::set_disconnected()
{
	has_disconnected = true;
}

bool Bomber::is_disconnected()
{
	return has_disconnected;
}
