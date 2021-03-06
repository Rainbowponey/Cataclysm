#include "game.h"
#include "setvector.h"
#include "output.h"
#include "keypress.h"
#include "player.h"
#include "inventory.h"
#include "mapdata.h"
#include "skill.h"

#define PICKUP_RANGE 2

bool will_flood_stop(map *m, bool fill[SEEX * 3][SEEY *3], int x, int y);

void game::init_construction()
{
 int id = -1;
 int tl, cl, sl;

 #define CONSTRUCT(name, difficulty, able, done) \
  sl = -1; id++; \
  constructions.push_back( constructable(id, name, difficulty, able, done))

 #define STAGE(...)\
  tl = 0; cl = 0; sl++; \
  constructions[id].stages.push_back(construction_stage(__VA_ARGS__));
 #define TOOL(...)   setvector(constructions[id].stages[sl].tools[tl], \
                               __VA_ARGS__); tl++
 #define COMP(...)   setvector(constructions[id].stages[sl].components[cl], \
                               __VA_ARGS__); cl++

/* CONSTRUCT( name, time, able, done )
 * Name is the name as it appears in the menu; 30 characters or less, please.
 * time is the time in MINUTES that it takes to finish this construction.
 *  note that 10 turns = 1 minute.
 * able is a function which returns true if you can build it on a given tile
 *  See construction.h for options, and this file for definitions.
 * done is a function which runs each time the construction finishes.
 *  This is useful, for instance, for removing the trap from a pit, or placing
 *  items after a deconstruction.
 */

 CONSTRUCT("Dig Pit", 0, &construct::able_dig, &construct::done_pit);
  STAGE(t_pit_shallow, 10);
   TOOL(itm_shovel, NULL);
  STAGE(t_pit, 10);
   TOOL(itm_shovel, NULL);

 CONSTRUCT("Fill Pit", 0, &construct::able_pit, &construct::done_fill_pit);
  STAGE(t_pit_shallow, 5);
   TOOL(itm_shovel, NULL);
  STAGE(t_dirt, 5);
   TOOL(itm_shovel, NULL);

 CONSTRUCT("Clean Broken Window", 0, &construct::able_broken_window,
                                     &construct::done_nothing);
  STAGE(t_window_empty, 5);

 CONSTRUCT("Repair Door", 1, &construct::able_door_broken,
                             &construct::done_nothing);
  STAGE(t_door_c, 10);
   TOOL(itm_hammer, itm_nailgun, NULL);
   COMP(itm_2x4, 3, NULL);
   COMP(itm_nail, 12, NULL);

 CONSTRUCT("Board Up Door", 0, &construct::able_door, &construct::done_nothing);
  STAGE(t_door_boarded, 8);
   TOOL(itm_hammer, itm_nailgun, NULL);
   COMP(itm_2x4, 4, NULL);
   COMP(itm_nail, 8, NULL);

 CONSTRUCT("Board Up Window", 0, &construct::able_window,
                                 &construct::done_nothing);
  STAGE(t_window_boarded, 5);
   TOOL(itm_hammer, itm_nailgun, NULL);
   COMP(itm_2x4, 4, NULL);
   COMP(itm_nail, 8, NULL);

 CONSTRUCT("Build Wall", 2, &construct::able_empty, &construct::done_nothing);
  STAGE(t_wall_half, 10);
   TOOL(itm_hammer, itm_nailgun, NULL);
   COMP(itm_2x4, 10, NULL);
   COMP(itm_nail, 20, NULL);
  STAGE(t_wall_wood, 10);
   TOOL(itm_hammer, itm_nailgun, NULL);
   COMP(itm_2x4, 10, NULL);
   COMP(itm_nail, 20, NULL);

 CONSTRUCT("Build Window", 3, &construct::able_wall_wood,
                              &construct::done_nothing);
  STAGE(t_window_empty, 10);
   TOOL(itm_saw, NULL);
  STAGE(t_window, 5);
   COMP(itm_glass_sheet, 1, NULL);

 CONSTRUCT("Build Door", 4, &construct::able_wall_wood,
                              &construct::done_nothing);
  STAGE(t_door_frame, 15);
   TOOL(itm_saw, NULL);
  STAGE(t_door_b, 15);
   TOOL(itm_hammer, itm_nailgun, NULL);
   COMP(itm_2x4, 4, NULL);
   COMP(itm_nail, 12, NULL);
  STAGE(t_door_c, 15);
   TOOL(itm_hammer, itm_nailgun, NULL);
   COMP(itm_2x4, 4, NULL);
   COMP(itm_nail, 12, NULL);

