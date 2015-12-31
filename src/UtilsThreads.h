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

#ifndef UTILSTHREADS_H
#define UTILSTHREADS_H
#include "SDL_thread.h"

typedef SDL_Thread CB_Thread;
typedef int (*THREADFUNCTION)(void*);
typedef SDL_mutex CB_Mutex;
typedef SDL_cond CB_Condition;


CB_Thread *CB_ThreadCreate(THREADFUNCTION function, void *parameter);
void CB_ThreadKill(CB_Thread *thread);
void CB_ThreadSetInstantKill();
CB_Mutex *CB_MutexCreate();
void CB_MutexDestroy(CB_Mutex *mutex);
void CB_MutexLock(CB_Mutex *mutex);
void CB_MutexUnlock(CB_Mutex *mutex);
CB_Condition *CB_ConditionCreate();
void CB_ConditionDestroy(CB_Condition *condition);
void CB_ConditionSignal(CB_Condition *condition);
void CB_ConditionWait(CB_Condition *condition, CB_Mutex *mutex);
void CB_ConditionBroadcast(CB_Condition *condition);
#endif
