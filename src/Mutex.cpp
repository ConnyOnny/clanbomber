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

#include "Mutex.h"
#include "ClanBomber.h"

Mutex::Mutex()
{
  //pthread_mutex_init(&mutex_handle, 0);
  mutex_handle = CB_MutexCreate();
}

Mutex::~Mutex()
{
  //pthread_mutex_destroy(&mutex_handle);
  CB_MutexDestroy(mutex_handle);
}

void Mutex::lock()
{
    if(ClanBomberApplication::is_client()) {
      //pthread_mutex_lock(&mutex_handle);
      CB_MutexLock(mutex_handle);
    }
}

void Mutex::unlock()
{
    if(ClanBomberApplication::is_client()) {
      //pthread_mutex_unlock(&mutex_handle);
      CB_MutexUnlock(mutex_handle);
    }
}