/*  Removed until we have some way of auto-aligning fences!
 CONSTRUCT("Build Fence", 1, 15, &construct::able_empty);
  STAGE(t_fence_h, 10);
   TOOL(itm_hammer, NULL);
   COMP(itm_2x4, 5, itm_nail, 8, NULL);
*/

 CONSTRUCT("Build Roof", 4, &construct::able_between_walls,
                            &construct::done_nothing);
  STAGE(t_floor, 40);
   TOOL(itm_hammer, itm_nailgun, NULL);
   COMP(itm_2x4, 8, NULL);
   COMP(itm_nail, 40, NULL);

}

void game::construction_menu()
{
 WINDOW *w_con = newwin(25, 80, 0, 0);
 wborder(w_con, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
                LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );
 mvwprintz(w_con, 0, 1, c_red, "Construction");
 mvwputch(w_con,  0, 30, c_white, LINE_OXXX);
 mvwputch(w_con, 24, 30, c_white, LINE_XXOX);
 for (int i = 1; i < 24; i++)
  mvwputch(w_con, i, 30, c_white, LINE_XOXO);

 mvwprintz(w_con,  1, 31, c_white, "Difficulty:");

 wrefresh(w_con);

 bool update_info = true;
 int select = 0;
 char ch;

 inventory total_inv;
 total_inv.form_from_map(this, point(u.posx, u.posy), PICKUP_RANGE);
 total_inv.add_stack(u.inv_dump());

 do {
// Determine where in the master list to start printing
  int offset = select - 11;
  if (offset > constructions.size() - 22)
   offset = constructions.size() - 22;
  if (offset < 0)
   offset = 0;
// Print the constructions between offset and max (or how many will fit)
  for (int i = 0; i < 22 && i + offset < constructions.size(); i++) {
   int current = i + offset;
   nc_color col = (player_can_build(u, total_inv, constructions[current], 0) ?
                   c_white : c_dkgray);
   if (current == select)
    col = hilite(col);
   mvwprintz(w_con, 1 + i, 1, col, constructions[current].name.c_str());
  }

  if (update_info) {
   update_info = false;
   constructable current_con = constructions[select];
// Print difficulty
   int pskill = u.sklevel[sk_carpentry], diff = current_con.difficulty;
   mvwprintz(w_con, 1, 43, (pskill >= diff ? c_white : c_red),
             "%d   ", diff);
// Clear out lines for tools & materials
   for (int i = 2; i < 24; i++) {
    for (int j = 31; j < 79; j++)
     mvwputch(w_con, i, j, c_black, 'x');
   }

// Print stages and their requirements
   int posx = 33, posy = 2;
   for (int n = 0; n < current_con.stages.size(); n++) {
    nc_color color_stage = (player_can_build(u, total_inv, current_con, n) ?
                            c_white : c_dkgray);
    mvwprintz(w_con, posy, 31, color_stage, "Stage %d: %s", n + 1,
              terlist[current_con.stages[n].terrain].name.c_str());
    posy++;
// Print tools
    construction_stage stage = current_con.stages[n];
    bool has_tool[3] = {stage.tools[0].empty(),
                        stage.tools[1].empty(),
                        stage.tools[2].empty()};
    for (int i = 0; i < 3; i++) {
     while (has_tool[i])
      i++;
     for (int j = 0; j < stage.tools[i].size() && i < 3; j++) {
      itype_id tool = stage.tools[i][j];
      nc_color col = c_red;
      if (total_inv.has_amount(tool, 1)) {
       has_tool[i] = true;
       col = c_green;
      }
      int length = itypes[tool]->name.length();
      if (posx + length > 79) {
       posy++;
       posx = 33;
      }
      mvwprintz(w_con, posy, posx, col, itypes[tool]->name.c_str());
      posx += length + 1; // + 1 for an empty space
      if (j < stage.tools[i].size() - 1) { // "OR" if there's more
       if (posx > 77) {
        posy++;
        posx = 33;
       }
       mvwprintz(w_con, posy, posx, c_white, "OR");
       posx += 3;
      }
     }
    }
// Print components
    posy++;
    posx = 33;
    bool has_component[3] = {stage.components[0].empty(),
                             stage.components[1].empty(),
                             stage.components[2].empty()};
    for (int i = 0; i < 3; i++) {
     posx = 33;
     while (has_component[i])
      i++;
     for (int j = 0; j < stage.components[i].size() && i < 3; j++) {
      nc_color col = c_red;
      component comp = stage.components[i][j];
      if (( itypes[comp.type]->is_ammo() &&
           total_inv.has_charges(comp.type, comp.count)) ||
          (!itypes[comp.type]->is_ammo() &&
           total_inv.has_amount(comp.type, comp.count))) {
       has_component[i] = true;
       col = c_green;
      }
      int length = itypes[comp.type]->name.length();
      if (posx + length > 79) {
       posy++;
       posx = 33;
      }
      mvwprintz(w_con, posy, posx, col, "%s x%d",
                itypes[comp.type]->name.c_str(), comp.count);
      posx += length + 3; // + 2 for " x", + 1 for an empty space
// Add more space for the length of the count
      if (comp.count < 10)
       posx++;
      else if (comp.count < 100)
       posx += 2;
      else
       posx += 3;

      if (j < stage.components[i].size() - 1) { // "OR" if there's more
       if (posx > 77) {
        posy++;
        posx = 33;
       }
       mvwprintz(w_con, posy, posx, c_white, "OR");
       posx += 3;
      }
     }
     posy++;
    }
   }
   wrefresh(w_con);
  } // Finished updating
 
  ch = input();
  switch (ch) {
   case 'j':
    update_info = true;
    if (select < constructions.size() - 1)
     select++;
    else
     select = 0;
    break;
   case 'k':
    update_info = true;
    if (select > 0)
     select--;
    else
     select = constructions.size() - 1;
    break;
   case '\n':
   case 'l':
    if (player_can_build(u, total_inv, constructions[select], 0)) {
     place_construction(constructions[select]);
     ch = 'q';
    } else {
     popup("You can't build that!");
     for (int i = 1; i < 24; i++)
      mvwputch(w_con, i, 30, c_white, LINE_XOXO);
     update_info = true;
    }
    break;
  }
 } while (ch != 'q' && ch != 'Q' && ch != KEY_ESCAPE);
 refresh_all();
}

