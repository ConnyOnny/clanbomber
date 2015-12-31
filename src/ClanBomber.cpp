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

#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <ctime>

//#define BOOST_FILESYSTEM_NO_DEPRECATED
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include "SDL.h"

#include "config.h"
#include "Controller.h"
#include "Observer.h"
#include "GameConfig.h"
#include "Menu.h"
#include "Timer.h"
#include "PlayerSetup.h"
#include "ServerSetup.h"
#include "Server.h"
#include "Chat.h"
#include "ClientSetup.h"
#include "Client.h"
#include "Map.h"
#include "Credits.h"
#include "MapEditor.h"
#include "MapTile.h"
#include "Bomber.h"
#include "MapSelector.h"
#include "Bomb.h"
#include "Utils.h"
#include "Mutex.h"
#include "Event.h"
#include "UtilsSleep.h"
#include "UtilsGetHome.h"
#include "cbe/PluginManager.h"

#ifdef ENABLE_CONSOLE
boost::thread start_console();
#endif

ClanBomberApplication *app;

SDL_Surface *primary   = NULL;
Uint8       *keyboard  = NULL;

boost::filesystem::path ClanBomberApplication::map_path;
boost::filesystem::path ClanBomberApplication::local_map_path;

int ClanBomberApplication::server_frame_counter = 0;

static unsigned short next_object_id  = 0;
static int run_server_with_players_nr = 0;

bool game_object_compare(GameObject* go1, GameObject* go2)
{
  return go1->get_z() < go2->get_z();
}

ClanBomberApplication::ClanBomberApplication()
{
  cb_server = NULL;
  cb_client = NULL;
  server_status = 0;
  observer = NULL;
  map = NULL;
  cb_mutex = new Mutex();
  cb_event = new Event();
  client_setup_menu = NULL;
  server_setup_menu = NULL;
  pause_game = false;
  client_disconnected_from_server = false;
  client_connecting_to_new_server = false;
  bombers_received_by_client = false;
}

ClanBomberApplication::~ClanBomberApplication()
{
  std::cout << _("(+) clanbomber: deallocating resources...") << std::endl;

  AS->close();
  TTF_Quit();
  SDL_Quit();

  if (map) {
    delete map;
    map = NULL;
  }

  delete cb_event;
  cb_event = NULL;
  delete cb_mutex;
  cb_mutex = NULL;

  if (cb_server) {
    delete cb_server;
    cb_server = NULL;
  }

  if (server_setup_menu) {
    delete server_setup_menu;
    server_setup_menu = NULL;
  }

  if (cb_client) {
    delete cb_client;
    cb_client = NULL;
  }

  if (client_setup_menu) {
    delete client_setup_menu;
    client_setup_menu = NULL;
  }
}

int ClanBomberApplication::init_SDL()
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK)) {
    std::cerr << _("Cannot Initialise SDL!") << std::endl;
    SDL_Quit();
    return -1;
  }

  keyboard = SDL_GetKeyState(NULL);

  Uint32 fullscreen = 0;
  if (Config::get_fullscreen()) {
    fullscreen = SDL_FULLSCREEN;
  }
  primary = SDL_SetVideoMode(800, 600, 16, SDL_SWSURFACE | fullscreen);
  //primary = SDL_SetVideoMode(1024, 768, 32, SDL_HWSURFACE | SDL_FULLSCREEN);

  SDL_WM_SetCaption(PACKAGE_STRING, NULL);

  PM = new cbe::PluginManager();
  PM->search();
  AS = PM->createAudioSimple("SDL");
  if (AS == NULL) {
    std::cerr << _("Cannot initialise the requested audio plugin!")
              << std::endl;
  }
  if (!AS->init()) {
    std::cerr << _("Cannot Initialise SDL audio!") << std::endl;
  }

  if (TTF_Init()) {
    std::cerr << _("Cannot Initialise SDL ttf!") << std::endl;
    TTF_Quit();
    AS->close();
    SDL_Quit();
    return -1;
  }

  return 0;
}

void ClanBomberApplication::start_net_game()
{
  if (is_server()) {
    cb_server->send_SERVER_FULL_BOMBER_CONFIG();
    cb_server->send_SERVER_CONFIG();
  }
  init_net_game();
  run_game();
  deinit_game();
}

bool ClanBomberApplication::paused_game()
{
  return pause_game;
}

void ClanBomberApplication::set_pause_game(bool paused)
{
  pause_game = paused;
}

void ClanBomberApplication::reload_map()
{
  if (map) {
    delete map;
  }
  map = new Map(this);
}

unsigned short ClanBomberApplication::get_next_object_id()
{
  if (next_object_id++ > 65534) {
    next_object_id = 23;
  }
  return next_object_id;
}

bool ClanBomberApplication::is_client_disconnected_from_server()
{
  return client_disconnected_from_server;
}

void ClanBomberApplication::set_client_disconnected_from_server(bool d)
{
  client_disconnected_from_server = d;
}

void ClanBomberApplication::set_client_connecting_to_new_server(bool c)
{
  client_connecting_to_new_server = c;
}

bool ClanBomberApplication::is_client_connecting_to_new_server()
{
  return client_connecting_to_new_server;
}

void ClanBomberApplication::lock()
{
  app->cb_mutex->lock();
}

void ClanBomberApplication::unlock()
{
  app->cb_mutex->unlock();
}

void ClanBomberApplication::wait()
{
  app->cb_event->lock();
  app->cb_event->wait();
  app->cb_event->unlock();
}

void ClanBomberApplication::signal()
{
  app->cb_event->lock();
  app->cb_event->signal();
  app->cb_event->unlock();
}

