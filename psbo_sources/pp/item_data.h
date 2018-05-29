#pragma once

#include <string>
#include <vector>
#include <map>
#include "binarystream.h"
#include "common.h"

extern std::map<std::string, unsigned> shop_kinds;
extern std::map<std::string, unsigned> part_kinds;
extern std::map<std::string, unsigned> item_ranks;
extern std::map<std::string, unsigned> avatar_lookup;

struct item_price_t
{
	unsigned char is_shop;
	unsigned short day_count;
	unsigned price;
	item_price_t() {}
	item_price_t(unsigned char is_shop, unsigned short day_count, unsigned price)
		: is_shop(is_shop)
		, day_count(day_count)
		, price(price)
	{}
	void operator <<(BinaryWriter & bw) const
	{
		bw.write(is_shop, day_count, price);
	}
};


struct item_list_data_t
{
	bool valid;
	unsigned shop_kind;
	unsigned part_kind;
	std::string name;
	unsigned item_rank;
	unsigned avatar;
	std::string text1;
	std::string text2;
	std::vector<item_price_t> prices;
	std::vector<unsigned char> gem_slots;

	bool is_shop() const
	{
		return shop_kind == 1;
	}

	item_list_data_t()
		: valid(false)
	{}
	item_list_data_t(std::vector<std::string> const & data)
	{
		try
		{
			if (data.size() >= 40)
			{
				valid = true;
				shop_kind = shop_kinds[to_lower(data.at(0))];
				{
					auto i = part_kinds.find(to_lower(data.at(2)));
					assert(i != part_kinds.end());
				}
				part_kind = part_kinds[to_lower(data.at(2))];
				name = data.at(3);
				item_rank = item_ranks[to_lower(data.at(5))];
				avatar = avatar_lookup[to_lower(data.at(6))];
				text1 = data.at(7);
				text2 = data.at(8);
				for (unsigned i = 0; i < 4; ++i)
				{
					unsigned price1 = atoi(data.at(14 + 1 + i * 3).c_str());
					unsigned price2 = atoi(data.at(14 + 2 + i * 3).c_str());

					if (price1 > 0 || price2 > 0)
					{
						unsigned day_count = atoi(data.at(14 + 0 + i * 3).c_str());
						if (day_count > 0)
						{
							switch (day_count)
							{
							case 7:
								break;
							case 30:
								day_count = 28;
								break;
							default:
								continue;
							}
							prices.emplace_back(price2 > 0 ? 2 : 0, day_count, price2 > 0 ? price2 : price1);
						}

					}

				}
				for (unsigned i = 0; i < 6; ++i)
				{
					if (data.at(29 + i * 2) != "0")
						gem_slots.push_back(i + 1);
				}
			}
			else
				valid = false;
		}
		catch (std::exception & e)
		{
			catch_message(e);
			valid = false;
		}
	}
};