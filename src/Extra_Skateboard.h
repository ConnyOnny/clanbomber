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

#ifndef Extra_Skateboard_h
#define Extra_Skateboard_h

#include "Extra.h"

/**
 * Increases movement speed.
 *
 * @author Andreas Hundt
 * @author Denis Oliver Kropp
 */
class Extra_Skateboard : public Extra
{
public:
	Extra_Skateboard( int _x, int _y, ClanBomberApplication *_app );
	~Extra_Skateboard();

	ExtraType get_ExtraType()
	{
		return SKATEBOARD;
	}

	
protected:
	virtual void effect( Bomber* bomber );
};

#endif









