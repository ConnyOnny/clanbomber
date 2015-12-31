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

#include "UtilsThreads.h"
#include <cstdio>


CB_Thread *CB_ThreadCreate(THREADFUNCTION function, void *parameter)
{
  CB_Thread *thread;
  //printf("Creating a thread\n");
  thread = SDL_CreateThread(function, parameter);
  //TODO check if in reality thread can be NULL
  if (thread == NULL) {
    printf("Error creating a thread\n");
  }
  return thread;
}

void CB_ThreadKill(CB_Thread *thread)
{
  //printf("Killing a thread\n");
  SDL_KillThread(thread);
}

void CB_ThreadSetInstantKill()
{
  //printf("CB_ThreadSetInstantKill\n");
}

CB_Mutex *CB_MutexCreate()
{
  CB_Mutex *mutex;
  //printf("Creating a Mutex\n");
  mutex = SDL_CreateMutex();
  //TODO check if in reality mutex can be NULL
  if (mutex == NULL) {
    printf("Error creating mutex\n");
  }
  return mutex;
}

void CB_MutexDestroy(CB_Mutex *mutex)
{
  //printf("Destroying a Mutex\n");
  SDL_DestroyMutex(mutex);
}

void CB_MutexLock(CB_Mutex *mutex)
{
  //printf("Locking Mutex\n");
  SDL_mutexP(mutex);
}

void CB_MutexUnlock(CB_Mutex *mutex)
{
  //printf("Unlocking Mutex\n");
  SDL_mutexV(mutex);
}

CB_Condition *CB_ConditionCreate()
{
  CB_Condition *condition;
  //printf("Creating a Condition\n");
  condition = SDL_CreateCond();
  //TODO check if in reality condition can be NULL
  if (condition == NULL) {
    printf("Error creating condition\n");
  }
  return condition;
}

void CB_ConditionDestroy(CB_Condition *condition)
{
  //printf("Destroying a Condition\n");
  SDL_DestroyCond(condition);
}

void CB_ConditionSignal(CB_Condition *condition)
{
  //printf("Condition Signal\n");
  //TODO add error checking
  SDL_CondSignal(condition);
}

void CB_ConditionWait(CB_Condition *condition, CB_Mutex *mutex)
{
  //printf("Condition Wait\n");
  //TODO add error checking
  SDL_CondWait(condition, mutex);
}

void CB_ConditionBroadcast(CB_Condition *condition)
{
  //printf("Condition Broadcast\n");
  //TODO add error checking
  SDL_CondBroadcast(condition);
}
