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

#include "physics/physics.hpp"
#include "physics/physics.hpp"
#include "world.hpp"
#include "utils.hpp"
#include "entities/entity.hpp"
#include "player.hpp"
#include <functional>
#include <cstring>

#include <iostream> // DEBUG


namespace hCraft {
	
	physics_update::physics_update (world *w, int x, int y, int z, int extra, int tick,
		std::chrono::steady_clock::time_point nt, physics_block_callback cb)
		: params (), nt (nt)
	{
		this->type = PU_BLOCK;
		
		this->w = w;
		this->data.blk.x = x;
		this->data.blk.y = y;
		this->data.blk.z = z;
		this->data.blk.cb = cb;
		this->data.blk.extra = extra;
		this->tick = tick;
	}
	
	physics_update::physics_update (world *w, entity *e, bool persistent,
		int tick, std::chrono::steady_clock::time_point nt)
		: params (), nt (nt)
	{
		this->type = PU_ENTITY;
		
		this->w = w;
		this->data.ent.e = e;
		this->data.ent.persistent = persistent;
		this->tick = tick;
	}
		
		
		
	physics_params::physics_params ()
	{
		this->actions[0].type = PA_NONE;
	}
	
	
	
	ph_mem_subchunk::ph_mem_subchunk ()
	{
		std::memset (this->blocks, 0, 4096 * sizeof (unsigned short));
	}
	
	
	
	/* 
	 * Constructs and starts the worker thread.
	 */
	physics_worker::physics_worker (physics_manager &man)
		: paused (false), ticks (0), man (man),
			rnd (utils::ns_since_epoch ()), _running (true),
		
			// and finally, the thread:
			th (std::bind (std::mem_fn (&hCraft::physics_worker::main_loop), this))
		{ }
	
	/* 
	 * Destructor - stops the worker thread.
	 */
	physics_worker::~physics_worker ()
	{
		this->_running = false;
		if (this->th.joinable ())
			this->th.join ();
	}
	
	
	physics_manager::~physics_manager ()
	{
		this->updates.clear ();
		this->workers.clear ();
	}
	
	
	
	static bool
	handle_param_dissipate (physics_update& u, physics_action& act, std::minstd_rand& rnd)
	{
		std::uniform_int_distribution<> dis (0, act.val);
		if (dis (rnd) == 0)
			{
				u.w->queue_update (u.data.blk.x, u.data.blk.y, u.data.blk.z, BT_AIR);
				return false;
			}
		
		return true;
	}
	
	static bool
	handle_params (physics_update& u, physics_manager &man, std::minstd_rand& rnd)
	{
		bool expire = true;
		
		for (int i = 0; i < 8; ++i)
			{
				physics_action& act = u.params.actions[i];
				if (act.type == PA_NONE)
					break;
				if (act.expire == 0)
					continue;
				
				switch (act.type)
					{
					case PA_DISSIPATE:
						if (!handle_param_dissipate (u, act, rnd))
							return false;
						break;
					
					default: break;
					}
				
				if (act.expire == 0xFFFF)
					expire = false;
				else if (act.expire > 0)
					{
						-- act.expire;
						expire = false;
					}
			}
		
		if (!expire)
			{
				physics_update nu = u;
				nu.nt = std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * nu.tick);
				man.updates.push (nu);
			}
		
