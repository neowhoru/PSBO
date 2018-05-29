#include "stdafx.h"
#include "event_config.h"
#include <atomic>
#include "common.h"
#include "item_data.h"
#include "config.h"

extern std::atomic<unsigned> drop_event;
namespace
{
	unsigned const itemid_disp = 10000;
}
extern std::vector<item_list_data_t> itemlist;
std::string const & get_map_name(unsigned mapid);

std::map<unsigned, event_struct_t> events;
mutex_t event_mutex;

event_struct_t::event_struct_t()
{
	mapid = ~0;
	chance7 = 0;
	chance30 = 0;
	chance90 = 0;
}

event_struct_t::event_struct_t(unsigned mapid)
	:mapid(mapid)
{
	chance7 = 0;
	chance30 = 0;
	chance90 = 0;
}

namespace
{
	void add_event_config(std::string const & _left, std::string const & right, unsigned & mapid)
	{
		std::string left = to_lower(_left);
		if (left == "mapid")
		{
			mapid = convert<unsigned>(right);
			if (mapid < 0x80000000)
				events.emplace(mapid, mapid);
		}
		else if (left == "chance7")
		{
			auto i = events.find(mapid);
			if (i != events.end())
				i->second.chance7 = std::min<int>(0x7fffffff, std::max<int>(0, convert<int>(right)));
		}
		else if (left == "chance30")
		{
			auto i = events.find(mapid);
			if (i != events.end())
				i->second.chance30 = std::min<int>(0x7fffffff, std::max<int>(0, convert<int>(right)));
		}
		else if (left == "chance90")
		{
			auto i = events.find(mapid);
			if (i != events.end())
				i->second.chance90 = std::min<int>(0x7fffffff, std::max<int>(0, convert<int>(right)));
		}
		else if (left == "itemid")
		{
			auto i = events.find(mapid);
			if (i != events.end())
			{
				std::vector<std::string> d = split(right, ',');
				for (std::string const & line : d)
				{
					unsigned itemid = convert<unsigned>(trim(line)) - 1;
					if (itemid < itemlist.size() && itemlist[itemid].valid)
						i->second.item_ids.emplace_back(itemid);
					else
						logger << "Invalid itemid in event config: " << itemid;
				}
			}
		}
	}

	unsigned add_event_config_line(std::string const & str, unsigned & mapid)
	{
		std::vector<std::string> line = split(str, '=');

		if (line.size() == 0)
			return 0;
		if (line.size() == 2)
		{
			std::string left = trim(line[0]);
			std::string right = trim(line[1]);
			if (right.size() > 0 && right.back() == ';')
				right = trim(right.substr(0, right.size() - 1));
			if (left.size() > 0 && right.size() > 0)
			{
				add_event_config(left, right, mapid);
				return 2;
			}
			return 0;
		}
		return line.size();
	}
}

void init_event()
{
	unsigned current_load_mapid = ~0;
	impl_lock_guard(lock, event_mutex);
	events.clear();
	FILE * f1 = fopen("event_config.ini", "r+b");
	if (f1 != nullptr)
	{
		fseek(f1, 0, SEEK_END);
		long s = ftell(f1);
		fseek(f1, 0, SEEK_SET);
		std::string filedata;
		filedata.resize(s);

		if (fread(&filedata[0], 1, s, f1) == s)
		{
			unsigned l;

			std::vector<std::string> lines = split(filedata, 10);
			for (unsigned i = 0; i < lines.size(); ++i)
			{
				bool only_ws = true;
				for (l = 0; l < lines[i].size(); ++l)
				{
					only_ws &= lines[i][l] <= 32;
					if (lines[i][l] == '#')
						break;
				}
				if (only_ws)
					continue;

				unsigned line_size = add_event_config_line(lines[i].substr(0, l), current_load_mapid);
				if (line_size == 2)
					continue;
				if (line_size == 1 && trim(lines[i].substr(0, l)).size() == 0)
					continue;

				logger << LIGHT_RED << "Invalid event config on line " << (i + 1) << ": " << lines[i].substr(0, l) << endl;
			}

		}

		fclose(f1);
	}

	{
		auto l = logger.get_session();

		l << "Map Events:\r\n";
		for (auto i = events.begin(); i != events.end(); ++i)
		{

			l << "Map " << i->second.mapid << ": " << get_map_name(i->second.mapid) << " (" << i->second.chance7 << "%, " << i->second.chance30 << "%, " << i->second.chance90 << "%) ";
			for (unsigned j = 0; j < i->second.item_ids.size(); ++j)
				l << i->second.item_ids[j] << ' ';
			l << "\r\n";
		}
		l << "Drop event is " << (get_config<unsigned>("drop_event") == 0 ? "off" : "on");
	}
}

std::pair<unsigned, unsigned> get_event_item(unsigned mapid, unsigned placement, unsigned player_count)
{
	std::pair<unsigned, unsigned> r(0, 0);
	if (drop_event && placement + 1 < player_count && placement < 3)
	{
		impl_lock_guard(lock, event_mutex);

		auto _i = events.find(mapid);
		if (_i != events.end() && _i->second.item_ids.size() > 0)
		{

			unsigned chance7 = _i->second.chance7 * 1024;
			unsigned chance30 = _i->second.chance30 * 1024;
			unsigned chance90 = _i->second.chance90 * 1024;

			if (placement == 1)
			{
				chance7 /= 2;
				chance30 /= 2;
				chance90 /= 2;
			}
			else if (placement == 2)
			{
				chance7 /= 4;
				chance30 /= 4;
				chance90 /= 4;
			}

			unsigned rnum = rng::gen_uint(0, 100 * 1024);

			if (rnum < chance7)
				r.second = 7;
			else if (rnum < chance30)
				r.second = 30;
			else if (rnum < chance90)
				r.second = 90;

			if (r.second > 0)
				r.first = _i->second.item_ids[rng::gen_uint(0, _i->second.item_ids.size() - 1)];

		}
	}


	return r;
}