bool game::player_can_build(player &p, inventory inv, constructable con,
                            int level, bool cont)
{
 if (p.sklevel[sk_carpentry] < con.difficulty)
  return false;

 if (level < 0)
  level = con.stages.size();

 int start = 0;
 if (cont)
  start = level;
 for (int i = start; i < con.stages.size() && i <= level; i++) {
  construction_stage stage = con.stages[i];
  for (int j = 0; j < 3; j++) {
   if (stage.tools[j].size() > 0) {
    bool has_tool = false;
    for (int k = 0; k < stage.tools[j].size() && !has_tool; k++) {
     if (inv.has_amount(stage.tools[j][k], 1))
      has_tool = true;
    }
    if (!has_tool)
     return false;
   }
   if (stage.components[j].size() > 0) {
    bool has_component = false;
    for (int k = 0; k < stage.components[j].size() && !has_component; k++) {
     if (( itypes[stage.components[j][k].type]->is_ammo() &&
          inv.has_charges(stage.components[j][k].type,
                          stage.components[j][k].count)    ) ||
         (!itypes[stage.components[j][k].type]->is_ammo() &&
          inv.has_amount (stage.components[j][k].type,
                          stage.components[j][k].count)    ))
      has_component = true;
    }
    if (!has_component)
     return false;
   }
  }
 }
 return true;
}