		return true;
	}
	
	

	/* 
	 * Where everything happens.
	 */
	void
	physics_worker::main_loop ()
	{
		physics_update u {};
		const static int updates_per_tick = 8000;
		int i, fcount;
		
		std::minstd_rand rnd ((utils::ns_since_epoch ()));
		
		while (this->_running)
			{
				std::this_thread::sleep_for (std::chrono::milliseconds (50));
				++ this->ticks;
				if (paused)
					continue;
				
				fcount = 0; // failure counter
				for (i = 0; i < updates_per_tick; ++i)
					{
						if (!this->_running || paused || this->man.updates.empty ())
							break;
						if (!this->man.updates.try_pop (u))
							{
								++ fcount;
								if (fcount % 15 == 0)
									std::this_thread::sleep_for (std::chrono::milliseconds (2));
								if (fcount == 60)
									break;
								continue;
							}
						
						if (u.nt > std::chrono::steady_clock::now ())
							{
								this->man.updates.push (u);
								continue;
							}
						
						// parameters
						if (!handle_params (u, this->man, this->rnd))
							continue;
						
						if (u.type == PU_BLOCK)
							{
								auto blk = u.data.blk;
								this->man.remove_block (u.w, blk.x, blk.y, blk.z);
								
								// does this block have a custom callback attached?
								if (blk.cb)
									{
										blk.cb (*u.w, blk.x, blk.y, blk.z, blk.extra, rnd);
									}
								else
									{
										// nope, use the one associated with its ID
										physics_block *pb = (u.w)->get_physics_at (blk.x, blk.y, blk.z);
										if (pb)
											pb->tick (*u.w, blk.x, blk.y, blk.z, blk.extra, nullptr, rnd);
									}
							}
						else if (u.type == PU_ENTITY)
							{
								auto ent = u.data.ent;
								
								if (ent.e->get_type () == ET_PLAYER)
									{
										player *pl = dynamic_cast<player *> (ent.e);
										if (pl->get_world () != u.w)
											continue;
									}
								
								if (!ent.e->tick (*u.w) && ent.persistent)
									{
										// requeue
										physics_update nu = u;
										nu.nt = std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * nu.tick);
										man.updates.push (nu);
									}
							}
					}
			}
	}
	
	
	
	bool
	physics_manager::block_exists_nolock (world *w, int x, int y, int z)
	{
		if (y < 0 || y > 255) return false;
		
		auto w_itr = this->block_mem.find (w);
		if (w_itr == this->block_mem.end ())
			return false;
		std::unordered_map<chunk_pos, ph_mem_chunk, chunk_pos_hash>&
			mem_chunks = w_itr->second;
		
		auto ch_itr = mem_chunks.find ({x >> 4, z >> 4});
		if (ch_itr == mem_chunks.end ())
			return false;
		ph_mem_chunk& ch = ch_itr->second;
		ph_mem_subchunk* sub = ch.subs[y >> 4];
		if (sub == nullptr)
			return false;
		return (sub->blocks[((y & 0xF) << 8) | ((z & 0xF) << 4) | (x & 0xF)] > 0);
	}
	
	void
	physics_manager::add_block_nolock (world *w, int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		
		std::unordered_map<chunk_pos, ph_mem_chunk, chunk_pos_hash>&
			mem_chunks = this->block_mem[w];
		ph_mem_chunk& ch = mem_chunks[{x >> 4, z >> 4}];
		ph_mem_subchunk* sub = ch.subs[y >> 4];
		if (sub == nullptr)
			sub = ch.subs[y >> 4] = new ph_mem_subchunk ();
		
		unsigned int index = ((y & 0xF) << 8) | ((z & 0xF) << 4) | (x & 0xF);
		if (sub->blocks[index] < 0xFFFF)
			++ sub->blocks[index];
		else
			std::cout << "!!!" << std::endl;
	}
	
		
	bool
	physics_manager::block_exists (world *w, int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		return this->block_exists_nolock (w, x, y, z);
	}
	
	void
	physics_manager::add_block (world *w, int x, int y, int z)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		this->add_block_nolock (w, x, y, z);
	}
	
	void
	physics_manager::remove_block (world *w, int x, int y, int z)
	{
		if (y < 0 || y > 255) return;
		std::lock_guard<std::mutex> guard {this->lock};
		
		auto w_itr = this->block_mem.find (w);
		if (w_itr == this->block_mem.end ())
			return;
		std::unordered_map<chunk_pos, ph_mem_chunk, chunk_pos_hash>&
			mem_chunks = w_itr->second;
		
		auto ch_itr = mem_chunks.find ({x >> 4, z >> 4});
		if (ch_itr == mem_chunks.end ())
			return;
		ph_mem_chunk& ch = ch_itr->second;
		ph_mem_subchunk* sub = ch.subs[y >> 4];
		if (sub == nullptr)
			return;
		
		unsigned int index = ((y & 0xF) << 8) | ((z & 0xF) << 4) | (x & 0xF);
		if (sub->blocks[index] > 0)
			-- sub->blocks[index];
	}
	
	
	
//-----------
	
	/* 
	 * Changes the number of worker threads to utilize.
	 */
	void
	physics_manager::set_thread_count (unsigned int count)
	{
		if (count > 20) count = 20;
		std::lock_guard<std::mutex> guard {this->lock};
		
		if (count == this->workers.size ())
			return; // nothing to do
		
		if (count > this->workers.size ())
			{
				while (this->workers.size () < count)
					this->workers.emplace_back (new physics_worker (*this));
				return;
			}
		
		if (count == 0)
			{
				this->workers.clear ();
				return;
			}
		
		this->workers.resize (count);
	}
	
	
	
	/* 
	 * Queues an update to be processed by one of the workers:
	 */
	void
	physics_manager::queue_physics (world *w, int x, int y, int z,
		int extra, int tick_delay, physics_params *params,
		physics_block_callback cb)
	{
		if (tick_delay == 0) tick_delay = 1;
		-- tick_delay;
		
		std::lock_guard<std::mutex> guard {this->lock};
		this->add_block_nolock (w, x, y, z);
		
		physics_update u (w, x, y, z, extra, tick_delay,
			std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * tick_delay),
			cb);
		if (params)
			for (int i = 0; i < 8; ++i)
				{
					u.params.actions[i] = params->actions[i];
					if (params->actions[i].type == PA_NONE)
						break;
				}
		
		this->updates.push (u);
	}
	
	/* 
	 * Queues an update only if one with the same xyz coordinates does not
	 * already exist.
	 */
	void
	physics_manager::queue_physics_once (world *w, int x, int y, int z,
		int extra, int tick_delay, physics_params *params,
		physics_block_callback cb)
	{
		std::lock_guard<std::mutex> guard {this->lock};
		if (this->block_exists_nolock (w, x, y, z))
			return;
		
		if (tick_delay == 0) tick_delay = 1;
		-- tick_delay;
		
		this->add_block_nolock (w, x, y, z);
		physics_update u (w, x, y, z, extra, tick_delay,
			std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * tick_delay),
			cb);
		if (params)
			for (int i = 0; i < 8; ++i)
				{
					u.params.actions[i] = params->actions[i];
					if (params->actions[i].type == PA_NONE)
						break;
				}
		
		this->updates.push (u);
	}
	
	
	/* 
	 * Queues an entity update.
	 */
	void
	physics_manager::queue_physics (world *w, entity *e, bool persistent,
		int tick_delay, physics_params *params)
	{
		if (tick_delay == 0) tick_delay = 1;
		-- tick_delay;
		
		std::lock_guard<std::mutex> guard {this->lock};
		
		physics_update u (w, e, persistent, tick_delay,
			std::chrono::steady_clock::now () + std::chrono::milliseconds (50 * tick_delay));
		if (params)
			for (int i = 0; i < 8; ++i)
				{
					u.params.actions[i] = params->actions[i];
					if (params->actions[i].type == PA_NONE)
						break;
				}
		
		this->updates.push (u);
	}
}

