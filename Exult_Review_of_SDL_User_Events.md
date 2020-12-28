# Review and Regularization of SDL User Events in Exult
December 2020 by Dragon Baroque.

## Analysis

The SDL Event Type is an integer in the range 0 to 65535, out of which the range 32768 to 65535 is left to the developer
as 32768 User Events.

The `SDL_USERVENT` constant, set to 32768, is actually the base of the User Event range, not its single member.

Exult uses two User Events in an inconsistent way :

1. The `ShortcutBar_gump` builds a range of icons for a quick access to some basic services in Exult. It uses the hardwired `SDL_USEREVENT` with
a private User Event Code to escape User Event collisions,
2. The `TouchUI` uses a registered User Event with several different User Event Codes to provide new modes of User Interface. Its current use
is limited to some Text Entry fields either in the New Game panel or in any Gump.

SDL registers User Event requests by slices : each request is for a number of User Events, each request is satisfied by a new slice of User Event types,
each slice is returned to the requester as the base of the slice, either above the top of the previous slice or at `SDL_USEREVENT` for the first slice.

As the `ShortcutBar_gump` grabs the `SDL_USEREVENT` without registering a request for one User Event, both `ShortcutBar_gump` and `TouchUI` end up using
the _same_ User Event `SDL_USEREVENT`. Collisions are escaped because `ShortcutBar_gump` and `TouchUI` use very different User Event codes, but this is
not a desirable situation. Better would be to use _two_ different User Events.

On the other hand, the advantage of using constant User Event types, like `SDL_USEREVENT`, is that they can be used in `switch` statements because the
C compiler is able to build the branch tables. Using the result of `SDL_RegisterEvents` in a `switch` statement is not possible.

## Proposal

Knowing the logic of `SDL_RegisterEvents`, Exult can register at once two User Events, one for `ShortcutBar_gump` and one for `TouchUI` immediately
after the initialization of SDL.

It can check with `assert` that the base of the slice is equal to `SDL_USEREVENT`, and then can proceed to use the two Event Types with constant
Event Types `SDL_USEREVENT` and `SDL_USEREVENT+1`, which are suitable for `switch` statements.

Of course, to allow for future User Events, the set of known User Event types is represented by a C `enum`, and the call to `SDL_RegisterEvents`
uses the top of the `enum` for the size of the slice of User Events.

This way, basically ensuring that the registration of SDL User Events is entrusted to one place only, Exult can dispatch User Event types
known at compilation time, and therefore suitable to the `switch` statement.

## State of the code change

The proposal is made of :

* A new `userevents.h` file which contains the `ExultUserEvents` enum, with current _constant_ members `SHORTCUT_BAR_USER_EVENT` and `TOUCH_UI_USER_EVENT`,
as well as an inline function `register_user_events`,
* Changes into `exult.cc` to call `register_user_events` immediately after `SDL_Init`,
* Changes into `touchui.h` and `touchui.cc` to remove the now useless `eventType`,
* Changes into `gumps/Gump_manager.h`, `gumps/ShortcutBar_gump.h`, `exult.cc`, `touchui.h`, `gamemgr/sigame.cc` and `gamemgr/bggame.cc` to `#include "uservents"`,
* Changes into `gamemgr/sigame.cc`, `gamemgr/bggame.cc`, `gumps/Gump_manager.cc`, `touchui.cc` to handle the `TOUCH_UI_USER_EVENT`,
* Changes into `exult.cc` and `gumps/ShortcutBar_gump.cc` to handle the `SHORTCUT_BAR_USER_EVENT`.

## Discussion and perspectives

The `userevents.h` contains its own `#include <SDL_events.h>`. This is a choice that may be amended once a policy of its systematic inclusion is defined.
At this moment, such a policy does not exist, as `#include "userevents.h"` have been inserted only when needed, preferably in a header file.

Should Exult need a new User Event, the process is quite simple :

1. Add a new User Event member in the `ExultUserEvents` `enum`, such as `SAMPLE_USER_EVENT`, just prior to the `MAX_USER_EVENT`,
2. Add `#include "userevents.h"` into the source files that handle the new `SAMPLE_USER_EVENT`,
3. Use the `SAMPLE_USER_EVENT` in any C statement, including `switch` statements on SDL Event types. As it is constant, this is permitted.

Dragon Baroque, December 28, 2020.