void game::place_construction(constructable con)
{
 refresh_all();
 inventory total_inv;
 total_inv.form_from_map(this, point(u.posx, u.posy), PICKUP_RANGE);
 total_inv.add_stack(u.inv_dump());

 std::vector<point> valid;
 for (int x = u.posx - 1; x <= u.posx + 1; x++) {
  for (int y = u.posy - 1; y <= u.posy + 1; y++) {
   if (x == u.posx && y == u.posy)
    y++;
   construct test;
   bool place_okay = (test.*con.able)(this, point(x, y));
   for (int i = 0; i < con.stages.size() && !place_okay; i++) {
    if (m.ter(x, y) == con.stages[i].terrain)
     place_okay = true;
   }

   if (place_okay) {
// Make sure we're not trying to continue a construction that we can't finish
    int starting_stage = 0, max_stage = 0;
    for (int i = 0; i < con.stages.size(); i++) {
     if (m.ter(x, y) == con.stages[i].terrain)
      starting_stage = i + 1;
     if (player_can_build(u, total_inv, con, i, true))
      max_stage = i;
    }

    if (max_stage >= starting_stage) {
     valid.push_back(point(x, y));
     m.drawsq(w_terrain, u, x, y, true, false);
     wrefresh(w_terrain);
    }
   }
  }
 }
 mvprintz(0, 0, c_red, "Pick a direction in which to construct:");
 int dirx, diry;
 get_direction(dirx, diry, input());
 if (dirx == -2) {
  add_msg("Invalid direction.");
  return;
 }
 dirx += u.posx;
 diry += u.posy;
 bool point_is_okay = false;
 for (int i = 0; i < valid.size() && !point_is_okay; i++) {
  if (valid[i].x == dirx && valid[i].y == diry)
   point_is_okay = true;
 }
 if (!point_is_okay) {
  add_msg("You cannot build there!");
  return;
 }

// Figure out what stage to start at, and what stage is the maximum
 int starting_stage = 0, max_stage = 0;
 for (int i = 0; i < con.stages.size(); i++) {
  if (m.ter(dirx, diry) == con.stages[i].terrain)
   starting_stage = i + 1;
  if (player_can_build(u, total_inv, con, i, true))
   max_stage = i;
 }

 u.activity = player_activity(ACT_BUILD, con.stages[starting_stage].time * 1000,
                              con.id);
 u.moves = 0;
 std::vector<int> stages;
 for (int i = starting_stage; i <= max_stage; i++)
  stages.push_back(i);
 u.activity.values = stages;
 u.activity.placement = point(dirx, diry);
}

