#ifndef _EVENT_H_
#define _EVENT_H_

#include "faction.h"
#include "line.h"

class game;

enum event_type {
 EVENT_NULL,
 EVENT_HELP,
 EVENT_WANTED,
 EVENT_ROBOT_ATTACK,
 EVENT_SPAWN_WYRMS,
 EVENT_AMIGARA,
 NUM_EVENT_TYPES
};

struct event {
 event_type type;
 int turn;
 int faction_id;
 point map_point;

 event() {
  type = EVENT_NULL;
  turn = 0;
  faction_id = -1;
  map_point.x = -1;
  map_point.y = -1;
 }

 event(event_type e_t, int t, int f_id, int x, int y) {
  type = e_t;
  turn = t;
  faction_id = f_id;
  map_point.x = x;
  map_point.y = y;
 }

 void actualize(game *g); // When the time runs out
 void per_turn(game *g);  // Every turn
};

#endif
