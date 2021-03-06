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

#include "generator.hpp"
#include "world.hpp"
#include "chunk.hpp"
#include "player.hpp"
#include <functional>
#include <chrono>

#include <iostream> // DEBUG


namespace hCraft {
	
	chunk_generator::chunk_generator ()
	{
		this->th = nullptr;
		this->_running = false;
	}
	
	chunk_generator::~chunk_generator ()
	{
		this->stop ();
	}
	
	
	
	/* 
	 * Starts the internal thread and begins accepting generation requests.
	 */
	void
	chunk_generator::start ()
	{
		if (this->_running)
			return;
		
		this->_running = true;
		this->th = new std::thread (
			std::bind (std::mem_fn (&hCraft::chunk_generator::main_loop), this));
	}
	
	/* 
	 * Stops the generation thread and cleans up resources.
	 */
	void
	chunk_generator::stop ()
	{
		if (!this->_running)
			return;
		
		this->_running = false;
	}
	
	
	
	/* 
	 * Where everything happens.
	 */
	void
	chunk_generator::main_loop ()
	{
		bool should_rest = false;
		int counter = 0;
		int req_counter = 0;
		
		while (this->_running)
			{
				if (should_rest)
					std::this_thread::sleep_for (std::chrono::milliseconds (4));
				else if (counter % 250 == 0)
					std::this_thread::sleep_for (std::chrono::milliseconds (20));
				should_rest = false;
				++ counter;
				
				if (!this->requests.empty ())
					{
						gen_request req;
						
						// pop off queue
						{
							std::lock_guard<std::mutex> guard {this->request_mutex};
							if (this->requests.empty ())
								continue;
							
							req = this->requests.front ();
							this->requests.pop ();
							++ req_counter;
						}
						
						player *pl = req.pl;
						world *w = req.w;
						int flags = req.flags;
						
						if (!(flags & GFL_NOABORT) && (pl->get_world () != w || !pl->can_see_chunk (req.cx, req.cz)))
							{
								if (!(flags & GFL_NODELIVER))
									pl->deliver_chunk (w, req.cx, req.cz, nullptr, GFL_ABORTED, req.extra);
								continue;
							}
						
						if ((flags & GFL_NODELIVER) && (w->get_chunk (req.cx, req.cz) != nullptr))
							continue;
						
						// generate chunk
						chunk *ch = w->load_chunk (req.cx, req.cz);
						if (!ch) continue; // shouldn't happen :X
						
						// deliver
						// TODO: If the player disconnects at the right moment... this might
						//       cause some problems, since the player pointer will remain
						//       dangling...
						if (!(flags & GFL_NODELIVER))
							pl->deliver_chunk (w, req.cx, req.cz, ch, GFL_NONE, req.extra);
					}
				
				should_rest = true;
			}
	}
	
	
	
	/* 
	 * Requests the chunk located at the given coordinates to be generated.
	 * The specified player is then informed when it's ready.
	 */
	void
	chunk_generator::request (world *w, int cx, int cz, player *pl, int flags, int extra)
	{
		std::lock_guard<std::mutex> guard {this->request_mutex};
		this->requests.push ({pl, w, cx, cz, flags, extra});
	}
}