int ClanBomberApplication::main()
{
  std::srand(std::time(NULL));

  if (init_SDL()) {
    return -1;
  }

  show_fps = false;
  key_F1 = false;

  map_path = CB_DATADIR"/maps";

  boost::filesystem::path configpath(GetConfigHome() / "clanbomber");
  if (!RecursiveDirCreation(configpath)) {
    boost::format fmt(_("Config directory (%1$s) cannot be created"));
    std::cout << fmt % configpath << std::endl;
  }

  Config::set_path(configpath);
  Config::set_filename("clanbomber.cfg");

  boost::filesystem::path mappath(GetDataHome() / "clanbomber" / "maps");
  if (!RecursiveDirCreation(mappath)) {
    boost::format fmt(_("Data directory (%1$s) cannot be created"));
    std::cout << fmt % mappath << std::endl;
  }
  local_map_path = mappath;

  Resources::init();
  Config::load();
  run_intro();

  CB_FillScreen(0, 0, 0);
  Resources::Font_big()->render(_("Loading..."), 400, 300,
                                cbe::FontAlignment_0center);
  //SDL_Flip(primary);
  CB_Flip();

  map = NULL;
  observer = NULL;
  chat = new Chat(this);

  menu = new Menu(_("Main Menu"), this);

  menu->add_item(_("New Game"), MENU_GAME);
  menu->add_item(_("Start Local"), LOCALGAME_START, MENU_GAME);
  menu->add_item(_("Create Server"), SERVERGAME_START, MENU_GAME);
  menu->add_item(_("Join Server"), CLIENTGAME_START, MENU_GAME);
  menu->add_item(_("Local Player Setup"), MENU_PLAYER_SETUP, MENU_GAME);
  menu->add_item(_("Map Selection"), CONFIG_MAP_SEL, MENU_GAME);
  menu->add_value(_("Random Bomber Positions"), CONFIG_RANDOM_POSITIONS,
                  MENU_GAME, 0, 1, Config::get_random_positions());
  menu->add_value(_("Random Map Order"), CONFIG_RANDOM_MAP_ORDER, MENU_GAME, 0,
                  1, Config::get_random_map_order());
  menu->add_value(_("Points to win"), CONFIG_POINTS, MENU_GAME, 1, 10,
                  Config::get_points_to_win());
  menu->add_value(_("Round Time"), CONFIG_ROUND_TIME, MENU_GAME, 5, 300,
                  Config::get_round_time());

  menu->add_item(_("Options"), MENU_OPTIONS);
  menu->add_item(_("Start/Max Extras"), MENU_EXTRA_VALUES, MENU_OPTIONS);
  menu->add_value(_("Start Bombs"), CONFIG_START_BOMBS, MENU_EXTRA_VALUES, 1,
                  15, Config::get_start_bombs());
  menu->add_value(_("Start Power"), CONFIG_START_POWER, MENU_EXTRA_VALUES, 1,
                  15, Config::get_start_power());
  menu->add_value(_("Start Skateboards"), CONFIG_START_SKATES,
                  MENU_EXTRA_VALUES, 0, 10, Config::get_start_skateboards());
  menu->add_value(_("Start Kick"), CONFIG_START_KICK, MENU_EXTRA_VALUES, 0, 1,
                  Config::get_start_kick());
  menu->add_value(_("Start Glove"), CONFIG_START_GLOVE, MENU_EXTRA_VALUES, 0, 1,
                  Config::get_start_glove());
  menu->add_value(_("Max. Bombs"), CONFIG_MAX_BOMBS, MENU_EXTRA_VALUES, 1, 15,
                  Config::get_max_bombs());
  menu->add_value(_("Max. Power"), CONFIG_MAX_POWER, MENU_EXTRA_VALUES, 1, 15,
                  Config::get_max_power());
  menu->add_value(_("Max. Skateboards"), CONFIG_MAX_SKATES, MENU_EXTRA_VALUES,
                  0, 10, Config::get_max_skateboards());
  menu->add_item(_("Enable/Disable Extras"), MENU_EXTRA_ONOFF, MENU_OPTIONS);
  menu->add_value(_("Bombs"), CONFIG_BOMBS, MENU_EXTRA_ONOFF, 0, 1,
                  Config::get_bombs());
  menu->add_value(_("Power"), CONFIG_POWER, MENU_EXTRA_ONOFF, 0, 1,
                  Config::get_power());
  menu->add_value(_("Skateboard"), CONFIG_SKATES, MENU_EXTRA_ONOFF, 0, 1,
                  Config::get_skateboards());
  menu->add_value(_("Kick"), CONFIG_KICK, MENU_EXTRA_ONOFF, 0, 1,
                  Config::get_kick());
  menu->add_value(_("Glove"), CONFIG_GLOVE, MENU_EXTRA_ONOFF, 0, 1,
                  Config::get_glove());
  menu->add_item(_("Enable/Disable Diseases"), MENU_DISEASE_ONOFF,
                 MENU_OPTIONS);
  menu->add_value(_("Joint"), CONFIG_JOINT, MENU_DISEASE_ONOFF, 0, 1,
                  Config::get_joint());
  menu->add_value(_("Viagra"), CONFIG_VIAGRA, MENU_DISEASE_ONOFF, 0, 1,
                  Config::get_viagra());
  menu->add_value(_("Koks"), CONFIG_KOKS, MENU_DISEASE_ONOFF, 0, 1,
                  Config::get_koks());
  menu->add_item(_("Bomb Timing and Speed"), MENU_TIMING, MENU_OPTIONS);
  menu->add_value(_("Bomb Countdown (1/10 s)"), CONFIG_BOMB_COUNTDOWN,
                  MENU_TIMING, 0, 50, Config::get_bomb_countdown());
  menu->add_value(_("Bomb Chain Reaction Delay (1/100 s)"), CONFIG_BOMB_DELAY,
                  MENU_TIMING, 0, 50, Config::get_bomb_delay());
  menu->add_value(_("Moving Bombs Speed (pixels per second)"),
                  CONFIG_BOMB_SPEED, MENU_TIMING, 10, 500,
                  Config::get_bomb_speed());
  menu->add_item(_("Graphic Options"), MENU_GRAPHICS, MENU_OPTIONS);
  menu->add_value(_("Kidz Mode"), CONFIG_KIDS_MODE, MENU_GRAPHICS, 0, 1,
                  Config::get_kids_mode());
  menu->add_value(_("Corpse Parts"), CONFIG_CORPSE_PARTS, MENU_GRAPHICS, 0, 100,
                  Config::get_corpse_parts());
  menu->add_value(_("Shaky Explosions"), CONFIG_SHAKE, MENU_GRAPHICS, 0, 1,
                  Config::get_shaky_explosions());
  menu->add_value(_("Random Bomber Positions"),
                  CONFIG_RANDOM_POSITIONS_DUPLICATE, MENU_OPTIONS, 0, 1,
                  Config::get_random_positions());
  menu->add_value(_("Random Map Order"), CONFIG_RANDOM_MAP_ORDER_DUPLICATE,
                  MENU_OPTIONS, 0, 1, Config::get_random_map_order());
  menu->add_value(_("Points to win"), CONFIG_POINTS_DUPLICATE, MENU_OPTIONS, 1,
                  10, Config::get_points_to_win());
  menu->add_value(_("Round Time"), CONFIG_ROUND_TIME_DUPLICATE, MENU_OPTIONS, 5,
                  300, Config::get_round_time());

  menu->backup_options_values();
  menu->save_common_options(0, false, true);

  menu->add_item(_("Map Editor"), MENU_MAP_EDITOR);
  menu->add_item(_("Show Credits"), MENU_CREDITS);
  menu->add_item(_("Help Screen"), MENU_HELP);
  menu->add_item(_("Quit Game"), MENU_EXIT);

  menu->scroll_in();

  bool already_started_auto_server = false;
  while (1) {
    int result;
    if (is_client_disconnected_from_server()) {
      menu->go_to_game_menu(true);
      result = SERVERGAME_START;
    }
    else if (is_client_connecting_to_new_server()) {
      result = CLIENTGAME_START;
    }
    else if (ClanBomberApplication::run_server_with_players()
             && !already_started_auto_server) {
      menu->go_to_game_menu(false);
      result = SERVERGAME_START;
      already_started_auto_server = true;
    }
    else {
      result = menu->execute(false);
    }

    MenuItem* item = menu->get_item_by_id(result);
    switch (result) {
    case MENU_EXIT:
      menu->scroll_out();
      delete menu;
      return 0;
    case MENU_PLAYER_SETUP:
      menu->scroll_out();
      {
        PlayerSetup ps(this);
        ps.exec();
      }
      menu->scroll_in();
      break;
    case CONFIG_START_BOMBS:
      Config::set_start_bombs(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_START_POWER:
      Config::set_start_power(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_START_SKATES:
      Config::set_start_skateboards(static_cast<MenuItem_Value*>(item)
                                    ->get_value());
      Config::save();
      break;
    case CONFIG_START_KICK:
      Config::set_start_kick(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_START_GLOVE:
      Config::set_start_glove(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_BOMBS:
      Config::set_bombs(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_POWER:
      Config::set_power(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_SKATES:
      Config::set_skateboards(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_KICK:
      Config::set_kick(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_GLOVE:
      Config::set_glove(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_JOINT:
      Config::set_joint(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_VIAGRA:
      Config::set_viagra(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_KOKS:
      Config::set_koks(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_MAX_BOMBS:
      Config::set_max_bombs(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_MAX_POWER:
      Config::set_max_power(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_MAX_SKATES:
      Config::set_max_skateboards(static_cast<MenuItem_Value*>(item)
                                  ->get_value());
      Config::save();
      break;
    case MENU_CREDITS:
      menu->scroll_out();
      {
        Credits credits(this);
        credits.exec();
      }
      menu->scroll_in();
      break;
    case MENU_MAP_EDITOR:
      menu->scroll_out();
      {
        MapEditor me(this);
        me.exec();
      }
      menu->scroll_in();
      break;
    case MENU_HELP:
      menu->scroll_out();
      show_tutorial();
      menu->scroll_in();
      break;
    case CONFIG_MAP_SEL:
      menu->scroll_out();
      {
        MapSelector ms(this);
        ms.exec();
      }
      menu->scroll_in();
      break;
    case CONFIG_POINTS:
      Config::set_points_to_win(static_cast<MenuItem_Value*>(item)
                                ->get_value());
      Config::save();
      break;
    case CONFIG_POINTS_DUPLICATE:
      Config::set_points_to_win(static_cast<MenuItem_Value*>(item)
                                ->get_value());
      Config::save();
      break;
    case CONFIG_ROUND_TIME:
      if (static_cast<MenuItem_Value*>(item)->get_value()%5 == 1) {
        static_cast<MenuItem_Value*>(item)
          ->set_value(static_cast<MenuItem_Value*>(item)->get_value() + 4);
      }
      else if (static_cast<MenuItem_Value*>(item)->get_value()%5 == 4) {
        static_cast<MenuItem_Value*>(item)
          ->set_value(static_cast<MenuItem_Value*>(item)->get_value() - 4);
      }
      Config::set_round_time(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_ROUND_TIME_DUPLICATE:
      if (static_cast<MenuItem_Value*>(item)->get_value()%5 == 1) {
        static_cast<MenuItem_Value*>(item)
          ->set_value(static_cast<MenuItem_Value*>(item)->get_value() + 4);
      }
      else if (static_cast<MenuItem_Value*>(item)->get_value()%5 == 4) {
        static_cast<MenuItem_Value*>(item)
          ->set_value(static_cast<MenuItem_Value*>(item)->get_value() - 4);
      }
      Config::set_round_time(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_KIDS_MODE:
      Config::set_kids_mode(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_CORPSE_PARTS:
      Config::set_corpse_parts(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_SHAKE:
      Config::set_shaky_explosions(static_cast<MenuItem_Value*>(item)
                                   ->get_value());
      Config::save();
      break;
    case CONFIG_BOMB_COUNTDOWN:
      Config::set_bomb_countdown(static_cast<MenuItem_Value*>(item)
                                 ->get_value());
      Config::save();
      break;
    case CONFIG_BOMB_DELAY:
      Config::set_bomb_delay(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_BOMB_SPEED:
      if (static_cast<MenuItem_Value*>(item)->get_value()%10 == 1) {
        static_cast<MenuItem_Value*>(item)
          ->set_value(static_cast<MenuItem_Value*>(item)->get_value() + 9);
      }
      else if (static_cast<MenuItem_Value*>(item)->get_value()%10 == 9) {
        static_cast<MenuItem_Value*>(item)
          ->set_value(static_cast<MenuItem_Value*>(item)->get_value() - 9);
      }
      Config::set_bomb_speed(static_cast<MenuItem_Value*>(item)->get_value());
      Config::save();
      break;
    case CONFIG_RANDOM_POSITIONS:
      Config::set_random_positions(static_cast<MenuItem_Value*>(item)
                                   ->get_value());
      Config::save();
      break;
    case CONFIG_RANDOM_POSITIONS_DUPLICATE:
      Config::set_random_positions(static_cast<MenuItem_Value*>(item)
                                   ->get_value());
      Config::save();
      break;
    case CONFIG_RANDOM_MAP_ORDER:
      Config::set_random_map_order(static_cast<MenuItem_Value*>(item)
                                   ->get_value());
      Config::save();
      break;
    case CONFIG_RANDOM_MAP_ORDER_DUPLICATE:
      Config::set_random_map_order(static_cast<MenuItem_Value*>(item)
                                   ->get_value());
      Config::save();
      break;
    case LOCALGAME_START:
      menu->scroll_out();
      if (Config::get_number_of_opponents() > 1) {
        if (init_game()) {
          run_game();
          deinit_game();
        }
      }
      else {
        menu->scroll_in();
      }
      break;
    case SERVERGAME_START:
      menu->scroll_out();
      Config::save();
      Config::set_filename("clanbomber_net.cfg");
      Config::load();
      {
        if (!init_server_game()) {
          break;
        }
      }
      Config::save();
      Config::set_filename("clanbomber.cfg");
      Config::load();
      menu->scroll_in();
      menu->set_left_netgame_setup();
      break;
    case CLIENTGAME_START:
      Config::save();
      Config::set_filename("clanbomber_net.cfg");
      Config::load();
      menu->scroll_out();
      {
        if (!init_client_game()) {
          break;
        }
      }
      Config::save();
      Config::set_filename("clanbomber.cfg");
      Config::load();
      menu->scroll_in();
      menu->set_left_netgame_setup();
      break;
    default:
      std::cout << result << std::endl;
      break;
    }
  }
}

bool ClanBomberApplication::init_server_game()
{
  make_map();
  if (!map->any_valid_map()) {
    delete map;
    return false;
  }

  int previous_client_index = -1;

  server_status = 1;

  for (int i = 0; i < 8; i++) {
    Config::bomber[i].disable();
  }

  Config::save();

  cb_server = new Server(this);
  if (cb_server->get_last_error() != NULL) {
    delete cb_server;
    cb_server = NULL;
    return false;
  }

  if (is_client_disconnected_from_server()) {
    previous_client_index = cb_client->get_my_index();
    cb_client->init_server_from_client_info();
    cb_server->send_SERVER_CONNECT_CLIENT(previous_client_index);
    delete cb_client;
    cb_client = NULL;
  }

  cb_client = new Client(this, "localhost");
  if (cb_client->get_last_error() != NULL) {
    delete cb_client;
    cb_client = NULL;
    return false;
  }

  Chat::set_client (cb_client);

  server_setup_menu = new ServerSetup(this, cb_server, cb_client);
  cb_server->start_receiving_from_clients();
  cb_server->start_updating_objects();
  cb_client->start_receiving_from_server();
  cb_client->start_updating_objects();

  if (is_client_disconnected_from_server()) {
    cb_client->send_old_bomber_config_to_new_server(previous_client_index);
    set_client_disconnected_from_server(false);
  }

  while (!server_setup_menu->end_setup()) {
    server_setup_menu->exec();
    Config::save();
    cb_server->stop_accepting_connections();
    if (cb_client->server_started_game()) {
      start_net_game();
    }
    cb_server->reset();
    cb_client->reset();
  }

  if (cb_client != NULL) {
    delete cb_client;
    cb_client = NULL;
  }

  if (cb_server != NULL) {
    delete cb_server;
    cb_server = NULL;
  }

  if (server_setup_menu != NULL) {
    delete server_setup_menu;
    server_setup_menu = NULL;
  }

  server_status = 0;
  return true;
}

bool ClanBomberApplication::init_client_game()
{
  server_status = 2;
  int previous_client_index = -1;
  std::string new_string(Config::get_last_server());
  if (is_client_connecting_to_new_server()) {
    previous_client_index = cb_client->get_previous_client_index();
    new_string = cb_client->get_new_server_name();
    cb_mutex->lock();
    cb_client->stop_updating_objects();
    cb_mutex->unlock();
    delete cb_client;
    cb_client = NULL;
    SimpleTimer ct;
    ct.reset();
    while (ct.elapsed() < 999) {
      CB_FillRect(200, 170, 400, 40, 130, 220, 128);
      Resources::Font_big()->render(_("enter server NAME or IP:"), 400, 150,
                                    cbe::FontAlignment_0center);
      Resources::Font_big()->render(new_string, 400, 200,
                                    cbe::FontAlignment_0center);
      CB_Flip();
    }
  }
  else {
    int enter_status = 0;
    while (1) {
      CB_FillRect(200, 170, 400, 40, 130, 220, 128);
      Resources::Font_big()->render(_("enter server NAME or IP:"), 400, 150,
                                    cbe::FontAlignment_0center);
      Resources::Font_big()->render(new_string, 400, 200,
                                    cbe::FontAlignment_0center);
      CB_Flip();
      enter_status = CB_EnterText(new_string);
      if (enter_status) {
	break;
      }
    }
    if (enter_status < 0) {
      return false;
    }
  }

  cb_client = new Client(this, new_string);
  if (cb_client->get_last_error() != NULL) {
    delete cb_client;
    cb_client = NULL;
    return false;
  }

  Chat::set_client(cb_client);

  Config::set_last_server(cb_client->get_server_name());
  client_setup_menu = new ClientSetup(this, cb_client);
  cb_client->start_receiving_from_server();
  cb_client->start_updating_objects();
  cb_client->send_CLIENT_HANDSHAKE();

  make_map();

  if (!client_setup_menu->is_disconnected()) {
    cb_client->send_CLIENT_MAP_CHECKSUMS();
  }

  if (is_client_connecting_to_new_server()) {
    cb_client->send_old_bomber_config_to_new_server(previous_client_index);
    set_client_connecting_to_new_server(false);
  }
  while (!client_setup_menu->end_setup()) {
    if (client_setup_menu->is_disconnected()
        || is_client_disconnected_from_server()) {
      break;
    }
    client_setup_menu->exec();
    Config::save();
    if (cb_client->server_started_game()) {
      start_net_game();
    }
    cb_client->reset();
  }
  if (cb_client != NULL) {
    if (!is_client_disconnected_from_server()
        && !is_client_connecting_to_new_server()) {
      delete cb_client;
      cb_client = NULL;
    }
  }
  if (client_setup_menu != NULL) {
    delete client_setup_menu;
    client_setup_menu = NULL;
  }
  server_status = 0;
  return true;
}

int ClanBomberApplication::get_server_frame_counter()
{
  return server_frame_counter;
}

void ClanBomberApplication::inc_server_frame_counter()
{
  server_frame_counter++;
}

void ClanBomberApplication::run_game()
{
  if (!is_server() && is_client()) {
    cb_event->lock();
    if (!map->is_received_by_client) {
      cb_event->wait();
    }
    cb_event->unlock();

    cb_event->lock();
    if (!bombers_received_by_client) {
      cb_event->wait();
    }
    bombers_received_by_client = false;
    observer->make_client_game_status();
    cb_event->unlock();
  }

  Timer::reset();

  bool escape = false;
  while (!escape) {
    if (is_server()) {
      cb_server->disconnect_dead_clients();
      cb_server->send_SERVER_KEEP_ALIVE();
      cb_server->update_traffic_statistics();
    }
    else if (is_client()) {
      cb_client->disconnect_from_server();
      cb_client->send_CLIENT_KEEP_ALIVE();
      cb_client->update_traffic_statistics();
    }

    Timer::time_elapsed(true);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
        case SDLK_p:
          if (is_server() || !is_client()) {
            pause_game = !pause_game;
            if (is_server()) {
              cb_server->send_SERVER_PAUSE_GAME(pause_game);
            }
          }
          break;
        case SDLK_F1:
          show_fps = !show_fps;
          break;
        case SDLK_t:
          //XXX What is the purpouse of this?
          if (is_client()) {
            for (std::list<Bomber*>::iterator bomber_object_iter
                   = bomber_objects.begin();
                 bomber_object_iter != bomber_objects.end();
                 bomber_object_iter++) {
              if (Config::bomber[(*bomber_object_iter)->get_number()].is_local()
                  && ((*bomber_object_iter)->controller->get_type()
                      == Controller::KEYMAP_1
                      || (*bomber_object_iter)->controller->get_type()
                      == Controller::KEYMAP_2
                      || (*bomber_object_iter)->controller->get_type()
                      == Controller::KEYMAP_3)) {
                (*bomber_object_iter)->controller->deactivate();
              }
            }
            for (std::list<Bomber*>::iterator bomber_object_iter
                   = bomber_objects.begin();
                 bomber_object_iter != bomber_objects.end();
                 bomber_object_iter++) {
              if (Config::bomber[(*bomber_object_iter)->get_number()].is_local()
                  && ((*bomber_object_iter)->controller->get_type()
                      == Controller::KEYMAP_1
                      || (*bomber_object_iter)->controller->get_type()
                      == Controller::KEYMAP_2
                      || (*bomber_object_iter)->controller->get_type()
                      == Controller::KEYMAP_3)) {
                (*bomber_object_iter)->controller->activate();
              }
            }
          }
          break;
        case SDLK_ESCAPE:
          if (is_server()) {
            if (!pause_game) {
              escape = true;
            }
          }
          else {
            escape = true;
            if (is_client()) {
              ClientSetup::end_session();
              cb_client->send_CLIENT_DISCONNECT();
            }
          }
        }
      }
    }

    if (pause_game) {
      if (is_server()) {
        cb_server->send_update_messages_to_clients(server_frame_counter);
      }
      show_all();
      CB_FillRect(0, 0, 800, 600, 0, 0, 0, 123);
      Resources::Font_big()->render(_("- PAUSED -"), 400, 300,
                                    cbe::FontAlignment_0topcenter);
      CB_Flip();
      continue;
    }

    if (observer != NULL) {
      observer->act();
      if (observer->end_of_game_requested()) {
        if (is_server()) {
          cb_server->send_update_messages_to_clients(server_frame_counter);
        }
        break;
      }
    }

    delete_some();
    act_all();
    show_all();

    if (is_server()) {
      cb_server->send_update_messages_to_clients(server_frame_counter);
    }
    else if (is_client() && cb_client->end_game()) {
      break;
    }

    CB_Flip();
    frame_count++;

    frame_time += Timer::time_elapsed();
    if (frame_time > 2) {
      fps = (int)(frame_count/frame_time + 0.5f);
      frame_time = 0;
      frame_count = 0;
    }

    server_frame_counter++;
    if (server_frame_counter > 777777777) {
      server_frame_counter = 0;
    }
  }

  if (is_server()) {
    cb_server->send_SERVER_END_OF_GAME();
    cb_server->send_update_messages_to_clients(server_frame_counter);
  }
}

int ClanBomberApplication::get_server_status()
{
  return server_status;
}

Server* ClanBomberApplication::get_server()
{
  return app->cb_server;

}

Client* ClanBomberApplication::get_client()
{
  return app->cb_client;
}

ServerSetup* ClanBomberApplication::get_server_setup()
{
  return app->server_setup_menu;
}

ClientSetup* ClanBomberApplication::get_client_setup()
{
  return app->client_setup_menu;
}

Chat* ClanBomberApplication::get_chat()
{
  return app->chat;
}

Menu* ClanBomberApplication::get_menu()
{
  return app->menu;
}

int ClanBomberApplication::run_server_with_players()
{
  return run_server_with_players_nr;
}

bool ClanBomberApplication::is_server()
{
  return (app->get_server_status() == 1 && app->get_server() != NULL
          && app->get_client() != NULL);
}

bool ClanBomberApplication::is_client()
{
  return ((app->get_server_status() == 1 || app->get_server_status() == 2)
          && app->get_client() != NULL);
}

void ClanBomberApplication::make_observer()
{
  if (observer == NULL) {
    observer = new Observer(0, 0, this);
  }
}

void ClanBomberApplication::make_map()
{
  if (map == NULL) {
    map = new Map(this);
  }
}

Observer* ClanBomberApplication::get_observer()
{
  return observer;
}

Map* ClanBomberApplication::get_map()
{
  return map;
}

void ClanBomberApplication::delete_all_game_objects()
{
  objects.clear();
  bomber_objects.clear();
}

GameObject* ClanBomberApplication::get_object_by_id(int object_id)
{
  for (std::list<Bomber*>::iterator bomber_object_iter = bomber_objects.begin();
       bomber_object_iter != bomber_objects.end();
       bomber_object_iter++) {
    if ((*bomber_object_iter)->get_object_id() == object_id) {
      return *bomber_object_iter;
    }
  }
  for (std::list<GameObject*>::iterator game_object_iter = objects.begin();
       game_object_iter != objects.end();
       game_object_iter++) {
    if ((*game_object_iter)->get_object_id() == object_id) {
      return *game_object_iter;
    }
  }
  return NULL;
}

Bomb* ClanBomberApplication::activate_suspended_client_bomb_by_id(int object_id)
{
  for (std::list<Bomb*>::iterator bomb_object_iter
         = suspended_client_bombs.begin();
       bomb_object_iter != suspended_client_bombs.end();
       bomb_object_iter++) {
    if ((*bomb_object_iter)->get_object_id() == object_id) {
      Bomb* b = (*bomb_object_iter);
      objects.push_back(b);
      suspended_client_bombs.erase(bomb_object_iter);
      return b;
    }
  }
  return NULL;
}

void ClanBomberApplication::act_all()
{
  // Map acts first
  cb_mutex->lock();
  if (map != NULL) {
    map->act();
  }
  // Let them do their stuff
  for (std::list<GameObject*>::iterator object_iter = objects.begin();
       object_iter != objects.end();
       object_iter++) {
    if (!is_server() && is_client()) {
      if ((*object_iter)->get_type() == GameObject::BOMB
          && !(*object_iter)->is_flying()
          && (*object_iter)->get_maptile()->get_type() == MapTile::TRAP) {
        Bomb* sb = static_cast<Bomb*>(*object_iter);
        if (cb_client->get_server_frame_counter()
            > sb->get_reactivation_frame_counter()) {
          suspended_client_bombs.push_back(sb);
          object_iter = objects.erase(object_iter);
        }
        else {
          sb->set_reactivation_frame_counter
            (sb->get_reactivation_frame_counter() - 1);
        }
        continue;
      }
    }
    (*object_iter)->act();
  }
  for (std::list<Bomber*>::iterator bomber_object_iter = bomber_objects.begin();
       bomber_object_iter != bomber_objects.end();
       bomber_object_iter++) {
    if (!is_client()) {
      (*bomber_object_iter)->act();
    }
    else {
      (*bomber_object_iter)->act_net();
    }
    if (is_server()) {
      if ((*bomber_object_iter)->direction_has_changed(true)) {
        cb_server->send_SERVER_UPDATE_BOMBER((*bomber_object_iter)->get_x(),
                                             (*bomber_object_iter)->get_y(),
                                             (*bomber_object_iter)
                                             ->get_server_send_dir(),
                                             (*bomber_object_iter)
                                             ->get_object_id());
      }
    }
    else if (is_client()) {
      if ((*bomber_object_iter)->direction_has_changed(false)
          && Config::bomber[(*bomber_object_iter)->get_number()].is_local()) {
        cb_client->send_CLIENT_BOMBER_DIR((*bomber_object_iter)->get_number(),
                                          (*bomber_object_iter)
                                          ->get_client_dir());
        (*bomber_object_iter)->set_local_dir((*bomber_object_iter)
                                             ->get_client_dir());
      }
    }
  }
  cb_mutex->unlock();
}

void ClanBomberApplication::delete_some()
{
  int nr = 0;
  int obj_ids[66];
  // Check if we should delete some
  cb_mutex->lock();
  for (std::list<GameObject*>::iterator object_iter = objects.begin();
       object_iter != objects.end();
       object_iter++) {
    if ((*object_iter)->delete_me) {
      obj_ids[nr++] = (*object_iter)->get_object_id();
      delete *object_iter;
      object_iter = objects.erase(object_iter);
    }
  }
  for (std::list<Bomber*>::iterator bomber_object_iter = bomber_objects.begin();
       bomber_object_iter != bomber_objects.end();
       bomber_object_iter++) {
    if ((*bomber_object_iter)->delete_me) {
      if (is_client()) {
        (*bomber_object_iter)->reset();
        (*bomber_object_iter)->controller->deactivate();
        (*bomber_object_iter)->set_dead();
      }
      obj_ids[nr++] = (*bomber_object_iter)->get_object_id();
    }
  }
  cb_mutex->unlock();
  if (is_server() && nr > 0) {
    ClanBomberApplication::get_server()->send_SERVER_DELETE_OBJECTS(nr,
                                                                    obj_ids);
  }
}

void ClanBomberApplication::show_all()
{
  cb_mutex->lock();
  GameObject* draw_list[objects.size()+bomber_objects.size()];
  int n = 0;
  int i;
  // clear top and sides
  const int top_height = 40;
  CB_FillRect(0, 0, 800, top_height, 0, 0, 0);
  const int margin = 60;
  CB_FillRect(0, top_height, 0 + margin, 600 - top_height, 0, 0, 0);
  CB_FillRect(800 - margin, top_height, margin, 600 - top_height, 0, 0, 0);
  if (show_fps) {
    std::string nstr = str(boost::format(_("%1$d fps")) % fps);
    Resources::Font_small()->render(nstr, 535, 4,
                                    cbe::FontAlignment_0topcenter);
  }

  for (std::list<GameObject*>::iterator object_iter = objects.begin();
       object_iter != objects.end();
       object_iter++) {
    draw_list[n] = (*object_iter);
    n++;
  }
  for (std::list<Bomber*>::iterator bomber_object_iter = bomber_objects.begin();
       bomber_object_iter != bomber_objects.end();
       bomber_object_iter++) {
    draw_list[n] = (*bomber_object_iter);
    n++;
  }

  //XXX Now using a stable sort as originally
  std::sort(draw_list, draw_list + n, game_object_compare);

  if (map != NULL) {
    map->refresh_holes();
  }
  bool drawn_map = false;
  for (i=0; i<n; i++) {
    if (map != NULL && draw_list[i]->get_z() >= Z_GROUND
        && drawn_map == false) {
      map->show();
      drawn_map = true;
    }
    if (draw_list[i] != NULL) {
      draw_list[i]->show();
    }
  }
  if (map != NULL && !drawn_map) {
    map->show();
  }
  if (observer != NULL) {
    if (observer->get_round_time() > 0) {
      observer->show();
    }
    else {
      observer->show();
    }
  }
  cb_mutex->unlock();
}

void ClanBomberApplication::init_net_game()
{
  frame_count = 0;
  frame_time = 0;
  fps = 0;

  client_disconnected_from_server = false;
  client_connecting_to_new_server = false;

  // init map
  cb_mutex->lock();
  if (is_server()) {
    if (Config::get_random_map_order()) {
      map->load_random_valid();
    }
    else {
      if (Config::get_start_map() > map->get_map_count() - 1) {
        Config::set_start_map(map->get_map_count() - 1);
      }
      map->load_next_valid(Config::get_start_map());
    }
    if (Config::get_random_positions()) {
      map->randomize_bomber_positions();
    }
    int j = 0;
    for (int i=0; i< 8 ; i++) {
      if (Config::bomber[i].is_enabled()) {
        CL_Vector pos = map->get_bomber_pos(j++);
        new Bomber((int)(pos.x*40),
                   (int)(pos.y*40),
                   (Bomber::COLOR)(Config::bomber[i].get_skin()),
                   Controller::create(static_cast<Controller::CONTROLLER_TYPE>
                                      (Config::bomber[i].get_controller())),
                   Config::bomber[i].get_name(),
                   Config::bomber[i].get_team(),
                   i,
                   this);
      }
    }
    // this is for removing teams which only one player is in
    int team_count[] = { 0,0,0,0};
    for (int team=0;team<4;team++) {
      for (std::list<Bomber*>::iterator bomber_object_iter
             = cb_server->bomber_objects.begin();
           bomber_object_iter != cb_server->bomber_objects.end();
           bomber_object_iter++) {
        if ((*bomber_object_iter)->get_team() - 1 == team) {
          team_count[team]++;
        }
      }
    }
    for (std::list<Bomber*>::iterator bomber_object_iter
           = cb_server->bomber_objects.begin();
         bomber_object_iter != cb_server->bomber_objects.end();
         bomber_object_iter++) {
      if ((*bomber_object_iter)->get_team() != 0) {
        if (team_count[(*bomber_object_iter)->get_team() - 1] == 1) {
          (*bomber_object_iter)->set_team (0);
        }
      }
      (*bomber_object_iter)->set_pos(350, 270);
      (*bomber_object_iter)->controller->deactivate();
    }
    cb_server->send_SERVER_ADD_BOMBER();
    cb_server->send_SERVER_UPDATE_MAP();
    cb_server->send_SERVER_START_NEW_LEVEL();
    cb_server->reset_traffic_statistics();
    for (std::list<Bomber*>::iterator bomber_object_server_iter
           = cb_server->bomber_objects.begin();
         bomber_object_server_iter != cb_server->bomber_objects.end();
         bomber_object_server_iter++) {
      (*bomber_object_server_iter)
        ->fly_to((*bomber_object_server_iter)->get_orig_x(),
                 (*bomber_object_server_iter)->get_orig_y(), 300);
    }
  }
  if (observer == NULL) {
    observer = new Observer(0, 0, this);
  }
  cb_mutex->unlock();
}

bool ClanBomberApplication::init_game()
{
  frame_count = 0;
  frame_time = 0;
  fps = 0;

  // init map
  if (map == NULL) {
    map = new Map(this);
  }

  if (!map->any_valid_map()) {
    delete map;
    map = NULL;
    return false;
  }

  if (Config::get_random_map_order()) {
    map->load_random_valid();
  }
  else {
    if (Config::get_start_map() > map->get_map_count() - 1) {
      Config::set_start_map(map->get_map_count() - 1);
    }
    map->load_next_valid(Config::get_start_map());
  }
  // init GameObjects
  if (Config::get_random_positions()) {
    map->randomize_bomber_positions();
  }
  CL_Vector pos;
  int j = 0;
  for (int i=0; i< 8 ; i++) {
    if (Config::bomber[i].is_enabled()) {
      pos = map->get_bomber_pos(j++);
      new Bomber((int)(pos.x*40), (int)(pos.y*40),
                 static_cast<Bomber::COLOR>(Config::bomber[i].get_skin()),
                 Controller::create(static_cast<Controller::CONTROLLER_TYPE>
                                    (Config::bomber[i].get_controller())),
                 Config::bomber[i].get_name(), Config::bomber[i].get_team(), i,
                 this);
      bomber_objects.back()->set_pos(350, 270);
      bomber_objects.back()->fly_to((int)(pos.x*40), (int)(pos.y*40), 300);
      bomber_objects.back()->controller->deactivate();
    }
  }
  // this is for removing teams which only one player is in
  int team_count[] = { 0,0,0,0};
  for (int team = 0;team < 4; team++) {
    for (std::list<Bomber*>::iterator bomber_object_iter
           = bomber_objects.begin();
         bomber_object_iter != bomber_objects.end();
         bomber_object_iter++) {
      if ((*bomber_object_iter)->get_team() - 1 == team) {
        team_count[team]++;
      }
    }
  }
  for (std::list<Bomber*>::iterator bomber_object_iter = bomber_objects.begin();
       bomber_object_iter != bomber_objects.end();
       bomber_object_iter++) {
    if ((*bomber_object_iter)->get_team() != 0) {
      if (team_count[(*bomber_object_iter)->get_team() - 1] == 1) {
        (*bomber_object_iter)->set_team (0);
      }
    }
  }

  observer = new Observer(0, 0, this);

  return true;
}

void ClanBomberApplication::deinit_game()
{
  // delete all GameObjects
  cb_mutex->lock();
  for (std::list<GameObject*>::iterator object_iter = objects.begin();
       object_iter != objects.end();
       object_iter++) {
    delete *object_iter;
  }
  objects.clear();
  for (std::list<Bomber*>::iterator bomber_object_iter = bomber_objects.begin();
       bomber_object_iter != bomber_objects.end();
       bomber_object_iter++) {
    delete *bomber_object_iter;
  }
  bomber_objects.clear();
  if (is_server()) {
    cb_server->bomber_objects.clear();
  }
  if (observer) {
    delete observer;
    observer = NULL;
  }
  cb_mutex->unlock();
}

void ClanBomberApplication::run_intro()
{
  SDLKey escape = SDLK_ESCAPE;
  float alpha = 0;
  std::string domination(_("A WORLD  DOMINATION PROJECT"));
#ifdef CB_NO_WSTRING
  std::string &domihack = domination;
#else
  //TODO reimplement using codecvt
  size_t len = std::mbstowcs(NULL, domination.c_str(), domination.length());
  wchar_t wstr[len+1];
  std::mbstowcs(wstr, domination.c_str(), domination.length() + 1);
  std::wstring domihack(wstr);
#endif

  sleep(2);

  Timer::reset();
  Resources::Intro_winlevel()->play();
  while (!keyboard[escape]) {
    CB_FillScreen(0, 0, 0);
    SDL_PumpEvents();

    if (alpha < 255) {
      Resources::Intro_fl_logo()->blit(100, 250, alpha);
      alpha += Timer::time_elapsed(true) * 130.0f;
      CB_Flip();
    }
    else {
      Resources::Intro_fl_logo()->blit(100, 250);
      break;
    }
  }

  int stringwidth;

  Resources::Font_small()->getSize(domination, &stringwidth, NULL);

  if (!keyboard[escape]) {
    usleep(500000);

    for (int domispell=0;domispell <= domihack.length();domispell++) {
      CB_FillScreen(0, 0, 0);

      SDL_PumpEvents();
      if (keyboard[escape]) {
        break;
      }

      if (domination.substr(domispell,1) != " ") {
        Resources::Intro_typewriter()->play();
      }
      Resources::Intro_fl_logo()->blit(100, 250);
      Resources::Font_small()->render(domihack.substr(0, domispell),
                                      400-stringwidth/2, 360,
                                      cbe::FontAlignment_0topleft);

      CB_Flip();

      usleep(rand()%100000 + 80000);
    }

    if (keyboard[escape]) {
      usleep(1500000);
    }

    // Scroll out
    Resources::Menu_back()->play();
    Timer::reset();
    float scroll = 100;
    while (!keyboard[escape] && scroll < 800) {
      CB_FillScreen(0, 0, 0);

      SDL_PumpEvents();

      Resources::Intro_fl_logo()->blit(scroll, 250);

      Resources::Font_small()->render(domination, 400, 330+scroll*3/10,
                                      cbe::FontAlignment_0topcenter);

      CB_Flip();

      scroll += Timer::time_elapsed(true)*1100.0f;
    }
  }
}

void ClanBomberApplication::show_tutorial()
{
  int y = 25;
  Resources::Titlescreen()->blit(0, 0);
  CB_FillRect(0, 0, 800, 600, 0, 0, 0, 128);
  Resources::Font_big()->render(_("ClanBomber Extras"), 400, y,
                                cbe::FontAlignment_0topcenter);
  y+=80;
  Resources::Extras_extras()->put_screen(15, y-5, 0);
  Resources::Font_big()->render(_("Bomb:"), 70, y, cbe::FontAlignment_0topleft);
  Resources::Font_big()->render(_("You can place an additional bomb"), 250, y,
                                cbe::FontAlignment_0topleft);

  y+=50;
  Resources::Extras_extras()->put_screen(15, y-5, 1);
  Resources::Font_big()->render(_("Power:"), 70, y,
                                cbe::FontAlignment_0topleft);
  Resources::Font_big()->render(_("Explosions grow one field in each "
                                  "direction"), 250, y,
                                cbe::FontAlignment_0topleft);

  y+=50;
  Resources::Extras_extras()->put_screen(15, y-5, 2);
  Resources::Font_big()->render(_("Skateboard:"), 70, y,
                                cbe::FontAlignment_0topleft);
  Resources::Font_big()->render(_("Let's you move faster"), 250, y,
                                cbe::FontAlignment_0topleft);

  y+=50;
  Resources::Extras_extras()->put_screen(15, y-5, 3);
  Resources::Font_big()->render(_("Kick:"), 70, y, cbe::FontAlignment_0topleft);
  Resources::Font_big()->render(_("Kick bombs if you walk against one"),
                                250, y, cbe::FontAlignment_0topleft);

  y+=50;
  Resources::Extras_extras()->put_screen(15, y-5, 4);
  Resources::Font_big()->render(_("Throw:"), 70, y,
                                cbe::FontAlignment_0topleft);
  Resources::Font_big()->render(_("Throw Bombs if you press the button twice"),
                                250, y, cbe::FontAlignment_0topleft);
  Resources::Font_big()->render(_("without moving"), 250, y+40,
                                cbe::FontAlignment_0topleft);

  Resources::Font_big()->render(_("Press any key"), 400, 520,
                                cbe::FontAlignment_0topcenter);
  CB_Flip();

  CB_WaitForKeypress();

  y = 25;
  Resources::Titlescreen()->blit(0, 0);
  CB_FillRect(0, 0, 800, 600, 0, 0, 0, 128);

  Resources::Font_big()->render(_("ClanBomber Drugs"), 400, y,
                                cbe::FontAlignment_0topcenter);

  y+=80;
  Resources::Extras_extras()->put_screen(15, y-5, 5);
  Resources::Font_big()->render(_("Joint:"), 70, y,
                                cbe::FontAlignment_0topleft);
  Resources::Font_big()->render(_("Controller will be reversed"), 250, y,
                                cbe::FontAlignment_0topleft);

  y+=50;
  Resources::Extras_extras()->put_screen( 15,y-5,6);
  Resources::Font_big()->render(_("Viagra:"), 70, y,
                                cbe::FontAlignment_0topleft);
  Resources::Font_big()->render(_("Autofire, this can be very dangerous!"), 250,
                                y, cbe::FontAlignment_0topleft);

  y+=50;
  Resources::Extras_extras()->put_screen(15, y-5, 7);
  Resources::Font_big()->render(_("Cocaine:"), 70, y,
                                cbe::FontAlignment_0topleft);
  Resources::Font_big()->render(_("Let's you move very fast!! (too fast)"), 250,
                                y, cbe::FontAlignment_0topleft);

  Resources::Font_big()->render(_("Press any key"), 400, 520,
                                cbe::FontAlignment_0topcenter);
  CB_Flip();

  CB_WaitForKeypress();
}

boost::filesystem::path ClanBomberApplication::get_map_path()
{
  return map_path;
}

boost::filesystem::path ClanBomberApplication::get_local_map_path()
{
  return local_map_path;
}

int main(int argc, char **argv)
{
  CB_Locale();
  // parse command line options
  for (int i = 1; i < argc; i++) {
    if (!strcmp (argv[i], "--help")) {
      std::cout
        << _("\n    usage: clanbomber2 [--runserver=X] [--fullscreen]")
        << _("\n           (X: start server and run game when X players "
             "are enabled)\n\n");
      return 0;
    }
    else if (strstr (argv[i],"--runserver=") == argv[i]) {
      run_server_with_players_nr = atoi (1 + strchr(argv[i], '='));
      if (run_server_with_players_nr < 2 || run_server_with_players_nr > 8) {
        std::cout
          << _("\n    --runserver expects a number between 2 and 8\n\n");
        return -666;
      }
    }
    else if (!strcmp(argv[i], "--fullscreen")) {
      Config::set_fullscreen(true);
    }
    else {
      std::cout << _("Invalid argument") << std::endl;
    }
  }

#ifdef ENABLE_CONSOLE
  boost::thread thread = start_console();
#endif
  app = new ClanBomberApplication();
  app->main();
  delete app;

#ifdef ENABLE_CONSOLE
  exit(0);
#endif
  return 0;
}