void game::complete_construction()
{
 inventory map_inv;
 map_inv.form_from_map(this, point(u.posx, u.posy), PICKUP_RANGE);
 int stage_num = u.activity.values[0];
 constructable built = constructions[u.activity.index];
 construction_stage stage = built.stages[stage_num];
 std::vector<component> player_use;
 std::vector<component> map_use;

 u.practice(sk_carpentry, built.difficulty * 10);
 if (built.difficulty < 1)
  u.practice(sk_carpentry, 10);
 for (int i = 0; i < 3; i++) {
  while (stage.components[i].empty()) 
   i++;
  if (i < 3) {
   std::vector<component> player_has;
   std::vector<component> map_has;
   for (int j = 0; j < stage.components[i].size(); j++) {
    component comp = stage.components[i][j];
    if (itypes[comp.type]->is_ammo()) {
// Check if we have components in both locations and enough to do the job
     if ((u.inv.charges_of(comp.type) + map_inv.charges_of(comp.type)) >= comp.count &&
	u.inv.charges_of(comp.type) > 0 && map_inv.charges_of(comp.type) > 0) {
      player_has.push_back(comp);
      map_has.push_back(comp);
// If not, check to see if either location has enough
     } else {
      if (u.has_charges(comp.type, comp.count)) {
       player_has.push_back(comp);
// dummy component to make our indexes line up
       map_has.push_back(comp);
       map_has[0].count = 0;
      } else if (map_inv.has_charges(comp.type, comp.count)) {
       map_has.push_back(comp);
       player_has.push_back(comp);
       player_has[0].count = 0;
      }
     }
    } else {
// repeat the above
     if ((u.inv.amount_of(comp.type) + map_inv.amount_of(comp.type)) >= comp.count &&
	u.inv.amount_of(comp.type) > 0 && map_inv.amount_of(comp.type) > 0) {
      player_has.push_back(comp);
      map_has.push_back(comp);
     } else {
      if (u.has_amount(comp.type, comp.count)) {
       player_has.push_back(comp);
       map_has.push_back(comp);
       map_has[0].count = 0;
      } else if (map_inv.has_amount(comp.type, comp.count)) {
       map_has.push_back(comp);
       player_has.push_back(comp);
       player_has[0].count = 0;
      }
     }
    }
   }

   if (player_has[0].count == 0 && map_has[0].count > 0)
// One on map, none in inventory, default to the one in the map
    map_use.push_back(map_has[i]);

   else if (player_has[0].count > 0 && map_has[0].count == 0)
// One in inventory, none on map, default to the one in inventory
    player_use.push_back(player_has[i]);

   else { // Let the player pick which component they want to use
    std::vector<std::string> options; // List for the menu_vec below
// Populate options with the names of the items
    for (int j = 0; j < map_has.size(); j++) {
     if (map_has[j].count != 0) {
      std::string tmpStr = itypes[map_has[j].type]->name + " (nearby)";
      options.push_back(tmpStr);
     }
    }
    for (int j = 0; j < player_has.size(); j++)
     if (player_has[j].count != 0) 
      options.push_back(itypes[player_has[j].type]->name);
// Get the selection via a menu popup
    int selection = menu_vec("Use which component first?", options) - 1;
    if (selection < map_has.size()) {
// Since we have dummy components, need to weed them out
     while(map_has[selection].count == 0)
      selection++;
     map_use.push_back(map_has[selection]);
// check if it's ammo
     if (itypes[map_use[map_use.size()-1].type]->is_ammo()) {
// if not enough on the ground, we have to pull from inventory
      if (map_inv.charges_of(map_use[map_use.size()-1].type) < map_has[0].count) {
       player_use.push_back(player_has[0]);
       player_use[player_use.size()-1].count -= map_inv.charges_of(map_has[0].type);
       map_use[map_use.size()-1].count = map_inv.charges_of(map_has[0].type);
      }
// not enough on the ground. go to inventory
     } else if (map_inv.amount_of(map_has[0].type) < map_has[0].count) {
      player_use.push_back(player_has[0]);
      player_use[player_use.size()-1].count -= map_inv.amount_of(map_has[0].type);
      map_use[map_use.size()-1].count = map_inv.amount_of(map_has[0].type);
     } 
// not a map selection
    } else {
     selection -= map_has.size();
// weed out dummies
     while(player_has[selection].count == 0)
      selection++;
     player_use.push_back(player_has[selection]);
// ammo
     if (itypes[player_use[player_use.size()-1].type]->is_ammo()) {
// grab from the ground if required
      if (u.inv.charges_of(player_use[player_use.size()-1].type) < player_has[0].count) {
       map_use.push_back(map_has[0]);
       map_use[map_use.size()-1].count -= u.inv.charges_of(player_has[0].type);
       player_use[player_use.size()-1].count = u.inv.charges_of(player_has[0].type);
      }
// not ammo & we don't have enough in pockets
     } else if (u.inv.amount_of(player_has[0].type) < player_has[0].count) {
      map_use.push_back(map_has[0]);
      map_use[map_use.size()-1].count -= u.inv.amount_of(player_use[player_use.size()-1].type);
      player_use[player_use.size()-1].count = u.inv.amount_of(player_has[0].type);
     }
    }
   }
  }
 } // Done looking at components
 
// Use up materials
 for (int i = 0; i < player_use.size(); i++) {
  if (itypes[player_use[i].type]->is_ammo())
   u.use_charges(player_use[i].type, player_use[i].count);
  else
   u.use_amount(player_use[i].type, player_use[i].count);
 }
 for (int i = 0; i < map_use.size(); i++) {
  if (itypes[map_use[i].type]->is_ammo())
   m.use_charges(point(u.posx, u.posy), PICKUP_RANGE,
                 map_use[i].type, map_use[i].count);
  else
   m.use_amount(point(u.posx, u.posy), PICKUP_RANGE,
                map_use[i].type, map_use[i].count);
 }

// Make the terrain change
 int terx = u.activity.placement.x, tery = u.activity.placement.y;
 m.ter(terx, tery) = stage.terrain;
 construct effects;
 (effects.*built.done)(this, point(terx, tery));

// Strip off the first stage in our list...
 u.activity.values.erase(u.activity.values.begin());
// ...and start the next one, if it exists
 if (u.activity.values.size() > 0) {
  construction_stage next = built.stages[u.activity.values[0]];
  u.activity.moves_left = next.time * 1000;
 } else // We're finished!
  u.activity.type = ACT_NULL;
}

