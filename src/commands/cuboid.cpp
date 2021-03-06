/* 
 * hCraft - A custom Minecraft server.
 * Copyright (C) 2012	Jacob Zhitomirsky
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "commands/drawc.hpp"
#include "player.hpp"
#include "server.hpp"
#include "world.hpp"
#include "stringutils.hpp"
#include "utils.hpp"
#include "drawops.hpp"
#include <sstream>


namespace hCraft {
	namespace commands {
	
		namespace {
			struct cuboid_data {
				blocki bl;
			};
		}
		
		
		static bool
		on_blocks_marked (player *pl, block_pos marked[], int markedc)
		{
			cuboid_data *data = static_cast<cuboid_data *> (pl->get_data ("cuboid"));
			if (!data) return true; // shouldn't happen
			
			dense_edit_stage es (pl->get_world ());
			draw_ops draw (es);
			draw.fill_cuboid (marked[0], marked[1], data->bl);
			es.commit ();
			
			pl->delete_data ("cuboid");
			pl->message ("§3Cuboid complete");
			return true;
		}
		
		/* 
		 * /cuboid -
		 * 
		 * Fills a region marked by two points with a specified block.
		 * 
		 * Permissions:
		 *   - command.draw.cuboid
		 *       Needed to execute the command.
		 */
		void
		c_cuboid::execute (player *pl, command_reader& reader)
		{
			if (!pl->perm (this->get_exec_permission ()))
					return;
		
			if (!reader.parse (this, pl))
					return;
			if (reader.no_args () || reader.arg_count () > 1)
				{ this->show_summary (pl); return; }
			
			std::string& str = reader.next ().as_str ();
			if (!sutils::is_block (str))
				{
					pl->message ("§c * §7Invalid block§f: §c" + str);
					return;
				}
			
			blocki bl = sutils::to_block (str);
			if (bl.id == BT_UNKNOWN)
				{
					pl->message ("§c * §7Unknown block§f: §c" + str);
					return;
				}
			
			cuboid_data *data = new cuboid_data {bl};
			pl->create_data ("cuboid", data,
				[] (void *ptr) { delete static_cast<cuboid_data *> (ptr); });
			pl->get_nth_marking_callback (2) += on_blocks_marked;
			
			pl->message ("§8Cuboid §7(§8Block§7: §b" + str + "§7):");
			pl->message ("§8 * §7Please mark §btwo §7blocks§7.");
		}
	}
}
		
