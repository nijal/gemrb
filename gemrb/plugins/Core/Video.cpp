/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id$
 *
 */

#include "../../includes/win32def.h"
#include "Video.h"
#include "Palette.h"

Video::Video(void)
{
	Evnt = NULL;
}

Video::~Video(void)
{
}

/** Set Event Manager */
void Video::SetEventMgr(EventMgr* evnt)
{
	//if 'evnt' is NULL then no Event Manager will be used
	Evnt = evnt;
}

/** Mouse is invisible and cannot interact */
void Video::SetMouseEnabled(int enabled)
{
	DisableMouse = enabled^MOUSE_DISABLED;
}

/** Mouse cursor is grayed and doesn't click (but visible and movable) */
void Video::SetMouseGrayed(bool grayed)
{
	if (grayed) {
		DisableMouse |= MOUSE_GRAYED;
	} else {
		DisableMouse &= ~MOUSE_GRAYED;
	}
}
