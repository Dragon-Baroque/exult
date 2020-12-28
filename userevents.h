/*
 *  userevents.h - SDL User Events.
 *
 *  Copyright (C) 2000-2020  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef USEREVENTS_H
#define USEREVENTS_H  1

#include <SDL_events.h>
#include <cassert>
#ifdef DEBUG
#include <iostream>
#endif

/*
 * To Define a new SDL User Event for Exult :
 *   - Add a member to the enum, with a suitable name like SAMPLE_USER_EVENT,
 *     just before the MAX_USER_EVENT,
 *   - Include the userevents.h in the source files that handle
 *     the new SAMPLE_USER_EVENT,
 *   - Place the constant SAMPLE_USER_EVENT in switch statements.
 *
 * NB : The funcion acquire_user_events is for use by Exult main once only.
 */

enum ExultUserEvents {
	SHORTCUT_BAR_USER_EVENT = SDL_USEREVENT,
	TOUCH_UI_USER_EVENT,
	MAX_USER_EVENT
};

inline void register_user_events() {
	int user_event_base = SDL_RegisterEvents(MAX_USER_EVENT - SDL_USEREVENT);
#ifdef DEBUG
	std::cerr << "Registering " << (MAX_USER_EVENT - SDL_USEREVENT)
	          << " user events, base " << user_event_base
	          << ", expected base " << SDL_USEREVENT << std::endl;
#endif
	assert(user_event_base == SDL_USEREVENT);
}

#endif
