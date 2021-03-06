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

#include "sqlops.hpp"
#include "sql.hpp"
#include "server.hpp"
#include <ctime>


namespace hCraft {
	
	/* 
	 * Total number of players registered in the database.
	 */
	int
	sqlops::player_count (sql::connection& conn)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT Count(*) FROM `players`");
		if (stmt.step (row))
			return row.at (0).as_int ();
		return 0;
	}
	
	/* 
	 * Checks whether the database has a player with the given name registered.
	 */
	bool
	sqlops::player_exists (sql::connection& conn, const char *name)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT Count(*) FROM `players` WHERE `name`=?");
		stmt.bind (1, name, sql::pass_transient);
		if (stmt.step (row))
			return (row.at (0).as_int () == 1);
		return false;
	}
	
	
	
	/* 
	 * Saves all information about the given player (named @{name}) stored in @{in}
	 * to the database.
	 * 
	 * NOTE: The 'id' (always) and 'name' (if overwriting) fields are ignored.
	 * 
	 * Returns true if the player already existed before the function was called.
	 */
	bool
	sqlops::save_player_data (sql::connection& conn, const char *name,
		server &srv, const player_info& in)
	{
		bool exists;
		sql::statement stmt {conn};
		if ((exists = player_exists (conn, name)))
			{
				stmt.prepare ("UPDATE `players`  SET `name`=?, `nick`=?, `ip`=?, `op`=?, "
					"`rank`=?, `blocks_destroyed`=?, `blocks_created`=?, "
					"`messages_sent`=?, `first_login`=?, `last_login`=?, "
					"`login_count`=?, `balance`=?, `banned`=?   WHERE `name`=?");
			}
		else
			{
				stmt.prepare ("INSERT INTO `players` (`name`, `nick`, "
					"`ip`, `op`, `rank`, `blocks_destroyed`, `blocks_created`, "
					"`messages_sent`, `first_login`, `last_login`, "
					"`login_count`, `balance`, `banned`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
			}
		
		int i = 1;
		
		stmt.bind (i++, name);
		stmt.bind (i++, in.nick.c_str ());
		stmt.bind (i++, in.ip.c_str ());
		stmt.bind (i++, in.op ? 1 : 0);
		
		std::string rank_str;
		in.rnk.get_string (rank_str);
		stmt.bind (i++, rank_str.c_str ());
		
		stmt.bind (i++, in.blocks_destroyed);
		stmt.bind (i++, in.blocks_created);
		stmt.bind (i++, in.messages_sent);
		stmt.bind (i++, (long long)in.first_login);
		stmt.bind (i++, (long long)in.last_login);
		stmt.bind (i++, in.login_count);
		stmt.bind (i++, in.balance);
		stmt.bind (i++, in.banned ? 1 : 0);
		
		if (exists)
			stmt.bind (i++, name);
		
		sql::row row;
		while (stmt.step (row))
			;
		
		return exists;
	}
		
	/* 
	 * Fills the specified player_info structure with information about
	 * the given player. Returns true if the player found, false otherwise.
	 */
	bool
	sqlops::player_data (sql::connection& conn, const char *name,
		server &srv, sqlops::player_info& out)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT * FROM `players` WHERE `name`=?");
		stmt.bind (1, name, sql::pass_transient);
		if (stmt.step (row))
			{
				out.id = row.at (0).as_int ();
				out.name.assign (row.at (1).as_cstr ());
				out.nick.assign (row.at (2).as_cstr ());
				out.ip.assign (row.at (3).as_cstr ());
				
				out.op = (row.at (4).as_int () == 1);
				
				try
					{
						out.rnk.set (row.at (5).as_cstr (), srv.get_groups ());
					}
				catch (const std::exception& str)
					{
						// invalid rank
						out.rnk.set (srv.get_groups ().default_rank);
						srv.get_logger () (LT_ERROR) << "Player \"" << name << "\" has an invalid rank." << std::endl;
					}
				
				out.blocks_destroyed = row.at (6).as_int ();
				out.blocks_created = row.at (7).as_int ();
				out.messages_sent = row.at (8).as_int ();
				
				out.first_login = (std::time_t)row.at (9).as_int ();
				out.last_login = (std::time_t)row.at (10).as_int ();
				out.login_count = row.at (11).as_int ();
				
				out.balance = row.at (12).as_double ();
				
				out.banned = (row.at (13).as_int () == 1);
				
				return true;
			}
		return false;
	}
	
	/* 
	 * Returns the rank of the specified player.
	 */
	rank
	sqlops::player_rank (sql::connection& conn, const char *name, server& srv)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT `rank` FROM `players` WHERE `name`=?");
		stmt.bind (1, name, sql::pass_transient);
		
		if (!stmt.step (row))
			return srv.get_groups ().default_rank;
		
		rank rnk;
		try
			{
				rnk.set (row.at (0).as_cstr (), srv.get_groups ());
			}
		catch (const std::exception& str)
			{
				// invalid rank
				srv.get_logger () (LT_ERROR) << "Player \"" << name << "\" has an invalid rank." << std::endl;
				return srv.get_groups ().default_rank;
			}
		
		return rnk;
	}
	
	
	
	/* 
	 * Fills the given player_info structure with default values for a player
	 * with the specified name.
	 */
	void
	sqlops::default_player_data (const char *name, server &srv, player_info& pd)
	{
		pd.id = -1;
		pd.name.assign (name);
		pd.nick.assign (name);
		pd.ip.assign ("0.0.0.0");
		pd.op = false;
		pd.rnk = srv.get_groups ().default_rank;
		pd.blocks_created = 0;
		pd.blocks_destroyed = 0;
		pd.messages_sent = 0;
		pd.first_login = (std::time_t)0;
		pd.last_login = (std::time_t)0;
		pd.login_count = 0;
		pd.balance = 0.0;
		pd.banned = false;
	}
	
	
	
	/* 
	 * Player name-related:
	 */
	
	std::string
	sqlops::player_name (sql::connection& conn, const char *name)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT `name` FROM `players` WHERE `name`=?");
		stmt.bind (1, name, sql::pass_transient);
		if (stmt.step (row))
			return row.at (0).as_str ();
		return "";
	}
	
	std::string
	sqlops::player_colored_name (sql::connection& conn, const char *name, server &srv)
	{
		rank rnk = player_rank (conn, name, srv);
		std::string str;
		str.append ("§");
		str.push_back (rnk.main ()->color);
		str.append (player_name (conn, name));
		return str;
	}
	
	std::string
	sqlops::player_nick (sql::connection& conn, const char *name)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT `nick` FROM `players` WHERE `name`=?");
		stmt.bind (1, name, sql::pass_transient);
		if (stmt.step (row))
			return row.at (0).as_str ();
		return "";
	}
	
	std::string
	sqlops::player_colored_nick (sql::connection& conn, const char *name, server &srv)
	{
		rank rnk = player_rank (conn, name, srv);
		std::string str;
		str.append ("§");
		str.push_back (rnk.main ()->color);
		str.append (player_nick (conn, name));
		return str;
	}
	
	
		
	/* 
	 * Changes the rank of the player that has the specified name.
	 */
	void
	sqlops::modify_player_rank (sql::connection& conn, const char *name,
		const char *rankstr)
	{
		auto stmt = conn.query ("UPDATE `players` SET `rank`=? WHERE `name`=?");
		stmt.bind (1, rankstr, sql::pass_transient);
		stmt.bind (2, name, sql::pass_transient);

		while (stmt.step ())
			;
	}
	
	
	
	/* 
	 * Money-related:
	 */
	
	void
	sqlops::set_money (sql::connection& conn, const char *name, double amount)
	{
		auto stmt = conn.query ("UPDATE `players` SET `balance`=? WHERE `name`=?");
		stmt.bind (1, amount);
		stmt.bind (2, name, sql::pass_transient);
		while (stmt.step ())
			;
	}
	
	void
	sqlops::add_money (sql::connection& conn, const char *name, double amount)
	{
		auto stmt = conn.query ("UPDATE `players` SET `balance`=`balance`+? WHERE `name`=?");
		stmt.bind (1, amount);
		stmt.bind (2, name, sql::pass_transient);
		while (stmt.step ())
			;
	}
	
	double
	sqlops::get_money (sql::connection& conn, const char *name)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT `balance` FROM `players` WHERE `name`=?");
		stmt.bind (1, name, sql::pass_transient);
		if (stmt.step (row))
			return row.at (0).as_double ();
		return 0.0;
	}
	
	
	
	/* 
	 * Recording bans\kicks:
	 */
	
	void
	sqlops::record_kick (sql::connection& conn, const char *target,
		const char *kicker, const char *reason)
	{
		auto stmt = conn.query ("INSERT INTO `kicks` (`target`, `kicker`, `reason`, "
			"`kick_time`) VALUES (?, ?, ?, ?)");
		std::time_t t = std::time (nullptr);
		
		stmt.bind (1, target);
		stmt.bind (2, kicker);
		stmt.bind (3, reason);
		stmt.bind (4, (long long)t);
		while (stmt.step ())
			;
	}
	
	void
	sqlops::record_ban (sql::connection& conn, const char *target,
		const char *banner, const char *reason)
	{
		auto stmt = conn.query ("INSERT INTO `bans` (`target`, `banner`, `reason`, "
			"`ban_time`) VALUES (?, ?, ?, ?)");
		std::time_t t = std::time (nullptr);
		
		stmt.bind (1, target);
		stmt.bind (2, banner);
		stmt.bind (3, reason);
		stmt.bind (4, (long long)t);
		while (stmt.step ())
			;
	}
	
	void
	sqlops::record_unban (sql::connection& conn, const char *target,
		const char *unbanner, const char *reason)
	{
		auto stmt = conn.query ("INSERT INTO `unbans` (`target`, `unbanner`, `reason`, "
			"`unban_time`) VALUES (?, ?, ?, ?)");
		std::time_t t = std::time (nullptr);
		
		stmt.bind (1, target);
		stmt.bind (2, unbanner);
		stmt.bind (3, reason);
		stmt.bind (4, (long long)t);
		while (stmt.step ())
			;
	}
	
	void
	sqlops::modify_ban_status (sql::connection& conn, const char *username, bool ban)
	{
		auto stmt = conn.query ("UPDATE `players` SET `banned`=? WHERE `name`=?");
		stmt.bind (1, ban ? 1 : 0);
		stmt.bind (2, username, sql::pass_transient);
		while (stmt.step ())
			;
	}
	
	bool
	sqlops::is_banned (sql::connection& conn, const char *username)
	{
		sql::row row;
		auto stmt = conn.query ("SELECT `banned` FROM `players` WHERE `name`=?");
		stmt.bind (1, username, sql::pass_transient);
		if (stmt.step (row))
			return (row.at (0).as_int () == 1);
		return false;
	}
}

