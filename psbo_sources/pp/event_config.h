#pragma once

#include <vector>
#include <map>

struct event_struct_t
{
	unsigned mapid;
	std::vector<unsigned> item_ids;
	unsigned chance7;
	unsigned chance30;
	unsigned chance90;
	event_struct_t();
	event_struct_t(unsigned mapid);
};

void init_event();
std::pair<unsigned, unsigned> get_event_item(unsigned mapid, unsigned placement, unsigned player_count);	//itemid, daycount