bool construct::able_empty(game *g, point p)
{
 return (g->m.move_cost(p.x, p.y) == 2);
}

bool construct::able_window(game *g, point p)
{
 return (g->m.ter(p.x, p.y) == t_window_frame ||
         g->m.ter(p.x, p.y) == t_window_empty ||
         g->m.ter(p.x, p.y) == t_window);
}

bool construct::able_broken_window(game *g, point p)
{
 return (g->m.ter(p.x, p.y) == t_window_frame);
}

bool construct::able_door(game *g, point p)
{
 return (g->m.ter(p.x, p.y) == t_door_c ||
         g->m.ter(p.x, p.y) == t_door_b ||
         g->m.ter(p.x, p.y) == t_door_o ||
         g->m.ter(p.x, p.y) == t_door_locked);
}

bool construct::able_door_broken(game *g, point p)
{
 return (g->m.ter(p.x, p.y) == t_door_b);
}

bool construct::able_wall(game *g, point p)
{
 return (g->m.ter(p.x, p.y) == t_wall_h || g->m.ter(p.x, p.y) == t_wall_v ||
         g->m.ter(p.x, p.y) == t_wall_wood);
}

bool construct::able_wall_wood(game *g, point p)
{
 return (g->m.ter(p.x, p.y) == t_wall_wood);
}

bool construct::able_between_walls(game *g, point p)
{
 bool fill[SEEX * 3][SEEY * 3];
 for (int x = 0; x < SEEX * 3; x++) {
  for (int y = 0; y < SEEY * 3; y++)
   fill[x][y] = false;
 }

 return (will_flood_stop(&(g->m), fill, p.x, p.y)); // See bottom of file
}

bool construct::able_dig(game *g, point p)
{
 return (g->m.has_flag(diggable, p.x, p.y));
}

bool construct::able_pit(game *g, point p)
{
 return (g->m.ter(p.x, p.y) == t_pit || g->m.ter(p.x, p.y) == t_pit_shallow);
}

bool will_flood_stop(map *m, bool fill[SEEX * 3][SEEY * 3], int x, int y)
{
 if (x == 0 || y == 0 || x == SEEX * 3 - 1 || y == SEEY * 3 - 1)
  return false;

 fill[x][y] = true;
 bool skip_north = (fill[x][y - 1] || m->ter(x, y - 1) == t_wall_h ||
                                      m->ter(x, y - 1) == t_wall_v ||  
                                      m->ter(x, y - 1) == t_wall_wood),
      skip_south = (fill[x][y + 1] || m->ter(x, y + 1) == t_wall_h ||
                                      m->ter(x, y + 1) == t_wall_v ||
                                      m->ter(x, y + 1) == t_wall_wood),
      skip_east  = (fill[x + 1][y] || m->ter(x + 1, y) == t_wall_h ||
                                      m->ter(x + 1, y) == t_wall_v ||  
                                      m->ter(x + 1, y) == t_wall_wood),
      skip_west  = (fill[x - 1][y] || m->ter(x - 1, y) == t_wall_h ||
                                      m->ter(x - 1, y) == t_wall_v ||
                                      m->ter(x - 1, y) == t_wall_wood);

 return ((skip_north || will_flood_stop(m, fill, x    , y - 1)) &&
         (skip_east  || will_flood_stop(m, fill, x + 1, y    )) &&
         (skip_south || will_flood_stop(m, fill, x    , y + 1)) &&
         (skip_west  || will_flood_stop(m, fill, x - 1, y    ))   );
}

void construct::done_pit(game *g, point p)
{
 if (g->m.ter(p.x, p.y) == t_pit)
  g->m.add_trap(p.x, p.y, tr_pit);
}

void construct::done_fill_pit(game *g, point p)
{
 if (g->m.tr_at(p.x, p.y) == tr_pit)
  g->m.tr_at(p.x, p.y) = tr_null;
}
