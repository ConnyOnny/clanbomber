/*
 * This file is part of ClanBomber;
 * you can get it at "http://www.nongnu.org/clanbomber".
 *
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

#include <string>
#include <cstdlib>

#include <libguile.h>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>

#include "ClanBomber.h"
#include "Bomber.h"
#include "Map.h"
#include "Controller.h"
#include "UtilsGetHome.h"
#include "GameConfig.h"

extern ClanBomberApplication *app;

const bool need_lock = true;

//----------------C++ helper funcions------------------
void scm_cxx_primitive_load(boost::filesystem::path filename)
{
  scm_c_primitive_load(filename.string().c_str());
}

template<class funcp>
void scm_cxx_define_gsubr(const std::string &scm_name, int req, int opt,
                          int rest, funcp wrapper)
{
  scm_c_define_gsubr(scm_name.c_str(), req, opt, rest, (SCM (*)()) wrapper);
}

//------Conversions------
template<class T>
T scm_cxx_to_native(SCM p)
{
  //just a place holder
}

template<>
unsigned int scm_cxx_to_native<unsigned int>(SCM p)
{
  return scm_to_uint(p);
}

template<>
int scm_cxx_to_native<int>(SCM p)
{
  return scm_to_int(p);
}

template<>
char scm_cxx_to_native<char>(SCM p)
{
  return scm_to_char(p);
}

template<>
bool scm_cxx_to_native<bool>(SCM p)
{
  return scm_to_bool(p);
}

template<class T>
SCM scm_cxx_from_native(T p)
{
  //just a place holder
}

template<>
SCM scm_cxx_from_native<bool>(bool p)
{
  return scm_from_bool(p);
}

template<>
SCM scm_cxx_from_native<int>(int p)
{
  return scm_from_int(p);
}

//------End of Conversions------

template<class Tp, class Tr, Tr (*func)(Tp)>
SCM wrapper_1p_r(SCM ip)
{
  if (need_lock) {
    app->lock();
  }
  return scm_cxx_from_native(func(scm_cxx_to_native<Tp>(ip)));
  if (need_lock) {
    app->unlock();
  }
}

template<void (*func)()>
SCM wrapper_np_nr()
{
  if (need_lock) {
    app->lock();
  }
  func();
  if (need_lock) {
    app->unlock();
  }
  return SCM_BOOL_T;
}

template<class T, T (*a)(), void (*m)(T)>
SCM scm_cxx_acmu(SCM p)
{
  if (p == SCM_UNDEFINED) {
    return scm_cxx_from_native(a());
  }
  else {
    m(scm_cxx_to_native<T>(p));
    return SCM_BOOL_T;
  }
}

template<class T, T (*a)(), void (*m)(T)>
void scm_cxx_define_acmu(const char *scm_name)
{
  scm_cxx_define_gsubr(scm_name, 0, 1, 0, scm_cxx_acmu<T, a, m>);
}

//----------------End if C++ helper funcions------------------

bool kill_by_number(int player_num)
{
  int counter = 0;
  for (std::list<Bomber*>::iterator bomber_iter = app->bomber_objects.begin();
       bomber_iter != app->bomber_objects.end(); bomber_iter++, counter++) {
    if (counter == player_num) {
      (*bomber_iter)->die();
      return true;
    }
  }
  return false;
}

void next_map()
{
  app->map->load_next_valid();
}

void previous_map()
{
    app->map->load_previous_valid();
}

void add_bot(std::string &name, Controller::CONTROLLER_TYPE con_type,
             Bomber::COLOR color)
{
  int x = 0, y = 0;
  Bomber* bomber = new Bomber(x, y, color, Controller::create(con_type),
                              name, 0, 0, app);
  bomber->fly_to(app->map->get_passable());
  bomber->controller->activate();
}

bool shell = true;

bool get_shell()
{
  return shell;
}

void set_shell(bool new_shell)
{
  shell = new_shell;
}

//----------------Wrapper funcions------------------
SCM add_bot_wrapper(SCM name, SCM con_type, SCM color)
{
  if (need_lock) {
    app->lock();
  }
  //XXX most of this can be outside of lock

  char *tmp_name = scm_to_locale_string(name);
  std::string c_name(tmp_name);
  delete tmp_name;

  Controller::CONTROLLER_TYPE c_con_type = Controller::CONTROLLER_TYPE(0);
  if (con_type != SCM_UNDEFINED) {
    c_con_type = Controller::CONTROLLER_TYPE(scm_to_int(con_type));
  }

  Bomber::COLOR c_color = Bomber::COLOR(0);
  if (color != SCM_UNDEFINED) {
    c_color = Bomber::COLOR(scm_to_int(color));
  }
  /*switch (scm_to_int(color)) {
  case Bomber:
    c_color = Bomber::
  case 1:

		RED		= 0,
		BLUE	= 1,
		YELLOW	= 2,
		GREEN	= 3,
		RED2	= 4,
		BLUE2	= 5,
		YELLOW2	= 6,
		GREEN2	= 7,
  */
  add_bot(c_name, c_con_type, c_color);
  if (need_lock) {
    app->unlock();
  }
  return SCM_BOOL_T;
}

//----------------End of Wrapper funcions------------------

void add_functions()
{
  scm_cxx_define_gsubr("kill-by-number", 1, 0, 0,
                       wrapper_1p_r<int, bool, kill_by_number>);
  scm_cxx_define_gsubr("next-map", 0, 0, 0, wrapper_np_nr<next_map>);
  scm_cxx_define_gsubr("previous-map", 0, 0, 0, wrapper_np_nr<previous_map>);
  scm_cxx_define_gsubr("add-bot", 1, 2, 0, add_bot_wrapper);
  scm_cxx_define_acmu
    <int, Config::get_highlight_maptiles, Config::set_highlight_maptiles>
    ("config.highlight-maptiles");
  scm_cxx_define_acmu
    <bool, Config::get_fullscreen, Config::set_fullscreen>
    ("config.fullscreen");
  scm_cxx_define_acmu
    <bool, get_shell, set_shell>
    ("guile-config.shell");
}

void eval()
{
  //Maybe this should be GetConfigHome, not sure
  scm_cxx_primitive_load(GetDataHome() / "clanbomber" / "user.scm");

  scm_c_eval_string("(use-modules (ice-9 readline))");
  scm_c_eval_string("(activate-readline)");

}

void *inner(void *closure)
{
  add_functions();
  eval();
  if (shell) {
    scm_shell(0, NULL);
  }
}

class Console
{
public:
  void operator () ()
  {
    scm_with_guile(inner, NULL);
  }
};

boost::thread start_console()
{
  Console console;
  return boost::thread(console);
}
