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

#include "Event.h"

#include "ClanBomber.h"

Event::Event()
{
  //pthread_mutex_init (&event_mutex, 0);
  event_mutex = CB_MutexCreate();
  //pthread_cond_init (&wait_condition, 0);
  wait_condition = CB_ConditionCreate();
}

Event::~Event()
{
  //pthread_mutex_destroy (&event_mutex);
  CB_MutexDestroy(event_mutex);
  //pthread_cond_destroy (&wait_condition);
  CB_ConditionDestroy(wait_condition);
}

void Event::lock()
{
    if(ClanBomberApplication::is_client()) {
      //pthread_mutex_lock (&event_mutex);
      CB_MutexLock(event_mutex);
    }
}

void Event::unlock()
{
    if(ClanBomberApplication::is_client()) {
      //pthread_mutex_unlock (&event_mutex);
      CB_MutexUnlock(event_mutex);
    }
}

void Event::signal()
{
    if(ClanBomberApplication::is_client()) {
      //pthread_cond_signal (wait_condition);
      CB_ConditionSignal(wait_condition);
    }
}

void Event::wait()
{
    if(ClanBomberApplication::is_client()) {
      //pthread_cond_wait (wait_condition, event_mutex);
      CB_ConditionWait(wait_condition, event_mutex);
    }
}

void Event::broadcast()
{
    if(ClanBomberApplication::is_client()) {
      //pthread_cond_broadcast (wait_condition);
      CB_ConditionBroadcast(wait_condition);
    }
}
