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

#ifndef _hCraft__WORLDGENERATOR_H_
#define _hCraft__WORLDGENERATOR_H_

#include "chunk.hpp"


namespace hCraft {
	
	class world;
	
	/* 
	 * Base class for all world generators.
	 */
	class world_generator
	{
	public:
		virtual ~world_generator () { }
		virtual void generate (world& wr, chunk *out, int cx, int cz) = 0;
		virtual void generate_edge (world& wr, chunk *out);
		
		
		/* 
		 * Returns the name of this generator.
		 */
		virtual const char* name () = 0;
		virtual long seed () = 0;
		
		/* 
		 * Finds and instantiates a new world generator from the given name.
		 */
		static world_generator* create (const char *name, long seed);
		static world_generator* create (const char *name);
	};
	
	
	/* 
	 * Unlike the world_generator, full chunks are not generated by the classes
	 * that derive this one. Instead, detail_generators are more suited to
	 * ornament a world with *usually* smaller things (e.g.:  trees, lakes, etc...)
	 */
	class detail_generator
	{
	public:
		virtual ~detail_generator () { }
		
		virtual void seed (long s) { };
		virtual void generate (world &wr, int x, int y, int z) = 0;
	};
}

#endif

