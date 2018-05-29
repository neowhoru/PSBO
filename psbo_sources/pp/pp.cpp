#include "stdafx.h"
#include "network.h"
#include "config.h"
#include "common.h"
#include "database.h"
#include "file_io.h"
#include "sha256.h"
#include <ctime>
#include <random>
#include <memory>
#define _USE_MATH_DEFINES
#include <math.h>
#include "counters.h"
#include "event_config.h"
#include "item_data.h"

#ifdef AS_DAEMON
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#endif

std::array<int, 1024> tv = { 0 };
std::array<std::string, 1024> ts;

unsigned const version = 61;

unsigned const max_client_count = 1000;	// < Network::max_client_count
unsigned const connection_count_limit = 100;
namespace
{
	unsigned const itemid_disp = 10000;
}
unsigned const max_gem_count = 8;
unsigned const default_item_daycount = 30;
unsigned long long const day_seconds = 24 * 60 * 60;

std::atomic<long long> max_run_id(0);
std::atomic<unsigned> drop_event(0);
std::atomic<unsigned> pw_room_on_kick(0);
std::atomic<unsigned> daily_cash(0);
std::atomic<float> season_daycount(30);
std::atomic<unsigned long long> current_season_id(0);
std::atomic<float> runtime_treshold(20.0f);
std::atomic<float> min_runtime(80.0f);
std::atomic<float> tangent_treshold(5.0f);
std::atomic<unsigned> spike_score_min(5000);
std::atomic<unsigned> spike_detection_treshold(5000);
std::atomic<unsigned> report_spikes(1);
std::atomic<unsigned> min_spike_ms(350);
std::atomic<unsigned> min_spike_report(2);
std::atomic<unsigned> timed_shutdown(0);
std::atomic<unsigned> shutdown_time(0);
std::atomic<unsigned> sh_tick(0);
std::atomic<unsigned> show_chat(false);
std::atomic<unsigned short> relay_port(0);
volatile unsigned return_gems = 0;

std::map<unsigned, float> global_map_records;
std::map<unsigned, float> seasonal_map_records;

//counters

counter_t<unsigned> db_error_count("db_error_count");
extern sampler_t<unsigned> db_queue_length;
counter_t<unsigned> file_io_error_count("file_io_error_count");
extern sampler_t<unsigned> file_io_length;
counter_t<unsigned> login_count("login_count");
counter_t<unsigned> logout_count("logout_count");
counter_t<unsigned> race_start_count("race_start_count");
counter_t<unsigned> race_end_count("race_end_count");
counter_t<unsigned> solo_race_count("solo_race_count");
counter_t<unsigned> race_mode_count("race_mode_count");
counter_t<unsigned> battle_mode_count("battle_mode_count");
counter_t<unsigned> coin_mode_count("coin_mode_count");
counter_t<unsigned> gem_acquire_count("gem_acquire_count");
sampler_t<unsigned> coins_collected("coins_collected");
counter_t<unsigned> boxes_collected("boxes_collected");
counter_t<unsigned> trick_success("trick_success");
counter_t<unsigned> trick_fail("trick_fail");
sampler_t<int> relay_pings("relay_pings");
counter_t<unsigned> replay_count("replay_count");
sampler_t<unsigned> online_count_s("online_count", nullptr, false);


//---------

std::vector<std::pair<std::string, std::string> > const help_text =
{
	std::pair<std::string, std::string>(".endrace", "Quit the race"),
	std::pair<std::string, std::string>(".haircolor R G B", "Change haircolor"),
	std::pair<std::string, std::string>(".rules", "Show Server Rules"),
	std::pair<std::string, std::string>(".time", "Show Server Time"),
	std::pair<std::string, std::string>(".invite [player name]", "Invites player to room"),
	std::pair<std::string, std::string>(".replay [replay name]", "Replays race"),
	std::pair<std::string, std::string>(".save_replay [replay name]", "Saves last replay"),
	std::pair<std::string, std::string>(".random", "Changes map to random"),
	std::pair<std::string, std::string>(".forcestart", "Forces race start (doesn't ignore ready state)"),
};

std::vector<std::pair<std::string, std::string> > rules_text =
{
	std::pair<std::string, std::string>("Server Rules",""),
	std::pair<std::string, std::string>("1", "No insulting of other players, we all love this game and insults have no place here."),
	std::pair<std::string, std::string>("2", "No cheating or hacking. Yes, this includes changing a single file of your installation no matter what you try to achieve with it."),
	std::pair<std::string, std::string>("3", "No glitch/bug using. We all know that there are maps that have game breaking bugs that cause you to cut most of the track and lead to impossible to reach times without those bugs."),
	std::pair<std::string, std::string>("4", "No spamming. We didn't directly encounter someone spamming yet. Please keep it like that in the future."),
	std::pair<std::string, std::string>("5", "No multi accounting to avoid a ban. Should we encounter you using another account to play the game because you got one account banned we will ban the multi as well."),
};

std::vector<std::pair<std::string, std::string> > const cmd_text =
{
	std::pair<std::string, std::string>("Rank 0 [User]", ""),
	std::pair<std::string, std::string>(".endrace", "Quit the race"),
	std::pair<std::string, std::string>(".Haircolor R G B", "Change haircolor"),
	std::pair<std::string, std::string>(".rules", "Show Server Rules"),
	std::pair<std::string, std::string>(".time", "Show Server Time"),
	std::pair<std::string, std::string>(".help", "Show Help"),
	std::pair<std::string, std::string>(".invite", "Invite Player"),
	std::pair<std::string, std::string>(".replay [replay name]", "Replays race"),
	std::pair<std::string, std::string>(".save_replay [replay name]", "Saves last replay"),
	std::pair<std::string, std::string>(".random", "changes map to random"),
	std::pair<std::string, std::string>(".forcestart", "forces race start (doesn't ignore ready state)"),
	std::pair<std::string, std::string>("Rank 1 [MOD]", ""),
	std::pair<std::string, std::string>(".report", "Report a Player"),
	std::pair<std::string, std::string>(".server / .info", "Show Server Info"),
	std::pair<std::string, std::string>(".notice / .announce", "Server Message"),
	std::pair<std::string, std::string>("Rank 2 [GM]", ""),
	std::pair<std::string, std::string>(".clear_equips", "Clear your Equips"),
	std::pair<std::string, std::string>(".mute", "Mute a Player"),
	std::pair<std::string, std::string>(".unmute", "Unmute a Player"),
	std::pair<std::string, std::string>(".flushchat", "Flush the Chat"),
	std::pair<std::string, std::string>(".gift", "Send a Gift"),
	std::pair<std::string, std::string>(".join", "Join"),
	std::pair<std::string, std::string>(".join_room", "Join Room"),
	std::pair<std::string, std::string>(".reload_records", "Reload the Records"),
	std::pair<std::string, std::string>(".level", "Set Level"),
	std::pair<std::string, std::string>(".exp", "Set Exp"),
	std::pair<std::string, std::string>(".cash", "Set Cash"),
	std::pair<std::string, std::string>(".gem", "Get a Gem"),
	std::pair<std::string, std::string>(".kick", "Kick a Player"),
	std::pair<std::string, std::string>(".ban", "Ban a Player"),
	std::pair<std::string, std::string>("Rank 3 [DEV]", ""),
	std::pair<std::string, std::string>(".exp_mult", "Set Server EXP rate"),
	std::pair<std::string, std::string>(".pro_mult", "Set Server Coin rate"),
};

std::vector<std::vector<unsigned> > const exp_race_single =
{
	std::vector<unsigned>{21},
	std::vector<unsigned>{95, 21},
	std::vector<unsigned>{125, 80, 29},
	std::vector<unsigned>{155, 110, 110, 33},
	std::vector<unsigned>{195, 135, 115, 115, 40},
	std::vector<unsigned>{230, 156, 156, 120, 120, 40},
	std::vector<unsigned>{265, 187, 187, 140, 140, 140, 40},
	std::vector<unsigned>{300, 210, 210, 145, 145, 145, 145, 44},
};

std::vector<std::vector<unsigned> > const exp_race_team =
{
	std::vector<unsigned>{21},
	std::vector<unsigned>{95, 21},
	std::vector<unsigned>{130, 130, 90},
	std::vector<unsigned>{130, 130, 90, 90},
	std::vector<unsigned>{170, 170, 170, 120, 120},
	std::vector<unsigned>{170, 170, 170, 120, 120, 120},
	std::vector<unsigned>{220, 220, 220, 220, 140, 140, 140},
	std::vector<unsigned>{220, 220, 220, 220, 140, 140, 140, 140},
};

std::vector<std::vector<unsigned> > const exp_coin_single =
{
	std::vector<unsigned>{60},
	std::vector<unsigned>{70, 60},
	std::vector<unsigned>{90, 85, 80},
	std::vector<unsigned>{120, 110, 105, 100},
	std::vector<unsigned>{140, 130, 125, 120, 115},
	std::vector<unsigned>{160, 150, 145, 140, 135, 130},
	std::vector<unsigned>{ 180,170,165,160,155,150,145 },
	std::vector<unsigned>{ 200,190,185,180,175,170,165,160 },
};

std::vector<std::vector<unsigned> > const exp_coin_team =
{
	std::vector<unsigned>{ 60 },
	std::vector<unsigned>{ 70, 60 },
	std::vector<unsigned>{ 120, 120, 100 },
	std::vector<unsigned>{ 120, 120, 100, 100 },
	std::vector<unsigned>{ 160, 160, 160, 130, 130 },
	std::vector<unsigned>{ 160, 160, 160, 130, 130, 130 },
	std::vector<unsigned>{ 200,200,200,200,160,160,160 },
	std::vector<unsigned>{ 200,200,200,200,160,160,160,160 },
};

std::vector<std::vector<unsigned> > const gold_race_single =
{
	std::vector<unsigned>{ 70 },
	std::vector<unsigned>{ 70, 70 },
	std::vector<unsigned>{ 92, 92, 92 },
	std::vector<unsigned>{ 122, 122, 122, 122 },
	std::vector<unsigned>{ 142, 142, 142, 142, 142 },
	std::vector<unsigned>{ 164, 164, 164, 164, 164, 164 },
	std::vector<unsigned>{ 186, 186, 186, 186, 186, 186, 186 },
	std::vector<unsigned>{ 200, 200, 200, 200, 200, 200, 200, 200 }
};

std::vector<std::vector<unsigned> > const gold_race_team =
{
	std::vector<unsigned>{ 70 },
	std::vector<unsigned>{ 70, 70 },
	std::vector<unsigned>{ 122, 122, 122 },
	std::vector<unsigned>{ 122, 122, 122, 122 },
	std::vector<unsigned>{ 164, 164, 164, 164, 164 },
	std::vector<unsigned>{ 164, 164, 164, 164, 164, 164 },
	std::vector<unsigned>{ 200, 200, 200, 200, 200, 200, 200 },
	std::vector<unsigned>{ 200, 200, 200, 200, 200, 200, 200, 200 }
};

std::array<std::string, 3> room_kind_str =
{
	"race",
	"battle",
	"coin",
};

std::array<std::array<unsigned, 9>, 9> cash_rewards =
{
	std::array<unsigned, 9>{0,0,0,0,0,0,0,0,0},
	std::array<unsigned, 9>{1,0,0,0,0,0,0,0,0},
	std::array<unsigned, 9>{2,1,0,0,0,0,0,0,0},
	std::array<unsigned, 9>{3,2,1,0,0,0,0,0,0},
	std::array<unsigned, 9>{5,4,3,2,1,0,0,0,0},
	std::array<unsigned, 9>{6,5,4,3,2,1,0,0,0},
	std::array<unsigned, 9>{7,6,5,4,3,2,1,0,0},
	std::array<unsigned, 9>{10,8,7,5,4,3,2,1,0},
	std::array<unsigned, 9>{0,0,0,0,0,0,0,0,0},
};


inline unsigned get_reward_cash(unsigned player_count, unsigned position)
{
	return cash_rewards[std::min<unsigned>(8, player_count - 1)][std::min<unsigned>(8, position)];
}

std::tuple<unsigned, unsigned> get_exp_gold(unsigned player_count, unsigned position, unsigned race_kind, unsigned is_single)
{
	//0 race, 1 battle, 2 coin
	unsigned r = 0;
	unsigned r2 = 0;
	--player_count;
	if (player_count < 8)
	{
		std::vector<std::vector<unsigned> > const * table = nullptr;
		std::vector<std::vector<unsigned> > const * table2 = is_single ? &gold_race_single : &gold_race_team;
		switch (race_kind)
		{
		case 0: //race
		case 1:	//battle
			if (is_single)
				table = &exp_race_single;
			else
				table = &exp_race_team;
			break;
		case 2:	//coin
			if (is_single)
				table = &exp_coin_single;
			else
				table = &exp_coin_team;
			break;
		}


		if (table != nullptr && position < (*table)[player_count].size())
		{
			r = (*table)[player_count][position];
			r2 = (*table2)[player_count][position];
		}
	}
	return std::make_tuple(r, r2);
}



mutex_t mapids_mutex;
std::vector<unsigned> mapids = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
30, 32, 33, 34, 35 };
std::vector<unsigned> mapids_battle = { 10, 0, 3, 7, 11, 8, 4, 5, 2 };
std::vector<unsigned> mapids_coin = { 19, 21, 17, 22, 20, 18, 23 };
//{ "exlude_map_reward", "9, 33, 27, 6" },

std::map<unsigned, std::string> map_id_to_name =
{
	std::pair<unsigned, std::string>(0, "Oblivion 1"),
	std::pair<unsigned, std::string>(1, "Eisen Watercourse 1"),
	std::pair<unsigned, std::string>(2, "Eisen Watercourse 2"),
	std::pair<unsigned, std::string>(3, "Oblivion 2"),
	std::pair<unsigned, std::string>(4, "Chachapoyas 1"),
	std::pair<unsigned, std::string>(5, "Santa Claus 1"),
	std::pair<unsigned, std::string>(6, "Jansen's Forest 1"),
	std::pair<unsigned, std::string>(7, "Smallpox 1"),
	std::pair<unsigned, std::string>(8, "Smallpox 2"),
	std::pair<unsigned, std::string>(9, "Equilibrium 1"),
	std::pair<unsigned, std::string>(10, "Chachapoyas 2"),
	std::pair<unsigned, std::string>(11, "Smallpox 3"),
	std::pair<unsigned, std::string>(12, "Jansen's Forest 2"),
	std::pair<unsigned, std::string>(13, "Eisen Watercourse 3"),
	std::pair<unsigned, std::string>(14, "Oblivion 3"),
	std::pair<unsigned, std::string>(15, "Chagall 1"),
	std::pair<unsigned, std::string>(16, "Chagall 2"),
	std::pair<unsigned, std::string>(17, "Smallpox 1 Mirror"),
	std::pair<unsigned, std::string>(18, "Santa Claus 1 Mirror"),
	std::pair<unsigned, std::string>(19, "Chagall 1 Mirror"),
	std::pair<unsigned, std::string>(20, "Chachapoyas 1 Mirror"),
	std::pair<unsigned, std::string>(21, "Oblivion 2 Mirror"),
	std::pair<unsigned, std::string>(22, "Smallpox 3 Mirror"),
	std::pair<unsigned, std::string>(23, "Smallpox 2 Mirror"),
	std::pair<unsigned, std::string>(24, "Chachapoyas 2 Mirror"),
	std::pair<unsigned, std::string>(25, "Chagall 2 Mirror"),
	std::pair<unsigned, std::string>(26, "Equilibrium 1 Mirror"),
	std::pair<unsigned, std::string>(27, "Jansen's Forest 1 Mirror"),
	std::pair<unsigned, std::string>(28, "Jansen's Forest 2 Mirror"),
	std::pair<unsigned, std::string>(29, "Eisen Watercourse 1 Mirror"),
	std::pair<unsigned, std::string>(30, "Eisen Watercourse 2 Mirror"),

	std::pair<unsigned, std::string>(32, "Chachapoyas 3"),
	std::pair<unsigned, std::string>(33, "Sand Madness"),
	std::pair<unsigned, std::string>(34, "Shangri - La"),
	std::pair<unsigned, std::string>(35, "Giant Ruin"),
};

enum team : int { none = 0, red = 1, blue = 2 };
std::array<char, 141> recipient_offline_text;;


std::map<std::string, unsigned> shop_kinds = {
	std::pair<std::string, unsigned>{"accessory", 2},
	std::pair<std::string, unsigned>{"department", 3},
	std::pair<std::string, unsigned>{"deck", 4},
	std::pair<std::string, unsigned>{"discount", 5},
	std::pair<std::string, unsigned>{"etc", 0},
	std::pair<std::string, unsigned>{"default", 0},
	std::pair<std::string, unsigned>{"CashShop", 1}
};

std::map<std::string, unsigned> avatar_lookup = {
	std::pair<std::string, unsigned>{"jack", 2},
	std::pair<std::string, unsigned>{"sun", 3},
	std::pair<std::string, unsigned>{"ed", 4},
	std::pair<std::string, unsigned>{"rose", 5},
};

std::map<unsigned, std::string> avatar_names =
{
	std::pair<unsigned, std::string>{2, "Max"},
	std::pair<unsigned, std::string>{3, "Beth"},
	std::pair<unsigned, std::string>{4, "Ross"},
	std::pair<unsigned, std::string>{5, "Rose"},

};

std::map<std::string, unsigned> item_ranks = {
	std::pair<std::string, unsigned>{"tier 1", 1},
	std::pair<std::string, unsigned>{"tier 2", 2},
	std::pair<std::string, unsigned>{"tier 3", 3},
	std::pair<std::string, unsigned>{"tier 4", 4},
};

//enum class parts_t : unsigned {/*cr = 0, hr = 2, */fc = 3, ub = 4, lb = 5, gv = 6, sh = 7, dk = 8 };


std::array<std::string, 7> gem_kinds = { "", "speed", "booster", "turn", "jump", "trick", "luck" };

std::map<std::string, unsigned> part_kinds = {
	std::pair<std::string, unsigned>{"cr", 0},
	std::pair<std::string, unsigned>{"bp", 2},
	std::pair<std::string, unsigned>{"fc", 1},
	std::pair<std::string, unsigned>{"hd", 3},
	std::pair<std::string, unsigned>{"mk", 9},
	std::pair<std::string, unsigned>{"hm", 3},
	std::pair<std::string, unsigned>{"gg", 11},
	std::pair<std::string, unsigned>{"er", 12},
	std::pair<std::string, unsigned>{"ub", 4},
	std::pair<std::string, unsigned>{"ubh", 4},
	std::pair<std::string, unsigned>{"lb", 5},
	std::pair<std::string, unsigned>{"sk", 5},
	std::pair<std::string, unsigned>{"gv", 6},
	std::pair<std::string, unsigned>{"sh", 7},
	std::pair<std::string, unsigned>{"dk", 8},
};

std::array<unsigned, 24> gem_levels =
{
	0, 0, 0, 0, 0, 0,
	6, 6, 6, 11, 11, 11,
	16, 16, 16, 21, 21, 21,
	26, 26, 26, 31, 31, 31
};

std::array<float, 24> gem_bonuses =
{
	0, 0, 0, 3.8f, 3.8f, 4.0f,
	6.7f, 6.7f, 7.0f, 10.4f, 10.4f, 11.0f,
	12.9f, 13.9f, 15.0f, 16.3f, 17.7f, 19.0f,
	19.8f, 21.4f, 23.0f, 24.1f, 26.0f, 28.0f
};

std::array<unsigned, 8> gem_drop_chances = { 40, 35, 30, 25, 20, 15, 10, 5 };



unsigned short gen_gem(unsigned pos, unsigned count, unsigned limit_level)
{
	unsigned r = 0;
	unsigned chance = gem_drop_chances[gem_drop_chances.size() - count + pos] + count * 2;
	unsigned value = rng::gen_uint(0, 100);
	if (value < chance)
	{
		float chance = 0.4f;
		value = rng::gen_uint(0, 100);

		unsigned kind = 6;

		if (limit_level >= 31)
			kind = 21;
		else if (limit_level >= 26)
			kind = 18;
		else if (limit_level >= 21)
			kind = 15;
		else if (limit_level >= 16)
			kind = 12;
		else if (limit_level >= 11)
			kind = 9;
		else
			kind = 6;

		for (unsigned _i = 0; _i < 3; ++_i)
		{
			assert(kind < gem_levels.size());
			if (kind + 1 >= gem_levels.size())
				break;
			if (gem_levels[kind + 1] > limit_level)
				break;

			if (value >= chance * 100.0f)
				break;
			++kind;
			chance *= 0.4f;
		}

		//unsigned gem_kind = (client.gems[gemslot].first - 1) / 24 + 1;
		value = rng::gen_uint(0, 5);
		r = value * 24 + kind + 1;
	}

	return r;
}

inline unsigned get_gem_kind(unsigned short gem)
{
	return (gem - 1) / 24;
}

inline float get_gem_bonus(unsigned short gem)
{
	return gem_bonuses[(gem - 1) % 24];
}

std::string float2time(float t)
{
	int minutes = t / 60.0f;
	int seconds = (int)fabs(t) % 60;
	return std::to_string(minutes) + (seconds < 10 ? ":0" : ":") + std::to_string(seconds);
}

inline bool valid_gem(unsigned id)
{
	return id >= 1 && id <= 6 * 24;
}

std::array<unsigned, 3> cube_prices = { 400, 800, 4000 };

std::vector<item_list_data_t> itemlist;

unsigned const room_id_disp = 200000;
unsigned const client_check_interval = 60000;
unsigned const max_client_idle_time = 300000;

float exp_mult = 1.0f;
float pro_mult = 1.0f;
//bool use_ip_as_id = false;

volatile unsigned use_launcher = 0;
volatile unsigned check_version_string = 1;
mutex_t version_string_mutex;
std::string client_version_string = "test_string";
std::string html_dir;

std::atomic<unsigned> game_ip(0);
std::atomic<unsigned> race_result_timeout(8000);
unsigned game_port = 0;

std::atomic<unsigned> enable_all_tricks(1);

unsigned game_ip_update_interval = 5;
unsigned private_port_to_public = 0;
unsigned null_ips = 0;
unsigned rs_mode = 0;
unsigned disable_relay = 0;
unsigned disable_relay_2 = 0;
unsigned log_relay = 0;

volatile unsigned log_inpacket_ids = 0;
volatile unsigned log_inpackets = 0;
//volatile unsigned log_outpacket_ids = 0;
//volatile unsigned log_outpackets = 0;

std::atomic<unsigned> online_count(0);

//1
unsigned hourly_online_count = 0;
bool hourly_online_count_is_set = false;
std::list<std::pair<time_t, unsigned> > online_count_list;

struct connection_count_t
{
	unsigned count = 0;
	bool inc(unsigned treshold = 0)
	{
		if (count >= connection_count_limit - treshold)
			return false;
		++count;
		return true;
	}

	bool dec()
	{
		if (count > 0)
			--count;

		return count > 0;
	}
};

mutex_t chatban_mutex;
std::unordered_map<std::string, time_t> chatban;
FILE * fchat = nullptr;


mutex_t connection_count_mutex;
std::map<unsigned, connection_count_t> connection_count;

std::unique_ptr<class SinkPacketHandler> sinkPacketHandler;
std::unique_ptr<class CtpPacketHandler> ctpPacketHandlerSend;
std::unique_ptr<class CtpPacketHandler> ctpPacketHandlerRecv;
std::unique_ptr<class AuthPacketHandler> authPacketHandler;
std::unique_ptr<class GamePacketHandler> gamePacketHandler;
std::unique_ptr<class RelayPacketHandler> relayPacketHandler;
std::unique_ptr<class TrickPacketHandler> trickPacketHandler;
std::unique_ptr<class ShopPacketHandler> shopPacketHandler;
std::unique_ptr<class GuildPacketHandler> guildPacketHandler;
std::unique_ptr<class MessengerPacketHandler> messengerPacketHandler;
std::unique_ptr<class RoomPacketHandler> roomPacketHandler;

typedef std::tuple<unsigned, std::string, std::string, unsigned long long, std::string, std::string, std::string, std::string, std::string, unsigned long long> account_tuple;
typedef std::tuple<unsigned, unsigned, std::string, std::string, unsigned> bulletin_tuple;
typedef std::tuple<unsigned, std::string, unsigned, unsigned long long, int, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, int, int, int, int, unsigned, unsigned, unsigned, unsigned, unsigned long long, long long, unsigned long long, int> character_tuple;
typedef std::tuple<unsigned, std::string, std::string, std::string, std::string, unsigned, unsigned> guilds_tuple;
typedef std::tuple<unsigned, unsigned, unsigned, float, unsigned, unsigned, unsigned> licenses_tuple;
typedef std::tuple<unsigned, unsigned, unsigned, std::string, std::string, std::string, std::string, unsigned long long> messages_tuple;
typedef std::tuple<unsigned, unsigned, float, unsigned, float, int, int, int, int> records_tuple;
typedef std::tuple<unsigned long long, unsigned, unsigned, float, unsigned, float, int, int, int, int> season_records_tuple;
typedef std::tuple<unsigned long long, unsigned, unsigned, unsigned long long, std::string, std::string, long long, unsigned> gifts_tuple;
typedef std::tuple<unsigned long long, unsigned, unsigned, std::string, std::string, std::string, long long, unsigned> gifts_tuple2;
typedef std::tuple<unsigned long long, unsigned, unsigned, int, unsigned long long, short, short, short, short, short, short, short, short, long long, unsigned> fashion_tuple;

const std::array<unsigned, 65> exp_list =
{
	800,
	2000,
	4000,
	6800,
	10400,
	16400,
	24400,
	34400,
	46400,
	62400,
	82400,
	107400,
	138400,
	176400,
	222400,
	278400,
	345400,
	425400,
	519400,
	629400,
	751400,
	893400,
	1057400,
	1245400,
	1461400,
	1707400,
	1987400,
	2303400,
	2659400,
	3057400,
	3495400,
	3975400,
	4501400,
	5077400,
	5707400,
	6395400,
	7147400,
	7965400,
	8857400,
	9825400,
	10851400,
	11901400,
	12973400,
	14069400,
	15187400,
	16327400,
	17491400,
	18679400,
	19889400,
	21129400,
	22399400,
	23699400,
	25029400,
	26389400,
	27779400,
	29199400,
	30649400,
	32129400,
	33639400,
	35179400,
	36749400,
	38349400,
	39979400,
	41639400,
	43329400,
};

unsigned long long get_level_exp(unsigned level)
{
	unsigned long long r = 0;
	if (level >= 1 && level - 1 < exp_list.size())
		r = exp_list[level - 1];
	return r;
}

const std::map<unsigned, std::pair<unsigned, unsigned> > levelup_msg_data =
{
	std::pair<unsigned, std::pair<unsigned, unsigned> >{2, std::pair<unsigned, unsigned>{2, 2}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{3, std::pair<unsigned, unsigned>{4, 4}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{4, std::pair<unsigned, unsigned>{3, 5}},

	std::pair<unsigned, std::pair<unsigned, unsigned> >{5, std::pair<unsigned, unsigned>{3, 5}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{6, std::pair<unsigned, unsigned>{5, 8}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{7, std::pair<unsigned, unsigned>{3, 8}},

	std::pair<unsigned, std::pair<unsigned, unsigned> >{8, std::pair<unsigned, unsigned>{1, 4}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{9, std::pair<unsigned, unsigned>{1, 4}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{10, std::pair<unsigned, unsigned>{1, 2}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{11, std::pair<unsigned, unsigned>{0, 1}},

	std::pair<unsigned, std::pair<unsigned, unsigned> >{12, std::pair<unsigned, unsigned>{1, 3}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{13, std::pair<unsigned, unsigned>{1, 3}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{14, std::pair<unsigned, unsigned>{1, 2}},

	std::pair<unsigned, std::pair<unsigned, unsigned> >{15, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{16, std::pair<unsigned, unsigned>{0, 2}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{17, std::pair<unsigned, unsigned>{0, 1}},

	std::pair<unsigned, std::pair<unsigned, unsigned> >{18, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{19, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{20, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{21, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{22, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{23, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{24, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{25, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{26, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{27, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{28, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{29, std::pair<unsigned, unsigned>{0, 1}},
	std::pair<unsigned, std::pair<unsigned, unsigned> >{30, std::pair<unsigned, unsigned>{0, 1}},
};

std::pair<unsigned, unsigned> get_levelup_msg_data(unsigned level)
{
	std::pair<unsigned, unsigned> r(0, 0);
	auto i = levelup_msg_data.find(level);
	if (i != levelup_msg_data.end())
		r = i->second;
	return r;
}

const std::vector<unsigned> quest_clear_exp = {
	0, 0, 0, 0, 0, 0, 0, 1, 0, 1,
	0, 1, 0, 2, 0, 1, 0, 2, 0, 1,
	0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
	0, 1, 1, 0, 1, 1, 0, 1, 1, 0,
	1, 1, 0, 1, 2, 3, 0, 1, 2, 3,
	0, 1, 2, 3, 0, 2, 2, 4, 5, 0,
	2, 2, 4, 5, 0, 2, 2, 4, 5, 0,
	1, 0, 1,
};


boost::asio::ip::udp::endpoint to_udp(unsigned ip, unsigned short port)
{
	boost::asio::ip::udp::endpoint e(boost::asio::ip::address_v4(ip), port);
	//	boost::asio::ip::address remote_ad = e.address();
	//	logger << "Recv Address: " << remote_ad.to_string() << " port: " << e.port() << "\r\n";
	return e;
}

std::string runtime2str(float runtime);


struct replay_item_data_t
{
	unsigned id;
	unsigned color;
	std::vector<unsigned short> gems;

	void operator >>(BinaryReader & br)
	{
		br >> id >> color >> gems;
	}
	void operator <<(BinaryWriter & bw) const
	{
		bw << id << color << gems;
	}

	bool valid() const
	{
		return id > itemid_disp && id - 1 - itemid_disp < itemlist.size() && itemlist[id - 1 - itemid_disp].valid;
	}

	item_list_data_t const & data() const
	{
		assert(valid());
		if (valid())
			return itemlist[id - 1 - itemid_disp];
		throw std::runtime_error("replay_item_data_t::data() call on invalid item");
	}

};

struct replay_data_t
	: public std::enable_shared_from_this<replay_data_t>
{
	std::string name;
	std::string guild_name;
	std::array<float, 8> stats;
	unsigned haircolor;
	unsigned avatar;
	unsigned level;
	unsigned room_slot;
	unsigned room_kind;
	unsigned mapid;
	unsigned is_single;
	unsigned server_start_tick;
	unsigned start_tick;
	std::vector<replay_item_data_t> item_data;
	BinaryWriter packets;

	void operator >>(BinaryReader & br)
	{
		br >> name >> guild_name;
		br.read(stats, haircolor, avatar, level, room_slot, room_kind, mapid, is_single, server_start_tick, start_tick, item_data, packets.data);
	}
	void operator <<(BinaryWriter & bw) const
	{
		bw << name << guild_name;
		bw.write(stats, haircolor, avatar, level, room_slot, room_kind, mapid, is_single, server_start_tick, start_tick, item_data, packets.data);
	}
};

struct relay_run_stats_t
{
	float avg, avg_s, avg_e;
	float diff_max;
	float tangent;
};

struct relay_run_data_t
{
	int coin_count = 0;
	unsigned trick_points = 0;
	unsigned trick_success = 0;
	unsigned trick_fail = 0;
	bool pos_error = false;
	bool time_error = false;
	bool arrived = false;
	bool coin_warning = false;
	std::shared_ptr<replay_data_t> replay_data;
	std::vector<std::pair<unsigned, float> > pings;
	std::vector<std::pair<unsigned, unsigned> > trick_scores;
	std::vector<std::pair<unsigned, unsigned short> > trick_motions;
	relay_run_data_t() = default;
	relay_run_data_t(relay_run_data_t const &) = default;
	relay_run_data_t(relay_run_data_t &&) = default;
	relay_run_data_t& operator=(relay_run_data_t const &) = default;
	relay_run_data_t& operator=(relay_run_data_t &&) = default;

	std::string to_csv(relay_run_stats_t const & rs, unsigned race_start_time)
	{
		std::ostringstream oss;
		unsigned j = 0;
		unsigned points = 0;
		float w, f;
		oss << "Avarage: ;" << rs.avg << "\r\nSpike cutoff: ;" << rs.diff_max << "\r\nTangent: ;" << rs.tangent << "\r\n";
		oss << "Time(seconds);Ping(ms);Fitted avg(ms);Trick points\r\n";
		for (unsigned i = 0; i < pings.size(); ++i)
		{
			if (j < trick_scores.size() && trick_scores[j].first <= pings[i].first)
			{
				points = trick_scores[j].second;
				++j;
			}
			w = (float)i / pings.size();
			f = (rs.avg_e - rs.avg_s) * w + rs.avg_s;


			oss << ((int)(pings[i].first - race_start_time) / 1000.0f - 8.0f) << ';' << pings[i].second << ';' << f << ';' << points << "\r\n";
		}

		return oss.str();
	}

	std::string to_html(relay_run_stats_t const & rs, unsigned race_start_time, std::string const & message)
	{
		std::ostringstream oss;
		unsigned j;
		unsigned points = 0;
		float w, f;

		//--

		oss << "<html><head>\r\n";
		oss << "<script type = \"text/javascript\" src = \"https://www.gstatic.com/charts/loader.js\"></script>\r\n";
		oss << "<script type = \"text/javascript\">\r\n";
		oss << "google.charts.load('current', { 'packages':['corechart'] });\r\n";
		oss << "google.charts.setOnLoadCallback(drawChart);\r\n\r\n";

		oss << "function drawChart() {\r\n";
		oss << "var data = google.visualization.arrayToDataTable([\r\n";
		oss << "['Time(seconds)', 'Ping(ms)', 'Fitted avg(ms)'],\r\n";

		for (unsigned i = 0; i < pings.size(); ++i)
		{
			w = (float)i / pings.size();
			f = (rs.avg_e - rs.avg_s) * w + rs.avg_s;

			oss << '[' << ((int)(pings[i].first - race_start_time) / 1000.0f - 8.0f) << ',' << pings[i].second << ',' << f << ']';
			if (i == pings.size() - 1)
				oss << "\r\n";
			else
				oss << ",\r\n";
		}

		oss << "]);\r\n";

		oss << "var data2 = google.visualization.arrayToDataTable([\r\n";
		oss << "['Time(seconds)', 'Trick points'],\r\n";
		j = 0;
		for (unsigned i = 0; i < pings.size(); ++i)
		{
			if (j < trick_scores.size() && trick_scores[j].first <= pings[i].first)
			{
				points = trick_scores[j].second;
				++j;
			}

			oss << '[' << ((int)(pings[i].first - race_start_time) / 1000.0f - 8.0f) << ',' << points << ']';
			if (i == pings.size() - 1)
				oss << "\r\n";
			else
				oss << ",\r\n";
		}
		oss << "]);\r\n";



		oss << "var options = {\r\n";
		oss << "title: 'Ping',\r\n";
		oss << "curveType : 'function',\r\n";
		oss << "legend : { position: 'bottom' }\r\n";
		oss << "};\r\n";

		oss << "var options2 = {\r\n";
		oss << "title: 'Trick score',\r\n";
		oss << "curveType : 'function',\r\n";
		oss << "legend : { position: 'bottom' }\r\n";
		oss << "};\r\n";

		oss << "var chart = new google.visualization.LineChart(document.getElementById('curve_chart'));\r\n";
		oss << "var chart2 = new google.visualization.LineChart(document.getElementById('curve_chart2'));\r\n";

		oss << "chart.draw(data, options);\r\n";
		oss << "chart2.draw(data2, options2);\r\n";
		oss << "}\r\n";
		oss << "function eval_func() { eval(document.getElementById(\"tex1\").value); }";
		oss << "</script></head><body>\r\n";


		oss << message;

		oss << "Avarage: " << rs.avg << "ms<BR>\r\nAvg_s: " << rs.avg_s << "ms<BR>\r\nAvg_e: " << rs.avg_e << "ms<BR>\r\nDiff: " << rs.diff_max << "ms<BR>\r\n";

		if (pings.size() > 0)
		{
			oss << "Spikes with trick scores:<BR>\r\n";
			unsigned r = 0;
			unsigned j = 0;
			unsigned k = 0;
			unsigned short current_trick = 0;
			float w, f;
			unsigned const spike_detection_treshold = ::spike_detection_treshold;
			unsigned last_spike = pings[0].first - (spike_detection_treshold + 1);
			float spike_treshold = std::max<float>(rs.diff_max, min_spike_ms);
			bool in_spike = false;
			unsigned current_trick_score = 0, acquired_score;
			bool trick_ended;
			bool trick_ended_upon_spike_enter = false;
			for (unsigned i = 0; i < pings.size(); ++i)
			{
				trick_ended = false;
				if (k < trick_motions.size() && trick_motions[k].first <= pings[i].first)
				{
					if (current_trick != 0 && trick_motions[k].second == 0)
						trick_ended = true;

					current_trick = trick_motions[k].second;
					++k;
				}

				w = (float)i / pings.size();
				f = (rs.avg_e - rs.avg_s) * w + rs.avg_s;

				if (fabs(pings[i].second - f) > spike_treshold)	//spike
				{
					last_spike = pings[i].first;
					if (!in_spike)
					{
						in_spike = true;
						trick_ended_upon_spike_enter = trick_ended;
					}
				}
				else
				{
					if (in_spike)
					{
						in_spike = false;
					}
				}

				if (j < trick_scores.size() && trick_scores[j].first <= pings[i].first)
				{
					acquired_score = trick_scores[j].second - current_trick_score;
					if (((int)trick_scores[j].first - (int)last_spike) < (int)spike_detection_treshold && trick_ended_upon_spike_enter && acquired_score > spike_score_min)
					{
						float f = ((int)(trick_scores[j].first - race_start_time) / 1000.0f - 8.0f);
						oss << f << " (" << runtime2str(f) << ")<BR>\r\n";
						++r;
					}
					current_trick_score = trick_scores[j].second;
					++j;
				}
			}
		}
		oss << "<textarea id = \"tex1\">document.getElementById(\"curve_chart\").style = \"width: 1500px; height: 800px;\";\r\ndrawChart();\r\n</textarea><button type=\"button\" onclick=\"eval_func()\">eval</button>";
		oss << "<div id = \"curve_chart\" style = \"width: 1800px; height: 800px\"></div>\r\n";
		oss << "<div id = \"curve_chart2\" style = \"width: 1800px; height: 800px\"></div>\r\n";
		oss << "</body></html>";


		return oss.str();
	}

	unsigned calc_tangent(relay_run_stats_t & rs)
	{
		unsigned r = 0;
		float f_min = 0, f_max = 0;
		unsigned count;
		rs.avg = 0;
		rs.avg_s = 0;
		rs.avg_e = 0;
		rs.tangent = 0;


		if (pings.size() > 0)
		{
			for (unsigned i = 0; i < pings.size(); ++i)
				rs.avg += pings[i].second;
			rs.avg /= pings.size();

			f_min = pings[0].second;
			f_max = pings[0].second;
			for (unsigned i = 1; i < pings.size(); ++i)
			{
				if (f_min > pings[i].second)
					f_min = pings[i].second;
				if (f_max < pings[i].second)
					f_max = pings[i].second;
			}

			rs.diff_max = rs.avg - f_min;

			count = 0;
			for (unsigned i = 0; i < pings.size(); ++i)
			{
				if (fabs(pings[i].second - rs.avg) <= rs.diff_max)
				{
					++count;
					float w = 1.0f - (float)i / pings.size();
					rs.avg_s += (pings[i].second - rs.avg) * w;
				}
			}
			if (count > 0)
				rs.avg_s /= count;
			rs.avg_s += rs.avg;

			count = 0;
			for (unsigned i = 0; i < pings.size(); ++i)
			{
				if (fabs(pings[i].second - rs.avg) <= rs.diff_max)
				{
					++count;
					float w = (float)i / pings.size();
					rs.avg_e += (pings[i].second - rs.avg) * w;
				}
			}
			if (count > 0)
				rs.avg_e /= count;
			rs.avg_e += rs.avg;


			float vy = (pings.back().first - pings[0].first) / 1000.0f;
			float vx = rs.avg_e - rs.avg_s;
			float len = sqrtf(vx * vx + vy * vy);
			if (len > 0 && vy > 0)
			{
				vx /= len;
				vy /= len;
				if(vx > 0)
					rs.tangent = std::atan2(vx, vy) * 180.0f / M_PI;
			}
		}

		if(pings.size() > 0)
		{
			unsigned j = 0;
			unsigned k = 0;
			unsigned short current_trick = 0;
			float w, f;
			unsigned const spike_detection_treshold = ::spike_detection_treshold;
			unsigned last_spike = pings[0].first - (spike_detection_treshold + 1);
			float spike_treshold = std::max<float>(rs.diff_max, min_spike_ms);
			bool in_spike = false;
			unsigned current_trick_score = 0, acquired_score;
			bool trick_ended;
			bool trick_ended_upon_spike_enter = false;
			for (unsigned i = 0; i < pings.size(); ++i)
			{
				trick_ended = false;
				if (k < trick_motions.size() && trick_motions[k].first <= pings[i].first)
				{
					if (current_trick != 0 && trick_motions[k].second == 0)
						trick_ended = true;

					current_trick = trick_motions[k].second;
					++k;
				}

				w = (float)i / pings.size();
				f = (rs.avg_e - rs.avg_s) * w + rs.avg_s;

				if (fabs(pings[i].second - f) > spike_treshold)	//spike
				{
					last_spike = pings[i].first;
					if (!in_spike)
					{
						in_spike = true;
						trick_ended_upon_spike_enter = trick_ended;
					}
				}
				else
				{
					if (in_spike)
					{
						in_spike = false;
					}
				}

				if (j < trick_scores.size() && trick_scores[j].first <= pings[i].first)
				{
					acquired_score = trick_scores[j].second - current_trick_score;
					if (((int)trick_scores[j].first - (int)last_spike) < (int)spike_detection_treshold && trick_ended_upon_spike_enter && acquired_score > spike_score_min)
					{
						++r;
					}
					current_trick_score = trick_scores[j].second;
					++j;
				}
			}
		}

		return r;
	}
};


void announce(std::string const & str);
void announce_cafe(std::string const & str);
void announce_guild(std::string const & str);
void announce_whisper(std::string const & str);
void send_whisper(unsigned dbid, std::string const & name, std::string const & msg);
void send_message(std::string const & name, unsigned msgid, std::string const & str1 = "", std::string const & str2 = "", std::string const & str3 = "", std::string const & str4 = "");
void send_message2(std::string const & name, std::string const & str);
void send_message(unsigned dbid, unsigned msgid, std::string const & str1 = "", std::string const & str2 = "", std::string const & str3 = "", std::string const & str4 = "");
void send_message2(unsigned dbid, std::string const & str);
void do_command(std::string const & client_name, const char * msg, unsigned const e);
void report(std::string const & client_name, std::string const & target, std::string const & reason, unsigned send_gms = 2, std::shared_ptr<replay_data_t> rd = nullptr, std::shared_ptr<std::string> csv = nullptr, std::shared_ptr<std::string> html = nullptr, unsigned playerid = 0, int mapid = -1);
void announce_packet(unsigned id, std::string const & str);
bool kick(std::string const & name);
void message(unsigned clientid, const std::string & str);
void message(std::string const & name, std::string const & str);
unsigned get_friend_count(std::string const & name);
unsigned get_friend_count(unsigned id);
void add_guild_points(unsigned guildid, int add_points, unsigned dbid);
std::string get_guild_name(unsigned guildid);
bool add_runtime(unsigned clientid, unsigned mapid, float runtime, unsigned trickscore, float client_time, float server_time, unsigned runkind, std::shared_ptr<replay_data_t> rd = nullptr, std::shared_ptr<std::string> csv = nullptr, std::shared_ptr<std::string> html = nullptr);
bool check_client_in_trick_server(unsigned clientid);

std::string runtime2str(float runtime)
{
	char time_str[128];
	char time_str2[128];
	sprintf(time_str, "%02d:%02d", (int)runtime / 60, (int)runtime % 60);
	sprintf(time_str2, "%.3f", runtime - (int)runtime);
	return (std::string)time_str + &time_str2[1];
}

//FILE * records_file = nullptr;
//std::string records_filename;

std::array<std::string, 4> const record_color_text =
{
	"<Format,2,Color,0,0,255,0>",
	"<Format,2,Color,0,255,0,0>",
	"<Format,2,Color,0,255,255,0>",
	"<Format,2,Color,0,255,165,0>",
};

std::array<std::string, 4> const record_kind_text =
{
	"personal",
	"global",
	"seasonal",
	"seasonal personal",
};

std::string const & get_map_name(unsigned mapid)
{
	impl_lock_guard(l, mapids_mutex);
	return map_id_to_name[mapid];
}

void post_record(std::string const & player_name, unsigned playerid, unsigned mapid, float runtime, unsigned record_kind, float client_time, float server_time, unsigned reward_runkind, std::shared_ptr<replay_data_t> rd = nullptr, std::shared_ptr<std::string> csv = nullptr, std::shared_ptr<std::string> html = nullptr)
{
	if (reward_runkind == 2)
		return;
	assert(record_kind < 4);
	std::string runtime_str = runtime2str(runtime);
	std::string const & map_name = get_map_name(mapid);
	std::string message = player_name + " has a new " + record_kind_text[record_kind] + " record on " + map_name + " - " + runtime_str;

	//	announce_cafe(record_color_text[record_kind] + message);
	announce_whisper(record_color_text[record_kind] + message);

	if (record_kind == 1)	//global record
	{
		std::string message2 = message + " client time: " + runtime2str(client_time) + " server time: " + runtime2str(server_time) + ' ';
		switch (reward_runkind)
		{
		case 0:
			message2 += "solo run";
			break;
		case 1:
			message2 += "multiplayer run";
			break;
		case 2:
			message2 += "not crossed finish line";
			break;
		default:
			message2 += "[not implemented]";
		}
		report("System", player_name, message2, 2, rd, csv, html, playerid, mapid);
	}
	else if (record_kind == 2)	//seasonal record
	{
		std::string message2 = message + " (seasonal), client time: " + runtime2str(client_time) + " server time: " + runtime2str(server_time) + ' ';
		switch (reward_runkind)
		{
		case 0:
			message2 += "solo run";
			break;
		case 1:
			message2 += "multiplayer run";
			break;
		case 2:
			message2 += "not crossed finish line";
			break;
		default:
			message2 += "[not implemented]";
		}
		report("System", player_name, message2, 0, rd, csv, html, playerid, mapid);
	}

//	if (records_file)
//	{
//		fprintf(records_file, "%s;%s;%s;%u;%llu\r\n",
//			player_name.c_str(), map_name.c_str(), runtime_str.c_str(), record_kind, (unsigned long long)time(0));
//		fflush(records_file);
//	}

	if (record_kind == 1)
	{
		query<std::tuple<unsigned> >("SELECT id FROM global_ranking ORDER BY rank ASC LIMIT 100",
			[message](std::vector<std::tuple<unsigned> > const & data, bool success)
		{
			if (success && data.size() > 0)
			{
				for (unsigned i = 0; i < data.size(); ++i)
					send_message2(std::get<0>(data[i]), message);
			}

		});
	}
}


void log_chat(std::string const & name, std::string const & target, std::string const & text)
{
	time_t t = time(0);
	impl_lock_guard(lock, chatban_mutex);

	if (fchat == nullptr)
		fchat = fopen("chat.log", "a+b");
	if (fchat)
	{
		std::string date_str = datetime2str(t);
		fprintf(fchat, "%s [%s]->[%s] %s\r\n", date_str.c_str(), name.c_str(), target.c_str(), text.c_str());
	}

}

void flush_chatlog()
{
	impl_lock_guard(lock, chatban_mutex);
	if (fchat)
		fflush(fchat);
}

bool test_chatban(std::string const & name, std::string const & text)
{
	time_t t = time(0);
	impl_lock_guard(lock, chatban_mutex);
	auto i = chatban.find(name);
	if (i != chatban.end())
	{
		if (t < i->second)
			return true;
		chatban.erase(i);
	}

	if (fchat == nullptr)
		fchat = fopen("chat.log", "a+b");
	if (fchat)
	{
		std::string date_str = datetime2str(t);
		fprintf(fchat, "%s [%s] %s\r\n", date_str.c_str(), name.c_str(), text.c_str());
	}


	return false;
}


void add_chatban(std::string const & name, unsigned minutes);
bool remove_chatban(std::string const & name);



#ifdef _DEBUG
#define d_dump(a, b, c) /*Network::dump((a), (b), (c))*/
#else
#define d_dump(a, b, c)
#endif
/*
std::vector<unsigned short> str2short_array(std::string const & str)
{
std::vector<unsigned short> r;
r.reserve(str.size());
for (char ch : str)
r.push_back((unsigned char)ch);
return r;
}
*/

class SinkPacketHandler
	: public Network::NetworkEventHandler
{
	virtual DisconnectKind onConnect(Network::session_id_t session_id, std::string && ip, unsigned port)
	{
		disconnect(session_id);
		return DisconnectKind::IMMEDIATE_DC;
	}
	virtual void onRecieve(Network::session_id_t session_id, const char * data, unsigned length)
	{
	}

	virtual void onDisconnect(Network::session_id_t session_id) {}
	virtual void onInit() {}
public:
	virtual ~SinkPacketHandler() {}
};

class CtpPacketHandler
	: public Network::UdpNetworkEventHandler
{
	unsigned port;
	virtual void onRecieveDatagram(const char * data, unsigned len, boost::asio::ip::udp::endpoint const & endpoint)
	{
		d_dump(data, len, "CTP");
		/*
		if (len == 1)
		{
		if (*data == 'R')
		{
		other->sendDatagramTo(endpoint, Network::datagram((short)0, (short)1, (short)2, (char)0, (short)8, (short)0, (char)0, (short)0x8c75, (char)0, (short)4, (short)1, (short)2));
		sendDatagramTo(endpoint, Network::datagram((short)0, (short)1, (short)2, (char)0, (short)8, (short)0, (char)0, (short)0x8c75, (char)0, (short)4, (short)1, (short)2));
		logger << "Sent CTP test";
		}
		}
		*/
	}
	virtual void onInit()
	{
		default_color = LIGHT_RED;
	}
public:
	CtpPacketHandler * other = nullptr;
	CtpPacketHandler(unsigned port)
		: port(port)
	{}
	virtual ~CtpPacketHandler() {}
	unsigned get_port() const
	{
		return port;
	}
};

class AuthPacketHandler
	: public Network::NetworkEventHandler
{
	virtual std::string getConnectionData(Network::session_id_t id) const
	{
		impl_lock_guard(lock, mutex);
		auto i = connections.find(id);
		if (i != connections.end())
			return i->second.name;
		return std::string();
	}
	virtual DisconnectKind onConnect(Network::session_id_t session_id, std::string && ip, unsigned port)
	{
		if (online_count >= max_client_count)
		{
			sendTo(session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("Server is full")));
			return DisconnectKind::SOFT_DC;
		}

		unsigned dwip = Network::string2ipv4(ip);
		{
			impl_lock_guard(lock, connection_count_mutex);
			if (!connection_count[dwip].inc(8))
			{
				sendTo(session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("Connections per ip limit")));
				return DisconnectKind::SOFT_DC;
			}
		}

		impl_lock_guard(lock, mutex);
		connection_t & client = connections[session_id];
		client.session_id = session_id;
		client.ip = std::move(ip);
		client.port = port;
		client.dwip = dwip;
		client.ptime = get_tick();

		return DisconnectKind::ACCEPT;
	}
	virtual void onRecieve(Network::session_id_t session_id, const char * data, unsigned length)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return;
		connection_t & client = _i->second;
		client.check_valid();

		BinaryReader br(data, length);
		br.skip(4);
		unsigned packetid = br.read<unsigned>();

		if (log_inpacket_ids)
			logger.print("auth inpacket %08x\r\n", packetid);
		if (log_inpackets)
			Network::dump(data, length, "auth");

		if (packetid == 0x0e78a9ba)
		{
			std::string client_name;
			std::string passwd;
			{
				impl_lock_guard(lock, mutex);

				client.ptime = get_tick();

				if (client.sent_auth)
				{
					client.disconnect(DisconnectKind::IMMEDIATE_DC);
					return;
				}
				else
					client.sent_auth = true;
				if (use_launcher)
				{
					fixed_stream_array<char, 21> version_string;
					br >> version_string;
					if (check_version_string)
					{
						std::string str = to_string(version_string);
						bool result = false;
						{
							impl_lock_guard(lock, version_string_mutex);
							result = str == client_version_string;
						}
						if (!result)
						{
							authPacketHandler->sendTo(session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("Please login useing the launcher")));
							client.disconnect();
							return;
						}
					}
					fixed_stream_array<char, 2000> _login_data;
					br >> _login_data;
					std::string login_data = to_string(_login_data);
					if (login_data.size() < 66)
					{
						authPacketHandler->sendTo(session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("Please login useing the launcher")));
						client.disconnect();
						return;
					}
					passwd = login_data.substr(1, 64);
					client.name = login_data.substr(65);
					client.auth_token = std::move(login_data);
				}
				else
				{
					fixed_stream_array<char, 21> name, _passwd;
					br >> name;
					br >> _passwd;
					client.name = to_string(name);
					passwd = to_string(_passwd);
				}
				client_name = client.name;
			}

			if (!query<account_tuple>("SELECT id,name, passwd, banned, character_name, salt, email, reg_date, ban_reason, unix_timestamp(last_login) FROM accounts where lower(name) = " + to_query(to_lower(client_name)), [session_id, passwd](std::vector<account_tuple> const & data, bool success)
			{
				if (AuthPacketHandler::connection_t * client = authPacketHandler->get_client(session_id))
				{
					time_t now = time(0);
					if (!success)
					{
						authPacketHandler->sendTo(session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("Internal server error")));
						client->disconnect();
					}
					else if (data.size() == 0)
					{
						authPacketHandler->sendTo(session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("Incorrect username")));
						client->disconnect();
					}
					else if (!use_launcher && sha256(std::get<2>(data[0]).substr(64) + sha256(passwd)) != std::get<2>(data[0]).substr(0, 64))
					{
						authPacketHandler->sendTo(session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("Incorrect password")));
						client->disconnect();
					}
					else if (use_launcher && sha256(std::get<2>(data[0]).substr(64) + passwd) != std::get<2>(data[0]).substr(0, 64))
					{
						authPacketHandler->sendTo(session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("Incorrect password")));
						client->disconnect();
					}
					else if (std::get<3>(data[0]) != 0 && (time_t)std::get<3>(data[0]) > now)
					{
						time_t t = std::get<3>(data[0]) - now;
						unsigned daycount = t / (24 * 60 * 6);
						authPacketHandler->sendTo(session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("You are banned for " + std::to_string(daycount / 10) + '.' + std::to_string(daycount % 10) + " days")));
						client->disconnect();
					}
					else
					{
						unsigned token;
						{
							impl_lock_guard(lock, authPacketHandler->mutex);
							client->authenticated = true;
							client->character_name = std::get<4>(data[0]);
							authPacketHandler->connections_by_name[client->character_name] = client;
							client->token = rng::gen_uint(0, 0xffffffff);//rand() | (rand() << 16);
							token = client->token;
							client->last_login = (time_t)std::get<9>(data[0]);
						}


						unsigned dbid = std::get<0>(data[0]);
						//unsigned game_port = get_config<unsigned>("game_port");
						//unsigned game_ip = Network::string2ipv4(get_config("game_ip"));

						query("INSERT INTO login_log SET acc_id = " + to_query(dbid) + ", ip = " + to_query(client->ip));
						query("UPDATE accounts SET login_ip = " + to_query(client->ip) + " WHERE id = " + to_query(dbid));

						BinaryWriter bw;
						bw.emplace<Network::packet_header_t>();
						bw << 0x0df4c6fa;
						bw << dbid;	//database id
						bw << 3; 	//array length
									//for (unsigned i = 0; i < 3; ++i)
						{
							bw << (char)0x41 << 1 << game_ip << (short)game_port << (char)0;	//0x41 seems to be important...
							bw << (char)0x41 << 30 << game_ip << (short)game_port << (char)0;	//0x41 seems to be important...
							bw << (char)0x41 << 65 << game_ip << (short)game_port << (char)0;	//0x41 seems to be important...
						}

						bw << token;
						bw.write_strn(client->character_name, 21);


						bw.at<unsigned>(0) = bw.data.size() - 4;
						authPacketHandler->sendTo(session_id, std::move(bw.data));
					}
				}
			}))
			{
				repply(Network::packet(0x0aea93ba, (char)0, str2array<32>("Internal server error")));
				client.disconnect();
			}
			return;
		}
		else
		{
			client.disconnect(DisconnectKind::IMMEDIATE_DC);
			return;
		}
	}
	virtual void onDisconnect(Network::session_id_t session_id)
	{
		impl_lock_guard(lock, mutex);
		auto i = connections.find(session_id);
		if (i != connections.end())
		{
			{
				impl_lock_guard(lock, connection_count_mutex);
				if (!connection_count[i->second.dwip].dec())
					connection_count.erase(i->second.dwip);
			}

			i->second.invalidate();
			connections.erase(i);
		}
	}
	virtual void onInit()
	{
		srand(0);
	}

	unsigned last_client_check = get_tick();
	virtual void onTimer()
	{
		if (this->destroyed())
			return;
		impl_lock_guard(lock, mutex);

		unsigned tick = get_tick();
		if (tick - last_client_check > client_check_interval)
		{
			last_client_check = tick;
			for (auto i = connections.begin(), e = connections.end(); i != e; )
			{
				if (tick - i->second.ptime > max_client_idle_time)
				{
					Network::session_id_t id = i->second.session_id;
					++i;
					disconnect(id, DisconnectKind::IMMEDIATE_DC);
				}
				else
					++i;
			}
		}

		unsigned dc_all = disconnect_all;
		switch (dc_all)
		{
		case 1:
			for (auto i = connections.begin(); i != connections.end(); ++i)
				disconnect(i->second.session_id);
			if (connections.size() == 0)
				disconnect_all = 0;
			else
				disconnect_all = 2;
			break;
		case 2:
			if (connections.size() == 0)
				disconnect_all = 0;
			break;
		}
	}

public:

	std::atomic<unsigned> disconnect_all;

	struct connection_t
		: public Network::NetworkClientBase
	{
		Network::session_id_t session_id;
		std::string ip;
		unsigned port;
		unsigned dwip;
		std::string name;
		std::string character_name;
		std::string auth_token;
		time_t last_login = 0;

		unsigned token = 0;
		unsigned ptime;
		bool sent_auth = false;
		bool authenticated = false;

		virtual void invalidate()
		{
			if (_valid)
			{
				_valid = false;
				authPacketHandler->connections_by_name.erase(character_name);
			}
		}
		void disconnect(DisconnectKind disconnectKind = DisconnectKind::SOFT_DC)
		{
			invalidate();
			authPacketHandler->disconnect(session_id, disconnectKind);
		}
	};

	connection_t * get_client(Network::session_id_t session_id)
	{
		impl_lock_guard(lock, mutex);
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return nullptr;
		if (!_i->second.valid())
			return nullptr;
		return &_i->second;
	}

	mutable mutex_t mutex;
	std::map<Network::session_id_t, connection_t> connections;
	std::unordered_map<std::string, connection_t*> connections_by_name;

	virtual ~AuthPacketHandler()
	{
		impl_lock_guard(lock, mutex);
		this->_destroyed = true;
	}
	AuthPacketHandler()
        : disconnect_all(0)
	{}
};

struct room_list_data_t
{
	unsigned id;
	unsigned char players_max;
	unsigned char current_players;
	unsigned char kind;
	unsigned char joinable;
	unsigned mapid;
	unsigned char has_pw;
	unsigned unk1;

	void operator >> (BinaryReader & br)
	{
		br.read(id, players_max, current_players, kind, joinable, mapid, has_pw, unk1);
	}
	void operator << (BinaryWriter & bw) const
	{
		bw.write(id, players_max, current_players, kind, joinable, mapid, has_pw, unk1);
	}
};


struct ingame_charinfo_t
{
	std::string str1;
	std::string name;
	std::array<float, 8> stats;

	void operator >> (BinaryReader & br)
	{
		br.read(str1, name, stats);
	}

	void operator << (BinaryWriter & bw) const
	{
		bw.write(str1, name, stats);
	}


	static ingame_charinfo_t make_empty(std::string const & name)
	{
		ingame_charinfo_t r;
		r.str1 = name;
		r.name = name;
		for (unsigned i = 0; i < r.stats.size(); ++i)
			r.stats[i] = 0;

		return r;
	}
};

bool get_stats(unsigned player_id, ingame_charinfo_t & data);
bool get_replay_item_data(unsigned player_id, replay_data_t & rd);

struct room_packet_data_t
{
	unsigned char is_single;
	unsigned char kind;
	unsigned char player_limit;
	unsigned char has_password;
	std::array<char, 13> password;
	unsigned unk1 = 0;
	unsigned unk2 = 0;
	unsigned unk3 = 0;
	unsigned room_id;
	unsigned char unk4 = 0;
	unsigned mapid = 0;
	unsigned char unk6 = 0;
	unsigned unk7 = 0;
	unsigned long long unk8 = 0;
	unsigned unk9 = 0;

	void operator >> (BinaryReader & br)
	{
		//br >> is_single >> kind >> player_limit >> has_password >> password;
		//br >> unk1 >> unk2 >> unk3 >> room_id;
		//br >> unk4 >> mapid >> unk6 >> unk7 >> unk8 >> unk9;

		br.read(is_single, kind, player_limit, has_password, password, unk1, unk2, unk3, room_id, unk4, mapid, unk6, unk7, unk8, unk9);
	}

	void operator << (BinaryWriter & bw) const
	{
		//bw << is_single << kind << player_limit << has_password << password;
		//bw << unk1 << unk2 << unk3 << room_id;
		//bw << unk4 << mapid << unk6 << unk7 << unk8 << unk9;

		bw.write(is_single, kind, player_limit, has_password, password, unk1, unk2, unk3, room_id, unk4, mapid, unk6, unk7, unk8, unk9);
	}
};

struct character_packet_data_t
{
	unsigned dbid;
	unsigned clientip;
	std::string name;
	unsigned char level;
	unsigned char is_host;
	unsigned long long exp;
	std::string crew_name;
	///0 = none, 1 = red, 2 = blue
	unsigned team_id;
	unsigned is_not_ready = 1;
	unsigned ip1 = 0;
	unsigned short port1 = 0;
	unsigned ip2 = 0;
	unsigned short port2 = 0;
	unsigned ip3 = 0;
	unsigned short port3 = 0;
	unsigned ip4 = 0;
	unsigned short port4 = 0;
	float room_x = 0.0f;
	float room_y = 0.0f;
	unsigned room_dir = 0;
	unsigned room_unk1 = 0;
	bool b_pc = false;

	void operator << (BinaryWriter & bw) const
	{
		bw << 0 << clientip;
		bw.write_strn(name, 21);
		bw.write_strn(name, 21);
		bw << 0;
		bw.write_charn(1, 21);
		bw.write_charn(1, 81);
		bw << 0 << level << exp << (char)0;
		bw << 0 << 0 << 0 << 0;
		bw << (char)0 << (char)0;
		bw << 0 << 0 << dbid << 0 << 0;
		bw << (char)0;
		bw << room_x << room_y << room_dir << room_unk1 << is_not_ready;
		bw << is_host;
		bw << team_id;
		bw << (char)0 << (char)0 << (char)0;
		bw << ip1 << port1;
		bw << ip2 << port2;
		bw << ip3 << port3;
		bw << ip4 << port4;
		bw << 0;
		char * flags = bw.allocate<char>(18);
		for (unsigned i = 0; i < 18; ++i)
			flags[i] = 0;
		flags[17] = b_pc;

		bw << 0 << crew_name << 0 << 0;
		bw << 0 << 0 << 0 << 0;
		bw << 0 << 0 << 0 << 0;
	}
};

struct race_result_t
{
	std::string name;
	unsigned handle;	//player room seq id?
	float f1;
	float runtime;
	unsigned userdata;	//same as coins?
	unsigned coins;
	unsigned sum_team_ruv;
	unsigned bonus_team_ruv;
	unsigned trickscore;
	unsigned bonus_exp;	//0x51
	unsigned unk8;
	unsigned unk9;
	unsigned acquired_ore;	//-1
	unsigned unk11;
	std::string unk12;

	race_result_t() {}
	race_result_t(std::string const & name)
		: name(name)
		, handle(0)
		, f1(0)
		, runtime(0)
		, userdata(0)
		, coins(0)
		, sum_team_ruv(0)
		, bonus_team_ruv(0)
		, trickscore(0)
		, bonus_exp(0)
		, unk8(0)
		, unk9(0)
		, acquired_ore(~0)
		, unk11(0)
	{
	}

	void operator >> (BinaryReader & br)
	{
		br.read(name, handle, f1, runtime, userdata, coins, sum_team_ruv, bonus_team_ruv, trickscore, bonus_exp, unk8, unk9, acquired_ore, unk11, unk12);
	}
	void operator << (BinaryWriter & bw) const
	{
		bw.write(name, handle, f1, runtime, userdata, coins, sum_team_ruv, bonus_team_ruv, trickscore, bonus_exp, unk8, unk9, acquired_ore, unk11, unk12);
	}
};


unsigned add_pro(unsigned clinetid, unsigned pro);
void add_gem(unsigned clientid, unsigned gem_id);
unsigned get_quest_exp(unsigned clientid);
std::string getGuildName(unsigned guild_id);
void set_level_in_guild(unsigned guild_id, std::string const & name, unsigned level);

class GamePacketHandler
	: public Network::NetworkEventHandler
{
	virtual std::string getConnectionData(Network::session_id_t id) const
	{
		auto i = connections.find(id);
		if (i != connections.end())
			return i->second.name;
		return std::string();
	}
	virtual DisconnectKind onConnect(Network::session_id_t session_id, std::string && ip, unsigned port)
	{
		unsigned dwip = Network::string2ipv4(ip);
		{
			impl_lock_guard(lock, connection_count_mutex);
			if (!connection_count[dwip].inc())
				return DisconnectKind::IMMEDIATE_DC;
		}

		connection_t & client = connections[session_id];
		client.session_id = session_id;
		client.ip = std::move(ip);
		client.port = port;
		client.dwip = dwip;

		client.ptime = get_tick();
		query("REPLACE INTO online_player_count VALUES(0, " + to_query(connections.size()) + ", NOW())");
		//#ifdef _DEBUG
		//		logger << "Connection count: " << connections.size();
		//#endif
		online_count = connections.size();
		online_count_s.add((unsigned int)connections.size());
		if (!hourly_online_count_is_set)
		{
			hourly_online_count_is_set = true;
			hourly_online_count = online_count;
		}
		else
			hourly_online_count = std::max<unsigned>(hourly_online_count, connections.size());

		return DisconnectKind::ACCEPT;
	}
	virtual void onRecieve(Network::session_id_t session_id, const char * data, unsigned length)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
		{
			if (!session_id)
			{
				unsigned cmd_id = 0;
				BinaryReader br(data, length);
				br.skip<Network::packet_header_t>();
				br >> cmd_id;

				switch (cmd_id)
				{
				case 0:	//kick
				{
					std::string name;
					br >> name;
					kick(name);
				}
				break;
				case 1:	//ban
				{
					std::string name;
					br >> name;
					if (connection_t * player = get_client_by_name(name))
						query<std::tuple<> >("UPDATE accounts SET banned = 1 WHERE id = " + to_query(player->dbid), [](std::vector<std::tuple<> > const & data, bool success)
					{
						if (!success || affected_rows < 1)
							logger << "Ban failed";
						else
							logger << "Banned";
					});
					else
						query<std::tuple<> >("UPDATE accounts SET banned = 1 WHERE character_name = " + to_query(name), [](std::vector<std::tuple<> > const & data, bool success)
					{
						if (!success || affected_rows < 1)
							logger << "Ban failed";
						else
							logger << "Banned";
					});
					kick(name);
				}
				break;
				case 3:	//connected to relay
				{
					unsigned clientid;
					unsigned room_id;
					stream_array<char> name;
					std::string auth_token;
					Network::session_id_t sid;
					br >> room_id >> clientid >> name >> auth_token >> sid;
					if (connection_t * player = get_client_by_id(clientid))
						if ((!use_launcher || player->auth_token == auth_token) && room_id < rooms.size())
							if (rooms[room_id].on_connected_to_relay(clientid, name))
							{
								((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(4, sid));
								break;
							}
					((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(3, sid));

				}
				break;
				case 4:	//disbanded relay room
				{
					unsigned room_id;
					br >> room_id;
					if (room_id < rooms.size())
						rooms[room_id].on_relay_room_disband();
				}
				break;
				case 7:	//do_command
				{
					std::string name;
					std::string text;
					br >> name >> text;
					do_command(name, text.c_str(), text.size());
				}
				break;
				}
			}
			return;
		}

		BinaryReader br(data, length);
		br.skip(4);
		unsigned packetid = br.read<unsigned>();

		connection_t & client = _i->second;
		client.check_valid();
		client.ptime = get_tick();

		switch (log_inpacket_ids)
		{
		case 1:
			if (packetid != 0x01541aca)
				logger.print("game inpacket %08x\r\n", packetid);
			break;
		case 2:
			logger.print("game inpacket %08x\r\n", packetid);
			break;
		}
		if (log_inpackets)
			Network::dump(data, length, "game");

		if (client.name.size() == 0 && packetid != 0x0d2d702a)
		{
			client.disconnect(DisconnectKind::IMMEDIATE_DC);
			return;
		}


		switch (packetid)
		{
		case 0x00d614ea:	//register CTP port
		{
			d_dump(data, length, "game");
			unsigned ip1, ip2, public_ip1, public_ip2;
			unsigned short port1, port2, public_port1, public_port2;

			//br >> ip1 >> port1 >> ip2 >> port2;	//send, recv
			//br >> public_ip1 >> public_port1 >> public_ip2 >> public_port2;

			br.read(ip1, port1, ip2, port2, public_ip1, public_port1, public_ip2, public_port2);
			/*
			#ifdef _DEBUG
			logger << "Client name: " << client.name << " id: " << client.session_id.id << " ip: " << client.dwip;
			logger << "pseudo private recv : " << Network::ipv42string(ip2) << ':' << port2;
			logger << "pseudo private send : " << Network::ipv42string(ip1) << ':' << port1;
			logger << "pseudo public recv : " << Network::ipv42string(public_ip2) << ':' << public_port2;
			logger << "pseudo public send : " << Network::ipv42string(public_ip1) << ':' << public_port1;
			#endif
			*/
			client.private_recv_ip = ip2;
			client.private_recv_port = port2;
			client.private_send_ip = ip1;
			client.private_send_port = port1;

			//ctpPacketHandler1->sendDatagramTo(to_udp(ip2, port2+ 10), Network::packet(0));
			//ctpPacketHandler1->sendDatagramTo(to_udp(ip2, port2 + 10), Network::packet(0));
			//ctpPacketHandler1->sendDatagramTo(to_udp(ip1, port1 + 10), Network::packet(0));
			//ctpPacketHandler1->sendDatagramTo(to_udp(ip1, port1 + 10), Network::packet(0));

			//client.public_recv_ip = public_ip2;
			//client.public_recv_port = public_port2;
			//client.public_send_ip = public_ip1;
			//client.public_send_port = public_port1;

			//original:
			client.public_recv_ip = client.dwip;
			client.public_recv_port = public_port2;
			client.public_send_ip = client.dwip;
			client.public_send_port = public_port1;

			if (private_port_to_public)
			{
				client.public_recv_port = client.private_recv_port;
				client.public_send_port = client.private_send_port;
			}


			/*
			client.public_recv_ip = ip2;
			client.public_recv_port = port2;
			client.public_send_ip = ip1;
			client.public_send_port = port1;
			*/
			/*9*/
			/*
			if (client.name == "qwer")
			{
			client.private_recv_ip = 0;
			client.private_send_ip = 0;
			client.public_recv_ip = 0;
			client.public_send_ip = 0;
			}
			*/
		}
		break;
		case 0x0d2d702a:
		{
			if (client.name.size() > 0)
				break;
			fixed_stream_array<char, 21> _name;
			unsigned dbid, token;
			d_dump(data, length, "game");
			std::string auth_token;

			if (use_launcher)
				br >> auth_token;
			//br >> _name;
			//br >> dbid >> token;
			br.read(_name, dbid, token);
			client.name = to_string(_name);

			if (!online_names.insert(client.name).second)	//name already logged in
			{
				std::string client_name = client.name;
				bool ok = false;
				unsigned token2;
				std::string auth_token2;

				{
					impl_lock_guard(lock, authPacketHandler->mutex);
					auto i = authPacketHandler->connections_by_name.find(client.name);
					if (i != authPacketHandler->connections_by_name.end())
					{
						authPacketHandler->sendTo(i->second->session_id, Network::packet(0x0aea93ba, (char)0, str2array<32>("Already logged in. Kicking")));
						ok = true;
						token2 = i->second->token;
						auth_token2 = i->second->auth_token;
					}
				}

				client.name.clear();
				client.disconnect(DisconnectKind::IMMEDIATE_DC);

				if (ok && token == token2 && auth_token == auth_token2)
				{
					::kick(client_name);
				}
				return;
			}

			time_t last_login_time = 0;
			{
				bool ok = false;
				unsigned token2;
				std::string auth_token2;
				Network::session_id_t auth_session = Network::session_id_t{ 0, 0 };
				{
					impl_lock_guard(lock, authPacketHandler->mutex);

					auto i = authPacketHandler->connections_by_name.find(client.name);
					if (i != authPacketHandler->connections_by_name.end())
					{
						ok = true;
						token2 = i->second->token;
						auth_session = i->second->session_id;
						auth_token2 = i->second->auth_token;
						last_login_time = i->second->last_login;
						if (last_login_time == 0)
							last_login_time = 1;
					}
				}

				if (!ok || token != token2 || auth_token != auth_token2)
				{
					client.disconnect(DisconnectKind::IMMEDIATE_DC);
					return;
				}
				//				else
				//					authPacketHandler->disconnect(auth_session);
			}

			client.auth_token = auth_token;
			client.dbid = dbid;
			std::string client_name = client.name;



			query("UPDATE accounts SET last_login = NOW() WHERE id = " + to_query(client.dbid));
			if (last_login_time != 0)
			{
                unsigned daily_cash = ::daily_cash;
				if (daily_cash)
				{
					struct tm player_login_time = get_localtime(last_login_time);
					unsigned year_day = player_login_time.tm_yday;
					struct tm now = get_localtime(time(0));
					if (now.tm_yday != year_day)
						client.daily_login = true;
				}
			}



			query<character_tuple>("SELECT id,name,level,experience,popularity,guildid,guild_points,guild_rank,pro,cash,avatar,race_point,battle_point,player_rank,get_global_rank(id),hair2,hair3,hair4,hair5,last_daily_cash,skill_points,skill_season_id,get_seasonal_rank(id) FROM characters where id = '" + std::to_string(dbid) + "';", [session_id, client_name, dbid](std::vector<character_tuple> const & data, bool success)
			{
				if (GamePacketHandler::connection_t * client = gamePacketHandler->get_client(session_id))
				{
					if (!success)
					{
						client->disconnect();
					}
					else if (data.size() == 0)
					{
						query<std::tuple<> >("INSERT INTO characters set id = " + to_query(dbid) + ", name = " + to_query(client_name) + ", skill_season_id = " + to_query(current_season_id), [session_id, client_name, dbid](std::vector<std::tuple<> > const & data, bool success)
						{
							if (success)
							{
								query<character_tuple>("SELECT id,name,level,experience,popularity,guildid,guild_points,guild_rank,pro,cash,avatar,race_point,battle_point,player_rank,get_global_rank(id),hair2,hair3,hair4,hair5,last_daily_cash,skill_points,skill_season_id,get_seasonal_rank(id) FROM characters where id = '" + std::to_string(dbid) + "';", [session_id](std::vector<character_tuple> const & data, bool success)
								{
									if (GamePacketHandler::connection_t * client = gamePacketHandler->get_client(session_id))
									{
										if (!success)
										{
											client->disconnect();
										}
										else if (data.size() >= 0)
										{
											login_count.add();
											//											gamePacketHandler->enableRecv(session_id);
											if (client->dec_read_lock())
												gamePacketHandler->callPostponed(client->session_id);
											client->level = std::get<2>(data[0]);
											bool newchar = client->level < 1;
											if (client->level < 1)
												client->level = 1;

											client->exp = std::get<3>(data[0]);
											client->popularity = std::get<4>(data[0]);
											if (client->guild_id == 0)
											{
												client->guild_id = std::get<5>(data[0]);
												client->guild_points = std::get<6>(data[0]);
												client->guild_rank = std::get<7>(data[0]);
												client->guild_name = getGuildName(client->guild_id);
											}
											client->pro = std::get<8>(data[0]);
											client->cash = std::get<9>(data[0]);
											client->avatar = std::get<10>(data[0]);
											client->race_point = std::get<11>(data[0]);
											client->battle_point = std::get<12>(data[0]);
											client->player_rank = std::get<13>(data[0]);
											client->race_rank = std::get<14>(data[0]);


											client->hair_color[0] = std::get<15>(data[0]) & 0x00ffffff;
											client->hair_color[1] = std::get<16>(data[0]) & 0x00ffffff;
											client->hair_color[2] = std::get<17>(data[0]) & 0x00ffffff;
											client->hair_color[3] = std::get<18>(data[0]) & 0x00ffffff;


											client->last_daily_cash = std::get<19>(data[0]);
											if (current_season_id == std::get<21>(data[0]))
											{
												client->skill_points = std::get<20>(data[0]);
												client->skill_points_rank = std::get<22>(data[0]);
											}
											else
											{
												client->skill_points = 0;
												client->skill_points_rank = -1;
											}

											gamePacketHandler->connections_by_dbid[client->dbid] = client;
											gamePacketHandler->connections_by_ip[client->dwip] = client;
											gamePacketHandler->connections_by_name[client->name] = client;
											client->id = (~0 - 1024 + client->session_id.id);//use_ip_as_id ? client->dwip : client->session_id.id;
											gamePacketHandler->connections_by_id[client->id] = client;
											client->validated = true;

											query("UPDATE characters SET is_online = 1 WHERE id = " + to_query(client->dbid));

											BinaryWriter bw;
											bw.emplace<Network::packet_header_t>();
											bw << 0x0b57d42a;
											unsigned short CTP_send_port = ctpPacketHandlerSend->get_port();
											unsigned short CTP_recv_port = ctpPacketHandlerRecv->get_port();
											//unsigned game_ip = Network::string2ipv4(get_config("game_ip"));

											bw << game_ip << (short)CTP_send_port << game_ip << (short)CTP_recv_port << client->id;
											bw.write_strn(client->name, 21);
											bw.write_strn(client->name, 21);

											bw << 0xd;

											//bw << str2array<21>(client.crew);
											for (unsigned i = 0; i < 21; ++i)
												bw << (char)0;
											for (unsigned i = 0; i < 81; ++i)
												bw << (char)0;

											bw << 1 << (char)client->level << (unsigned long long)client->exp << (char)1;

											bw << 1 << 1 << 1 << 1;
											bw << (char)1 << (char)1;
											bw << 1 << 1;
											bw << client->dbid;	//clientid for roomserver
											bw << 0xb << 0xc;

											for (unsigned i = 0; i < 18; ++i)
												bw << (char)0;
											bw << (char)0;

											gamePacketHandler->sendTo(session_id, std::move(bw.data));
											if (newchar)
											{
												send_message(client->name, 5, "", "", "", get_config("join_message"));
												query("UPDATE characters SET level = 1 WHERE id = " + to_query(client->dbid));
											}
										}
									}
								});
							}
							else
							{
								logger << "Failed to create new character for " << client_name << '(' << dbid << ')';
							}
						});
					}
					else
					{
						login_count.add();
						//gamePacketHandler->enableRecv(session_id);
						if (client->dec_read_lock())
							gamePacketHandler->callPostponed(client->session_id);
						client->level = std::get<2>(data[0]);
						bool newchar = client->level < 1;
						if (client->level < 1)
							client->level = 1;
						client->exp = std::get<3>(data[0]);
						client->popularity = std::get<4>(data[0]);
						if (client->guild_id == 0)
						{
							client->guild_id = std::get<5>(data[0]);
							client->guild_points = std::get<6>(data[0]);
							client->guild_rank = std::get<7>(data[0]);
							client->guild_name = getGuildName(client->guild_id);
						}
						client->pro = std::get<8>(data[0]);
						client->cash = std::get<9>(data[0]);
						client->avatar = std::get<10>(data[0]);
						client->race_point = std::get<11>(data[0]);
						client->battle_point = std::get<12>(data[0]);
						client->player_rank = std::get<13>(data[0]);
						client->race_rank = std::get<14>(data[0]);

						client->hair_color[0] = std::get<15>(data[0]) & 0x00ffffff;
						client->hair_color[1] = std::get<16>(data[0]) & 0x00ffffff;
						client->hair_color[2] = std::get<17>(data[0]) & 0x00ffffff;
						client->hair_color[3] = std::get<18>(data[0]) & 0x00ffffff;

						client->last_daily_cash = std::get<19>(data[0]);
						if (current_season_id == std::get<21>(data[0]))
						{
							client->skill_points = std::get<20>(data[0]);
							client->skill_points_rank = std::get<22>(data[0]);
						}
						else
						{
							client->skill_points = 0;
							client->skill_points_rank = -1;
						}

						gamePacketHandler->connections_by_dbid[client->dbid] = client;
						gamePacketHandler->connections_by_ip[client->dwip] = client;
						gamePacketHandler->connections_by_name[client->name] = client;
						client->id = (~0 - 1024 + client->session_id.id); //use_ip_as_id ? client->dwip : client->session_id.id;
						gamePacketHandler->connections_by_id[client->id] = client;
						client->validated = true;
						query("UPDATE characters SET is_online = 1 WHERE id = " + to_query(client->dbid));

						BinaryWriter bw;
						bw.emplace<Network::packet_header_t>();
						bw << 0x0b57d42a;
						unsigned short CTP_send_port = 40000;
						unsigned short CTP_recv_port = 40001;
						//unsigned game_ip = Network::string2ipv4(get_config("game_ip"));

						bw << game_ip << (short)CTP_send_port << game_ip << (short)CTP_recv_port << client->id;

						bw.write_strn(client->name, 21);
						bw.write_strn(client->name, 21);

						bw << 0xd;

						//bw << str2array<21>(client.crew);
						for (unsigned i = 0; i < 21; ++i)

							bw << (char)0;
						for (unsigned i = 0; i < 81; ++i)
							bw << (char)0;

						bw << 1 << (char)client->level << (unsigned long long)client->exp << (char)1;

						bw << 1 << 1 << 1 << 1;
						bw << (char)1 << (char)1;
						bw << 1 << 1;
						bw << client->dbid;	//clientid for roomserver
						bw << 0xb << 0xc;

						for (unsigned i = 0; i < 18; ++i)
							bw << (char)0;
						bw << (char)0;

						gamePacketHandler->sendTo(session_id, std::move(bw.data));
						if (newchar)
						{
							send_message(client->name, 5, "", "", "", get_config("join_message"));
							query("UPDATE characters SET level = 1 WHERE id = " + to_query(client->dbid));
						}
					}
				}
			});

			client.inc_read_lock();
			/*
			{
			unsigned count = 4, count2 = 5;
			BinaryWriter bw;
			bw.emplace<Network::packet_header_t>();
			bw << 0x024ae39a;
			bw << count;
			for (unsigned i = 0; i < count; ++i)
			bw << std::string(count2, 'a') << (float)i;
			bw.at<unsigned>(0) = bw.data.size() - 4;
			repply(std::move(bw.data));
			}
			*/

			return;
		}
		break;
		case 0x07d6076a:	//run loaded
		{
			//d_dump(data, length, "game");
			stream_array<char> name;
			unsigned char state;
			stream_array<char> unk1;
			br >> name >> state >> unk1;
			assert(name == client.name);
			if (client.room)
				client.room->loaded(client, state, unk1);
		}
		break;
		case 0x0a7376fa:	//client ready for run (sent in room, after connected to relay)
		{
			//d_dump(data, length, "game");
			ingame_charinfo_t current_character;
			br >> current_character;

			if (client.room)
				client.room->ready_run(client, std::move(current_character));
		}
		break;

		case 0x07f5a4ca:
		{
			/*
			d_dump(data, length, "game");
			unsigned clientid;
			unsigned reward_exp;
			unsigned reward_money;
			unsigned char unk1;
			unsigned char unk2;
			unsigned unk3;
			unsigned unk4;
			unsigned unk5;
			unsigned trickscore;
			unsigned unk7;
			br.read(clientid, reward_exp, reward_money);
			try
			{
				br.read(unk1, unk2, unk3, unk4, unk5, trickscore, unk7);
			}
			LOG_CATCH();

			//assert(clientid == client.id);
			//assert(client.eligible_for_reward);
			if (clientid == client.id && client.eligible_for_reward)
			{
				bool recieved_daily_cash = false;
				time_t now = time(0);

				if (client.daily_login)
					client.daily_login = false;

			}
			*/
		}
		break;
		case 0x0c87a80a:	//crossed finishline
		{
			d_dump(data, length, "game");
			if (client.room)
			{
				race_result_t result;
				br >> result;
				client.room->crossed_finish_line(client, std::move(result));
			}
		}
		break;
		case 0x0f19871a:
		{

			fixed_stream_array<char, 91> text;
			br >> text;

			if (test_chatban(client.name, to_string(text)))
				break;

			bool send_text = true;

			{
				unsigned i = 0;
				unsigned const e = text.size();
				for (; i < e && text[i] != 0; ++i)
					if (text[i] != ' ' && text[i] != 9)
						break;
				if (i < e && (text[i] == '/' || text[i] == '.'))
				{
					std::string str = to_string(text);
					do_command(client.name, str.c_str(), str.size());
					break;
				}
			}

			std::vector<std::string> v = split(text, ' ');
			if (v.size() > 0)
			{
				if (v[0] == "start")
				{
					send_text = false;
					if (client.is_host())
						client.room->start_run();
				}
				else if (v[0] == "forcestart")
				{
					send_text = false;
					if (client.is_host())
						client.room->start_run(true);
				}
				else if (v[0] == "random")
				{
					send_text = false;
					if (client.is_host())
						client.room->set_map(0x03e9);
				}
				if (client.player_rank >= 2)
				{
					if ((v[0] == "map") && v.size() > 0)
					{
						if (client.is_host())
							client.room->set_map(convert<int>(v[1]));
					}
				}
				/*
				else if (v[0] == "nocanrun")
				{
				repply(Network::packet(0x0ffa3eba));
				send_text = false;
				}
				else if (v[0] == "canrun")
				{
				repply(Network::packet(0x0b83ec4a));
				send_text = false;
				}
				}
				*/
			}

			if (show_chat)
				logger << '[' << client.name << "]: " << text;

			if (client.room && send_text)
				client.room->chat(client, text);
		}
		break;
		case 0x03d4275a:
			if (client.room)
				client.room->leave(client);
			break;
		case 0x099c739a:	//set room map
			if (client.is_host())
				client.room->set_map(br.read<unsigned>());
			break;
			//case 0x052895aa:	//joined room
		case 0x0b98b01a:
			//case 0x03825f0a:
			if (client.room)
				client.room->get_player_data(client);
			break;
			//		case 0x099c3ea0:
			//			break;
			//		case 0x099c3e0a:	//avatar change in room
			//			d_dump(data, length, "game");
			//			if (client.room && (int)client.room->room_state < (int)room_t::loading)
			//			{
			//			}
			break;
		case 0x0e214cba:
			if (client.is_host())
				client.room->dec_player_limit();
			break;
		case 0x0efe49ba:
			if (client.is_host())
				client.room->inc_player_limit();
			break;
		case 0x032268aa:	//ready off
			if (client.room)
				client.room->reset_ready(client);
			break;
		case 0x01eed10a:	//ready on
			if (client.room)
				client.room->set_ready(client);
			break;
		case 0x0716791a:
			if (client.room)
				client.room->change_team(client, br.read<unsigned>());
			break;
		case 0x032ad67a:	//kick
			if (client.is_host())
				client.room->kick(br.read<unsigned>(), client.id);
			break;
		case 0x0ce0c1aa:	//room motion
			if (client.room)
			{
				br.read(client.room_x, client.room_y, client.room_dir, client.room_unk1);
				if (client.room->players.size() > 1)
					client.room->distribute(Network::packet(0x00d22c4a, client.id, client.room_x, client.room_y, client.room_dir, client.room_unk1), client.session_id);
			}
			break;
		case 0x06d1515a:	//room motion
			if (client.room)
			{
				br.read(client.room_x, client.room_y, client.room_dir, client.room_unk1);
				if (client.room->players.size() > 1)
					client.room->distribute(Network::packet(0x05e66fda, client.id, client.room_x, client.room_y, client.room_dir, client.room_unk1), client.session_id);
			}
			break;
		case 0x00d6604a:	//room motion
			if (client.room)
			{
				unsigned anim = 0;
				br.read(client.room_x, client.room_y, client.room_dir, client.room_unk1, anim);
				if (client.room->players.size() > 1)
					client.room->distribute(Network::packet(0x08962c8a, client.id, client.room_x, client.room_y, client.room_dir, client.room_unk1, anim), client.session_id);
			}
			break;

		case 0x0dd73eba:	//join room (matchmaker)
		{
			d_dump(data, length, "game");

			//unsigned char unk;
			//br >> unk;
			if (client.room)
				client.room->leave(client);
			else if (client.mm_room_id < rooms.size())
			{
				room_t & room = rooms[client.mm_room_id];
				if (room.players.size() > 0 && room.players.size() < room.player_limit && room.room_state == room_t::waiting && (room.password.size() == 0 || client.player_rank >= 2))
					room.join(client);
			}
			client.mm_room_id = ~0;
			if (client.room == nullptr)
				repply(Network::packet(0x08e6e24a));	//opens room list
		}
		break;

		case 0x0112409a:	//join room
		{
			unsigned room_id;
			br >> room_id;
			room_id -= room_id_disp;
			if (room_id < rooms.size())
				rooms[room_id].join(client);
		}
		break;
		case 0x0a67b11a:	//join room
		{
			unsigned room_id;
			//std::array<char, 13> password;
			fixed_stream_array<char, 13> password;
			br >> room_id >> password;
			room_id -= room_id_disp;
			if (room_id < rooms.size())
				rooms[room_id].join(client, password);
		}
		break;
		case 0x0c3e664a:	//create room
		{
			d_dump(data, length, "game");
			unsigned char b_is_single;
			unsigned char room_kind;	//0 = race, 1 = battle, 2 = coin
			unsigned char player_limit;
			unsigned char b_password;
			//std::array<char, 13> password;
			fixed_stream_array<char, 13> password;
			unsigned unk1;
			unsigned unk2;
			unsigned unk3;

			br >> b_is_single;
			br >> room_kind;
			br >> player_limit;
			br >> b_password;
			br >> password;
			br >> unk1;
			br >> unk2;
			br >> unk3;
			//password[12] = 0;

			create_room(client, b_is_single, room_kind, player_limit, b_password ? to_string(password) : "");
		}
		break;
		case 0x0c3ea8ea:	//request relay
#ifdef _DEBUG
			logger << "Clientid: " << client.id << " name: " << client.name;
#endif
			d_dump(data, length, "game");
			if (client.room)
				client.room->uhp_test_response(client, br.read<stream_array<unsigned> >());
			break;
		case 0x003cef5a:	//
#ifdef _DEBUG
			logger << "Clientid: " << client.id << " name: " << client.name;
#endif
			if (client.room)
				client.room->uhp_test_response(client);
			break;
		case 0x00f08b3a:	//connection test result
#ifdef _DEBUG
			logger << "Clientid: " << client.id << " name: " << client.name;
#endif
			d_dump(data, length, "game");
			if (client.room)
			{
				stream_array<unsigned> data;
				br >> data;
				client.room->connection_test_response(client, &data);
			}
			break;
		case 0x0a2e642a:	//solo run start
			if (client.room)
				client.room->solo_run_start(client);
			break;
		case 0x0477150a:
			d_dump(data, length, "game");
			break;
		case 0x01541aca:	//timesync(repeated) (only in room)
			client.roomtime_tick = get_tick();
			client.last_time_sent = client.roomtime_tick;
			br >> client.roomtime;
			repply(Network::packet(0x046a3f3a, client.roomtime, (client.roomtime_tick - server_start_tick) / 1000.0f));
			client.roomtime_set = true;
			break;
		case 0x065aa9ca:	//start run
			if (client.is_host())
				client.room->start_run();
			break;
		default:
			d_dump(data, length, "game");
		}
		return;
	}
	virtual void onDisconnect(Network::session_id_t session_id)
	{
		auto i = connections.find(session_id);
		if (i != connections.end())
		{
			{
				impl_lock_guard(lock, connection_count_mutex);
				if (!connection_count[i->second.dwip].dec())
					connection_count.erase(i->second.dwip);
			}

			i->second.invalidate();
			connections.erase(i);
		}
		query("REPLACE INTO online_player_count VALUES(0, " + to_query(connections.size()) + ", NOW())");
		online_count = connections.size();
		online_count_s.add((unsigned int)connections.size());
		if (!hourly_online_count_is_set)
		{
			hourly_online_count_is_set = true;
			hourly_online_count = online_count;
		}
		else
			hourly_online_count = std::max<unsigned>(hourly_online_count, connections.size());
	}
	virtual void onInit()
	{
		//		default_color = LIGHT_BLUE;
		srand(time(0));
	}

	std::set<std::string> online_names;
	std::set<unsigned> online_ips;

public:

	struct room_t;
	struct connection_t
		: public Network::NetworkClientBase
	{
		std::string ip;
		unsigned port = 0;
		Network::session_id_t session_id;
		std::string auth_token;
		unsigned dbid = 0;
		unsigned id = 0;
		unsigned dwip = 0;
		unsigned ptime = 0;
		unsigned guild_id = 0;
		unsigned guild_points = 0;
		unsigned guild_rank = 0;
		unsigned pro = 0;
		unsigned cash = 0;

		int player_rank = 0;
		unsigned race_rank = 0;

		unsigned private_recv_ip = 0;
		unsigned private_send_ip = 0;
		unsigned public_recv_ip = 0;
		unsigned public_send_ip = 0;
		unsigned short private_recv_port = 0;
		unsigned short private_send_port = 0;
		unsigned short public_recv_port = 0;
		unsigned short public_send_port = 0;


		unsigned hair_color[4];

		std::string name;
		std::string guild_name;
		float roomtime = 0.0f;
		unsigned roomtime_tick = 0;
		unsigned last_time_sent = 0;

		int race_point = 0;
		int battle_point = 0;
		unsigned avatar = 2;
		unsigned level = 1;
		unsigned long long exp = 0;
		unsigned popularity = 0;

		unsigned team_id;
		bool run_loaded;
		bool loaded_dummy;
		bool crossed_finish_line;
//		bool eligible_for_reward = false;
		bool room_ready;
		bool roomtime_set;
		bool connected_to_relay;
		bool sent_relay_port;
		bool requested_relay_port;
		bool requested_dummy;
		bool sent_connection_test_result = false;
		bool sent_uhp_test_result = false;
		bool daily_login = false;
		bool endrace_request = false;
		bool dummy_increased = false;
		bool sent_speed_report = false;
		unsigned last_speed_report = 0;
		//		std::vector<unsigned> connection_test_result;
		//		std::vector<unsigned> uhp_test_result;

		unsigned room_invite_id = ~0;
		unsigned room_invite_time;

		float room_x = 0.0f;
		float room_y = 0.0f;
		unsigned room_dir = 0;
		unsigned room_unk1 = 0;

		unsigned mm_room_id = ~0;
		room_t * room = nullptr;

//		unsigned reward_exp = 0;
//		unsigned reward_money = 0;
//		unsigned reward_map = ~0;
//		unsigned reward_cash = 0;
		unsigned long long last_daily_cash = 0;
		long long skill_points = 0;
		int skill_points_rank = -1;
		///0: race, 1: battle, 2: coin
//		unsigned reward_mode = ~0;
//		std::shared_ptr<replay_data_t> reward_replay;
		///0: solo, 1: multi, 2: mult with relay dc
//		unsigned reward_runkind = 0;
//		float reward_time = 0.0f;
//		float run_client_time = 0.0f;
//		float run_server_time = 0.0f;

		bool room_pw_response = false;
		unsigned room_pw_response_time = 0;

		bool validated = false;
		bool owns_ip = false;

		bool check_trick()
		{
			bool r = check_client_in_trick_server(id);

			if(!r)
				message("Connection error, please reconnect");

			return r;
		}

		virtual void invalidate()
		{
			if (_valid)
			{
				if (validated)
				{
					query("UPDATE characters SET is_online = 0 WHERE id = " + to_query(dbid));
					logout_count.add();
				}

				_valid = false;
				if (room) try
				{
					room->leave(*this);
				}
				LOG_CATCH();

				if (name.size() > 0)
				{
					gamePacketHandler->online_names.erase(name);
					if (validated)
						gamePacketHandler->connections_by_name.erase(name);
					name.clear();
				}
				/*
				if (use_ip_as_id && owns_ip)
				{
				owns_ip = false;
				gamePacketHandler->online_ips.erase(dwip);
				}
				*/
				if (validated)
				{
					gamePacketHandler->connections_by_dbid.erase(dbid);
					gamePacketHandler->connections_by_ip.erase(dwip);
					gamePacketHandler->connections_by_id.erase(id);
				}
			}
		}
		void disconnect(DisconnectKind disconnectKind = DisconnectKind::SOFT_DC)
		{
			invalidate();
			gamePacketHandler->disconnect(session_id, disconnectKind);
		}

		bool is_host() const
		{
			return room && room->host == session_id;
		}

		inline bool send(std::vector<char> && data)
		{
			return gamePacketHandler->sendTo(session_id, std::move(data));
		}
		inline bool send(BinaryWriter && bw)
		{
			return gamePacketHandler->sendTo(session_id, std::move(bw.data));
		}

		void message(std::string const & msg)
		{
			send(Network::packet(0x0bffbd2a, 0, msg, 0));
		}
	};

	connection_t * get_client(Network::session_id_t session_id)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return nullptr;
		if (!_i->second.valid())
			return nullptr;
		return &_i->second;
	}


	std::vector<unsigned> timeout_rooms;

	struct room_t
	{
		unsigned room_id;
		unsigned mapid;
		unsigned run_map;
		unsigned last_run_map;
		unsigned char is_single;
		unsigned char kind;
		unsigned char player_limit;
		std::string password;
		std::vector<Network::session_id_t> players;
		Network::session_id_t host;
		unsigned host_id;
		std::vector<ingame_charinfo_t> ingame_charinfo;
		std::vector<race_result_t> results;

		std::atomic<unsigned> sent_run_data;
		mutex_t arrival_mutex;
		//std::vector<std::tuple<unsigned, unsigned, int, bool, unsigned, unsigned, std::shared_ptr<replay_data_t>, unsigned, std::string > > arrival_times;
		std::vector<std::tuple<unsigned, unsigned, relay_run_data_t, std::string > > arrival_times;

		struct additional_result_data_t
		{
			unsigned level = 0;
			unsigned clientid = 0;
			Network::session_id_t id = Network::session_id_t{ 0, 0 };

			additional_result_data_t() {}
			additional_result_data_t(connection_t * player, unsigned arrival_time)
				: level(player->level)
				, clientid(player->id)
				, id(player->session_id)
				, name(player->name)
				, arrival_time(arrival_time)
			{
			}

			std::string name;
			float runtime = 0.0f;
			unsigned arrival_time = 0;
			float run_client_time = 0.0f;
			float run_server_time = 0.0f;
			bool report = false;
			bool speedhack = false;
			bool relay_crossed = false;
			bool relay_run_data = false;
			unsigned invalid_spike = 0;
			unsigned used_time = 1;
			relay_run_data_t rrd;
			relay_run_stats_t rs;
			std::shared_ptr<std::string> csv;
			std::shared_ptr<std::string> html;
		};
		std::vector<additional_result_data_t > result_levels;
		unsigned loaded_count = 0;
		unsigned loaded_dummy_count = 0;
		unsigned ready_count = 0;
		unsigned relay_ready_count = 0;
		//		unsigned connection_test_result_count = 0;
		//		unsigned uhp_test_result_count = 0;
		unsigned endrace_request_count = 0;
		unsigned red_count = 0;
		unsigned blue_count = 0;
		unsigned connection_test_state;
		unsigned connection_test_time;
		float start_time;
		unsigned start_tick;
		unsigned race_end_time;
		unsigned seed;
		unsigned dummy_slot;
		//unsigned solo_arrival_time;
		//		bool uses_relay;
		bool run_cancelled;
		bool race_end_timer = false;
		bool is_solo_run;
		std::shared_ptr<replay_data_t> replay_data;

		enum room_state_t { waiting = 0, /*connection_test, uhp_test, relay_startup, */getting_ready_solo, loading, running, relay_stop } room_state = waiting;

		void dec_player_limit(bool non_host = false)
		{
			if (player_limit > players.size())
			{
				--player_limit;
				if (!non_host)
					distribute(Network::packet(0x0683715a), host);
				else
					distribute(Network::packet(0x0683715a));
			}
		}

		void inc_player_limit(bool non_host = false)
		{
			if (player_limit < 8)
			{
				++player_limit;
				if (!non_host)
					distribute(Network::packet(0x00786a6a), host);
				else
					distribute(Network::packet(0x00786a6a));
			}
		}

		bool can_run()
		{
			return (ready_count == players.size() - 1) && (relay_ready_count == players.size())/* && (red_count == blue_count || is_single != 0 || players.size() == 1)*/;
		}

		bool can_run2()
		{
			return can_run();
			//return (ready_count == players.size() - 1);
		}

		void set_ready(connection_t & client)
		{
			if (room_state == waiting)
			{
				distribute(Network::packet(0x0311542a, client.id), client.session_id);
				client.room_ready = true;
				check_ready_state();
				if (can_run())
					gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
				else
					gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
			}
		}

		void reset_ready(connection_t & client)
		{
			if (room_state == waiting)
			{
				distribute(Network::packet(0x05ac146a, client.id), client.session_id);
				client.room_ready = false;
				check_ready_state();
				if (can_run())
					gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
				else
					gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
			}
		}

		void clear_ready_state()
		{
			ready_count = 0;
			for (Network::session_id_t i : players)
			{
				if (i == host)
					continue;
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(i))
					player->room_ready = false;
			}

			if (can_run())
				gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
			else
				gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
		}

		void check_ready_state()
		{
			ready_count = 0;
			for (Network::session_id_t i : players)
			{
				if (i == host)
					continue;
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(i))
					if (player->room_ready && player->roomtime_set)
						++ready_count;
			}

		}

		void kick(unsigned id, unsigned caller_id = 0x80000000)
		{
			for (Network::session_id_t i : players)
			{
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(i))
					if (player->id == id)
					{
						bool pw_is_set = false;
						if (pw_room_on_kick)
						{
							if (caller_id != 0x80000000 && caller_id != id && this->password == "")
							{
								for (unsigned i = 0; i < 4; ++i)
									this->password += (char)(rng::gen_uint(0, 9) + '0');
								pw_is_set = true;
							}
						}

						player->send(Network::packet(0x0ebb822a, id));
						leave(*player);

						if(pw_is_set)
							this->message("Room password is set to: " + this->password);

						break;
					}
			}
		}

		void on_host_changed()
		{

		}

		std::string room_str() const
		{
			return "[room" + std::to_string(room_id) + "] ";
		}

		void leave(connection_t & client)
		{
			for (unsigned i = 0; i < players.size(); ++i)
			{
				if (players[i] == client.session_id)
				{
					players[i] = players.back();
					players.pop_back();
					client.room = nullptr;
					//client.eligible_for_reward = false;
					//client.reward_replay.reset();

					((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(2, room_id, client.id));

					if (players.size() == 0)
						close();
					else
					{
						distribute(Network::packet(0x0ebb822a, client.id));
						if (host == client.session_id)
						{
							host = players[0];
							host_id = 0;
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(host))
							{
								host_id = player->id;
								distribute(Network::packet(0x0bf468ba, player->id));
								if (player->room_ready)
								{
									player->room_ready = false;
									distribute(Network::packet(0x05ac146a, player->id));
								}
							}
							on_host_changed();
						}

						if (client.connected_to_relay)
							--relay_ready_count;

						for (unsigned i = 0; i < ingame_charinfo.size(); ++i)
						{
							if (ingame_charinfo[i].name == client.name)
							{
								for (unsigned j = i + 1; j < ingame_charinfo.size(); ++j)
									ingame_charinfo[j - 1] = std::move(ingame_charinfo[j]);

								break;
							}
						}

						if (room_state == waiting)
						{
							check_ready_state();
							if (can_run())
								gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
							else
								gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
						}
						//						else if (room_state == connection_test)
						//						{
						//							if (client.sent_connection_test_result)
						//								--connection_test_result_count;
						//							if (connection_test_result_count >= players.size())
						//								connection_test_end();
						//						}

						//						else if (room_state == uhp_test)
						//						{
						//							if (client.sent_uhp_test_result)
						//							{
						//								--uhp_test_result_count;
						//								if (uhp_test_result_count == players.size())
						//									uhp_test_end();
						//							}
						//						}

						//						else if (room_state == relay_startup)
						//						{
						//							if (client.connected_to_relay)
						//								--relay_ready_count;
						//
						//							if (relay_ready_count >= players.size())
						//							{
						//								distribute(Network::packet(0x0b5b86fa));
						//								room_state = getting_ready;
						//							}
						//						}

						//						else if (room_state == getting_ready)
						//						{
						//							for (unsigned j = 0; j < ingame_charinfo.size(); ++j)
						//							{
						//								if (ingame_charinfo[j].name == client.name)
						//								{
						//									ingame_charinfo[j] = std::move(ingame_charinfo.back());
						//									ingame_charinfo.pop_back();
						//
						//									break;
						//								}
						//							}
						//
						//							if (ingame_charinfo.size() == players.size())
						//							{
						//								room_state = running;
						//								unsigned seed = time(0);
						//								distribute(Network::packet(0x09ccdeda, seed, mapid, ingame_charinfo));	//starts loading the map
						//							}
						//						}
						else if (room_state == getting_ready_solo)
						{
							logger << room_str() << "Party leave during loading: " << client.name;
							message("Player " + client.name + " disconnected");

							if (client.loaded_dummy)
								--loaded_dummy_count;

							if (this->loaded_dummy_count >= players.size())
								all_dummy_displayed();
						}
						else if (room_state == loading)
						{
							logger << room_str() << "Party leave during loading: " << client.name;
							message("Player " + client.name + " disconnected");

							if (client.run_loaded)
								--loaded_count;

							if (loaded_count >= players.size())
								start_run2();
						}
						else if (room_state == running)
						{
							logger << room_str() << "Party leave during running: " << client.name;
							message("Player " + client.name + " disconnected");
							if(!client.crossed_finish_line)
								ranking_penalaty(client);

							for (unsigned j = 0; j < results.size(); ++j)
								if (results[j].name == client.name)
								{
									results[j] = results.back();
									results.pop_back();
								}

							if (players.size() == results.size())
								send_results();
							else
							{
								if (endrace_request_count > 0)
								{
									if (client.endrace_request)
										--endrace_request_count;
								}
							}
						}
					}


					return;
				}
			}
			assert(false);
		}

		void room_error()
		{
			distribute(Network::packet(0x0a97b7ba));
			close();
		}

		void message(std::string const & msg)
		{
			distribute(Network::packet(0x0bffbd2a, 0, msg, 0));
		}

		void stop_run(connection_t & client)
		{
			if (room_state == GamePacketHandler::room_t::running && !client.endrace_request)
			{
				client.endrace_request = true;
				++endrace_request_count;
				if (endrace_request_count >= players.size())
				{
					message("Stopping run");

					run_cancelled = true;
					send_results();
				}
				else
				{
					message(client.name + " requested to end the run");
				}
			}
		}

		void on_room_timeout()
		{
			race_end_timer = false;
			send_results();
		}

		void ranking_penalaty(connection_t & client)
		{
			if (players.size() == 0)
				return;

			//  kind == 0 && is_single != 0 && this->password == ""
			if (kind != 0 || is_single == 0 || this->password != "")
				return;

			const double K = 32.0;
			double r[8];
			double E[8];
			//double S[8];

			for (int i = 0; i < players.size(); ++i)
				if (connection_t * player = gamePacketHandler->get_client(players[i]))
					r[i] = player->skill_points;

			r[players.size()] = client.skill_points;

//			double match_count = (players.size() + 1) * players.size() / 2.0;

			for (int i = 0; i < (players.size() + 1); ++i)
			{
				E[i] = 0.0;
				for (unsigned j = 0; j < (players.size() + 1); ++j)
					if (j != i)
						E[i] += 1.0 / (1.0 + pow(10.0, (r[j] - r[i]) / 400.0));

//				E[i] /= match_count;

//				S[i] = ((players.size() + 1) - (i + 1)) / match_count;
			}

			//for (int i = 0; i < players.size(); ++i)
			{
				int i = players.size();
				r[i] = r[i] + K * ( - E[i]);

				long long ullri = (long long)r[i];
				if (client.skill_points != ullri)
				{
					long long sp_diff = ullri - (long long)client.skill_points;


					client.skill_points = ullri;
					if (client.skill_points < 0)
						client.skill_points = 0;

					query("UPDATE characters SET skill_points = " + to_query(client.skill_points) + ", skill_season_id = " + to_query(current_season_id) + " WHERE id = " + to_query(client.dbid) + " AND skill_season_id <= " + to_query(current_season_id));

					logger << room_str() << "Points penalaty: " << client.name << ": " << sp_diff << " S[" << i << "]=0 E[" << i << "]=" << E[i];

				}
			}

			double sum_E = 0.0/*, sum_S = 0.0*/;
			for (int i = 0; i < (players.size() + 1); ++i)
				sum_E += E[i];
//			for (int i = 0; i < (players.size() + 1); ++i)
//				sum_S += S[i];

			logger << room_str() << "SUM E = " << sum_E;// << " SUM S = " << sum_S;
		}

		void send_results()
		{
			unsigned const tick = get_tick();
			if (room_state == running)
			{
				time_t now = time(0);
				race_end_count.add();
				race_end_timer = false;
				if (!run_cancelled)
				{
					long long run_id = ++max_run_id;
					std::string run_date = datetime2str();
					logger << room_str() << "Race results " << (is_single ? "Single" : "Team") << ' ' << (kind < 3 ? room_kind_str[kind] : "unspecified") << " playercount = " << players.size() << (is_solo_run ? " solo" : " not solo") << " map = " << this->run_map << " '" << get_map_name(this->run_map) << '\'' << " Run id = " << run_id;

					//13
					query("INSERT INTO run SET id=" + to_query(run_id) + ", datetime=NOW(), mode=" + to_query(this->kind + (is_single ? 0 : 3)) + ", room_nr=" + to_query(this->room_id) + ", player_count=" + to_query(this->players.size()) + ", map=" + to_query(get_map_name(this->run_map)) + ", season_id=from_unixtime(" + to_query(::current_season_id) + "), room_pw=" + to_query(this->password.size() > 0 ? 1 : 0) + ";");

					{
						//std::vector<std::tuple<unsigned, unsigned, int, bool, unsigned, unsigned, std::shared_ptr<replay_data_t>, unsigned, std::string > > arrival_times;
						std::vector<std::tuple<unsigned, unsigned, relay_run_data_t, std::string > > arrival_times;

						bool is_less = false;

						{
							impl_lock_guard(lock, arrival_mutex);
							if (this->arrival_times.size() < players.size())
								is_less = true;
							else
							{
								arrival_times = std::move(this->arrival_times);
								this->arrival_times.clear();
							}
						}

						if (is_less)
						{
							sent_run_data = 0;
							((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(9, room_id));

							while (sent_run_data == 0)
								sleep_for(10);

							{
								impl_lock_guard(lock, arrival_mutex);
								arrival_times = std::move(this->arrival_times);
								this->arrival_times.clear();
							}
						}

						for (unsigned i = 0; i < results.size(); ++i)
						{
							results[i].runtime -= start_time;
//							unsigned clientid = result_levels[i].clientid;
							bool found = false;
							//							unsigned arrival_tick = 0;
							//							int gold_count = 0;
							//							bool race_error = false;
							unsigned j = 0;
							for (; j < arrival_times.size(); ++j)
							{
								if (std::get<3>(arrival_times[j]) == result_levels[i].name)
								{
									found = true;
									break;
								}
							}

							float runtime = (((found ? std::get<1>(arrival_times[j]) : result_levels[i].arrival_time) - start_tick) / 1000.0f - 8.0f);

							result_levels[i].rs.tangent = 0.0f;

							if (!found)
							{
								result_levels[i].relay_crossed = false;
								result_levels[i].relay_run_data = false;
								result_levels[i].invalid_spike = 0;
							}
							else
							{
								result_levels[i].relay_run_data = true;
								result_levels[i].rrd = std::move(std::get<2>(arrival_times[j]));
								result_levels[i].relay_crossed = result_levels[i].rrd.arrived;
								result_levels[i].invalid_spike = result_levels[i].rrd.calc_tangent(result_levels[i].rs);
								result_levels[i].csv = std::make_shared<std::string>(result_levels[i].rrd.to_csv(result_levels[i].rs, start_tick));
								//14
								std::string html_message = "name = " + result_levels[i].name + "<BR>\r\n"
									+ "level = " + std::to_string(result_levels[i].level) + "<BR>\r\n"
									+ "map = " + std::to_string(this->run_map) + " " + get_map_name(this->run_map) + "<BR>\r\n"
									+ "room = " + std::to_string(this->room_id + room_id_disp) + ' ' + (is_single ? "Single" : "Team") + ' ' + (kind < 3 ? room_kind_str[kind] : "unspecified") + "<BR>\r\n"
									+ "date = " + run_date + "<BR>\r\n"
									+ "client time : " + std::to_string(results[i].runtime) + " (" + runtime2str(results[i].runtime) + ")<BR>\r\nserver time : " + std::to_string(runtime) + " (" + runtime2str(runtime) + ")<BR>\r\nrelay : " + (found ? "OK" : "not crossed finish line") + "<BR>\r\n"
									+ "tangent = " + std::to_string(result_levels[i].rs.tangent) + "<BR>\r\n"
									+ "spike = " + std::to_string(result_levels[i].invalid_spike) + "<BR>\r\n"
									+ (result_levels[i].relay_crossed ? "Crossed finish line" : "Not crossed finish line") + "<BR>\r\n";

								if (((runtime > 0.000001f && (results[i].runtime + start_time) > 0.0f && fabs(results[i].runtime - runtime) > (found ? (float)runtime_treshold : 26.0f)) || runtime < min_runtime) || fabs(result_levels[i].rs.tangent) >= tangent_treshold)
									html_message += "Possible speedhack<BR>\r\n";
								if (result_levels[i].csv && result_levels[i].invalid_spike > 0)
									html_message += "Possible spike-bug<BR>\r\n";

								result_levels[i].html = std::make_shared<std::string>(result_levels[i].rrd.to_html(result_levels[i].rs, start_tick, html_message));
							}

							result_levels[i].run_client_time = (results[i].runtime/* + start_time*/);
							result_levels[i].run_server_time = runtime;

							if (((runtime > 0.000001f && (results[i].runtime + start_time) > 0.0f && fabs(results[i].runtime - runtime) > (found ? (float)runtime_treshold : 26.0f)) || runtime < min_runtime)/* || fabs(result_levels[i].rs.tangent) >= tangent_treshold*/)
							{
								if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(result_levels[i].clientid))
								{
									result_levels[i].speedhack = runtime > 0.000001f;
									player->sent_speed_report = true;
									player->last_speed_report = tick;
									std::string reason = "Possible speedhack: client time: " + std::to_string(results[i].runtime) + "(" + runtime2str(results[i].runtime) + ") server time: " + std::to_string(runtime) + "(" + runtime2str(runtime) + ") relay: " + (found ? "OK" : "not crossed finish line")
										+ " mapid = " + std::to_string(this->run_map)
										+ " tangent = " + std::to_string(result_levels[i].rs.tangent)
										+ " spike = " + std::to_string(result_levels[i].invalid_spike);
									report("System", player->name, reason, runtime > 0.000001f ? 2 : 0, result_levels[i].rrd.replay_data, result_levels[i].csv, result_levels[i].html, player->dbid, this->run_map);
									syslog_warning(player->name + ": " + reason);
								}
							}
							/*
							else if (result_levels[i].csv && result_levels[i].invalid_spike >= min_spike_report && report_spikes != 0)
							{
								if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(result_levels[i].clientid))
								{
									player->sent_speed_report = true;
									player->last_speed_report = tick;
									std::string reason = "Possible spike-bug: client time: " + std::to_string(results[i].runtime) + "(" + runtime2str(results[i].runtime) + ") server time: " + std::to_string(runtime) + "(" + runtime2str(runtime) + ") relay: " + (found ? "OK" : "not crossed finish line")
										+ " mapid = " + std::to_string(this->run_map)
										+ " tangent = " + std::to_string(result_levels[i].rs.tangent)
										+ " spike = " + std::to_string(result_levels[i].invalid_spike);
									report("System", player->name, reason, 2, result_levels[i].rrd.replay_data, result_levels[i].csv, result_levels[i].html, player->dbid, this->run_map);
									syslog_warning(player->name + ": " + reason);
								}

//								std::string filename = result_levels[i].name + '_' + datetime2str(time(0), '-', '_');
//								save_file("test_csv/" + filename + ".csv", "", std::vector<char>(result_levels[i].csv->begin(), result_levels[i].csv->end()));
//								logger << room_str() << "Saving " << "test_csv/" + filename + ".csv";
							}
							*/
							if(found)
							{
								if ((results[i].runtime + start_time) > 0.0f && (result_levels[i].speedhack || fabs(runtime - results[i].runtime) >= 0.2f))
								{
									//results[i].runtime = std::max<float>(runtime, results[i].runtime);
									if (runtime > results[i].runtime)
									{
										results[i].runtime = runtime;
										result_levels[i].used_time = 1;	//server time
									}
									else
									{
//										results[i].runtime = results[i].runtime;
										result_levels[i].used_time = 0;	//client time
									}
								}
								else
									result_levels[i].used_time = 0;	//client time
							}
							else
							{
								if ((results[i].runtime + start_time) > 0.0f)
								{
									results[i].runtime = -2000000;
									result_levels[i].used_time = 2;	//server time
								}
							}
						}

						unsigned order_limit = results.size();

						for (unsigned i = 0; i < order_limit; )
						{
							if (results[i].runtime <= 0)
							{
								--order_limit;
								std::swap(results[i], results[order_limit]);
								std::swap(result_levels[i], result_levels[order_limit]);
							}
							else
								++i;
						}

						if (order_limit > 1)
						{
							unsigned indices[8];
							for (unsigned i = 0; i < results.size(); ++i)
								indices[i] = i;

							for (unsigned j = 0; j < order_limit - 1; ++j)
							{
								for (unsigned i = order_limit - 1; i > j; --i)
								{
									if (kind != 2)
									{
										if (results[indices[i - 1]].runtime > results[indices[i]].runtime)
											std::swap(indices[i - 1], indices[i]);
									}
									else
									{
										if (result_levels[indices[i - 1]].rrd.coin_count > result_levels[indices[i]].rrd.coin_count)
											std::swap(indices[i - 1], indices[i]);
									}
								}
							}

							std::vector<race_result_t> results2 = std::move(results);
							std::vector<additional_result_data_t > result_levels2 = std::move(result_levels);
							results.resize(results2.size());
							result_levels.resize(results2.size());

							for (unsigned i = 0; i < results.size(); ++i)
							{
								results[i] = std::move(results2[indices[i]]);
								result_levels[i] = std::move(result_levels2[indices[i]]);
							}
						}

						for (unsigned i = 0; i < arrival_times.size(); ++i)
							logger << room_str() << "Arrival " << i << ' ' << std::get<3>(arrival_times[i]) << '(' << std::dec << (int)std::get<0>(arrival_times[i]) << std::dec << ')';

						for (unsigned i = 0; i < results.size(); ++i)
							logger << room_str() << "Runtime " << i << ' ' << result_levels[i].name
							       << '(' << std::dec << (int)result_levels[i].clientid << std::dec << "): " << result_levels[i].run_client_time << ' ' << result_levels[i].run_server_time << (result_levels[i].relay_crossed ? " crossed" : " not crossed") << " rundata: " << (result_levels[i].relay_run_data ? " yes" : " no") << " tangent: " << result_levels[i].rs.tangent << " spike: " << result_levels[i].invalid_spike;
					}

					long long acquired_sp[8];
					for (unsigned i = 0; i < 8; ++i)
						acquired_sp[i] = 0;

					if (results.size() > 1 && kind == 0 && is_single != 0 && this->password == "")
					{
						std::vector<connection_t *> players;
						unsigned player_index[8];


						const double K = 32.0;
						double r[8];
						double E[8];
						double S[8];

						unsigned crossed_count = 0;

						for (int i = 0; i < results.size(); ++i)
							if (connection_t * player = gamePacketHandler->get_client(result_levels[i].id))
							{
								r[players.size()] = player->skill_points;
								player_index[players.size()] = i;
								players.push_back(player);

								if (results[i].runtime + start_time > 0)
									++crossed_count;
							}

//						double match_count = players.size() * (players.size() - 1) / 2.0;


						if (crossed_count > 0)
						{

							for (int i = 0; i < players.size(); ++i)
							{
								E[i] = 0.0;
								for (unsigned j = 0; j < players.size(); ++j)
									if (j != i)
										E[i] += 1.0 / (1.0 + pow(10.0, (r[j] - r[i]) / 400.0));

//								E[i] /= match_count;
								S[i] = (players.size() - (i + 1));// / match_count;
							}

							for (int i = 0; i < players.size(); ++i)
							{
								r[i] = r[i] + K * (S[i] - E[i]);

								long long ullri = (long long)r[i];
								long long sp_diff = 0;
								auto skill_points_pre = players[i]->skill_points;
								if (players[i]->skill_points != ullri)
								{
									sp_diff = ullri - (long long)players[i]->skill_points;
									acquired_sp[player_index[i]] = sp_diff;

									players[i]->skill_points = ullri;
									if (players[i]->skill_points < 0)
										players[i]->skill_points = 0;

									query("UPDATE characters SET skill_points = " + to_query(players[i]->skill_points) + ", skill_season_id = " + to_query(current_season_id) + " WHERE id = " + to_query(players[i]->dbid) + " AND skill_season_id <= " + to_query(current_season_id));

								}

								logger << room_str() << "Points: " << player_index[i] << ' ' << result_levels[player_index[i]].name << ": " << skill_points_pre << (sp_diff >= 0 ? "+" : "") << sp_diff << " S[" << i << "]=" << S[i] << " E[" << i << "]=" << E[i];
							}

							double sum_E = 0.0, sum_S = 0.0;
							for (int i = 0; i < players.size(); ++i)
								sum_E += E[i];
							for (int i = 0; i < players.size(); ++i)
								sum_S += S[i];

							logger << room_str() << "SUM E = " << sum_E << " SUM S = " << sum_S;
						}
					}

					for (unsigned i = 0; i < results.size(); ++i)
					{

						results[i].runtime += start_time;
						{
							if (connection_t * player = gamePacketHandler->get_client(result_levels[i].id))
							{
								if (!replay_data && result_levels[i].relay_run_data)
								{
									//14
									query("INSERT INTO runresults SET player_id=" + to_query(player->dbid) + ", position=" + to_query(i) + ", player_name=" + to_query(player->name) + ", player_time_client=" + to_query(result_levels[i].run_client_time) + ", player_time_server=" + to_query(result_levels[i].run_server_time) + ", used_time=" + to_query(result_levels[i].used_time) + ", player_skillpoints=" + to_query(player->skill_points) + ", player_skillpoints_change=" + to_query(acquired_sp[i]) + ", fk_run_id=" + to_query(run_id) + ", speedhack=" + to_query(result_levels[i].speedhack ? 1 : 0) + ", avatar=" + to_query(player->avatar));
								}

								if (!replay_data && !result_levels[i].speedhack && result_levels[i].relay_run_data)
								{
//									query("INSERT INTO Runresults SET player_id=" + to_query(player->dbid) + ", position=" + to_query(i) + ", player_name=" + to_query(player->name) + ", player_time_client=" + to_query(result_levels[i].run_client_time) + ", player_time_server=" + to_query(result_levels[i].run_server_time) + ", used_time=" + to_query(result_levels[i].used_time) + ", player_skillpoints=" + to_query(player->skill_points) + ", player_skillpoints_change=" + to_query(acquired_sp[i]) + ", fk_run_id=" + to_query(run_id));

									std::tuple<unsigned, unsigned> t = get_exp_gold(players.size(), i, kind, is_single);
									unsigned reward_exp = std::get<0>(t) * (100 + results[i].bonus_exp) / 100;
									unsigned reward_money = std::get<1>(t) * pro_mult;
									//player->reward_map = this->run_map;
									std::shared_ptr<replay_data_t> reward_replay = std::move(result_levels[i].rrd.replay_data);
									std::shared_ptr<std::string> csv = result_levels[i].csv;
									std::shared_ptr<std::string> html = result_levels[i].html;

									/*
									if(csv)
									{
										std::string filename = result_levels[i].name;
										for (char & ch : filename)
											if (ch == '[' || ch == ']')
												ch = '_';
										save_file("replays/" + filename + "_last.csv", "", std::vector<char>(csv->begin(), csv->end()));
									}
									*/

									if (html)
									{
										std::string html_dir;
										{
											impl_lock_guard(l, version_string_mutex);
											html_dir = ::html_dir;
										}
										std::string filename = result_levels[i].name;
										for (char & ch : filename)
											if (ch == '[' || ch == ']')
												ch = '_';
										save_file(html_dir + "/" + filename + "_last.html", "", std::vector<char>(html->begin(), html->end()));
									}

									float reward_time = -1;
									if (results[i].runtime > 0 && result_levels[i].relay_crossed)
										reward_time = results[i].runtime - start_time;

									unsigned reward_cash = 0;

									if (this->password.size() == 0)
										reward_cash = get_reward_cash(players.size(), i);

									//player->reward_mode = kind;
									float run_client_time = result_levels[i].run_client_time;
									float run_server_time = result_levels[i].run_server_time;
									unsigned reward_runkind = 0;
									if (is_solo_run)
										reward_runkind = 0;
									else
									{
										if (result_levels[i].relay_crossed)
											reward_runkind = 1;
										else
											reward_runkind = 2;
									}
									if (this->kind == 2 && !is_solo_run)
									{
										coins_collected.add(result_levels[i].rrd.coin_count);//16
										reward_money = result_levels[i].rrd.coin_count * pro_mult;
									}

									//----------/*99*/
									bool recieved_daily_cash = false;
									if (player->last_daily_cash / day_seconds < now / day_seconds)
									{
										reward_cash += daily_cash;
										player->last_daily_cash = now;
										recieved_daily_cash = true;
									}


									std::string query_str;
									unsigned query_str_count = 0;

									if (results.size() > 1 && kind == 0 && is_single != 0 && this->password == "")
									{
										if (query_str_count > 0)
											query_str += ", ";
										query_str += "spruns = spruns + 1 ";
										++query_str_count;
									}

									if (reward_cash > 0)
									{
										if (query_str_count > 0)
											query_str += ", ";
										query_str += "cash = cash + " + to_query(reward_cash) + (!recieved_daily_cash ? "" : (", last_daily_cash = " + to_query(now))) + " ";
										++query_str_count;

//										query("UPDATE characters SET cash = cash + " + to_query(reward_cash) +
//											(!recieved_daily_cash ? "" : (", last_daily_cash = " + to_query(now)))
//											+ " WHERE id = " + to_query(player->dbid));


										send_whisper(player->dbid, "System", "You have recieved " + std::to_string(reward_cash) + " psbocash");
									}

									reward_money = std::max<float>(0.0f, reward_money * pro_mult);
									/*2*/
									logger << room_str() << "Rewards for " << player->name
										   << '(' << std::dec << (int)player->id << std::dec << "): " << reward_exp << ' ' << reward_money << ' ' << reward_cash;

									if (reward_time > 0.0f && this->kind == 0)
										add_runtime(player->id, this->run_map, reward_time, result_levels[i].rrd.trick_points, run_client_time, run_server_time, reward_runkind, reward_replay, csv, html);

									if (i < 3 && i != results.size() - 1 && run_client_time >= 0)
									{
										std::pair<unsigned, unsigned> it = get_event_item(this->run_map, i, results.size());
										if (it.second != 0)
										{
											//3
											unsigned itemid = it.first;
											unsigned daycount = it.second;
											if (itemid < itemlist.size() && itemlist[itemid].valid)
											{
												std::string _room_str = room_str();
												std::string player_name = player->name;
												send_whisper(player->dbid, "System", "You have recieved " + itemlist[itemid].name);
												announce(player->name + " recieved " + itemlist[itemid].name);
												logger << _room_str << "Event item: " << player->name << " recieved " << itemlist[itemid].name << " (" << itemid << ")";

												//15
												query<std::tuple<> >("INSERT INTO gifts SET PlayerID = " + to_query(player->dbid) + ", ItemID = " + to_query(itemid + itemid_disp + 1) + ", message = " + to_query("Event reward") + ", sender = " + get_map_name(this->run_map) + ", DayCount = " + to_query(daycount), [_room_str, itemid, player_name](std::vector<std::tuple<> > const &, bool success)
												{
													logger << _room_str << "Sending gift " << itemid << " to player " << player_name << ' ' << (success ? "ok" : "fail");
												});
											}
											else
												logger << room_str() << "Invalid item or id:" << it.first;
										}
									}


									//client.eligible_for_reward = false;
									//reward_exp = std::max<float>(0.0f, reward_exp * exp_mult);
									if (reward_exp > 0 || reward_money > 0)
									{
										//					if(client.level < exp_list.size())
										player->exp += (unsigned long long)reward_exp;
										unsigned start_level = player->level;
										while (player->level < exp_list.size() && player->exp >= exp_list[player->level - 1])
										{
											++player->level;
											std::pair<unsigned, unsigned> levelup_msg_data = get_levelup_msg_data(player->level);
											send_message(player->name, 0, std::to_string(player->level), std::to_string(levelup_msg_data.first), std::to_string(levelup_msg_data.second), "");
											if (player->level == 2)
												send_message(player->name, 1, "", "", "", "");
										}

										if (player->level >= exp_list.size())
										{
											player->level = exp_list.size();
											//						client.exp = 0;
										}

										unsigned pro = add_pro(player->id, reward_money);
										//query("UPDATE characters SET level = " + to_query(player->level) + ", experience = " + to_query(player->exp) + ", pro = " + to_query(pro) + " WHERE id = " + to_query(player->dbid));
										if (query_str_count > 0)
											query_str += ", ";
										query_str += "level = " + to_query(player->level) + ", experience = " + to_query(player->exp) + ", pro = " + to_query(pro) + " ";
										++query_str_count;

										if (player->level != start_level && player->guild_id != 0)
											set_level_in_guild(player->guild_id, player->name, player->level);
									}
									//--------------

									if (players.size() > 1)
									{
										if (player->guild_id != 0 && players.size() > 3 && i < 3)
										{
											add_guild_points(player->guild_id, results.size() - i - 3, player->dbid);
										}

										if (i < (players.size() > 3 ? 4 : 1))
										{
											switch (kind)
											{
											case 0:	//race
												++player->race_point;
												//query("UPDATE characters SET race_point = " + to_query(player->race_point) + " WHERE id = " + to_query(player->dbid));
												if (query_str_count > 0)
													query_str += ", ";
												query_str += "race_point = " + to_query(player->race_point) + " ";
												++query_str_count;
												break;
											case 1:	//battle
												++player->battle_point;
												//query("UPDATE characters SET battle_point = " + to_query(player->battle_point) + " WHERE id = " + to_query(player->dbid));
												if (query_str_count > 0)
													query_str += ", ";
												query_str += "battle_point = " + to_query(player->battle_point) + " ";
												++query_str_count;
												break;
											}
										}
										else if (i > (players.size() > 3 ? 2 : 0))
										{
											switch (kind)
											{
											case 0:	//race
												--player->race_point;
												//query("UPDATE characters SET race_point = " + to_query(player->race_point) + " WHERE id = " + to_query(player->dbid));
												if (query_str_count > 0)
													query_str += ", ";
												query_str += "race_point = " + to_query(player->race_point) + " ";
												++query_str_count;
												break;
											case 1:	//battle
												--player->battle_point;
												//query("UPDATE characters SET battle_point = " + to_query(player->battle_point) + " WHERE id = " + to_query(player->dbid));
												if (query_str_count > 0)
													query_str += ", ";
												query_str += "battle_point = " + to_query(player->battle_point) + " ";
												++query_str_count;
												break;
											}
										}
									}


									if (query_str_count > 0)
									{
										query("UPDATE characters SET " + query_str + " WHERE id = " + to_query(player->dbid));
									}

								}
							}
						}

						if (results[i].runtime > 0 && !result_levels[i].speedhack && !replay_data)
						{
							unsigned gem = gen_gem(i, results.size(), result_levels[i].level);
							if (gem != 0)
							{
								gem_acquire_count.add();
								results[i].acquired_ore = gem;
								add_gem(result_levels[i].clientid, gem);
								//								announce_packet(result_levels[i].clientid, "<Format,2,Color,0,0,255,0> You have acquired a gem");
								//								if (connection_t * player = gamePacketHandler->get_client(result_levels[i].id))
								//									player->message("You have acquired a gem ");
							}
							else
								results[i].acquired_ore = 0xffffffff;
						}
						else
							results[i].acquired_ore = 0xffffffff;
					}


					for (unsigned i = 0; i < results.size(); ++i)
					{
						if (connection_t * player = gamePacketHandler->get_client(result_levels[i].id))
						{
							long long sp_diff = acquired_sp[i];
							if (sp_diff >= 0)
								send_whisper(player->dbid, "System", "Skill points: +" + std::to_string(sp_diff));
							else
								send_whisper(player->dbid, "System", "Skill points: " + std::to_string(sp_diff));
						}
					}

				}

				distribute(Network::packet(0x02f4e1aa, results));
				//				if (uses_relay)
				{
					for (Network::session_id_t id : players)
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(id))
						{
							player->requested_relay_port = false;
						}
					room_state = relay_stop;
					((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(1, room_id));
				}
				//				else
				//				{
				//					room_state = waiting;
				//					gamePacketHandler->racing_rooms.erase(room_id);
				//					gamePacketHandler->recruitting_rooms.insert(room_id);
				//				}
				clear_ready_state();
				check_ready_state();
				if (can_run())
					gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
				else
					gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
			}
		}


		void crossed_finish_line(connection_t & client, race_result_t && result)
		{
			if (room_state == running && !client.crossed_finish_line)
			{
				client.crossed_finish_line = true;
				//				if (is_solo_run)
				//					solo_arrival_time = get_tick();

				//result.bonus_exp = (unsigned)(int)((((trickPacketHandler->get_quest_exp(client.id) + 100) / 100.0f) * std::max<float>(1.0f, exp_mult)) * 100.0f) - 100;
				result.bonus_exp = get_quest_exp(client.id) + (unsigned)(std::max<int>(100, exp_mult * 100) - 100);
				//result.unk12 = ts[0];
				result.name = client.name;
				results.emplace_back(std::move(result));
				result_levels.emplace_back(&client, client.ptime);
				result_levels.back().runtime = results.back().runtime;
//				result_levels.back().race_error = false;
				results.back().acquired_ore = 0xffffffff;

				if (players.size() <= results.size())
					send_results();
				else if (!race_end_timer && players.size() > 1)
				{
					race_end_timer = true;
					race_end_time = get_tick();
					gamePacketHandler->timeout_rooms.push_back(room_id);
				}
			}
		}


		void start_run2()
		{
			room_state = running;
			start_tick = get_tick();
			start_time = (start_tick - gamePacketHandler->server_start_tick) / 1000.0f + 8.0f; //starts in +8 seconds
			distribute(Network::packet(0x0fb696ca, start_time));

			if (replay_data)
				((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(8, room_id, start_tick));
			else
				((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(7, room_id, start_tick));

//			if (log_relay || ::debug_build)
			{
				logger << room_str() << PINK << "Run starts at: " << start_time << "\r\n";
			}
		}

		void loaded(connection_t & client, unsigned char state, stream_array<char> const & unk1)
		{
			if (!client.run_loaded)
			{
				client.run_loaded = true;
				++loaded_count;
				distribute(Network::packet(0x07d6076a, client.name, state, unk1));
				if (loaded_count >= players.size())
					start_run2();
			}
		}

		void ready_run(connection_t & client, ingame_charinfo_t && current_character)
		{
			//client.run_loaded = false;
			//client.crossed_finish_line = false;
			//ingame_charinfo.push_back(std::move(current_character));
			//ingame_charinfo.emplace_back();
			//ingame_charinfo.back().name = client.charName;
			/*
			if (ingame_charinfo.size() == players.size())
			{
			room_state = loading;
			loaded_count = 0;
			unsigned seed = time(0);
			distribute(Network::packet(0x09ccdeda, seed, run_map, ingame_charinfo));	//starts loading the map
			}
			*/

			client.send(Network::packet(0x09ccdeda, seed, run_map, ingame_charinfo));
		}

		void chat(connection_t & client, fixed_stream_array<char, 91> const & text)
		{
			if (players.size() > 0)
				distribute(Network::packet(0x0e63d13a, client.id, text), client.session_id);

			if (client.session_id == host)
			{
				check_ready_state();
				if (can_run())
				{
					gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
					gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
				}
			}

		}

		void on_relay_room_disband()
		{
			//			if (uses_relay)
			{
				//				uses_relay = false;
				relay_ready_count = 0;
				gamePacketHandler->racing_rooms.erase(room_id);
				if (players.size() > 0)
				{
					room_state = waiting;
					gamePacketHandler->recruitting_rooms.insert(room_id);

					for (Network::session_id_t id : players)
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(id))
						{
							player->connected_to_relay = false;

							if (player->requested_relay_port)
							{
								player->requested_relay_port = false;
								player->sent_relay_port = true;
								player->send(Network::packet(0x02e49f7a, relay_port));
							}
							else
								player->sent_relay_port = false;
						}


				}
			}
		}

		bool on_connected_to_relay(unsigned clientid, stream_array<char> const & name)
		{
			//			if (!uses_relay || (room_state != relay_startup && room_state != connection_test))
			//				return;

			if (GamePacketHandler::connection_t * client = gamePacketHandler->get_client_by_id(clientid))
			{
				if (!client->connected_to_relay)
				{
					client->connected_to_relay = true;
					++relay_ready_count;
					//					if (relay_ready_count >= players.size() && room_state == relay_startup)
					//					{
					//						distribute(Network::packet(0x0b5b86fa));
					//						room_state = getting_ready;
					//					}
					return true;
				}
			}
			return false;
		}

		void start_run(bool forced = false)
		{
			if (room_state == waiting)
			{
				if (replay_data && players.size() > 7)
				{
					message("Can't start replay in a full room");
					return;
				}

				if (!forced)
				{
					if (!can_run())
					{
						for (Network::session_id_t id : players)
						{
							if (id == host)
								continue;
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(id))
								if (!player->room_ready)
									player->send(Network::packet(0x05720bdb));
						}

						if (relay_ready_count < players.size())
							message(std::to_string(players.size() - relay_ready_count) + " player(s) haven't connected to relay yet, please start later");
						//distribute(Network::packet(0x05720bdb), host);
						return;
					}
				}
				else
				{
					if (!can_run2())
					{
						for (Network::session_id_t id : players)
						{
							if (id == host)
								continue;
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(id))
								if (!player->room_ready)
									player->send(Network::packet(0x05720bdb));
						}
						if (relay_ready_count < players.size())
							message(std::to_string(players.size() - relay_ready_count) + " player(s) haven't connected to relay yet, please start later");
						//distribute(Network::packet(0x05720bdb), host);
						return;
					}
				}


				//1
				{
					std::vector<Network::session_id_t> _players = players;
					for (Network::session_id_t id : _players)
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(id))
						{
							if (!player->check_trick())
							{
								leave(*player);
							}
						}
					}
				}


				is_solo_run = false;// players.size() < 2;
									//				uses_relay = false;
				run_cancelled = false;
				race_end_timer = false;
				//				relay_ready_count = 0;
				seed = time(0);
				endrace_request_count = 0;
				//				connection_test_result_count = 0;
				ingame_charinfo.clear();

				if (mapid >= 1000)
				{
					impl_lock_guard(l, mapids_mutex);
					switch (kind)
					{
					case 1:
					{
						unsigned pos = std::max<int>(0, rng::gen_uint(0, ::mapids_battle.size() - 1));
						run_map = ::mapids_battle[pos];
						if (run_map == last_run_map)
							run_map = ::mapids_battle[(pos + 1) % mapids_battle.size()];
					}
					break;
					case 2:
					{
						unsigned pos = std::max<int>(0, rng::gen_uint(0, ::mapids_coin.size() - 1));
						run_map = ::mapids_coin[pos];
						if (run_map == last_run_map)
							run_map = mapids_coin[(pos + 1) % mapids_coin.size()];
					}
					break;
					case 0:
					default:
					{
						unsigned pos = std::max<int>(0, rng::gen_uint(0, ::mapids.size() - 1));
						run_map = ::mapids[pos];
						if (run_map == last_run_map)
							run_map = mapids[(pos + 1) % mapids.size()];
					}
					}
				}
				else
					run_map = mapid;
				last_run_map = run_map;

//				sent_run_data = 0;
				{
					impl_lock_guard(lock, arrival_mutex);
					arrival_times.clear();
					arrival_times.reserve(8);
				}

				{
					std::vector<Network::session_id_t> _players = players;
					for (Network::session_id_t id : _players)
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(id))
						{
							//						player->connected_to_relay = false;
							player->sent_connection_test_result = false;
							player->sent_uhp_test_result = false;
							//player->eligible_for_reward = false;
							player->crossed_finish_line = false;
							player->endrace_request = false;
							player->run_loaded = false;
							player->loaded_dummy = false;
							//							player->connection_test_result.clear();
							//							player->uhp_test_result.clear();
							player->dummy_increased = false;
							//player->reward_replay.reset();

							ingame_charinfo.emplace_back();
							ingame_charinfo.back().str1 = player->name;
							ingame_charinfo.back().name = player->name;

							if (!get_stats(player->id, ingame_charinfo.back()))
							{
								std::string msg = "Failed to load stats for player " + player->name;
								logger << room_str() << msg;
								//this->message(msg);
								ingame_charinfo.pop_back();
								leave(*player);
								continue;
							}

							if (!replay_data)
							{
								replay_data_t rd;
								rd.name = player->name;
								rd.room_slot = ingame_charinfo.size() - 1;
								rd.room_kind = kind;
								rd.mapid = run_map;
								rd.is_single = is_single;
								rd.server_start_tick = gamePacketHandler->server_start_tick;
								rd.stats = ingame_charinfo.back().stats;
								rd.avatar = player->avatar;
								rd.level = player->level;
								rd.guild_name = player->guild_name;
								if (player->avatar - 2 < (unsigned)4)
									rd.haircolor = player->hair_color[player->avatar - 2];
								else
									rd.haircolor = 0;
								get_replay_item_data(player->id, rd);
								((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(5, player->id, room_id, (char)1, rd));
							}
						}
					}
				}

				if (ingame_charinfo.size() == 0)
					return;

				dummy_slot = 0;

				logger << room_str() << "Race start room" << this->room_id << " mapid = " << this->run_map << '(' << this->mapid << ')';
				//16
				if (replay_data)
				{
					if (replay_data->room_slot >= ingame_charinfo.size())
					{
						ingame_charinfo.emplace_back();
						ingame_charinfo.back().str1 = "*" + replay_data->name;
						ingame_charinfo.back().name = "*" + replay_data->name;
						ingame_charinfo.back().stats = replay_data->stats;
						dummy_slot = ingame_charinfo.size() - 1;
					}
					else
					{
						ingame_charinfo.emplace_back();
						for (unsigned i = ingame_charinfo.size() - 1; i > replay_data->room_slot; --i)
							ingame_charinfo[i] = std::move(ingame_charinfo[i - 1]);
						ingame_charinfo[replay_data->room_slot].str1 = "*" + replay_data->name;
						ingame_charinfo[replay_data->room_slot].name = "*" + replay_data->name;
						ingame_charinfo[replay_data->room_slot].stats = replay_data->stats;
						dummy_slot = replay_data->room_slot;
					}

					room_state = getting_ready_solo;
					replay_count.add();
				}
				else
				{
					if (players.size() == 1)
					{
						room_state = getting_ready_solo;
						ingame_charinfo.push_back(ingame_charinfo_t::make_empty("     "));
					}
					else
						room_state = loading;

					if (players.size() == 1)
						solo_race_count.add();

					switch (kind)
					{
					case 0:
						race_mode_count.add();
						break;
					case 1:
						battle_mode_count.add();
						break;
					case 2:
						coin_mode_count.add();
						break;
					}
				}

				race_start_count.add();


				gamePacketHandler->recruitting_rooms.erase(room_id);
				gamePacketHandler->racing_rooms.insert(room_id);

				loaded_count = 0;
				loaded_dummy_count = 0;
				results.clear();
				result_levels.clear();


#ifdef _DEBUG
				logger << room_str() << PINK << "Connection test\r\n";
#endif

				connection_test_time = get_tick();
				connection_test_state = 0;



				if (room_state == getting_ready_solo)
				{
					bool increased;
					if (replay_data)
						increased = join_dummy("*" + replay_data->name, replay_data->level, replay_data->guild_name);
					else
						increased = join_dummy();
					distribute(Network::packet(0x05720bda));/*9*/		//start connection test
					for (Network::session_id_t id : players)
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(id))
							player->dummy_increased = increased;
				}
				else
				{
					distribute(Network::packet(0x05720bda));/*9*/		//start connection test
																		/*
																		std::vector<unsigned> data;
																		data.reserve(players.size());

																		for (Network::session_id_t id : players)
																		{
																		auto j = gamePacketHandler->connections.find(id);
																		if (j != gamePacketHandler->connections.end())
																		data.push_back(j->second.id);
																		}

																		distribute(Network::packet(0x05153d4a, data));

																		distribute(Network::packet(0x0b5b86fa));
																		*/
				}
			}

		}

		void on_dummy_displayed(connection_t & client)
		{
			if (room_state == getting_ready_solo)
			{
				if (!client.loaded_dummy)
				{
					client.loaded_dummy = true;
					++this->loaded_dummy_count;
				}
				if (this->loaded_dummy_count >= players.size())
					all_dummy_displayed();
			}
		}

		void all_dummy_displayed()
		{
			room_state = loading;

			std::vector<unsigned> data;
			data.reserve(players.size());

			std::vector<Network::session_id_t> players2;
			players2.reserve(players.size());

			for (Network::session_id_t id : players)
			{
				auto j = gamePacketHandler->connections.find(id);
				if (j != gamePacketHandler->connections.end())
				{
					data.push_back(j->second.id);
					if (j->second.sent_connection_test_result)
						players2.push_back(id);
				}
			}

			data.push_back(0x7fffffff);

			gamePacketHandler->sendTo(players2, Network::packet(0x05153d4a, data));
			gamePacketHandler->sendTo(players2, Network::packet(0x0b5b86fa));

			if (replay_data)
				((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(6, room_id, dummy_slot));
		}

		void solo_run_start(connection_t & client)
		{
			if (room_state != waiting && !client.sent_connection_test_result)
			{
				client.sent_connection_test_result = true;
				if (room_state == loading)
				{
					std::vector<unsigned> data;

					for (Network::session_id_t id : players)
					{
						auto j = gamePacketHandler->connections.find(id);
						if (j != gamePacketHandler->connections.end())
							data.push_back(j->second.id);
					}

					data.push_back(0x7fffffff);

					client.send(Network::packet(0x05153d4a, data));
					client.send(Network::packet(0x0b5b86fa));
				}
			}
		}

		void connection_test_response(connection_t & client, stream_array<unsigned> const *)
		{
			solo_run_start(client);
			/*
			if (room_state == connection_test)
			{
			if (!client.sent_connection_test_result)
			{
			client.sent_connection_test_result = true;
			//					client.connection_test_result.assign(clients->begin(), clients->end());

			if(players.size() > 1)
			{
			//						uses_relay = true;

			std::vector<unsigned> data;
			data.reserve(players.size());

			for (Network::session_id_t id : players)
			{
			auto j = gamePacketHandler->connections.find(id);
			if (j != gamePacketHandler->connections.end())
			data.push_back(j->second.id);
			}

			client.send(Network::packet(0x05153d4a, data));
			}

			++connection_test_result_count;
			if (connection_test_result_count == players.size())
			{
			connection_test_end();
			}
			}
			}
			*/
		}

		void connection_test_end()
		{
			//assert(room_state == connection_test);
			/*
			//unsigned game_ip = Network::string2ipv4(get_config("game_ip"));

			room_state = uhp_test;

			std::vector<unsigned> data;
			for (unsigned id : players)
			{
			auto j = gamePacketHandler->connections.find(id);
			if (j != gamePacketHandler->connections.end())
			data.push_back(j->second.dwip);
			}
			distribute(Network::packet(0x092935ea, data, game_ip, (short)ctpSendPort, game_ip, (short)ctpRecvPort));
			*/


			//			for (unsigned i = 0; i < players.size(); ++i)
			//			{
			//				gamePacketHandler->sendTo(players[i], Network::packet(0x05153d4a, std::vector<unsigned>()));
			//			}

			//			uhp_test_end();

			//			if (!uses_relay)
			//			{
			//				distribute(Network::packet(0x0b5b86fa));
			//				room_state = getting_ready;
			//			}
			//			else
			{
				//				if(relay_ready_count < players.size())
				//					room_state = relay_startup;
				//				else
				{
					/*
					distribute(Network::packet(0x0b5b86fa));
					//					room_state = getting_ready;
					room_state = loading;
					*/
				}
			}
		}

		void uhp_test_response(connection_t & client, stream_array<unsigned> const & clients)
		{
			/*
			if (room_state == uhp_test)
			{
			if (!client.sent_uhp_test_result)
			{
			client.sent_uhp_test_result = true;
			client.uhp_test_result.assign(clients.begin(), clients.end());

			++uhp_test_result_count;
			if (uhp_test_result_count == players.size())
			uhp_test_end();
			}
			}
			*/
		}

		void uhp_test_response(connection_t & client)
		{
			/*
			if (room_state == uhp_test)
			{
			if (!client.sent_uhp_test_result)
			{
			client.sent_uhp_test_result = true;

			++uhp_test_result_count;
			if (uhp_test_result_count == players.size())
			uhp_test_end();
			}
			}
			*/
		}

		void uhp_test_end()
		{
			//			assert(room_state == uhp_test);
			//			bool need_relay = false;
			/*
			for (unsigned i = 0; i < players.size(); ++i)
			{
			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(players[i]))
			{
			if (player->uhp_test_result.size() > 0)
			{
			need_relay = true;
			break;
			}
			}
			}
			*/

			//			need_relay = players.size() > 1;
			//
			//			if (!need_relay)
			//			{
			//				distribute(Network::packet(0x0b5b86fa));
			//				room_state = getting_ready;
			//			}
			//			else
			//			{
			/*
			if (disable_relay == 0)
			{
			std::vector<unsigned> data;
			data.reserve(players.size());

			for (Network::session_id_t id : players)
			{
			auto j = gamePacketHandler->connections.find(id);
			if (j != gamePacketHandler->connections.end())
			data.push_back(j->second.id);
			}

			distribute(Network::packet(0x05153d4a, data));
			}
			else if (disable_relay == 1)
			{
			distribute(Network::packet(0x05153d4a, std::vector<unsigned>()));
			}
			*/
			//				uses_relay = true;
			//				if (!disable_relay)
			//					distribute(Network::packet(0x02e49f7a, get_config<unsigned short>("relay_port")));
			//				room_state = relay_startup;
			//			}

		}


		void set_map(unsigned mapid)
		{
			if (room_state == waiting)
			{
				if (replay_data)
					distribute(Network::packet(0x000c50ba, this->mapid));
				else
				{
					logger << room_str() << "Map set for room" << this->room_id << " to " << mapid;
					this->mapid = mapid;
					distribute(Network::packet(0x000c50ba, mapid));

					check_ready_state();
					if (can_run())
					{
						gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
						gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
					}
				}
			}
		}

		room_t()
		{
			results.reserve(8);
			result_levels.reserve(8);
			players.reserve(8);
			ingame_charinfo.reserve(8);
		}

		void change_team(connection_t & client, unsigned team_id)
		{
			if (!is_single && team_id <= 2)
			{
				if (client.team_id == 1)
					--red_count;
				else if (client.team_id == 2)
					--blue_count;

				client.team_id = team_id;

				if (client.team_id == 1)
					++red_count;
				else if (client.team_id == 2)
					++blue_count;

				distribute(Network::packet(0x018b747a, client.id, team_id), client.session_id);

				check_ready_state();
				if (can_run())
					gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
				else
					gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
			}
		}

		bool open(connection_t & client, unsigned char is_single, unsigned char kind, unsigned char player_limit, std::string && password, std::shared_ptr<replay_data_t> rd = nullptr)
		{
			if (client.room)
				client.room->leave(client);

			client.room_x = 0.0f;
			client.room_y = 0.0f;
			client.room_dir = 6;
			client.room_unk1 = 0;

			players.clear();
			mapid = 0x03e9;

			race_end_timer = false;
			red_count = 0;
			blue_count = 0;
			this->is_single = is_single;
			this->kind = kind;
			this->player_limit = player_limit;
			this->password = std::move(password);
			this->replay_data = rd;

			players.push_back(client.session_id);
			client.room = this;
			host = client.session_id;
			host_id = client.id;
			client.roomtime_set = false;
			client.room_ready = false;
			//client.eligible_for_reward = false;
			client.connected_to_relay = false;
			client.sent_relay_port = false;
			client.requested_relay_port = false;
			client.dummy_increased = false;
			//client.reward_replay.reset();
			relay_ready_count = 0;
			last_run_map = ~0;

			client.mm_room_id = ~0;

			room_state = waiting;
			if (is_single)
				client.team_id = team::none;
			else
			{
				client.team_id = team::red;
				++red_count;
			}

			if (replay_data)
				this->mapid = replay_data->mapid;

			gamePacketHandler->recruitting_rooms.insert(room_id);

			client.send(Network::packet(0x01c9d71a, this->get_data(), 0, 0, 0, 0));
			client.send(Network::packet(0x08f6f67a, conv(client)));

			check_ready_state();
			if (can_run())
				gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
			else
				gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));

			on_host_changed();

			//client.send(Network::packet(0x02e49f7a, relay_port));


			return true;
		}

		room_packet_data_t get_data() const
		{
			room_packet_data_t r;
			r.is_single = is_single;
			r.kind = kind;
			r.player_limit = player_limit;
			r.has_password = (this->password.size() > 0) ? 1 : 0;
			r.password = str2array<13>(this->password);
			r.room_id = this->room_id + room_id_disp;
			r.mapid = this->mapid;
			r.unk1 = 0;
			r.unk2 = 0;
			r.unk3 = 0;
			r.unk4 = 0;
			r.unk6 = 0;
			r.unk7 = 0;
			r.unk8 = 0;
			r.unk9 = 0;
			return r;
		}

		void join(connection_t & client, fixed_stream_array<char, 13> const & password)
		{
			bool trick_valid = client.check_trick();

			if (to_string(password) != this->password || !trick_valid)
			{
				if (client.player_rank < 2 || !trick_valid)
				{
					client.room_pw_response = true;
					client.room_pw_response_time = get_tick() + 1000;
					//client.send(Network::packet(0x00bf5b6a));
				}
				else
					join(client, false, true);
			}
			else
				join(client, true);
		}

		void join(connection_t & client, bool had_password = false, bool invited = false)
		{
			if (!client.check_trick())
				return;

			if (client.room)
			{
				if (client.room != this)
					client.room->leave(client);
				else
				{
					if (client.session_id == host)
					{
						check_ready_state();
						if (can_run())
							gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
						else
							gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
					}
					return;
				}
			}

			if (room_state == waiting && players.size() > 0 && client.room == nullptr)
			{
				invited |= client.room_invite_id == this->room_id && client.ptime - client.room_invite_time < 20000;
				invited |= client.player_rank >= 2;


				if (this->password.size() > 0 && (!had_password && !invited))
				{
					gamePacketHandler->sendTo(client.session_id, Network::packet(0x00bf5b6a));
					return;
				}

				if ((players.size() >= player_limit && !invited) || players.size() >= 8)
				{
					client.message("Room is full");
					if (had_password)
					{
						client.room_pw_response = true;
						client.room_pw_response_time = get_tick() + 1000;
					}
					return;
				}

				client.room_invite_id = ~0;

				if (players.size() >= player_limit)
					inc_player_limit(true);

				client.room_x = 0.0f;
				client.room_y = 0.0f;
				client.room_dir = 6;
				client.room_unk1 = 0;
				client.mm_room_id = ~0;


				players.push_back(client.session_id);
				client.room = this;
				client.room_ready = false;
				client.roomtime_set = false;
				//client.eligible_for_reward = false;
				client.connected_to_relay = false;
				client.sent_relay_port = false;
				client.requested_relay_port = false;
				client.dummy_increased = false;
				//client.reward_replay.reset();
				if (is_single)
					client.team_id = team::none;
				else
				{
					if (red_count <= blue_count)
					{
						client.team_id = team::red;
						++red_count;
					}
					else
					{
						client.team_id = team::blue;
						++blue_count;
					}
				}
				client.send(Network::packet(0x02ce8f4a, this->get_data()));
				distribute(Network::packet(0x08f6f67a, conv(client)));


				check_ready_state();
				if (can_run())
					gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
				else
					gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));

				//client.send(Network::packet(0x02e49f7a, relay_port));

				//gamePacketHandler->sendTo(client.session_id, Network::packet(0x000c50ba, mapid));
			}
			else
			{
				if (had_password)
				{
					client.room_pw_response = true;
					client.room_pw_response_time = get_tick() + 1000;
				}
			}

		}
		/*
		void re_enter(connection_t & client)
		{
			if (room_state == waiting && client.room == this)
			{
				client.send(Network::packet(0x0ebb822a, client.id));
				//12
				client.send(Network::packet(0x02ce8f4a, this->get_data()));

				client.send(Network::packet(0x08f6f67a, conv(client)));

				//check_ready_state();
				if (host == client.session_id)
				{
					if (can_run())
						gamePacketHandler->sendTo(host, Network::packet(0x0b83ec4a));
					else
						gamePacketHandler->sendTo(host, Network::packet(0x0ffa3eba));
				}
			}
		}
		*/
		bool join_dummy(std::string const & dummy_name = "     ", unsigned dummy_level = 1, std::string const & dummy_guild_name = std::string())
		{
			bool increased = false;
			if (players.size() >= player_limit)
			{
				increased = true;
				inc_player_limit(true);
			}

			unsigned dummy_team = team::none;
			if (!is_single)
			{
				if (red_count < blue_count)
					dummy_team = team::red;
				else
					dummy_team = team::blue;
			}

			distribute(Network::packet(0x08f6f67a, conv_dummy(dummy_team, dummy_name, dummy_level, dummy_guild_name)));
			return increased;
		}
		/*
		void leave_dummy()
		{
		if (this->dummy_joined)
		{
		distribute(Network::packet(0x0ebb822a, 0x7fffffff));
		this->dummy_joined = false;
		if (this->dummy_increased)
		{
		this->dummy_increased = false;
		dec_player_limit(true);
		}
		}
		}
		*/
		void get_player_data(connection_t & client)
		{
			if (!client.check_trick())
			{
				this->leave(client);
				return;
			}

			std::vector<character_packet_data_t> cd;

			cd.reserve(players.size());
			for (unsigned i = 0; i < players.size(); ++i)
			{
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(players[i]))
					cd.push_back(conv(*player));
			}

			client.send(Network::packet(0x0202864a, cd));
			if (client.session_id == host)
			{
				if (can_run())
					client.send(Network::packet(0x0b83ec4a));
				else
					client.send(Network::packet(0x0ffa3eba));
				//				client.send(Network::packet(0x0b83ec4a));
			}

			client.send(Network::packet(0x000c50ba, mapid));
			if (room_state == waiting)
			{
				if (!client.sent_relay_port)
				{
					client.sent_relay_port = true;
					client.send(Network::packet(0x02e49f7a, relay_port));
				}
			}
			else
			{
				client.requested_relay_port = true;
			}

			if (client.dummy_increased)
			{
				client.dummy_increased = false;
				client.send(Network::packet(0x0683715a));
			}
		}

		void close()
		{
			//			if (uses_relay)
			{
				((NetworkEventHandler*)relayPacketHandler.get())->dispatch(Network::packet(1, room_id));
			}
			replay_data.reset();

			gamePacketHandler->recruitting_rooms.erase(room_id);
			gamePacketHandler->racing_rooms.erase(room_id);
			gamePacketHandler->free_room_ids.push_back(room_id);

			while (!players.empty())
			{
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(players[0]))
				{
					player->room = nullptr;
					player->send(Network::packet(0x0ebb822a, player->id));
				}
				players.pop_back();
			}

			//			enum room_state_t { waiting = 0, connection_test, uhp_test, relay_startup, getting_ready, loading, running, relay_stop } room_state = waiting;
			/*
			switch (room_state)
			{
			case waiting:
			logger << room_str() << "Room close state: waiting";
			break;
			//			case connection_test:
			//				logger << room_str() << "Room close state: connection_test";
			//				break;
			//			case uhp_test:
			//				logger << room_str() << "Room close state: uhp_test";
			//				break;
			//			case relay_startup:
			//				logger << room_str() << "Room close state: relay_startup";
			//				break;
			//			case getting_ready:
			//				logger << room_str() << "Room close state: getting_ready";
			//				break;
			case loading:
			logger << room_str() << "Room close state: loading";
			break;
			case running:
			logger << room_str() << "Room close state: running";
			break;
			case relay_stop:
			logger << room_str() << "Room close state: relay_stop";
			break;
			default:
			logger << room_str() << "Room close state: unspecified";
			}
			*/
		}

		inline bool distribute(std::vector<char> && data, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			return gamePacketHandler->sendTo(players, std::move(data), skipId);
		}
		inline bool distribute(BinaryWriter && bw, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			return gamePacketHandler->sendTo(players, std::move(bw.data), skipId);
		}
	};

	std::array<room_t, 1024> rooms;
	std::vector<unsigned> free_room_ids;
	std::set<unsigned> recruitting_rooms;
	std::set<unsigned> racing_rooms;
	unsigned server_start_tick;
	std::map<std::string, std::weak_ptr<replay_data_t> > loaded_replays;

	void add_replay(std::string const & replay_name, std::shared_ptr<replay_data_t> rd)
	{
		loaded_replays[replay_name] = rd;
	}

	std::shared_ptr<replay_data_t> find_replay(std::string const & replay_name)
	{
		std::shared_ptr<replay_data_t> r;
		auto i = loaded_replays.find(replay_name);
		if (i != loaded_replays.end())
			r = i->second.lock();

		return r;
	}

	bool can_create_room(unsigned dbid)
	{
		if (!free_room_ids.empty())
			if (connection_t * player = get_client_by_dbid(dbid))
				return player->room == nullptr;

		return false;
	}

	void create_room(connection_t & client, unsigned char b_is_single, unsigned char room_kind, unsigned char player_limit, std::string && password)
	{
		if (!client.check_trick())
			return;

		if (room_kind < 3 && player_limit > 0 && player_limit <= 8 && client.room == nullptr && !free_room_ids.empty())
		{
			unsigned room_id = free_room_ids.back();
			free_room_ids.pop_back();
			rooms[room_id].open(client, b_is_single, room_kind, player_limit, std::move(password));
		}
		else
			client.message("Failed to create room");
	}

	void create_room(connection_t & client, std::shared_ptr<replay_data_t> rd)
	{
		if (!client.check_trick())
			return;

		if (rd->room_kind < 3 && client.room == nullptr && !free_room_ids.empty())
		{
			unsigned room_id = free_room_ids.back();
			free_room_ids.pop_back();
			unsigned pw = rng::gen_uint(0, ~0);
			std::string pw_str(reinterpret_cast<const char *>(&pw), 4);
			rooms[room_id].open(client, rd->is_single, rd->room_kind, 7, std::move(pw_str), rd);
		}
		else
			client.message("Failed to create room");
	}


	static character_packet_data_t conv(connection_t const & client)
	{
		character_packet_data_t cd;
		cd.dbid = client.dbid;
		cd.clientip = client.id;
		cd.exp = client.exp;
		cd.is_host = client.is_host() ? 1 : 0;
		cd.level = client.level;
		cd.name = client.name;
		cd.team_id = client.team_id;
		cd.b_pc = false;

		if (null_ips)
		{
			cd.ip1 = 0;
			cd.ip2 = 0;
			cd.ip3 = 0;
			cd.ip4 = 0;
			cd.port1 = 0;
			cd.port2 = 0;
			cd.port3 = 0;
			cd.port4 = 0;
		}
		else
		{
			switch (rs_mode)
			{
			case 1:
				cd.ip1 = client.private_recv_ip;
				cd.port1 = client.private_recv_port;
				cd.ip2 = client.private_send_ip;
				cd.port2 = client.private_send_port;

				cd.ip3 = client.public_recv_ip;
				cd.port3 = client.public_recv_port;
				cd.ip4 = client.public_send_ip;
				cd.port4 = client.public_send_port;
				break;
			case 3:
				cd.ip3 = client.private_recv_ip;
				cd.port3 = client.private_recv_port;
				cd.ip4 = client.private_send_ip;
				cd.port4 = client.private_send_port;

				cd.ip1 = client.public_recv_ip;
				cd.port1 = client.public_recv_port;
				cd.ip2 = client.public_send_ip;
				cd.port2 = client.public_send_port;
				break;
			case 0:
				cd.ip2 = client.private_recv_ip;
				cd.port2 = client.private_recv_port;
				cd.ip1 = client.private_send_ip;
				cd.port1 = client.private_send_port;

				cd.ip4 = client.public_recv_ip;
				cd.port4 = client.public_recv_port;
				cd.ip3 = client.public_send_ip;
				cd.port3 = client.public_send_port;
				break;
			case 2:
				cd.ip4 = client.private_recv_ip;
				cd.port4 = client.private_recv_port;
				cd.ip3 = client.private_send_ip;
				cd.port3 = client.private_send_port;

				cd.ip2 = client.public_recv_ip;
				cd.port2 = client.public_recv_port;
				cd.ip1 = client.public_send_ip;
				cd.port1 = client.public_send_port;
				break;
			}
		}
		cd.crew_name = client.guild_name;
		cd.room_x = client.room_x;
		cd.room_y = client.room_y;
		cd.room_dir = client.room_dir;
		cd.room_unk1 = client.room_unk1;
		if (client.room && client.room_ready)
			cd.is_not_ready = 0;
		else
			cd.is_not_ready = 1;
			return cd;
	}

	static character_packet_data_t conv_dummy(unsigned team_id = 0, std::string const & dummy_name = "     ", unsigned dummy_level = 1, std::string const & dummy_guild_name = std::string())
	{
		character_packet_data_t cd;
		cd.dbid = 0x7fffffff;
		cd.clientip = 0x7fffffff;
		cd.exp = 0;
		cd.is_host = 0;
		cd.level = dummy_level;
		cd.name = dummy_name;
		cd.team_id = team_id;

		cd.ip1 = Network::make_ipv4(127, 0, 0, 1);
		cd.ip2 = cd.ip1;
		cd.ip3 = cd.ip1;
		cd.ip4 = cd.ip1;
		cd.port1 = 1;
		cd.port2 = 2;
		cd.port3 = 1;
		cd.port4 = 2;

		cd.room_x = 6.0f;
		cd.room_y = -6.0f;
		cd.room_dir = 6;
		cd.room_unk1 = 0;
		cd.is_not_ready = 0;

		cd.crew_name = dummy_guild_name;
		return cd;
	}
	std::map<Network::session_id_t, connection_t> connections;
	std::map<unsigned, connection_t*> connections_by_dbid;
	std::map<unsigned, connection_t*> connections_by_ip;
	std::map<unsigned, connection_t*> connections_by_id;
	std::unordered_map<std::string, connection_t*> connections_by_name;

	inline connection_t * get_client_by_id(unsigned id)
	{
		connection_t * r = nullptr;
		auto i = connections_by_id.find(id);
		if (i != connections_by_id.end())
			r = i->second;
		return r;
	}

	inline connection_t * get_client_by_ip(unsigned ip)
	{
		connection_t * r = nullptr;
		auto i = connections_by_ip.find(ip);
		if (i != connections_by_ip.end())
			r = i->second;
		return r;
	}

	inline connection_t * get_client_by_dbid(unsigned dbid)
	{
		connection_t * r = nullptr;
		auto i = connections_by_dbid.find(dbid);
		if (i != connections_by_dbid.end())
			r = i->second;
		return r;
	}

	inline connection_t * get_client_by_name(std::string const & name)
	{
		connection_t * r = nullptr;
		auto i = connections_by_name.find(name);
		if (i != connections_by_name.end())
			r = i->second;
		return r;
	}

	unsigned last_playercount_update = get_tick();
	unsigned last_client_check = get_tick();
	unsigned last_chat_flush = get_tick();
	unsigned last_tm_hour = ~0;
	mutex_t timer_mutex;
	std::atomic<unsigned> disconnect_all;
	virtual void onTimer()
	{
		if (this->destroyed())
			return;
		impl_lock_guard(lock, timer_mutex);


		{
			time_t curr_time = time(0);
			tm lt = get_localtime(curr_time);

			if (lt.tm_hour != last_tm_hour)
			{
				last_tm_hour = lt.tm_hour;
				online_count_list.emplace_back(curr_time, hourly_online_count);
				hourly_online_count_is_set = false;
				hourly_online_count = 0;
				while (online_count_list.size() > 72)
					online_count_list.pop_front();

				std::ostringstream oss;
				oss << "var data_1 = [\r\n['Time', 'Online count']\r\n";

				for (auto i = online_count_list.begin(); i != online_count_list.end(); ++i)
					oss << ",[\"" << datetime2str(i->first) << "\" ," << i->second << "]\r\n";

				oss << "];\r\n";

				std::string str = oss.str();

				save_file(html_dir + "/online_count.js", "", std::vector<char>(str.begin(), str.end()));
				//2

				{
					BinaryWriter bw;
					bw << 1 << online_count_list;
					save_file("online_count.dat", "", std::move(bw.data));
				}

			}
		}

		unsigned tick = get_tick();

		if (tick - last_playercount_update >= 4 * 60000)
		{
			last_playercount_update = tick;
			query("REPLACE INTO online_player_count VALUES(0, " + to_query(connections.size()) + ", NOW())");
		}

		if (tick - last_client_check > client_check_interval)
		{
			last_client_check = tick;
			distribute(Network::packet(0));
		}

		for (auto i = connections.begin(); i != connections.end(); ++i)
		{
			if (i->second.room_pw_response && (i->second.room_pw_response_time - tick > 0x80000000))
			{
				i->second.room_pw_response = false;
				gamePacketHandler->sendTo(i->second.session_id, Network::packet(0x00bf5b6a));
			}
		}

		unsigned rrt = race_result_timeout;
		for (unsigned i = 0; i < timeout_rooms.size(); )
		{
			unsigned room_id = timeout_rooms[i];
			if (rooms[room_id].race_end_timer && rooms[room_id].players.size() > 0)
			{
				if (tick - rooms[room_id].race_end_time >= rrt)
				{
					timeout_rooms[i] = timeout_rooms.back();
					timeout_rooms.pop_back();
					rooms[room_id].on_room_timeout();
				}
				else
					++i;
			}
			else
			{
				timeout_rooms[i] = timeout_rooms.back();
				timeout_rooms.pop_back();
			}
		}

		if (tick - last_chat_flush > 60000)
		{
			last_chat_flush = tick;
			if (fchat)
				fflush(fchat);
		}

		/*
		for (unsigned i = 0; i < rooms.size(); ++i)
		{
		if (rooms[i].race_end_timer && rooms[i].players.size() > 0 && tick - rooms[i].race_end_time >= 5000)
		rooms[i].on_room_timeout();
		}
		*/

		unsigned dc_all = disconnect_all;
		switch (dc_all)
		{
		case 1:
			for (auto i = connections.begin(); i != connections.end(); ++i)
				disconnect(i->second.session_id);
			if (connections.size() == 0)
				disconnect_all = 0;
			else
				disconnect_all = 2;
			break;
		case 2:
			if (connections.size() == 0)
				disconnect_all = 0;
			break;
		}

	}

	virtual ~GamePacketHandler()
	{
		impl_lock_guard(lock, timer_mutex);
		this->_destroyed = true;
	}
	GamePacketHandler()
        : disconnect_all(0)
	{
		server_start_tick = get_tick();
		free_room_ids.reserve(rooms.size());
		for (unsigned i = 0; i < rooms.size(); ++i)
		{
			rooms[i].room_id = i;
			free_room_ids.push_back(i);
		}
	}
};


class RelayPacketHandler
	: public Network::NetworkEventHandler
{
	virtual std::string getConnectionData(Network::session_id_t id) const
	{
		auto i = connections.find(id);
		if (i != connections.end())
			return i->second.name;
		return std::string();
	}
	virtual DisconnectKind onConnect(Network::session_id_t session_id, std::string && ip, unsigned port)
	{
		unsigned dwip = Network::string2ipv4(ip);
		{
			impl_lock_guard(lock, connection_count_mutex);
			if (!connection_count[dwip].inc())
				return DisconnectKind::IMMEDIATE_DC;
		}

		connection_t & client = connections[session_id];
		client.session_id = session_id;
		client.ip = std::move(ip);
		client.port = port;
		client.dwip = dwip;
		client.ptime = get_tick();
		client.client_race_time = (client.ptime - gamePacketHandler->server_start_tick) * 3 / 20 - 100;


#ifdef _DEBUG
		logger << "Client connected to relay\r\n";
#endif
		return DisconnectKind::ACCEPT;
	}
	virtual void onRecieve(Network::session_id_t session_id, const char * data, unsigned length)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
		{
			if (!session_id)
			{
				unsigned cmd_id = 0;
				BinaryReader br(data, length);
				br.skip<Network::packet_header_t>();
				br >> cmd_id;

				switch (cmd_id)
				{
				case 1:	//disband room
				{
					unsigned room_id;
					br >> room_id;
					if (room_id < rooms.size())
						rooms[room_id].disband();
				}
				break;
				case 2:	//kick from room
				{
					unsigned room_id;
					unsigned id;
					br >> room_id >> id;
					if (room_id < rooms.size())
						rooms[room_id].kick(id);
				}
				break;
				case 3:	//kick
				{
					Network::session_id_t sid;
					br >> sid;
					disconnect(sid, DisconnectKind::IMMEDIATE_DC);
				}
				break;
				case 4:	//enable recv
				{
					Network::session_id_t sid;
					br >> sid;
					auto i = connections.find(sid);
					if (i != connections.end())
						if (i->second.dec_read_lock())
							this->callPostponed(sid);
					//enableRecv(sid);
				}
				break;
				case 5:	//replay data
				{
					unsigned clientid;
					unsigned room_id;
					std::shared_ptr<replay_data_t> rd;
					br >> clientid >> room_id >> rd;
					if (room_id < rooms.size())
					{
//						bool found = false;
						Network::session_id_t sid;
						for (auto const & p : rooms[room_id].players_by_gameid)
						{
							if (p.first == clientid)
							{
								sid = p.second;
								break;
							}
						}

						auto i = connections.find(sid);
						if (i != connections.end())
						{
							i->second.replay_data = rd;
							i->second.replay_data2 = rd;
						}
					}
					//auto i = this->connections_by_id
				}
				break;
				case 6:	//replay room
				{
					unsigned room_id, dummy_slot;
					br.read(room_id, dummy_slot);
					if (room_id < rooms.size() && room_id < gamePacketHandler->rooms.size())
						rooms[room_id].set_replay(gamePacketHandler->rooms[room_id].replay_data, dummy_slot);
				}
				break;
				case 7:	//start run time
				{
					unsigned room_id;
					unsigned start_tick;
					br.read(room_id, start_tick);
					if (room_id < rooms.size())
						rooms[room_id].set_start_run_time(start_tick);
				}
				break;
				case 8:	//start run time (replay)
				{
					unsigned room_id;
					unsigned start_tick;
					br.read(room_id, start_tick);
					if (room_id < rooms.size())
						rooms[room_id].set_start_replay_time(start_tick);
				}
				break;
				case 9:	//request run data
				{
					unsigned room_id;
					br >> room_id;
					if (room_id < rooms.size())
						rooms[room_id].send_run_data();
				}
				break;
				}
			}
			return;
		}
		connection_t & client = _i->second;
		client.check_valid();

		client.ptime = get_tick();

		BinaryReader br(data, length);
		br.skip(4);
		unsigned packetid = br.read<unsigned>();
		//		logger.print("relay packet %08x\r\n", packetid);

		if (packetid == 0x074e26ca)
		{
			if (!client.room || client.clientid == 0)
			{
				client.disconnect(DisconnectKind::IMMEDIATE_DC);
				return;
			}

			unsigned sender;
			unsigned room_id;
			stream_array<unsigned> recipients;
			stream_array<char> packet_data;

			br.read(sender, room_id);

			br >> recipients >> packet_data;

			std::vector<Network::session_id_t> recipients_ids;
			recipients_ids.reserve(recipients.size());
			for (unsigned game_id : recipients)
			{
				auto const & v = client.room->players_by_gameid;
				for (auto const & i : v)
					if (i.first == game_id)
					{
						recipients_ids.push_back(i.second);
						break;
					}
			}


			bool do_send = true;

			BinaryReader br2(packet_data.data(), packet_data.size());
			unsigned short pid;
			br2 >> pid;

			if (pid != 0x758a && client.replay_data && client.room && !client.room->replay_data)
			{
				client.replay_data->packets << client.ptime << packet_data;
			}

			//if (pid == 0x758a)	//chat
			switch (pid)
			{
			case 0x7544:
			{
				if (!client.room->replaced_dummy)
				{
					client.room->replaced_dummy = true;
					if (recipients.size() == 1 && recipients[0] == 0x7fffffff && client.room && !client.room->replay_data)
					{
						std::vector<char> packet_data_2(packet_data.begin(), packet_data.end());
						*reinterpret_cast<float*>(&packet_data_2[10]) = 10000.0f;
						*reinterpret_cast<float*>(&packet_data_2[14]) = 10000.0f;
						*reinterpret_cast<float*>(&packet_data_2[18]) = 10000.0f;

						BinaryWriter outpacket = Network::packet(0x074e26ca, 0x7fffffff, client.room_id + room_id_disp, 1, sender, packet_data_2);
						sendTo(client.session_id, std::move(outpacket));
					}
				}

				unsigned unk1, unk3, unk4;
				float unk2;
				unsigned short motion_id;
				unsigned t;
				float x, y, z, xd, yd, zd;
				br2.read(unk1, t, x, y, z);
				br2.skip(6);
				unsigned state = br2.read<unsigned char>();		//0 normal, 1 boost, 2 fail
				br2 >> unk2;
				unk3 = br2.read<unsigned char>();
				br2 >> motion_id;
				unk4 = br2.read<unsigned char>();	//some kind of drag?

				if (motion_id >= 0x4000 && motion_id < 0x5000)
				{
					//trick
				}
				else
				{
					//no trick
					motion_id = 0;
				}

				if (client.trick_motions.size() > 0)
				{
					if (client.trick_motions.back().second != motion_id)
						client.trick_motions.emplace_back(client.ptime, motion_id);
				}
				else
					client.trick_motions.emplace_back(client.ptime, motion_id);


				if (state != client.last_fail_count)
				{
					client.last_fail_count = state;
					if (state == 2)
					{
						++client.trick_fail;
						trick_fail.add();
					}
					//					if (fail_count != 0)
					//						logger << "FAIL " << fail_count;
				}

				//				assert(t > client.client_race_time);

				if (t < client.client_race_time)
					client.time_error = true;
				else
					client.client_race_time = t;
				/*
				{
				int t1 = t * 20 / 3;
				int t2 = (client.ptime - gamePacketHandler->server_start_tick);
				int t3 = t2 - t1;
				std::cout << t1 << (char)9 << t2 << (char)9 << t3 << std::endl;
				}
				*/
				//t / 150 = (start_tick - gamePacketHandler->server_start_tick) / 1000.0f
				float pos_diff = 0.0f;
				if (client.pos_set)
				{
					xd = x - client.x;
					yd = y - client.y;
					zd = z - client.z;
					pos_diff = sqrtf(xd * xd + yd * yd + zd * zd);
					if (pos_diff > 500)
					{
						if (!client.last_pos_diff_big)
							client.last_pos_diff_big = true;
					}
					else
						client.last_pos_diff_big = false;
				}
				else
					client.pos_set = true;

				client.x = x;
				client.y = y;
				client.z = z;

				unsigned t2 = (client.ptime - gamePacketHandler->server_start_tick) * 3 / 20;
				int ping = (int)(t2 - t) * 20 / 3;
				relay_pings.add(ping);
				if (ping > 20000 || ping < -1000)
					client.time_error = true;
				client.pings.emplace_back(client.ptime, ping);
				if ((log_relay & 4) != 0)
				{
					unsigned ref_tick = client.ptime - gamePacketHandler->server_start_tick;
					float diff_len = sqrtf(xd * xd + yd * yd + zd * zd);
					logger << "Ping: " << ref_tick << "\t" << ping << "\t" << diff_len;
				}

				if ((log_relay & 8) != 0)
				{
					logger << ping <<"\t" << state << "\t" << unk3 << "\t" << std::hex << motion_id << std::dec << "\t" << unk4;
				}

				/*4*/
				//				if (log_relay)
				//					logger.print("%s time test: \t%d \t%f", client.name, ping, pos_diff);
			}
			break;
			case 0x758a:	//chat
			{
				unsigned unk1, unk2, unk3;
				br2.read(unk1, unk2, unk3);
				std::string message, name;
				//stream_array<char> message, name;
				br2 >> message >> name;

				if (name != client.name)
					do_send = false;
				else
				{
					if (test_chatban(client.name, message))
						do_send = false;
				}

				if (message.size() > 0 && (message[0] == '/' || message[0] == '.'))
				{
					//					do_send = false;
					//					do_command(client.name, message.data(), message.size());
					gamePacketHandler->dispatch(Network::packet(7, name, message));
				}
			}
			break;
			case 0x7545:	//crossed finish line
							//				unsigned char slot;
							//				float runtime;
							//				br2.read(slot, runtime);

				if (log_relay)
				{
					logger << "Client " << client.name << " crossed finish line";
				}
				if (client.room && !client.arrived)
				{
					client.arrived = true;
					GamePacketHandler::room_t & game_room = gamePacketHandler->rooms[client.room->room_id];
					std::string clientname = client.name;

					if(!client.sent_run_data)
					{
						impl_lock_guard(lock, game_room.arrival_mutex);
						client.sent_run_data = true;
						game_room.arrival_times.emplace_back(client.clientid, client.ptime, std::move(static_cast<relay_run_data_t&>(client)), std::move(clientname));
//						game_room.arrival_times.emplace_back(client.clientid, client.ptime, client.coin_count, client.time_error || client.pos_error, client.trick_success, client.trick_fail, client.replay_data, client.trick_points, std::move(clientname));
					}

					if (recipients.size() == 1 && recipients[0] == 0x7fffffff && client.room && !client.room->replay_data)
					{
						std::vector<char> packet_data_2(packet_data.begin(), packet_data.end());
						*reinterpret_cast<float*>(&packet_data_2[3]) = 0.0f;

						BinaryWriter outpacket = Network::packet(0x074e26ca, 0x7fffffff, client.room_id + room_id_disp, 1, sender, packet_data_2);
						sendTo(client.session_id, std::move(outpacket));
						//sendTo(client.session_id, Network::packet(0x03b549ca, client.room_id + room_id_disp, 0x7fffffff, "     "));
					}

				}
				break;
			case 0x758b:	//box (ice cube) collected
				boxes_collected.add();
				break;
			case 0x7549:	//trick points
			{
				unsigned trick_points;
				br2 >> trick_points;
				if (trick_points > client.trick_points)
				{
					if ((log_relay & 8) != 0)
					{
						logger << "Trick points: " << (trick_points - client.trick_points);
					}
					client.trick_points = trick_points;
					client.trick_scores.emplace_back(client.ptime, trick_points);
					++client.trick_success;
					trick_success.add();
				}
			}
			break;
			case 0x754a:
			{
				int coin_count;
				br2 >> coin_count;

				/*2*/
				//				logger << client.name << " coin: " << (coin_count - client.coin_count) << ' ' << (client.ptime - client.last_coin_time);
				if (coin_count <= client.coin_count + 32 && client.coin_count - 32 <= coin_count)
					client.coin_count = coin_count;
				else
				{
					if (!client.coin_warning)
					{
						client.coin_warning = true;
						logger << LIGHT_RED << client.name << " coin amount change exceeds limit(32): " << (coin_count - client.coin_count);
					}
				}
			}
			break;
			}



			if (do_send)
			{
				BinaryWriter outpacket = Network::packet(0x074e26ca, sender, client.room_id + room_id_disp, recipients, packet_data);


				if (pid != 0x7544)
					sendTo(recipients_ids, std::move(outpacket));
				else
					sendToDropable(recipients_ids, std::move(outpacket));
			}

			if (log_relay && (pid != 0x7544 || (log_relay & 2) != 0))
			{
				Network::dump(packet_data.data(), packet_data.size(), client.name.c_str());
//				if(packet_data.size() > 20)
//					Network::dump(&packet_data.data()[20], packet_data.size() - 20, client.name.c_str());
//				else
//					Network::dump(packet_data.data(), packet_data.size(), client.name.c_str());
			}
#ifdef _DEBUG
			++client.room->packet_count;
#endif
			return;
		}


		switch (packetid)
		{
		case 0x0dd7d57a:	//login to relay
		{

			if (client.name.size() == 0)
			{
				d_dump(data, length, "Relay");
				std::string auth_token;
				if (use_launcher)
					br >> auth_token;
				br.read(client.room_id, client.clientid, client.name);
#ifdef _DEBUG
				logger << "Relay login " << client.room_id << ' ' << client.clientid << ' ' << client.name;
#endif
				client.room_id -= room_id_disp;
				if (client.room_id < rooms.size())
					rooms[client.room_id].join(client);

				gamePacketHandler->dispatch(Network::packet(3, client.room_id, client.clientid, client.name, auth_token, session_id));
				client.inc_read_lock();
			}
			return;
		}
		break;
		case 0x0b89fc0a:
			break;
		default:
			d_dump(data, length, "Relay");
			client.disconnect(DisconnectKind::IMMEDIATE_DC);
			return;
		}

		return;
	}

	virtual void onDisconnect(Network::session_id_t session_id)
	{
		auto i = connections.find(session_id);
		if (i != connections.end())
		{
			{
				impl_lock_guard(lock, connection_count_mutex);
				if (!connection_count[i->second.dwip].dec())
					connection_count.erase(i->second.dwip);
			}
#ifdef _DEBUG
			logger << "Client " << i->second.name << " disconnected from relay\r\n";
#endif
			if (!i->second.asked_dc && i->second.room)
			{
				logger << "Client " << i->second.name << " disconnected from relay while still being in room\r\n";
			}
			i->second.invalidate();
			connections.erase(i);
		}
	}
	virtual void onInit()
	{
		default_color = LIGHT_CYAN;
		Network::timer_msecs = 1;
	}

	unsigned last_client_check = get_tick();
	virtual void onTimer()
	{
		unsigned tick = get_tick();
		if (tick - last_client_check > client_check_interval)
		{
			last_client_check = tick;
			distribute(Network::packet(0));
		}

		unsigned dc_all = disconnect_all;
		switch (dc_all)
		{
		case 1:
			for (auto i = connections.begin(); i != connections.end(); ++i)
				disconnect(i->second.session_id);
			if (connections.size() == 0)
				disconnect_all = 0;
			else
				disconnect_all = 2;
			break;
		case 2:
			if (connections.size() == 0)
				disconnect_all = 0;
			break;
		}

		{
			std::vector<unsigned> to_remove;
			for (unsigned room_id : replay_rooms)
			{
				try
				{
					rooms[room_id].replay(tick);
				}
				catch (std::exception & e)
				{
					to_remove.push_back(room_id),
						catch_message(e);
				}
			}
			for (unsigned room_id : to_remove)
				replay_rooms.erase(room_id);
		}
	}
public:
	std::atomic<unsigned> disconnect_all;

	struct room_t;
	struct connection_t
		: public Network::NetworkClientBase
		, public relay_run_data_t
	{
		Network::session_id_t session_id;
		std::string ip;
		unsigned port;
		unsigned clientid = 0;
		unsigned room_id;
		unsigned dwip;
		unsigned ptime;
		unsigned client_race_time = 0;
		float x, y, z, pos_diff = 0;
		unsigned last_fail_count = 0;
		//		unsigned last_coin_time;
		std::string name;
		room_t * room;

		bool pos_set = false;
		bool last_pos_diff_big = false;
		bool asked_dc = false;
		bool sent_run_data = false;

		std::shared_ptr<replay_data_t> replay_data2;

		virtual void invalidate()
		{
			if (_valid)
			{
				_valid = false;

				if (replay_data2)
				{
					BinaryWriter bw;
					bw << *replay_data2;
					std::string filename = name;
					for (char & ch : filename)
						if (ch == '[' || ch == ']')
							ch = '_';
					save_file("replays/" + filename + "_last.rpl", "PSFRPL", std::move(bw.data));
				}

				if (room) try
				{
					room->leave(*this);
				}
				LOG_CATCH();
			}
		}
		void disconnect(DisconnectKind disconnectKind = DisconnectKind::SOFT_DC)
		{
			invalidate();
			relayPacketHandler->disconnect(session_id, disconnectKind);
		}
	};

	struct room_t
	{
		unsigned room_id;
		std::vector<Network::session_id_t> players;
		std::vector<std::pair<unsigned, Network::session_id_t> > players_by_gameid;
		unsigned packet_count;
		bool replaced_dummy;
		std::shared_ptr<replay_data_t> replay_data;
		BinaryReader rr = BinaryReader(0, 0);
		unsigned replay_tick_diff = 0;
		bool replay_started = false;
		unsigned dummy_slot = 0;
		//		unsigned replay_tick_diff;
		//		unsigned replay_time_diff;

		void send_run_data()
		{
			GamePacketHandler::room_t & game_room = gamePacketHandler->rooms[room_id];
			unsigned ptime = get_tick();
			{
				impl_lock_guard(lock, game_room.arrival_mutex);
				for (unsigned i = 0; i < players.size(); ++i)
				{
					auto j = relayPacketHandler->connections.find(players[i]);
					if (j != relayPacketHandler->connections.end())
					{
						RelayPacketHandler::connection_t & client = j->second;
						if (!client.sent_run_data)
						{
							std::string clientname = client.name;
							{
								client.sent_run_data = true;
								game_room.arrival_times.emplace_back(client.clientid, ptime, std::move(static_cast<relay_run_data_t&>(client)), std::move(clientname));
							}
						}
					}
				}
			}
			game_room.sent_run_data = 1;
		}

		void set_start_run_time(unsigned start_tick)
		{
			for (auto id : players)
			{
				auto i = relayPacketHandler->connections.find(id);
				if (i != relayPacketHandler->connections.end())
					if (i->second.replay_data)
						i->second.replay_data->start_tick = start_tick;
			}
		}

		void set_start_replay_time(unsigned start_tick)
		{
			if (replay_data)
			{
				unsigned t1 = start_tick - gamePacketHandler->server_start_tick;
				unsigned t2 = replay_data->start_tick - replay_data->server_start_tick;
				replay_tick_diff = t1 - t2;
				replay_started = true;
			}
		}

		void set_replay(std::shared_ptr<replay_data_t> rd, unsigned dummy_slot)
		{
			replay_data = rd;
			if (replay_data)
			{
				rr = BinaryReader(replay_data->packets.data.data(), replay_data->packets.data.size());
				relayPacketHandler->replay_rooms.insert(room_id);
				replay_started = false;
				this->dummy_slot = dummy_slot;
				//				replay_tick_diff = 0;
				//				replay_tick_diff = gamePacketHandler->server_start_tick - replay_data->server_start_tick;
				//				replay_time_diff = replay_tick_diff * 3 / 20;

				//				unsigned t2 = (client.ptime - gamePacketHandler->server_start_tick) * 3 / 20;
				//				int ping = (int)(t2 - t) * 20 / 3;

			}
			else
				relayPacketHandler->replay_rooms.erase(room_id);
		}

		void replay(unsigned tick)
		{
			unsigned stick = tick - gamePacketHandler->server_start_tick;
			while (!rr.eof(4) && replay_data && replay_started)
			{
				//				unsigned sender;
				//				unsigned room_id;
				//				stream_array<unsigned> recipients;
				//				stream_array<char> packet_data;

				unsigned ptick = rr.peek<unsigned>() - replay_data->server_start_tick + replay_tick_diff;
				if (stick >= ptick)
				{
					rr.skip<unsigned>();
					std::vector<unsigned> recipients;
					recipients.resize(players_by_gameid.size());
					for (unsigned i = 0; i < players_by_gameid.size(); ++i)
						recipients[i] = players_by_gameid[i].first;

					std::vector<char> packet_data;
					rr >> packet_data;
					unsigned short pid = 0;
					BinaryReader br2(packet_data.data(), packet_data.size());
					br2 >> pid;
					switch (pid)
					{
					case 0x7544:
					{
						unsigned t;
						br2.skip(4);
						unsigned pos = br2.tell();
						br2 >> t;

						t = t * 20 / 3;
						t -= replay_data->server_start_tick;
						t += replay_tick_diff;
						t += gamePacketHandler->server_start_tick;
						t = t * 3 / 20;


						/*
						int t1 = t * 20 / 3;
						int t2 = (client.ptime - gamePacketHandler->server_start_tick);
						int t3 = t2 - t1;
						std::cout << t1 << (char)9 << t2 << (char)9 << t3 << std::endl;
						*/
						//				unsigned t2 = (client.ptime - gamePacketHandler->server_start_tick) * 3 / 20;
						//				int ping = (int)(t2 - t) * 20 / 3;

						*reinterpret_cast<unsigned*>(&packet_data[pos]) = t;// +replay_time_diff;

					}
					break;
					case 0x7545:
					{
						packet_data.at(br2.tell()) = dummy_slot;
						br2.skip<char>();
						unsigned pos = br2.tell();
						float runtime = br2.read<float>();
						runtime = ((runtime * 1000.0f) - replay_data->server_start_tick + gamePacketHandler->server_start_tick) / 1000.0f;
						*reinterpret_cast<float*>(&packet_data[pos]) = runtime;
					}
					break;
					}

					BinaryWriter outpacket = Network::packet(0x074e26ca, 0x7fffffff, room_id + room_id_disp, recipients, packet_data);

					if (pid != 0x7544)
						distribute(std::move(outpacket));
					else
						distribute_dropable(std::move(outpacket));
				}
				else
					break;
			}
		}

		void join(connection_t & client)
		{
			if (client.room && client.room != this)
				client.room->leave(client);
			if (client.room != this)
			{
				players.push_back(client.session_id);
				players_by_gameid.push_back(std::make_pair(client.clientid, client.session_id));
				client.room = this;

				if (players.size() == 1)
				{
					replay_started = false;
					//					replay_tick_diff = 0;
					packet_count = 0;
					replaced_dummy = false;
				}

			}
		}

		void kick(unsigned id)
		{
			for (unsigned i = 0; i < players_by_gameid.size(); ++i)
			{
				if (players_by_gameid[i].first == id)
				{
					Network::session_id_t id = players_by_gameid[i].second;
					auto j = relayPacketHandler->connections.find(id);
					if (j != relayPacketHandler->connections.end())
					{
						j->second.asked_dc = true;
						leave(j->second);
						j->second.disconnect(DisconnectKind::IMMEDIATE_DC);
					}
				}
			}
		}

		void leave(connection_t & client)
		{
			for (unsigned i = 0; i < players.size(); ++i)
			{
				if (players[i] == client.session_id)
				{
					players[i] = players.back();
					players.pop_back();
					for (unsigned j = 0; j < players_by_gameid.size(); ++j)
					{
						if (players_by_gameid[j].second == client.session_id)
						{
							players_by_gameid[j] = players_by_gameid.back();
							players_by_gameid.pop_back();
							break;
						}
					}
					//players_by_gameid.erase(client.game_id);
					client.room = nullptr;

					if (players.size() == 0)
					{
						if (replay_data)
						{
							relayPacketHandler->replay_rooms.erase(this->room_id);
							replay_data.reset();
						}
#ifdef _DEBUG
						logger.print("Relayed %d packets in room %d\r\n", packet_count, room_id);
#endif
						gamePacketHandler->dispatch(Network::packet(4, room_id));
					}
					/*1*/

					return;
				}
			}
			assert(false);
		}

		void disband()
		{
			if (replay_data)
			{
				relayPacketHandler->replay_rooms.erase(this->room_id);
				replay_data.reset();
			}

			std::vector <Network::session_id_t> players2 = players;
			for (Network::session_id_t player : players2)
			{
				auto i = relayPacketHandler->connections.find(player);
				if (i != relayPacketHandler->connections.end())
				{
					i->second.asked_dc = true;
					i->second.disconnect(DisconnectKind::IMMEDIATE_DC);
				}
			}
		}

		inline bool distribute(std::vector<char> && data, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			return relayPacketHandler->sendTo(players, std::move(data), skipId);
		}
		inline bool distribute(BinaryWriter && bw, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			return relayPacketHandler->sendTo(players, std::move(bw.data), skipId);
		}
		inline bool distribute_dropable(std::vector<char> && data, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			return relayPacketHandler->sendToDropable(players, std::move(data), skipId);
		}
		inline bool distribute_dropable(BinaryWriter && bw, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			return relayPacketHandler->sendToDropable(players, std::move(bw.data), skipId);
		}
	};

	std::set<unsigned> replay_rooms;
	std::array<room_t, 1024> rooms;
	std::map<Network::session_id_t, connection_t> connections;

	virtual ~RelayPacketHandler() {}
	RelayPacketHandler()
        : disconnect_all(0)
	{
		for (unsigned i = 0; i < rooms.size(); ++i)
			rooms[i].room_id = i;
	}
};

struct trick_record_data_t
{
	unsigned mapid;
	float laptime_best;
	float laptime_best_diff;
	float laptime_avarage;
	float laptime_avarage_diff;
	unsigned score_best;
	unsigned score_avarage;
	unsigned combo_best;
	unsigned combo_avarage;
	unsigned run_count;
	trick_record_data_t(unsigned mapid = 0)
		: mapid(mapid)
		, laptime_best(0.0f)
		, laptime_best_diff(0.0f)
		, laptime_avarage(0)
		, laptime_avarage_diff(0)
		, score_best(0)
		, score_avarage(0)
		, combo_best(0)
		, combo_avarage(0)
		, run_count(0)
	{
	}
	void operator <<(BinaryWriter & bw) const
	{
		//bw << mapid << laptime_best << run_count << laptime_avarage << unk2 << score_best << score_avarage << combo_best << combo_avarage;
		bw.write(mapid, laptime_best, laptime_best_diff, laptime_avarage, laptime_avarage_diff, score_best, score_avarage, combo_best, combo_avarage);
	}
};

struct trick_license_data_t
{
	unsigned id;
	unsigned char is_enabled;
	float runtime;
	unsigned score;
	unsigned collision_count;
	unsigned unk2;
	trick_license_data_t()
	{
		memset(this, 0, sizeof(*this));
	}
	void operator >>(BinaryReader & br)
	{
		//br >> id >> is_enabled >> runtime >> score >> collision_count >> unk2;
		br.read(id, is_enabled, runtime, score, collision_count, unk2);
	}
	void operator <<(BinaryWriter & bw) const
	{
		//bw << id << is_enabled << runtime << score << collision_count << unk2;
		bw.write(id, is_enabled, runtime, score, collision_count, unk2);
	}

};

class TrickPacketHandler
	: public Network::NetworkEventHandler
{
	virtual DisconnectKind onConnect(Network::session_id_t session_id, std::string && ip, unsigned port)
	{
		unsigned dwip = Network::string2ipv4(ip);
		{
			impl_lock_guard(lock, connection_count_mutex);
			if (!connection_count[dwip].inc())
				return DisconnectKind::IMMEDIATE_DC;
		}

		connection_t & client = connections[session_id];
		client.session_id = session_id;
		client.ip = std::move(ip);
		client.port = port;
		client.dwip = dwip;
		client.ptime = get_tick();
		return DisconnectKind::ACCEPT;
	}
	virtual void onRecieve(Network::session_id_t session_id, const char * data, unsigned length)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return;

		connection_t & client = _i->second;
		client.check_valid();
		client.ptime = get_tick();

		if (!client.can_read())
			throw Network::postpone();

		BinaryReader br(data, length);
		br.skip(4);
		unsigned packetid = br.read<unsigned>();

		switch (log_inpacket_ids)
		{
		case 1:
			if (packetid != 0x0c19593a)
				logger.print("trick inpacket %08x\r\n", packetid);
			break;
		case 2:
			logger.print("trick inpacket %08x\r\n", packetid);
			break;
		}
		if (log_inpackets)
			Network::dump(data, length, "trick");

		if (client.clientid == 0 && packetid != 0x0c19593a && packetid != 0x0204914a)
		{
			client.disconnect(DisconnectKind::IMMEDIATE_DC);
			return;
		}

		switch (packetid)
		{
		case 0x0c19593a:	//repeated (keepalive?)
			d_dump(data, length, "trick");
			break;
		case 0x0c3f37ca:
		{
			/*
			unsigned clientid;
			unsigned char unk1;
			float unk2;
			unsigned mapid;
			float runtime;
			unsigned trickscore;
			//br >> clientid >> unk1 >> unk2 >> mapid >> runtime >> trickscore;
			br.read(clientid, unk1, unk2, mapid, runtime, trickscore);

			if (clientid == client.clientid)
			{
			add_runtime(clientid, mapid, runtime, trickscore, &client);
			}
			*/
		}
		break;
		//		case 0x08e9d30a:
		//		{
		//			test1(client.clientid);
		//		}
		//			break;
		case 0x0f5f796a:	//tutorial
		{
			unsigned clientid;
			unsigned tutorialid;
			//br >> clientid >> tutorialid;
			br.read(clientid, tutorialid);
			repply(Network::packet(0x057e91aa, (char)(clientid == client.clientid)));
		}
		break;
		case 0x0220676a:
		{
			unsigned clientid;
			br >> clientid;
			repply(Network::packet(0x0b24dfba, (char)(clientid == client.clientid)));
		}
		break;
		case 0x0204914a:
		{
			if (client.clientid != 0)
				break;
			d_dump(data, length, "trick");
			if (use_launcher)
				br >> client.auth_token;

			unsigned clientid;
			br >> clientid;

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
			{
				onLogin(client, player);
				return;
			}
			else
			{
				login_req.emplace_back(session_id, clientid);
				client.inc_read_lock();
				return;
			}


		}
		break;
		case 0x0bee92da:
		{
			d_dump(data, length, "trick");

			unsigned unk1;
			unsigned msgid;
			//br >> unk1 >> msgid;
			//br.skip(4);
			br.read(unk1, msgid);


			for (unsigned i = 0; i < client.messages.size(); ++i)
			{
				if (client.messages[i].seq_id == msgid)
				{
					if (client.messages[i].id == 10)
						client.accepted_guild_name = std::move(client.messages[i].str3);
					++i;
					for (; i < client.messages.size(); ++i)
						client.messages[i - 1] = std::move(client.messages[i]);

					client.messages.pop_back();
					query("DELETE from messages WHERE char_id = " + to_query(client.dbid) + " AND seq_id = " + to_query(msgid));
					break;
				}
			}
		}
		break;
		case 0x05517dfa:
		{
			unsigned id;
			stream_array<char> place;
			stream_array<char> msg;
			br >> id >> place >> msg;
			std::string client_name;
			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
				client_name = player->name;

			logger << "LOG from client " << client_name << '(' << client.dbid << ") at '" << place << "': " << msg << "\r\n";

		}
		break;
		case 0x011b168a:	//challenge start
		{
			unsigned clientid;
			unsigned trickid;
			//br >> clientid >> trickid;
			br.read(clientid, trickid);
			if (clientid == client.clientid/* && client.challange_trick == ~0*/)
			{
				client.challange_trick = trickid;
				repply(Network::packet(0x05e626ca, (char)1));
			}
			else
				repply(Network::packet(0x05e626ca, (char)0));
			//				client.challange_trick = ~0;
		}
		break;

		case 0x0dc04f8a:
		{
			unsigned clientid;
			br >> clientid;
			/*
			if (clientid == client.clientid && client.challange_trick != ~0)
			{
			client.challange_trick = ~0;
			repply(Network::packet(0x02c7773a, (char)1));
			}
			else
			repply(Network::packet(0x02c7773a, (char)0));
			*/
		}
		break;

		case 0x00ffadda:	//challange result
		{
			unsigned clientid;
			trick_license_data_t l;
			br >> clientid >> l;
			if (clientid == client.clientid && l.id == client.challange_trick)
			{
				if (l.is_enabled)
				{
					client.challange_trick = ~0;
					client.licenses[l.id] = l;

					repply(Network::packet(0x02c7773a, (char)1));
					query("REPLACE INTO licenses VALUES(" + to_query(client.dbid, l.id, (unsigned int)l.is_enabled, l.runtime, l.score, l.collision_count, l.unk2) + ")");
				}
			}
		}
		break;
		case 0x0318db1a:	//qetquestdatareq
		{
			if (enable_all_tricks)
			{
				trick_license_data_t trd;
				trd.is_enabled = 1;
				trd.runtime = 1.0f;
				trd.score = 90000;
				trd.collision_count = 1;
				trd.unk2 = 2;

				BinaryWriter bw;
				bw << Network::packet_header_t{ 0 } << 0x004a640a;
				bw << (unsigned int)quest_clear_exp.size();
				for (unsigned i = 0; i < quest_clear_exp.size(); ++i)
				{
					trd.id = i;
					bw << trd;
				}

				repply(std::move(bw.data));

			}
			else
			{
				unsigned clientid = br.read<unsigned>();
				if (clientid == client.clientid)
				{
					BinaryWriter bw;
					bw << Network::packet_header_t{ 0 } << 0x004a640a;
					bw << (unsigned int)client.licenses.size();
					for (auto i = client.licenses.begin(), e = client.licenses.end(); i != e; ++i)
						bw << i->second;

					repply(std::move(bw.data));
				}
			}


		}
		break;

		case 0x0e9d612a:
			d_dump(data, length, "trick");
			break;
		case 0x0c3a015a:
			d_dump(data, length, "trick");
			break;

		case 0x084680ca:
		{
			std::string name;
			unsigned unk;
			//br >> name >> unk;
			br.read(name, unk);

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(name))
			{
				unsigned popularity = std::min<unsigned>(300, player->cash);// get_friend_count(name);
				float race_point = (float)player->race_point * (5.0f / 100.0f);	//-5, +5
				float battle_point = (float)player->battle_point * (10.0f / 100.0f);	//-10, +10
				std::string name_str;// = player->name;
				if (player->race_rank != 0)
					name_str += "GlobalRank:" + std::to_string(player->race_rank) + "| ";
				name_str += "SeasonalRank:" + (player->skill_points_rank < 0 ? "-" : std::to_string(player->skill_points_rank)) + "| ";
				name_str += "SkillPoints:" + std::to_string(player->skill_points);

				std::string guild_str;// = player->guild_name
				guild_str = player->name;
				if (player->guild_name.size() > 0)
					guild_str += " [" + player->guild_name + "]";

				repply(Network::packet(0x0820ec0a, name_str, (char)2, popularity, guild_str, (unsigned char)player->level, player->exp, (int)player->avatar, 0, 0, race_point, battle_point, 0, 0, 0, 0));
			}
			else
			{
				query<std::tuple<int, unsigned, unsigned, unsigned long long, unsigned, int, int, unsigned, unsigned, long long> >("select cash, guildid, level, experience, avatar, race_point, battle_point, get_global_rank(id), get_seasonal_rank(id), skill_points from characters where name = " + to_query(name), [name, session_id](std::vector<std::tuple<int, unsigned, unsigned, unsigned long long, unsigned, int, int, unsigned, unsigned, long long> > const & data, bool success)
				{
					if (success && data.size() > 0)
					{
						if (TrickPacketHandler::connection_t * client = trickPacketHandler->get_client(session_id))
						{
							auto const & result = data[0];
							unsigned popularity = std::min<unsigned>(300, std::get<0>(result));// get_friend_count(name);
							float race_point = (float)std::get<5>(result) * (5.0f / 100.0f);	//-5, +5
							float battle_point = (float)std::get<6>(result) * (10.0f / 100.0f);	//-10, +10
							std::string name_str;// = name;
							if (std::get<7>(result) != 0)
								name_str += "GlobalRank:" + std::to_string(std::get<7>(result)) + "| ";
							name_str += "SeasonalRank:" + ((int)std::get<8>(result) < 0 ? "-" : std::to_string((int)std::get<8>(result))) + "| ";
							name_str += "SkillPoints:" + std::to_string(std::get<9>(result));

							std::string guild_name = get_guild_name(std::get<1>(result));
							std::string guild_str;// = player->guild_name
							guild_str = name;
							if (guild_name.size() > 0)
								guild_str += " [" + guild_name + "]";

							trickPacketHandler->sendTo(session_id, Network::packet(0x0820ec0a, name_str, (char)2, popularity, guild_str, (unsigned char)std::get<2>(result), std::get<3>(result), (int)std::get<4>(result), 0, 0, race_point, battle_point, 0, 0, 0, 0));
						}
					}
				});
			}
		}
		break;

		case 0x08ff84fa:
		{

			unsigned clientid1, clientid2;
			//br >> clientid1 >> clientid2;
			br.read(clientid1, clientid2);

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid1))
			{
				unsigned popularity = std::min<unsigned>(300, player->cash);// get_friend_count(clientid1);
				float race_point = (float)player->race_point * (5.0f / 100.0f);	//-5, +5
				float battle_point = (float)player->battle_point * (10.0f / 100.0f);	//-10, +10
				std::string name_str;// = player->name;
				if (player->race_rank != 0)
					name_str += "GlobalRank:" + std::to_string(player->race_rank) + "| ";
				name_str += "SeasonalRank:" + (player->skill_points_rank < 0 ? "-" : std::to_string(player->skill_points_rank)) + "| ";
				name_str += "SkillPoints:" + std::to_string(player->skill_points);

				std::string guild_str;// = player->guild_name
				guild_str = player->name;
				if (player->guild_name.size() > 0)
					guild_str += " [" + player->guild_name + "]";

				repply(Network::packet(0x0820ec0a, name_str, (char)2, popularity, guild_str, (unsigned char)player->level, player->exp, (int)player->avatar, 0, 0, race_point, battle_point, 0, 0, 0, 0));
			}
			else
			{
				query<std::tuple<int, unsigned, unsigned, unsigned long long, unsigned, int, int, unsigned, std::string, unsigned, long long> >("select cash, guildid, level, experience, avatar, race_point, battle_point, get_global_rank(id), name, get_seasonal_rank(id), skill_points from characters where id = " + to_query(clientid1), [session_id](std::vector<std::tuple<int, unsigned, unsigned, unsigned long long, unsigned, int, int, unsigned, std::string, unsigned, long long> > const & data, bool success)
				{
					if (success && data.size() > 0)
					{
						if (TrickPacketHandler::connection_t * client = trickPacketHandler->get_client(session_id))
						{
							auto const & result = data[0];
							unsigned popularity = std::min<unsigned>(300, std::get<0>(result));// get_friend_count(name);
							float race_point = (float)std::get<5>(result) * (5.0f / 100.0f);	//-5, +5
							float battle_point = (float)std::get<6>(result) * (10.0f / 100.0f);	//-10, +10
							std::string name_str;// = std::get<8>(result);
							if (std::get<7>(result) != 0)
								name_str += "GlobalRank:" + std::to_string(std::get<7>(result)) + "| ";
							name_str += "SeasonalRank:" + ((int)std::get<9>(result) < 0 ? "-" : std::to_string((int)std::get<9>(result))) + "| ";
							name_str += "SkillPoints:" + std::to_string(std::get<10>(result));

							std::string guild_name = get_guild_name(std::get<1>(result));

							std::string guild_str;// = player->guild_name
							guild_str = std::get<8>(result);
							if (guild_name.size() > 0)
								guild_str += " [" + guild_name + "]";

							trickPacketHandler->sendTo(session_id, Network::packet(0x0820ec0a, name_str, (char)2, popularity, guild_str, (unsigned char)std::get<2>(result), std::get<3>(result), (int)std::get<4>(result), 0, 0, race_point, battle_point, 0, 0, 0, 0));
						}
					}
				});
			}
		}
		break;

		case 0x046ef82a:
		{
			std::string name;
			//std::string requester_name;
			//br >> name >> requester_name;
			//br.read(name, requester_name);
			br >> name;

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(name))
			{
				auto j = connections_by_id.find(player->id);
				if (j != connections_by_id.end())
				{
					trick_record_data_t data;

					BinaryWriter bw;
					bw.emplace<Network::packet_header_t>();
					bw << 0x0d4baf3a;
					bw << (unsigned int)j->second->records.size();
					for (auto i = j->second->records.begin(); i != j->second->records.end(); ++i)
					{
						data = i->second;
						auto k = client.records.find(data.mapid);
						if (k != client.records.end())
						{
							data.laptime_avarage_diff = k->second.laptime_avarage - data.laptime_avarage;
							data.laptime_best_diff = k->second.laptime_best - data.laptime_best;
						}
						bw << data;
					}

					repply(std::move(bw.data));
				}
			}
			else
			{
				/*7*/
				query<records_tuple>("SELECT * FROM records WHERE char_id = (select id from characters where name = " + to_query(name) + ')', [session_id](std::vector<records_tuple> const & data, bool success)
				{
					if (TrickPacketHandler::connection_t * client = trickPacketHandler->get_client(session_id))
					{
						if (success)
						{
							BinaryWriter bw;
							bw.emplace<Network::packet_header_t>();
							bw << 0x0d4baf3a;
							bw << (unsigned int)data.size();


							trick_record_data_t trd;
							for (unsigned i = 0; i < data.size(); ++i)
							{
								records_tuple const & r = data[i];
								trd.mapid = std::get<1>(r);
								trd.laptime_best = std::get<2>(r);
								trd.run_count = std::get<3>(r);
								trd.laptime_avarage = std::get<4>(r);
								trd.score_best = std::get<5>(r);
								trd.score_avarage = std::get<6>(r);
								trd.combo_best = std::get<7>(r);
								trd.combo_avarage = std::get<8>(r);

								//								client->records[trd.mapid] = trd;

								auto k = client->records.find(trd.mapid);
								if (k != client->records.end())
								{
									trd.laptime_avarage_diff = k->second.laptime_avarage - trd.laptime_avarage;
									trd.laptime_best_diff = k->second.laptime_best - trd.laptime_best;
								}

								bw << trd;

							}

							trickPacketHandler->sendTo(session_id, std::move(bw.data));
						}
					}
				});
			}
		}
		break;

		//		case 0x08ff84fa:
		case 0x0ebf468a:
			unsigned clientid1, clientid2;
			//br >> clientid1 >> clientid2;
			br.read(clientid1, clientid2);
			{
				auto j = connections_by_id.find(clientid1);
				if (j != connections_by_id.end())
				{
					trick_record_data_t data;
					BinaryWriter bw;
					bw.emplace<Network::packet_header_t>();
					bw << 0x0d4baf3a;
					bw << (unsigned int)j->second->records.size();
					for (auto i = j->second->records.begin(); i != j->second->records.end(); ++i)
					{
						data = i->second;
						auto k = client.records.find(data.mapid);
						if (k != client.records.end())
						{
							data.laptime_avarage_diff = k->second.laptime_avarage - data.laptime_avarage;
							data.laptime_best_diff = k->second.laptime_best - data.laptime_best;
						}
						bw << data;
					}

					repply(std::move(bw.data));
				}
				else
				{
					/*7*/
					query<records_tuple>("SELECT * FROM records WHERE char_id = " + to_query(clientid1), [session_id](std::vector<records_tuple> const & data, bool success)
					{
						if (TrickPacketHandler::connection_t * client = trickPacketHandler->get_client(session_id))
						{
							if (success)
							{
								BinaryWriter bw;
								bw.emplace<Network::packet_header_t>();
								bw << 0x0d4baf3a;
								bw << (unsigned int)data.size();


								trick_record_data_t trd;
								for (unsigned i = 0; i < data.size(); ++i)
								{
									records_tuple const & r = data[i];
									trd.mapid = std::get<1>(r);
									trd.laptime_best = std::get<2>(r);
									trd.run_count = std::get<3>(r);
									trd.laptime_avarage = std::get<4>(r);
									trd.score_best = std::get<5>(r);
									trd.score_avarage = std::get<6>(r);
									trd.combo_best = std::get<7>(r);
									trd.combo_avarage = std::get<8>(r);

									//								client->records[trd.mapid] = trd;

									auto k = client->records.find(trd.mapid);
									if (k != client->records.end())
									{
										trd.laptime_avarage_diff = k->second.laptime_avarage - trd.laptime_avarage;
										trd.laptime_best_diff = k->second.laptime_best - trd.laptime_best;
									}

									bw << trd;

								}

								trickPacketHandler->sendTo(session_id, std::move(bw.data));
							}
						}
					});
				}
			}
			break;
		default:
			d_dump(data, length, "trick");
		}
		return;
	}
	virtual void onDisconnect(Network::session_id_t session_id)
	{
		auto i = connections.find(session_id);
		if (i != connections.end())
		{
			{
				impl_lock_guard(lock, connection_count_mutex);
				if (!connection_count[i->second.dwip].dec())
					connection_count.erase(i->second.dwip);
			}

			i->second.invalidate();
			connections.erase(i);
		}
	}
	virtual void onInit()
	{
		default_color = LIGHT_GREEN;

		query<std::tuple<unsigned> >("SELECT max(seq_id) FROM messages", [this](std::vector<std::tuple<unsigned> > const & data, bool success)
		{
			if (success)
			{
				if (data.size() > 0)
					this->msg_seq_id = std::get<0>(data[0]);

			}

			this->initialized |= 1;
		});


		query<std::tuple<unsigned, float> >("CALL get_global_records()", [this](std::vector<std::tuple<unsigned, float> > const & data, bool success)
		{
			if (success)
			{
				for (unsigned i = 0; i < data.size(); ++i)
					global_map_records.emplace(std::get<0>(data[i]), std::get<1>(data[i]));
			}

			this->initialized |= 2;
		});

		query<std::tuple<unsigned, float> >("CALL get_seasonal_records()", [this](std::vector<std::tuple<unsigned, float> > const & data, bool success)
		{
			if (success)
			{
				for (unsigned i = 0; i < data.size(); ++i)
					seasonal_map_records.emplace(std::get<0>(data[i]), std::get<1>(data[i]));
			}

			this->initialized |= 4;
		});

	}

public:
	struct connection_t;
private:
	bool onLogin(connection_t & client, GamePacketHandler::connection_t * player)
	{
		if (use_launcher && player->auth_token != client.auth_token)
		{
			client.disconnect();
			return false;
		}
		Network::session_id_t session_id = client.session_id;
		client.clientid = player->id;
		client.dbid = player->dbid;
		connections_by_id[client.clientid] = &client;
		connections_by_dbid[client.dbid] = &client;

		unsigned current_season_id = ::current_season_id;

		client.inc_read_lock();
		query<season_records_tuple>("SELECT * FROM current_season_records WHERE season_id = " + to_query(current_season_id) + " AND char_id = " + to_query(client.dbid), [session_id, current_season_id](std::vector<season_records_tuple> const & data, bool success)
		{
			if (TrickPacketHandler::connection_t * client = trickPacketHandler->get_client(session_id))
			{
				if (client->dec_read_lock())
					trickPacketHandler->callPostponed(session_id);

				if (::current_season_id == current_season_id)
				{
					if (success)
					{
						trick_record_data_t trd;
						for (unsigned i = 0; i < data.size(); ++i)
						{
							season_records_tuple const & r = data[i];
							trd.mapid = std::get<2>(r);
							trd.laptime_best = std::get<3>(r);
							trd.run_count = std::get<4>(r);
							trd.laptime_avarage = std::get<5>(r);
							trd.score_best = std::get<6>(r);
							trd.score_avarage = std::get<7>(r);
							trd.combo_best = std::get<8>(r);
							trd.combo_avarage = std::get<9>(r);

							client->seasonal_records[trd.mapid] = trd;
						}
					}
					else
					{
						logger << "Falied to load seasonal records";
					}
				}
			}
		});

		client.inc_read_lock();
		query<records_tuple>("SELECT * FROM records WHERE char_id = " + to_query(client.dbid), [session_id, current_season_id](std::vector<records_tuple> const & data, bool success)
		{
			if (TrickPacketHandler::connection_t * client = trickPacketHandler->get_client(session_id))
			{
				if (client->dec_read_lock())
					trickPacketHandler->callPostponed(session_id);

				if (::current_season_id == current_season_id)
				{
					if (success)
					{
						trick_record_data_t trd;
						for (unsigned i = 0; i < data.size(); ++i)
						{
							records_tuple const & r = data[i];
							trd.mapid = std::get<1>(r);
							trd.laptime_best = std::get<2>(r);
							trd.run_count = std::get<3>(r);
							trd.laptime_avarage = std::get<4>(r);
							trd.score_best = std::get<5>(r);
							trd.score_avarage = std::get<6>(r);
							trd.combo_best = std::get<7>(r);
							trd.combo_avarage = std::get<8>(r);

							client->records[trd.mapid] = trd;
						}
					}
					else
					{
						logger << "Falied to load records";
					}
				}
			}
		});

		client.inc_read_lock();
		query<messages_tuple>("SELECT * FROM messages where char_id = " + to_query(client.dbid), [session_id](std::vector<messages_tuple> const & data, bool success)
		{
			if (TrickPacketHandler::connection_t * client = trickPacketHandler->get_client(session_id))
			{
				if (client->dec_read_lock())
					trickPacketHandler->callPostponed(session_id);
				if (success)
				{
					TrickPacketHandler::message_data_t msg;
					for (unsigned i = 0; i < data.size(); ++i)
					{
						messages_tuple const & r = data[i];
						msg.seq_id = std::get<1>(r);
						msg.id = std::get<2>(r);
						msg.str1 = std::get<3>(r);
						msg.str2 = std::get<4>(r);
						msg.str3 = std::get<5>(r);
						msg.str4 = std::get<6>(r);
						trickPacketHandler->sendTo(session_id, Network::packet(0x05d2d8aa, (char)client->today_message_id, (unsigned long long)msg.seq_id, msg.id, msg.str1, msg.str2, msg.str3, msg.str4));
						++client->today_message_id;
						client->messages.push_back(std::move(msg));
					}
				}
				else
				{
					client->disconnect();
					logger << "Falied to load messages";
				}
			}
		});

		client.inc_read_lock();
		query<licenses_tuple>("SELECT * FROM licenses WHERE char_id = " + to_query(client.dbid), [session_id](std::vector<licenses_tuple> const & data, bool success)
		{
			if (TrickPacketHandler::connection_t * client = trickPacketHandler->get_client(session_id))
			{
				if (client->dec_read_lock())
					trickPacketHandler->callPostponed(session_id);
				if (success)
				{
					for (unsigned i = 0; i < data.size(); ++i)
					{
						trick_license_data_t l;
						l.id = std::get<1>(data[i]);
						l.is_enabled = std::get<2>(data[i]);
						l.runtime = std::get<3>(data[i]);
						l.score = std::get<4>(data[i]);
						l.collision_count = std::get<5>(data[i]);
						l.unk2 = std::get<6>(data[i]);

						client->licenses[l.id] = l;
					}
				}
			}
		});

		sendTo(session_id, Network::packet(0x0e02366a));	//LoginAns
		return true;
	}

	std::vector<std::pair<Network::session_id_t, unsigned> > login_req;
	unsigned last_client_check = get_tick();
	mutex_t timer_mutex;
	unsigned last_hour = 26;
	virtual void onTimer()
	{
		if (this->destroyed())
			return;
		impl_lock_guard(lock, timer_mutex);
		unsigned tick = get_tick();
		for (unsigned i = 0; i < login_req.size();)
		{
			auto j = connections.find(login_req[i].first);
			if (j == connections.end())
			{
				login_req[i] = login_req.back();
				login_req.pop_back();
				continue;
			}
			if (tick - j->second.ptime > max_client_idle_time)
			{
				connection_t * p = &j->second;
				login_req[i] = login_req.back();
				login_req.pop_back();
				p->disconnect(DisconnectKind::IMMEDIATE_DC);
				continue;
			}

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(login_req[i].second))
			{
				try
				{
					if (onLogin(j->second, player))
						if (j->second.dec_read_lock())
							this->callPostponed(j->second.session_id);
				}
				LOG_CATCH();
				login_req[i] = login_req.back();
				login_req.pop_back();
			}
			else
				++i;
		}

		if (tick - last_client_check > client_check_interval)
		{
			last_client_check = tick;
			distribute(Network::packet(0));
		}


		/*
		{
			unsigned long long t = time(0);
			double season_length = season_daycount * 24 * 60 * 60;
			unsigned long long _current_seasion_id = ::current_season_id;
			if (t >= _current_seasion_id + season_length)
			{
				std::atomic<int> _flag(0);
				_current_seasion_id = t - fmod((t - _current_seasion_id), season_length);
				//_current_seasion_id = ((t - _current_seasion_id) / season_length) * season_length + _current_seasion_id;
				::current_season_id = _current_seasion_id;
				std::shared_ptr<result_handler_t> handler = query<std::tuple<unsigned long long> >("select start_season(" + to_query(_current_seasion_id) + ")", [&_flag, _current_seasion_id](std::vector<std::tuple<unsigned long long> > const & data, bool success)
				{
					if (success && data.size() > 0 && std::get<0>(data[0]) > 0)
					{
						logger << YELLOW << "Season switch ok, current season id is: " << _current_seasion_id << std::endl;
						_flag = 1;
						return;
					}
					_flag = 2;
					logger << LIGHT_RED << "FAILURE at switch season, current season id is: " << _current_seasion_id << std::endl;
					report("SYSTEM", "Season switch", "Failure at executing start_season(" + std::to_string(_current_seasion_id) + ")", true);
				});

				if (handler)
				{
					while (_flag == 0)
					{
						handler->call_result();
						sleep_for(100);
					}

					for (auto i = trickPacketHandler->connections.begin(); i != trickPacketHandler->connections.end(); ++i)
					{
						i->second.records.clear();
					}

					for (auto i = gamePacketHandler->connections.begin(); i != gamePacketHandler->connections.end(); ++i)
					{
						i->second.skill_points = 0;
						i->second.skill_points_rank = -1;
					}
				}
			}
		}
		*/
		struct tm now = get_localtime(time(0));
		{
			if (now.tm_hour != last_hour)
			{
				last_hour = now.tm_hour;

				query("delete from messages where unix_timestamp() - msg_time >= 2592000;");

				logger << "Updating rankings";
				query("call update_rankings()");

				if (connections.size())
				{
					bool player_online = false;
					std::string q = "SELECT id, rank FROM global_ranking WHERE id in (";
					std::string q2 = "SELECT id, rank FROM seasonal_ranking WHERE id in (";
//					std::string q3 = "SELECT id, skill_points FROM characters WHERE id in (";
					for (auto i = gamePacketHandler->connections.begin(); i != gamePacketHandler->connections.end(); ++i)
					{
						if (i->second.valid() && i->second.dbid != 0)
						{
							if (player_online)
							{
								q += ',';
								q2 += ',';
//								q3 += ',';
							}
							std::string numstr = std::to_string(i->second.dbid);
							q += numstr;
							q2 += numstr;
//							q3 += numstr;
							player_online = true;
						}
					}
					if (player_online)
					{
						q += ')';
						q2 += ')';
//						q3 += ')';


						query<std::tuple<unsigned, int> >(q, [](std::vector<std::tuple<unsigned, int> > const & data, bool success)
						{
							if (!success)
								logger << LIGHT_RED << "Failed to read rank data";
							else
							{
								for (unsigned i = 0; i < data.size(); ++i)
								{
									if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_dbid(std::get<0>(data[i])))
									{
										player->race_rank = std::get<1>(data[i]);
									}
								}
							}
							logger << "Rankings updated.";
						});
						query<std::tuple<unsigned, int> >(q2, [](std::vector<std::tuple<unsigned, int> > const & data, bool success)
						{
							if (!success)
								logger << LIGHT_RED << "Failed to read seasonal rank data";
							else
							{
								for (unsigned i = 0; i < data.size(); ++i)
								{
									if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_dbid(std::get<0>(data[i])))
									{
										player->skill_points_rank = std::get<1>(data[i]);
									}
								}
							}
							logger << "Seasonal rankings updated.";
						});
						/*
						query<std::tuple<unsigned, long long> >(q3, [](std::vector<std::tuple<unsigned, long long> > const & data, bool success)
						{
							if (!success)
								logger << LIGHT_RED << "Failed to read seasonal rank data";
							else
							{
								for (unsigned i = 0; i < data.size(); ++i)
								{
									if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_dbid(std::get<0>(data[i])))
									{
										player->skill_points = std::get<1>(data[i]);
									}
								}
							}
							logger << "Seasonal rankings updated.";
						});
						*/
					}
				}

			}
		}

		unsigned dc_all = disconnect_all;
		switch (dc_all)
		{
		case 1:
			for (auto i = connections.begin(); i != connections.end(); ++i)
				disconnect(i->second.session_id);
			if (connections.size() == 0)
				disconnect_all = 0;
			else
				disconnect_all = 2;
			break;
		case 2:
			if (connections.size() == 0)
				disconnect_all = 0;
			break;
		}

	}
public:

	unsigned get_quest_exp(unsigned clientid)
	{
		unsigned r = 0;
		if (enable_all_tricks)
		{
			for (unsigned i = 0; i < quest_clear_exp.size(); ++i)
				r += quest_clear_exp[i];
		}
		else
		{
			auto i = connections_by_id.find(clientid);
			if (i != connections_by_id.end())
			{
				connection_t * player = i->second;
				for (auto j = player->licenses.begin(); j != player->licenses.end(); ++j)
				{
					if (j->second.is_enabled != 0 && j->second.id < quest_clear_exp.size())
						r += quest_clear_exp[j->second.id];
				}
			}
		}
		return r;
	}

	std::atomic<unsigned> disconnect_all;
	std::atomic<int> initialized;
	struct message_data_t
	{
		unsigned seq_id;
		unsigned id;
		std::string str1;
		std::string str2;
		std::string str3;
		std::string str4;
	};

	struct connection_t
		: public Network::NetworkClientBase
	{
		Network::session_id_t session_id;
		std::string ip;
		std::string auth_token;
		unsigned port;
		unsigned clientid = 0;
		unsigned dwip;
		unsigned ptime;
		unsigned dbid = 0;
		unsigned char today_message_id = 0;

		virtual void invalidate()
		{
			if (_valid)
			{
				_valid = false;
				trickPacketHandler->connections_by_id.erase(clientid);
				trickPacketHandler->connections_by_dbid.erase(dbid);
			}
		}
		void disconnect(DisconnectKind disconnectKind = DisconnectKind::SOFT_DC)
		{
			invalidate();
			trickPacketHandler->disconnect(session_id, disconnectKind);
		}

		unsigned challange_trick = ~0;

		std::map<unsigned, trick_license_data_t> licenses;
		std::map<unsigned, trick_record_data_t> seasonal_records;
		std::map<unsigned, trick_record_data_t> records;
		std::vector<message_data_t> messages;

		std::string accepted_guild_name;

		/*
		unsigned disable_recv = 0;
		bool dec_read_lock()
		{
		assert(disable_recv > 0);
		--disable_recv;
		if (disable_recv == 0)
		trickPacketHandler->enableRecv(session_id);
		return disable_recv == 0;
		}

		inline bool inc_read_lock()
		{
		++disable_recv;
		return false;
		}

		inline bool can_read() const
		{
		return disable_recv == 0;
		}
		*/
	};

	std::map<Network::session_id_t, connection_t> connections;
	std::map<unsigned, connection_t*> connections_by_id;
	std::map<unsigned, connection_t*> connections_by_dbid;

	connection_t * get_client(Network::session_id_t session_id)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return nullptr;
		if (!_i->second.valid())
			return nullptr;
		return &_i->second;
	}

	bool check_client_in_trick_server(unsigned clientid)
	{
		auto i = connections_by_id.find(clientid);
		if (i != connections_by_id.end())
			if(i->second)
				return i->second->clientid == clientid;

		return false;
	}

	bool add_runtime(unsigned clientid, unsigned mapid, float runtime, unsigned trickscore, float client_time, float server_time, unsigned reward_runkind, std::shared_ptr<replay_data_t> rd = nullptr, std::shared_ptr<std::string> csv = nullptr, std::shared_ptr<std::string> html = nullptr, connection_t * client = nullptr)
	{
		if (client == nullptr)
		{
			auto i = connections_by_id.find(clientid);
			if (i != connections_by_id.end())
				client = i->second;
		}

		if (client == nullptr || (client != nullptr && clientid != client->clientid))
		{
			logger << LIGHT_RED << "error at add_runtime: client " << (int)clientid << " is not online in trickserver. Runtime data: mapid: " << mapid
				<< " runtime: " << runtime << " trickscore: " << trickscore << " client_time: " << client_time << " server_time " << server_time
				<< " reward_runkind: " << reward_runkind;
		}

		if (runtime > 0.0f && client && clientid == client->clientid)
		{
			bool new_best_old = false;
			bool new_best_season = false;

			{
				trick_record_data_t trd = client->records[mapid];
				if (trd.mapid != mapid || trd.laptime_best <= 0)
				{
					trd.mapid = mapid;
					trd.run_count = 1;
					trd.laptime_best = runtime;
					trd.laptime_avarage = runtime;
					trd.score_best = trickscore;
					trd.score_avarage = trickscore;
				}
				else
				{
					new_best_old = runtime < trd.laptime_best;
					trd.laptime_best = std::min(runtime, trd.laptime_best);
					trd.laptime_avarage = (trd.laptime_avarage * trd.run_count + runtime) / (trd.run_count + 1);
					trd.score_best = std::max(trickscore, trd.score_best);
					trd.score_avarage = (trd.score_avarage * trd.run_count + trickscore) / (trd.run_count + 1);
					++trd.run_count;
				}
				client->records[mapid] = trd;

				query("REPLACE INTO records values (" + to_query(client->dbid, trd.mapid, trd.laptime_best, trd.run_count, trd.laptime_avarage, trd.score_best, trd.score_avarage, trd.combo_best, trd.combo_avarage) + ")");
			}

			{
				trick_record_data_t trd = client->seasonal_records[mapid];
				if (trd.mapid != mapid || trd.laptime_best <= 0)
				{
					trd.mapid = mapid;
					trd.run_count = 1;
					trd.laptime_best = runtime;
					trd.laptime_avarage = runtime;
					trd.score_best = trickscore;
					trd.score_avarage = trickscore;
				}
				else
				{
					new_best_season = runtime < trd.laptime_best;
					trd.laptime_best = std::min(runtime, trd.laptime_best);
					trd.laptime_avarage = (trd.laptime_avarage * trd.run_count + runtime) / (trd.run_count + 1);
					trd.score_best = std::max(trickscore, trd.score_best);
					trd.score_avarage = (trd.score_avarage * trd.run_count + trickscore) / (trd.run_count + 1);
					++trd.run_count;
				}
				client->seasonal_records[mapid] = trd;

				query("REPLACE INTO current_season_records values (" + to_query(current_season_id, client->dbid, trd.mapid, trd.laptime_best, trd.run_count, trd.laptime_avarage, trd.score_best, trd.score_avarage, trd.combo_best, trd.combo_avarage) + ")");
			}


			bool record_posted = false;

			{
				auto i = global_map_records.find(mapid);
				if (i != global_map_records.end())
				{
					if (runtime < i->second)
					{
						i->second = runtime;
						new_best_old = false;

						global_map_records[mapid] = runtime;
						if (!record_posted)
						{
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
							{
								record_posted = true;
								post_record(player->name, player->dbid, mapid, runtime, 1, client_time, server_time, reward_runkind, rd, csv, html);
							}
						}
					}
				}
				else
				{
					//i->second = runtime;
					new_best_old = false;

					global_map_records[mapid] = runtime;
					if (!record_posted)
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
						{
							record_posted = true;
							post_record(player->name, player->dbid, mapid, runtime, 1, client_time, server_time, reward_runkind, rd, csv, html);
						}
					}
				}
			}
			{
				auto i = seasonal_map_records.find(mapid);
				if (i != seasonal_map_records.end())
				{
					if (runtime < i->second)
					{
						i->second = runtime;
						new_best_season = false;

						seasonal_map_records[mapid] = runtime;
						if (!record_posted)
						{
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
							{
								record_posted = true;
								post_record(player->name, player->dbid, mapid, runtime, 2, client_time, server_time, reward_runkind, rd, csv, html);
							}
						}
					}
				}
				else
				{
					//i->second = runtime;
					new_best_season = false;

					seasonal_map_records[mapid] = runtime;
					if (!record_posted)
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
						{
							record_posted = true;
							post_record(player->name, player->dbid, mapid, runtime, 2, client_time, server_time, reward_runkind, rd, csv, html);
						}
					}
				}
			}

			if (new_best_season && !record_posted)
			{
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
				{
					if (player->level >= 10)
					{
						record_posted = true;
						post_record(player->name, player->dbid, mapid, runtime, 3, client_time, server_time, reward_runkind, rd, csv, html);
					}
				}
			}

			if (new_best_old && !record_posted)
			{
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
				{
					if (player->level >= 10)
					{
						record_posted = true;
						post_record(player->name, player->dbid, mapid, runtime, 0, client_time, server_time, reward_runkind, rd, csv, html);
					}
				}
			}

			return true;
		}
		return false;
	}


	unsigned msg_seq_id = 0;
	TrickPacketHandler()
		: disconnect_all(0)
		, initialized(0)
	{}
	virtual ~TrickPacketHandler()
	{
		impl_lock_guard(lock, timer_mutex);
		this->_destroyed = true;
	}
};

bool add_runtime(unsigned clientid, unsigned mapid, float runtime, unsigned trickscore, float client_time, float server_time, unsigned reward_runkind, std::shared_ptr<replay_data_t> rd, std::shared_ptr<std::string> csv, std::shared_ptr<std::string> html)
{
	return trickPacketHandler->add_runtime(clientid, mapid, runtime, trickscore, client_time, server_time, reward_runkind, rd, csv, html);
}

bool check_client_in_trick_server(unsigned clientid)
{
	return trickPacketHandler->check_client_in_trick_server(clientid);
}


time_t shop_time = 0;
unsigned get_quest_exp(unsigned clientid)
{
	return trickPacketHandler->get_quest_exp(clientid);
}


//enum class parts_t : unsigned {/*cr = 0, hr = 2, */fc = 3, ub = 4, lb = 5, gv = 6, sh = 7, dk = 8 };


struct gift_data_t
{
	unsigned dbid;
	unsigned itemid;
	unsigned short unk2;
	std::string sender;
	std::string unk3;
	std::string message;
	std::string date;
	unsigned char isDisabled;

	void operator <<(BinaryWriter & bw) const
	{
		//bw << dbid << unk1 << unk2 << sender << unk3 << message << date << isDisabled;
		bw.write(dbid, itemid, unk2, sender, unk3, message, date, isDisabled);
	}
};

struct item_data_t
{
	unsigned short slot;
	unsigned short id;
	unsigned short count;

	void operator <<(BinaryWriter & bw) const
	{
		//bw << slot << id << count;
		bw.write(slot, id, count);
	}
};

/*
struct misc_item_t
{
unsigned short slot;
unsigned id;
std::string unk1;
std::string unk2;
unsigned unk3;
unsigned short unk4;
unsigned char unk5;

void operator <<(BinaryWriter & bw) const
{
bw << slot << id << unk1 << unk2 << unk3 << unk4 << unk5;
}
};
*/
struct cash_item_t
{
	unsigned char unk1;
	unsigned short slot;
	unsigned id;
	unsigned unk2;
	std::string unk3;
	unsigned short unk4;
	unsigned unk5;
	unsigned unk6;
	void operator <<(BinaryWriter & bw) const
	{
		bw << unk1 << slot << id << unk2 << unk3 << unk4 << unk5 << unk6;
	}
};


struct gem_slot_desc_t
{
	unsigned char slot;
	unsigned char unk2;
	unsigned char unk3;

	inline static gem_slot_desc_t make(unsigned char slot)
	{
		return gem_slot_desc_t{ slot, 0, 0 };
	}

	void operator <<(BinaryWriter & bw) const
	{
		bw.write(slot, unk2, unk3);
	}
};

struct itemlist_entry_t
{
	unsigned id = 0;
	std::string unk2;
	unsigned unk3 = 0;
	unsigned char unk4 = 0;
	unsigned char unk5 = 0;
	unsigned char unk6 = 0;
	unsigned char unk7 = 0;
	std::string name;
	std::string unk9;
	std::string unk10;
	std::string unk11;
	std::string unk12;
	std::vector<item_price_t> item_prices;
	std::vector<gem_slot_desc_t> gems;
	unsigned char unk15 = 0;	//unk1
	unsigned unk16 = 0;			//haveparts
	unsigned unk17 = 0;			//color

	void operator <<(BinaryWriter & bw) const
	{
		bw.write(id, unk2, unk3, unk4, unk5, unk6, unk7, name, unk9, unk10, unk11, unk12, item_prices, gems, unk15, unk16, unk17);
	}
};

unsigned short const dgv = ~0;

struct fashion_item_t
{
	long long dbid;
	unsigned id;
	int slot;
	time_t expiration;
	long long shopid;
	std::vector<unsigned short> gems;
	unsigned color = 0x00ffffff;

	fashion_item_t()
		: dbid(0xffffffffffffffffLL)
		, id(~0)
		, slot(-1)
		, expiration(0)
		, shopid(-1)
	{
	}
	/*
	fashion_item_t(unsigned id, int slot)
	: dbid(0xffffffffffffffffLL)
	, id(id)
	, slot(slot)
	, expiration(0)
	{
	}
	*/

	void set(fashion_tuple const & t, unsigned _slot)
	{
		dbid = std::get<0>(t);
		id = std::get<2>(t);
		expiration = std::get<4>(t);
		shopid = std::get<13>(t);
		color = std::get<14>(t) & 0x00ffffff;
		slot = _slot;
		gems.clear();
		if (std::get<5>(t) > 0)
			gems.push_back(std::get<5>(t));
		if (std::get<6>(t) > 0)
			gems.push_back(std::get<6>(t));
		if (std::get<7>(t) > 0)
			gems.push_back(std::get<7>(t));
		if (std::get<8>(t) > 0)
			gems.push_back(std::get<8>(t));
		if (std::get<9>(t) > 0)
			gems.push_back(std::get<9>(t));
		if (std::get<10>(t) > 0)
			gems.push_back(std::get<10>(t));
		if (std::get<11>(t) > 0)
			gems.push_back(std::get<11>(t));
		if (std::get<12>(t) > 0)
			gems.push_back(std::get<12>(t));
	}

	bool valid() const
	{
		return id > itemid_disp && id - 1 - itemid_disp < itemlist.size() && itemlist[id - 1 - itemid_disp].valid && (expiration == 0 || expiration > time(0));
	}

	item_list_data_t const & data() const
	{
		assert(valid());
		if (valid())
			return itemlist[id - 1 - itemid_disp];
		throw std::runtime_error("fashion_item_t::data() call on invalid item");
	}

};

struct cloth_item_data_t
{
	unsigned char unk1;
	unsigned char unk2;
	unsigned short slot;
	unsigned itemlist_id;
	std::string unk4;
	std::string unk5;
	unsigned remaining_time;
	unsigned unk6;
	unsigned unk7;
	unsigned color;
	unsigned short unk8;
	std::vector<unsigned short> gems;

	cloth_item_data_t() {}
	cloth_item_data_t(fashion_item_t const & it)
	{
		unk1 = 0;//0x23;	//0x23
		unk2 = 0;//	//1 = true, others = false
		slot = it.slot;
		itemlist_id = it.id;
		//			cloth.unk4 = "";// ts[4];// str1;//ts[0];
		//			cloth.unk5 = "";// ts[5];// str1;// ts[1];
		remaining_time = it.expiration == 0 ? 0x5038e0 : (it.expiration / 60 - shop_time / 60 >= 0x5038e0 ? 0x5038df : (it.expiration / 60 - shop_time / 60));		//unlimited: >= 0x5038e0
		unk6 = 0;	//0x24
		unk7 = 0;	//0x25
		color = it.color & 0x00ffffff;
		unk8 = 1;
		gems = it.gems;
	}

	void operator <<(BinaryWriter & bw) const
	{
		bw << unk1 << unk2 << slot << itemlist_id << unk4 << unk5 << remaining_time << unk6 << unk7 << color << unk8 << gems;
		//bw.write(unk1, unk2, slot, dbid, unk4, unk5, remaining_time, unk6, unk7, color, gems);
	}
};

struct player_item_data_t
{
	unsigned char eq_slot = 0;
	unsigned itemid = 0;
	unsigned color = 0x00ffffff;
	std::vector<unsigned short> gems;

	bool valid() const
	{
		unsigned _id1 = itemid - 1 - itemid_disp;
		if (_id1 < itemlist.size())
			return itemlist[_id1].valid;
		else
			return false;
	}

	void operator <<(BinaryWriter & bw) const
	{
		bw << eq_slot << itemid << color << gems;
	}

	static player_item_data_t make(fashion_item_t const & it)
	{
		player_item_data_t i;
		if (it.valid())
		{
			i.eq_slot = it.data().part_kind;
			i.itemid = it.id;
			i.gems = it.gems;
			i.color = it.color & 0x00ffffff;
		}
		return i;
	}
	static player_item_data_t make(replay_item_data_t const & it)
	{
		player_item_data_t i;
		if (it.valid())
		{
			i.eq_slot = it.data().part_kind;
			i.itemid = it.id;
			i.gems = it.gems;
			i.color = it.color & 0x00ffffff;
		}
		return i;
	}
};

struct player_equip_data_t
{
	unsigned clientId;
	unsigned char avatar = 2;
	unsigned haircolor = 0x00ffffff;
	std::vector<player_item_data_t> items;
	void operator <<(BinaryWriter & bw) const
	{
		//bw << clientId << avatar << haircolor << items;
		bw.write(clientId, avatar, haircolor);
		bw << items;
	}
};


class ShopPacketHandler
	: public Network::NetworkEventHandler
{
	virtual std::string getConnectionData(Network::session_id_t id) const
	{
		auto i = connections.find(id);
		if (i != connections.end())
			return i->second.name;
		return std::string();
	}
	virtual DisconnectKind onConnect(Network::session_id_t session_id, std::string && ip, unsigned port)
	{
		unsigned dwip = Network::string2ipv4(ip);
		{
			impl_lock_guard(lock, connection_count_mutex);
			if (!connection_count[dwip].inc())
				return DisconnectKind::IMMEDIATE_DC;
		}

		connection_t & client = connections[session_id];
		client.session_id = session_id;
		client.ip = std::move(ip);
		client.port = port;
		client.dwip = dwip;
		client.ptime = get_tick();
		client.last_cash_request = client.ptime - 2000;

		//		client.cubes.push_back(std::make_pair(1001, 100));
		//		client.cubes.push_back(std::make_pair(1002, 100));
		//		client.cubes.push_back(std::make_pair(1003, 100));
		//		for (unsigned i = 1; i <= 6*24; ++i)
		//			client.gems.push_back(std::make_pair(i, 100));

		//		for (unsigned i = 0; i < itemlist.size(); ++i)
		//			client.items.emplace_back(i + 1 + itemid_disp, i);

		return DisconnectKind::ACCEPT;
	}
	virtual void onRecieve(Network::session_id_t session_id, const char * data, unsigned length)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return;

		connection_t & client = _i->second;
		client.check_valid();
		client.ptime = get_tick();

		if (!client.can_read())
			throw Network::postpone();

		BinaryReader br(data, length);
		br.skip(4);
		unsigned packetid = br.read<unsigned>();

		switch (log_inpacket_ids)
		{
		case 1:
			if (packetid != 0x0c19593a)
				logger.print("shop inpacket %08x\r\n", packetid);
			break;
		case 2:
			logger.print("shop inpacket %08x\r\n", packetid);
			break;
		}
		if (log_inpackets)
			Network::dump(data, length, "shop");

		if (client.clientid == 0 && packetid != 0x0dfecb6a && packetid != 0x0c19593a)
		{
			client.disconnect(DisconnectKind::IMMEDIATE_DC);
			return;
		}

		shop_time = time(0);

		switch (packetid)
		{
		case 0x0c19593a:	//repeated (keepalive?)
		{
			d_dump(data, length, "shop");
			unsigned clientid;
			br >> clientid;
			//			repply(Network::packet(0x072be64a, (char)1));
			//			repply(Network::packet(0x0b697aca, (char)1));
		}
		break;
		/*
		case 0x038c1c8a:
		{
		unsigned short src, dst;
		br.read(src, dst);
		src -= 1;
		if (src != dst && src < client.gems.size() && dst < client.gems.size())
		{
		std::swap(client.gems[src], client.gems[dst]);
		send_gems(client, dst);
		}
		}
		break;
		*/
		case 0x0afcc52a:	//gems
		{
			d_dump(data, length, "shop");
			unsigned short start, end;
			br.read(start, end);
			send_gems(client, start);
		}
		break;
		/*
		case 0x0c72277a:	//cubes
		{
		d_dump(data, length, "shop");
		unsigned short start, end;
		br.read(start, end);
		send_cubes(client, start);
		}
		break;
		*/
		case 0x0c2da84a:
		{
			unsigned gift_id;
			br >> gift_id;

			unsigned b_avatar = ~0;

			for (unsigned i = 0; i < client.shown_gifts.size(); ++i)
			{
				if (gift_id == client.shown_gifts[i].first)
				{
					unsigned itemid = client.shown_gifts[i].second - 1 - itemid_disp;
					if (itemid < itemlist.size() && itemlist[itemid].valid)
					{
						b_avatar = itemlist[itemid].avatar - 2;
						break;
					}
				}
			}

			if (b_avatar >= 4)
				break;

			unsigned free_slot = ~0;
			for (unsigned i = 0; i < client.items[b_avatar].size(); ++i)
			{
				if (!client.items[b_avatar][i].valid())
				{
					free_slot = i;
					break;
				}
			}
			if (free_slot == ~0)
				free_slot = client.items[b_avatar].size();


			client.inc_read_lock();
			query<std::tuple<long long> >("SELECT accept_gift(" + to_query(client.dbid, gift_id, free_slot) + ")", [session_id, gift_id, b_avatar](std::vector<std::tuple<long long> > const & data, bool success)
			{
				if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
				{
					if (success && data.size() > 0)
					{
						long long id = std::get<0>(data[0]);
						if (id > 0)
						{
							query<fashion_tuple>("SELECT * FROM fashion WHERE id = " + to_query(id), [session_id, b_avatar](std::vector<fashion_tuple> const & data, bool success)
							{
								if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
								{
									if (client->dec_read_lock())
										shopPacketHandler->callPostponed(session_id);
									if (success && data.size() > 0)
									{
										unsigned const i = 0;

										int slot = std::get<3>(data[0]);
										if (slot < 0)
										{
											for (; slot < (int)client->items[b_avatar].size(); ++slot)
												if (!client->items[b_avatar][slot].valid())
													break;
										}

										if (client->items[b_avatar].size() <= (unsigned)slot)
											client->items[b_avatar].resize(slot + 1);
										fashion_item_t & it = client->items[b_avatar][slot];
										it.set(data[i], slot);

										shopPacketHandler->send_items(*client, 0);

										unsigned start = 0;

										query<gifts_tuple2>("SELECT * FROM gifts WHERE PlayerID = " + to_query(client->dbid) + " ORDER BY send_date", [session_id, start](std::vector<gifts_tuple2> const & results, bool success)
										{
											if (success)
											{
												if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
												{
													std::vector<gift_data_t> gifts;
													unsigned e = std::min<unsigned>(start + 4, results.size());

													gifts.reserve(4);
													client->shown_gifts.clear();
													for (unsigned i = start; i < e; ++i)
													{
														gifts.emplace_back();
														gift_data_t & gift = gifts.back();
														gift.dbid = std::get<0>(results[i]);
														gift.date = std::get<3>(results[i]);
														gift.isDisabled = 0;
														gift.sender = std::get<5>(results[i]);
														gift.itemid = std::get<2>(results[i]);
														client->shown_gifts.emplace_back(gift.dbid, gift.itemid);
														gift.unk2 = 0;
														//gift.unk3 = itemlist[i].text1;
														gift.message = std::get<4>(results[i]);

													}

													unsigned page_count = std::max<unsigned>(1, results.size() / 4 + ((results.size() % 4) ? 1 : 0));
													shopPacketHandler->sendTo(session_id, Network::packet(0x07df635a, page_count, (start / 4 + 1), gifts));
												}
											}
										});

									}
									else
										logger << LIGHT_RED << "Failed to load item";
								}
							});

							return;
						}
					}
					else
					{
						logger << LIGHT_RED << "Failed to accept gift id " << gift_id;
						shopPacketHandler->sendTo(session_id, Network::packet(0x0c00d6ca, 0, (char)0));
					}
					if (client->dec_read_lock())
						shopPacketHandler->callPostponed(client->session_id);
				}

			});
		}
		break;
		case 0x0180019a:
		{
			unsigned page;
			br >> page;

			if (page >= 1)
				--page;
			unsigned start = page * 4;

			query<gifts_tuple2>("SELECT * FROM gifts WHERE PlayerID = " + to_query(client.dbid) + " ORDER BY send_date", [session_id, start](std::vector<gifts_tuple2> const & results, bool success)
			{
				if (success)
				{
					if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
					{
						std::vector<gift_data_t> gifts;
						unsigned e = std::min<unsigned>(start + 4, results.size());

						gifts.reserve(4);
						client->shown_gifts.clear();
						for (unsigned i = start; i < e; ++i)
						{
							gifts.emplace_back();
							gift_data_t & gift = gifts.back();
							gift.dbid = std::get<0>(results[i]);
							gift.date = std::get<3>(results[i]);
							gift.isDisabled = 0;
							gift.sender = std::get<5>(results[i]);
							gift.itemid = std::get<2>(results[i]);
							client->shown_gifts.emplace_back(gift.dbid, gift.itemid);
							gift.unk2 = 0;
							//gift.unk3 = itemlist[i].text1;
							gift.message = std::get<4>(results[i]);

						}

						unsigned page_count = std::max<unsigned>(1, results.size() / 4 + ((results.size() % 4) ? 1 : 0));
						shopPacketHandler->sendTo(session_id, Network::packet(0x07df635a, page_count, (start / 4 + 1), gifts));
					}
				}
			});

			/*
			std::vector<gift_data_t> gifts;

			unsigned e = std::min<unsigned>(start + 4, itemlist.size());
			if(start < e)
			gifts.resize(e - start);
			for (unsigned i = start; i < e; ++i)
			{
			gift_data_t & gift = gifts[i - start];
			gift.dbid = i;
			gift.date = "today";
			gift.isDisabled = 0;
			gift.sender = "sender";
			gift.itemid = i + 1 + itemid_disp;
			gift.unk2 = tv[402];
			gift.unk3 = itemlist[i].text1;

			std::ostringstream oss;
			oss << i + 1 + itemid_disp << " " << itemlist[i].name << " " << itemlist[i].avatar;
			gift.message = oss.str();

			}
			unsigned page_count = itemlist.size() / 4 + ((itemlist.size() % 4) ? 1 : 0);
			repply(Network::packet(0x07df635a, page_count, page, gifts));
			*/
		}
		break;
		/*
		case 0x0e21f53a:	//avatar purchase
		{
		unsigned char avatar;
		br >> avatar;
		//			client.avatar = avatar;
		//			gamePacketHandler->dispatch(Network::packet(100, client.clientid, avatar));
		//repply(Network::packet(0x02e0a91a, ))
		}
		break;
		*/
		case 0x0b134e1a:
		{
			if ((int)(client.ptime - client.last_cash_request) > 1000)
			{
				client.last_cash_request = client.ptime;
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
				{
					unsigned pro = player->pro;
					unsigned clientid = client.clientid;
					query<std::tuple<unsigned> >("select cash from characters where id = " + to_query(player->dbid), [pro, session_id, clientid](std::vector<std::tuple<unsigned> > const & data, bool success)
					{
						if (success && data.size() > 0)
						{
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
								player->cash = std::get<0>(data[0]);
							if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
							{
								shopPacketHandler->sendTo(session_id, Network::packet(0x0f922c9a, pro, std::get<0>(data[0])));
							}
						}
						else
						{
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
								shopPacketHandler->sendTo(session_id, Network::packet(0x0f922c9a, pro, player->cash));
						}
					});
				}
			}
			else
			{
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
					repply(Network::packet(0x0f922c9a, player->pro, player->cash));
			}
		}
		break;
		case 0x0ebb260a:
		{
			bool dummy_displayed = false;
			stream_array<unsigned> ids;
			br >> ids;

			std::vector<player_equip_data_t> player_data;
			for (unsigned id : ids)
			{
				unsigned char avatar = 2;

				player_data.push_back(player_equip_data_t());
				player_data.back().clientId = id;
				if (id == 0x7fffffff)
				{
					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
					{
						if (player->room && player->room->replay_data)
						{
							std::shared_ptr<replay_data_t> rd = player->room->replay_data;

							player_data.back().avatar = rd->avatar;
							player_data.back().haircolor = rd->haircolor;

							for (unsigned i = 0; i < rd->item_data.size(); ++i)
							{
								player_item_data_t itemdata = player_item_data_t::make(rd->item_data[i]);
								if (itemdata.valid())
									player_data.back().items.push_back(itemdata);
							}
						}
						else
						{
							player_data.back().avatar = rng::gen_uint(2, 5);
							player_data.back().haircolor = 0;
						}
					}
					else
					{
						player_data.back().avatar = rng::gen_uint(2, 5);
						player_data.back().haircolor = 0;
					}
					dummy_displayed = true;
				}
				else
				{
					unsigned b_avatar = ~0;
					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(id))
					{
						avatar = player->avatar;
						b_avatar = player->avatar - 2;
						if (b_avatar < 4)
							player_data.back().haircolor = player->hair_color[b_avatar];
					}

					player_data.back().avatar = avatar;

					auto j = connections_by_id.find(id);
					if (j != connections_by_id.end() && b_avatar < 4)
					{

						player_data.back().items.reserve(j->second->equips[b_avatar].size());
						for (unsigned i = 0; i < j->second->equips[b_avatar].size(); ++i)
						{
							if (j->second->equips[b_avatar][i] >= j->second->items[b_avatar].size())
								continue;
							fashion_item_t const & it = j->second->items[b_avatar][j->second->equips[b_avatar][i]];
							if (!it.valid())
								continue;

							player_item_data_t itemdata = player_item_data_t::make(it);
							if (itemdata.valid())
								player_data.back().items.push_back(itemdata);
						}
					}
				}

			}

			repply(Network::packet(0x0cf9a10a, player_data));

			if (dummy_displayed)
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
					if (player->room)
						player->room->on_dummy_displayed(*player);
		}
		break;
		/*
		case 0x07fe7d1b:	//misc item page change
		{
		unsigned short start;
		unsigned short end;
		br.read(start, end);


		}
		break;
		*/

		case 0x07fe7d1c:	//cash item page change
		{
			unsigned short start;
			unsigned short end;
			br.read(start, end);

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
			{
				if (player->player_rank < 2)
					break;

				std::vector<cash_item_t> data;
				data.reserve(18);

				for (unsigned i = start; i < start + 18; ++i)
				{
					if (i >= itemlist.size())
						break;
					if (!itemlist[i].valid)
						continue;
					data.emplace_back();
					cash_item_t & item = data.back();
					item.slot = i;
					item.id = i + 1 + itemid_disp;
					item.unk1 = 0;
					item.unk2 = 0;
					item.unk4 = 0;
					item.unk5 = 0;
					item.unk6 = 0;
				}

				repply(Network::packet(0x0422842c, start, (short)player->avatar, (short)(itemlist.size()), data));
			}
		}
		break;

		case 0x0169a7fa:	//unenhance
		{
			unsigned itemid;
			unsigned short itemslot;

			br.read(itemid, itemslot);

			unsigned b_avatar = client.avatar - 2;
			if (b_avatar >= 4)
				break;

			if (itemslot < client.items[b_avatar].size() && client.items[b_avatar][itemslot].valid())
			{
				if (client.items[b_avatar][itemslot].gems.size() > 0)
				{
					client.gems_changed = true;
					unsigned short last_gem = client.items[b_avatar][itemslot].gems.back();
					client.items[b_avatar][itemslot].gems.pop_back();

					if (return_gems)
						client.add_gem(last_gem);

					//query("UPDATE fashion SET gem" + std::to_string(client.items[b_avatar][itemslot].gems.size() + 1) + " = '0' WHERE id = " + to_query(client.items[b_avatar][itemslot].dbid)) + " AND character_id = " + to_query(client.dbid);
					//query("UPDATE fashion SET gem1" + std::to_string(client.items[b_avatar][itemslot].gems.size() + 1) + " = '0' WHERE id = " + to_query(client.items[b_avatar][itemslot].dbid)) + " AND character_id = " + to_query(client.dbid);

					{
						std::string q = "UPDATE fashion SET ";

						bool first = true;
						unsigned j = 0;
						for (; j < client.items[b_avatar][itemslot].gems.size(); ++j)
						{
							if (first)
								first = false;
							else
								q += ", ";
							q += "gem" + std::to_string(j + 1) + " = " + to_query(client.items[b_avatar][itemslot].gems[j]) + ' ';
						}
						for (; j < max_gem_count; ++j)
						{
							if (first)
								first = false;
							else
								q += ", ";
							q += "gem" + std::to_string(j + 1) + " = '0' ";
						}


						q += "WHERE id = " + to_query(client.items[b_avatar][itemslot].dbid) + " AND character_id = " + to_query(client.dbid);

						query(q);
					}

					send_items(client, itemslot);
					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
						player->message("Removed last gem");
					for (unsigned i = 0; i < client.equips[b_avatar].size(); ++i)
					{
						if (client.equips[b_avatar][i] == itemslot)
						{
							send_equips(client);
							break;
						}
					}
				}
				else
				{
					if (client.last_unenhance_item == itemslot)
					{
						++client.last_unenhance_count;
						if (client.last_unenhance_count == 3)
						{
							query("INSERT INTO deleted_fashion SELECT * FROM fashion WHERE id = " + to_query(client.items[b_avatar][itemslot].dbid));
							query("DELETE FROM fashion WHERE id = " + to_query(client.items[b_avatar][itemslot].dbid));
							client.items[b_avatar][itemslot].id = 0;
							client.items[b_avatar][itemslot].dbid = 0xFFFFFFFFFFFFFFFFULL;
							send_items(client, itemslot);
						}
					}
					else
					{
						client.last_unenhance_item = itemslot;
						client.last_unenhance_count = 0;
					}
				}
			}
		}
		break;
		case 0x0a49dbda:	//add gem
		{
			d_dump(data, length, "shop");
			unsigned itemid;
			unsigned short gemid;
			unsigned short itemslot;
			br.read(itemid, gemid, itemslot);

			client.last_unenhance_item = ~0;
			unsigned gemslot = 0;
			for (; gemslot < client.gems.size(); ++gemslot)
				if (client.gems[gemslot].first == gemid)
					break;
			if (gemslot >= client.gems.size())
				break;

			unsigned b_avatar = client.avatar - 2;
			if (b_avatar >= 4)
				break;

			if (gemslot < client.gems.size() && itemslot < client.items[b_avatar].size() && client.items[b_avatar][itemslot].valid())
			{
				if (client.gems[gemslot].second > 0)
				{
					unsigned gem_kind = (client.gems[gemslot].first - 1) / 24 + 1;
					if (gem_kind < 1 || gem_kind >= gem_kinds.size())
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
							player->message("Invalid gem kind");
						break;
					}

					bool found = false;
					auto const & item = client.items[b_avatar][itemslot];
					auto const & itemdata = item.data();
					for (unsigned i = 0; i < itemdata.gem_slots.size(); ++i)
					{
						if (itemdata.gem_slots[i] == gem_kind)
						{
							found = true;
							break;
						}
					}

					if (!found)
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
							player->message("Item has no " + gem_kinds[gem_kind] + " slot");
						break;
					}

					found = false;
					bool has_better = false;
					for (unsigned i = 0; i < item.gems.size(); ++i)
					{
						unsigned gem_kind2 = (item.gems[i] - 1) / 24 + 1;
						if (gem_kind2 < 1 || gem_kind2 > gem_kinds.size())
							continue;

						if (gem_kind2 == gem_kind)
						{
							has_better = item.gems[i] >= client.gems[gemslot].first;
							found = true;
							if (!has_better)
							{
								//client.items[itemslot].gems[i] = client.gems[gemslot].first;
								for (int j = i; j < (int)client.items[b_avatar][itemslot].gems.size() - 1; ++j)
									client.items[b_avatar][itemslot].gems[j] = client.items[b_avatar][itemslot].gems[j + 1];
								client.items[b_avatar][itemslot].gems.back() = client.gems[gemslot].first;
								{
									std::string q = "UPDATE fashion SET ";

									bool first = true;
									unsigned j = 0;
									for (; j < client.items[b_avatar][itemslot].gems.size(); ++j)
									{
										if (first)
											first = false;
										else
											q += ", ";
										q += "gem" + std::to_string(j + 1) + " = " + to_query(client.items[b_avatar][itemslot].gems[j]) + ' ';
									}
									for (; j < max_gem_count; ++j)
									{
										if (first)
											first = false;
										else
											q += ", ";
										q += "gem" + std::to_string(j + 1) + " = '0' ";
									}


									q += "WHERE id = " + to_query(client.items[b_avatar][itemslot].dbid) + " AND character_id = " + to_query(client.dbid);

									query(q);
								}
							}
							break;
						}
					}

					if (has_better)
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
							player->message("Item has a better gem installed for " + gem_kinds[gem_kind] + " slot");
						break;
					}

					if (!found)
					{
						if (client.items[b_avatar][itemslot].gems.size() < max_gem_count)
						{
							client.items[b_avatar][itemslot].gems.push_back(client.gems[gemslot].first);
							query("UPDATE fashion SET gem" + std::to_string(client.items[b_avatar][itemslot].gems.size()) + " = " + to_query(client.gems[gemslot].first) + " WHERE id = " + to_query(client.items[b_avatar][itemslot].dbid)) + " AND character_id = " + to_query(client.dbid);
						}
						else
						{
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
								player->message("You can't apply more gems to this item");
							break;
						}
					}

					--client.gems[gemslot].second;
					client.gems_changed = true;

					send_items(client, itemslot);
					send_gems(client, gemslot);

					for (unsigned i = 0; i < client.equips[b_avatar].size(); ++i)
					{
						if (client.equips[b_avatar][i] == itemslot)
						{
							send_equips(client);
							break;
						}
					}

					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
						player->message("Gem applied");
				}
			}
		}
		break;
		/*
		case 0x03633aaa:	//purchase cube
		{
		unsigned short cube_id;
		unsigned count;
		br.read(cube_id, count);
		if (count > 0 && (cube_id == 1001 || cube_id == 1002 || cube_id == 1003))
		{
		if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
		{
		if (player->pro >= cube_prices[cube_id - 1001] * count)
		{
		client.add_cube(cube_id, count);
		{
		player->pro -= cube_prices[cube_id - 1001] * count;
		query("UPDATE characters SET pro = " + to_query(player->pro) + " WHERE id = " + to_query(client.dbid));
		player->message("Purchase complete");
		send_cubes(client, 0);
		}
		}
		}
		}
		}
		break;
		*/
		case 0x08da49da:
		{

			unsigned unk1;	// array?
			unsigned itemid;
			unsigned short day_count;	//15?

			br.read(unk1, itemid, day_count);

			unsigned id1 = itemid - 1 - itemid_disp;
			if (id1 < itemlist.size() && itemlist[id1].valid)
			{

				if (client.buy_item(itemid, -1, day_count))
				{
					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
					{
						repply(Network::packet(0x0f922c9a, player->pro, player->cash));
						player->message("Purchased " + itemlist[id1].name);
					}
				}
			}
		}
		break;
		case 0x04e752fc:	//move from cash inv
		{
			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
			{
				if (player->player_rank >= 2)
				{
					unsigned itemid = 0;
					br >> itemid;

					client.create_item(itemid);
				}
			}
		}
		case 0x04e752fa:	//equip/unequip
			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
			{
				d_dump(data, length, "shop");
				unsigned itemid;
				unsigned short source;	//-1 -> cashitem inv
				unsigned char is_equip;
				unsigned short dest;
				unsigned short unk1;
				br.read(itemid, source, is_equip, dest, unk1);

				bool equip_changed = false;

				unsigned b_avatar = client.avatar - 2;
				if (b_avatar >= 4)
					break;

				client.last_unenhance_item = ~0;
				client.last_equipped_item = ~0;

				if (is_equip == 0)
				{
					if (source == 0xffff)
					{
						for (unsigned i = 0; i < client.equips[b_avatar].size(); ++i)
						{
							if (client.equips[b_avatar][i] >= client.items[b_avatar].size())
								continue;

							if (client.items[b_avatar][client.equips[b_avatar][i]].id == itemid)
							{
								client.equips[b_avatar][i] = client.equips[b_avatar].back();
								client.equips[b_avatar].pop_back();
								equip_changed = true;
								break;
							}
						}
					}
					else
					{
						//item move source -> dest

						unsigned dest_limit = client.items[b_avatar].size();
						dest_limit -= dest_limit % 18;
						dest_limit += 2 * 18;

						if (source == dest || source >= client.items[b_avatar].size() || dest >= dest_limit)
							break;

						if (!client.items[b_avatar][source].valid() && !client.items[b_avatar][dest].valid())
							break;

						for (unsigned i = 0; i < client.equips[b_avatar].size(); ++i)
						{
							if (client.equips[b_avatar][i] == source)
								client.equips[b_avatar][i] = dest;
							else if (client.equips[b_avatar][i] == dest)
								client.equips[b_avatar][i] = source;
						}

						if (dest >= client.items[b_avatar].size())
							client.items[b_avatar].resize(dest + 1);

						std::swap(client.items[b_avatar][source], client.items[b_avatar][dest]);
						client.items[b_avatar][source].slot = source;
						client.items[b_avatar][dest].slot = dest;

						if (client.items[b_avatar][source].valid())
							query("UPDATE fashion SET itemslot = " + to_query(client.items[b_avatar][source].slot) + " WHERE id = " + to_query(client.items[b_avatar][source].dbid)) + " AND character_id = " + to_query(client.dbid);
						if (client.items[b_avatar][dest].valid())
							query("UPDATE fashion SET itemslot = " + to_query(client.items[b_avatar][dest].slot) + " WHERE id = " + to_query(client.items[b_avatar][dest].dbid)) + " AND character_id = " + to_query(client.dbid);


						send_items(client, dest);
					}
				}
				else
				{
					if (source >= client.items[b_avatar].size() || source == 0xffff)
						break;

					fashion_item_t const & it = client.items[b_avatar][source];
					if (!it.valid())
						break;

					if (it.data().avatar != player->avatar)
					{
						player->message("Cannot equip an item for " + avatar_names[it.data().avatar]);
						break;
					}

					bool found = false;
					unsigned slot = it.data().part_kind;

					for (unsigned i = 0; i < client.equips[b_avatar].size(); ++i)
					{
						if (client.equips[b_avatar][i] >= client.items[b_avatar].size())
							continue;
						if (!client.items[b_avatar][client.equips[b_avatar][i]].valid())
							continue;


						if (client.items[b_avatar][client.equips[b_avatar][i]].data().part_kind == slot)
						{
							found = true;
							equip_changed = true;
							client.equips[b_avatar][i] = source;
							break;
						}
					}

					if (!found)
					{
						client.equips[b_avatar].push_back(source);
						equip_changed = true;
					}
					client.last_equipped_item = source;
				}

				if (equip_changed)
				{
					client.equips_changed = true;

					send_equips(client);

					if (player->room)
					{
						update_room_clothes(player, &client);

					}
				}

			}
			break;
		case 0x07fe7d1a:	//cloth inventory page change
		{
			//d_dump(data, length, "shop");
			unsigned char unk1;	//0
			unsigned char _avatar;
			unsigned char is_equip;	//0 or 1
			unsigned short start;
			unsigned short end;
			br.read(unk1, _avatar, is_equip, start, end);


			if (is_equip)
				send_equips(client);
			else
				send_items(client, start);

		}
		break;
		case 0x0bd97dea:
			//case 0x0d7fb5ea:
			//Network::dump(data, length, "shop");
			if (client.sent_itemlist)
				break;
			client.sent_itemlist = true;

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
			{
				std::vector<itemlist_entry_t> data;

//				unsigned i = itemlist.size();
				for (unsigned i = 0; i < itemlist.size(); ++i)/*2*/
				{
					auto const & it = itemlist[i];
					if (!it.valid)
						continue;

					itemlist_entry_t s1;
					s1.id = i + 1 + itemid_disp;	//itemlist_id
													//					s1.unk2 = "";// strs[p[0]];//"Maximum 5";// str_test[i % str_test.size()];
					s1.unk3 = 11;
					s1.unk4 = it.shop_kind;		//0,2,3,4,5
					s1.unk5 = it.part_kind;		//parts
					s1.unk6 = it.item_rank;		//0,1,2,3	//tier
					s1.unk7 = it.avatar;		//avatar
					s1.name = it.name;//"Baseball Cap"; //str_test[i % 2];//strs[p[1]];// str_test[i % str_test.size()];
									  //					s1.unk9 = "";// strs[p[2]];// str_test[i % str_test.size()];
									  //					s1.unk10 = "";// strs[p[3]];// str_test[i % str_test.size()];
									  //					s1.unk11 = "";// strs[p[4]];// str_test[i % str_test.size()];
									  //					s1.unk12 = "";// strs[p[5]];// str_test[i % str_test.size()];

					s1.unk15 = 3;// rand() % 5;	//0,1,2,3,4
					s1.unk16 = 12;
					s1.unk17 = 13;

					//s1.item_prices.emplace_back(it.is_shop() ? 2 : 0, 32, it.price);
					s1.item_prices = it.prices;
					for (unsigned j = 0; j < it.gem_slots.size(); ++j)
						s1.gems.emplace_back(gem_slot_desc_t::make(it.gem_slots[j]));


					data.emplace_back(std::move(s1));
				}

				sendTo(session_id, Network::packet(0x0764594a, data, player->pro, player->cash));
			}
			break;
		case 0x04eac42a:	//avatar select
		{
			unsigned char new_avatar;
			br >> new_avatar;
			if (new_avatar >= 2 && new_avatar <= 5)
			{
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
				{
					if (player->avatar != new_avatar)
					{
						if (player->room == nullptr || (player->room != nullptr && player->room->room_state == GamePacketHandler::room_t::waiting && !player->room_ready))
						{
							player->avatar = new_avatar;
							client.avatar = new_avatar;
							query("UPDATE characters SET avatar = " + to_query(new_avatar) + " WHERE id = " + to_query(player->dbid));
							//repply(Network::packet(0x064ea1aa, (char)new_avatar, std::vector<char>{2, 3, 4, 5}, std::vector<unsigned>{1}));	/*6*/
							//							client.equips.clear();
							//							client.equips_changed = true;

							if (player->room)
								player->room->distribute(Network::packet(0x03c91b7a, GamePacketHandler::conv(*player)));

							repply(Network::packet(0x0deb3dda, (char)new_avatar, (char)0x59));
							send_equips(client);
							send_items(client, 0);
							update_room_clothes(player, &client);

						}
					}
				}
			}
		}
		break;
		case 0x09a281ea:
			//sendTo(session_id, Network::packet(0x0e1ce43a, (char)1/*, client.name*/));	//you already have characters
			repply(Network::packet(0x0e1ce43a, (char)1));
			break;
		case 0x0ba54cfa:
			client.send_avatars();
			break;
		case 0x0dfecb6a:
		{
			if (client.clientid != 0)
				break;
			d_dump(data, length, "shop");
			unsigned clientid;
			if (use_launcher)
				br >> client.auth_token;
			stream_array<char> charName, name;
			br >> clientid >> charName >> name;

			client.name = to_string(charName);

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
			{
				onLogin(client, player);
				return;
			}
			else
			{
				login_req.emplace_back(session_id, clientid);
				client.inc_read_lock();
				return;
			}
		}
		break;

		default:
			d_dump(data, length, "shop");
		}
		return;// client.can_read();
	}
	virtual void onDisconnect(Network::session_id_t session_id)
	{
		auto i = connections.find(session_id);
		if (i != connections.end())
		{
			{
				impl_lock_guard(lock, connection_count_mutex);
				if (!connection_count[i->second.dwip].dec())
					connection_count.erase(i->second.dwip);
			}

			i->second.invalidate();
			connections.erase(i);
		}
	}
	virtual void onInit()
	{
		shop_time = time(0);
		//		default_color = YELLOW;
	}

public:
	struct connection_t;
private:
	bool onLogin(connection_t & client, GamePacketHandler::connection_t * player)
	{
		if (player->name != client.name || (use_launcher && player->auth_token != client.auth_token))
		{
			client.disconnect();
			return false;
		}
		Network::session_id_t session_id = client.session_id;
		client.clientid = player->id;
		client.avatar = player->avatar;
		client.dbid = player->dbid;
		//---------
		connections_by_id[client.clientid] = &client;

		sendTo(session_id, Network::packet(0x0e02366a));

		unsigned dbid = client.dbid;

		client.inc_read_lock();
		query<std::tuple<std::vector<std::pair<unsigned short, unsigned short> >, std::vector<std::pair<unsigned short, unsigned short> >, std::array<std::vector<unsigned>, 4> > >("SELECT gems, cubes, equips FROM characters WHERE id = " + to_query(client.dbid), [session_id, dbid](std::vector<std::tuple<std::vector<std::pair<unsigned short, unsigned short> >, std::vector<std::pair<unsigned short, unsigned short> >, std::array<std::vector<unsigned>, 4> > > && results, bool success)
		{
			if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
			{
				if (success)
				{
					client->gems = std::move(std::get<0>(results[0]));
					client->cubes = std::move(std::get<1>(results[0]));
					client->equips = std::move(std::get<2>(results[0]));
					client->loaded_equips |= 1;

					//					client->gems.clear();
					//					for (unsigned i = 1; i <= 500; ++i)
					//						client->gems.push_back(std::make_pair(i, 100));
				}
				else
					logger << RED << "Failed to read gems, cubes and equips for player id " << dbid;

				if (client->dec_read_lock())
					shopPacketHandler->callPostponed(session_id);

				if (client->loaded_equips == 3)
					client->check_equips();
			}
		});

		client.inc_read_lock();
		query<fashion_tuple>("SELECT * FROM fashion WHERE character_id = " + to_query(client.dbid), [session_id, dbid](std::vector<fashion_tuple> const & data, bool success)
		{
			if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
			{
				if (success)
				{
					std::vector<unsigned> to_replace;

					for (unsigned i = 0; i < data.size(); ++i)
					{
						int slot = std::get<3>(data[i]);
						if (slot < 0)
							to_replace.push_back(i);
						else
						{
							unsigned itemid = std::get<2>(data[i]) - 1 - itemid_disp;
							if (itemid < itemlist.size() && itemlist[itemid].valid)
							{
								unsigned b_avatar = itemlist[itemid].avatar - 2;
								if (b_avatar < 4)
								{
									if (client->items[b_avatar].size() <= (unsigned)slot)
										client->items[b_avatar].resize(slot + 1);

									fashion_item_t & it = client->items[b_avatar][slot];
									if (it.valid())
										to_replace.push_back(i);
									else
										it.set(data[i], slot);
								}
							}
						}
					}

					int slot = 0;
					for (unsigned i : to_replace)
					{
						unsigned itemid = std::get<2>(data[i]) - 1 - itemid_disp;
						if (itemid < itemlist.size() && itemlist[itemid].valid)
						{
							unsigned b_avatar = itemlist[itemid].avatar - 2;
							if (b_avatar < 4)
							{
								for (; slot < (int)client->items[b_avatar].size(); ++slot)
									if (!client->items[b_avatar][slot].valid())
										break;

								if (client->items[b_avatar].size() <= (unsigned)slot)
									client->items[b_avatar].resize(slot + 1);
								fashion_item_t & it = client->items[b_avatar][slot];
								it.set(data[i], slot);

								query("UPDATE fashion SET itemslot = " + to_query(it.slot) + " WHERE id = " + to_query(it.dbid)) + " AND character_id = " + to_query(client->dbid);
								++slot;
							}
						}
					}
				}
				else
					logger << RED << "Failed to read fashion items for player id " << dbid;
				client->loaded_equips |= 2;

				if (client->dec_read_lock())
					shopPacketHandler->callPostponed(session_id);

				if (client->loaded_equips == 3)
					client->check_equips();
			}
		});

		return true;
	}

public:
	std::atomic<unsigned> save_all;
private:
	std::vector<std::pair<Network::session_id_t, unsigned> > login_req;
	unsigned last_client_check = get_tick();
	mutex_t timer_mutex;
	virtual void onTimer()
	{
		if (this->destroyed())
			return;
		impl_lock_guard(lock, timer_mutex);
		unsigned tick = get_tick();
		for (unsigned i = 0; i < login_req.size();)
		{
			auto j = connections.find(login_req[i].first);
			if (j == connections.end())
			{
				login_req[i] = login_req.back();
				login_req.pop_back();
				continue;
			}
			if (tick - j->second.ptime > max_client_idle_time)
			{
				connection_t * p = &j->second;
				login_req[i] = login_req.back();
				login_req.pop_back();
				p->disconnect(DisconnectKind::IMMEDIATE_DC);
				continue;
			}

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(login_req[i].second))
			{
				try
				{
					if (onLogin(j->second, player))
						if (j->second.dec_read_lock())
							this->callPostponed(j->second.session_id);
				}
				LOG_CATCH();
				login_req[i] = login_req.back();
				login_req.pop_back();
			}
			else
				++i;
		}

		if (save_all)
		{
			for (auto i = connections.begin(); i != connections.end(); ++i)
			{
				if (!i->second.valid())
					continue;
				i->second.save_items();
			}
			save_all = 0;
		}

		if (tick - last_client_check > client_check_interval)
		{
			last_client_check = tick;
			distribute(Network::packet(0));


			for (auto i = connections.begin(); i != connections.end(); ++i)
			{
				if (!i->second.valid())
					continue;
				i->second.save_items();
			}
		}

		unsigned dc_all = disconnect_all;
		switch (dc_all)
		{
		case 1:
			for (auto i = connections.begin(); i != connections.end(); ++i)
				disconnect(i->second.session_id);
			if (connections.size() == 0)
				disconnect_all = 0;
			else
				disconnect_all = 2;
			break;
		case 2:
			if (connections.size() == 0)
				disconnect_all = 0;
			break;
		}

	}

public:
	std::atomic<unsigned> disconnect_all;

	struct connection_t
		: public Network::NetworkClientBase
	{
		Network::session_id_t session_id;
		std::string ip;
		std::string auth_token;
		unsigned port;
		unsigned clientid = 0;
		unsigned dwip;
		unsigned dbid = 0;
		unsigned ptime;
		unsigned avatar;
		unsigned last_unenhance_item = ~0;
		unsigned last_unenhance_count = 0;
		unsigned last_cash_request = 0;
		unsigned loaded_equips = 0;
		unsigned last_equipped_item = ~0;
		std::vector<std::pair<unsigned, unsigned> > shown_gifts;
		std::string name;
		bool sent_itemlist : 1;
		bool gems_changed : 1;
		bool cubes_changed : 1;
		bool equips_changed : 1;
		connection_t()
			: sent_itemlist(false)
			, gems_changed(false)
			, cubes_changed(false)
			, equips_changed(false)
		{
		}

		virtual void invalidate()
		{
			if (_valid)
			{
				_valid = false;
				shopPacketHandler->connections_by_id.erase(clientid);
				save_items();
			}
		}
		void disconnect(DisconnectKind disconnectKind = DisconnectKind::SOFT_DC)
		{
			invalidate();
			shopPacketHandler->disconnect(session_id, disconnectKind);
		}

		void send_avatars()
		{
			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
				shopPacketHandler->sendTo(session_id, Network::packet(0x064ea1aa, (char)player->avatar, std::vector<char> { 2, 3, 4, 5 }, std::vector<unsigned>{player->hair_color[0], player->hair_color[1], player->hair_color[2], player->hair_color[3]}));
		}

		std::vector<std::pair<unsigned short, unsigned short> > gems;
		std::vector<std::pair<unsigned short, unsigned short> > cubes;
		std::array<std::vector<unsigned>, 4> equips;
		std::array<std::vector<fashion_item_t>, 4> items;
		/*
		unsigned disable_recv = 0;
		bool dec_read_lock()
		{
		assert(disable_recv > 0);
		--disable_recv;
		if(disable_recv == 0)
		shopPacketHandler->enableRecv(session_id);
		return disable_recv == 0;
		}

		inline bool inc_read_lock()
		{
		++disable_recv;
		return false;
		}

		inline bool can_read() const
		{
		return disable_recv == 0;
		}
		*/
		void save_items()
		{
			if (gems_changed || cubes_changed || equips_changed)
			{
				std::string q = "UPDATE characters SET ";
				bool first = true;
				if (gems_changed)
				{
					q += "gems = " + to_query(gems) + ' ';
					gems_changed = false;
					first = false;
				}
				if (cubes_changed)
				{
					if (!first)
						q += ',';
					q += "cubes = " + to_query(cubes) + ' ';
					cubes_changed = false;
					first = false;
				}
				if (equips_changed)
				{
					if (!first)
						q += ',';
					q += "equips = " + to_query(equips) + ' ';
					equips_changed = false;
					first = false;
				}
				q += "WHERE id = " + to_query(dbid);
				query(q);
			}
		}

		void check_equips()
		{
			unsigned b_avatar = avatar - 2;
			if (b_avatar >= 4)
				return;

			for (unsigned b_avatar = 0; b_avatar < 4; ++b_avatar)
			{
				std::vector<bool> taken;
				for (unsigned i = 0; i < equips[b_avatar].size(); )
				{
					if (equips[b_avatar][i] >= items[b_avatar].size())
					{
						equips[b_avatar][i] = equips[b_avatar].back();
						equips[b_avatar].pop_back();
						equips_changed = true;
						continue;
					}
					fashion_item_t const & it = items[b_avatar][equips[b_avatar][i]];
					if (!it.valid())
					{
						equips[b_avatar][i] = equips[b_avatar].back();
						equips[b_avatar].pop_back();
						equips_changed = true;
						continue;
					}

					unsigned slot = it.data().part_kind;
					if (taken.size() <= slot)
						taken.resize(slot + 1, false);
					if (taken[slot])
					{
						equips[b_avatar][i] = equips[b_avatar].back();
						equips[b_avatar].pop_back();
						equips_changed = true;
						continue;
					}
					taken[slot] = true;
					++i;
				}
			}
		}

		bool create_item(unsigned itemid, int slot = -1, time_t expiration = 0)
		{
			unsigned _id1 = itemid - 1 - itemid_disp;
			if (_id1 < itemlist.size() && itemlist[_id1].valid)
			{
				unsigned b_avatar = itemlist[_id1].avatar - 2;
				if (b_avatar >= 4)
					return false;

				if (slot < 0)
					for (slot = 0; slot < items[b_avatar].size(); ++slot)
						if (!items[b_avatar][slot].valid())
							break;

				Network::session_id_t session_id = this->session_id;
				inc_read_lock();

				query<std::tuple<long long> >("SELECT create_item(" + to_query(dbid, itemid, slot, expiration) + ")", [session_id, b_avatar](std::vector<std::tuple<long long> > const & data, bool success)
				{
					if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
					{
						if (success && data.size() > 0)
						{
							long long id = std::get<0>(data[0]);
							if (id > 0)
							{
								query<fashion_tuple>("SELECT * FROM fashion WHERE id = " + to_query(id), [session_id, b_avatar](std::vector<fashion_tuple> const & data, bool success)
								{
									if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
									{
										if (client->dec_read_lock())
											shopPacketHandler->callPostponed(session_id);
										if (success && data.size() > 0)
										{
											unsigned const i = 0;

											int slot = std::get<3>(data[0]);
											if (slot < 0)
											{
												slot = 0;
												for (; slot < (int)client->items[b_avatar].size(); ++slot)
													if (!client->items[b_avatar][slot].valid())
														break;
											}

											if (client->items[b_avatar].size() <= (unsigned)slot)
												client->items[b_avatar].resize(slot + 1);
											fashion_item_t & it = client->items[b_avatar][slot];
											it.set(data[i], slot);

											shopPacketHandler->send_items(*client, 0);
										}
										else
											logger << LIGHT_RED << "Failed to create item";
									}
								});
								return;
							}
						}
						else
							logger << LIGHT_RED << "Failed to create item";
						if (client->dec_read_lock())
							shopPacketHandler->callPostponed(session_id);
					}

				});

				return true;
			}
			return false;
		}

		bool buy_item(unsigned itemid, int slot = -1, unsigned day_count = 0)
		{
			unsigned day_count_2;

			switch (day_count)
			{
			case 7:
				day_count_2 = 30;
				break;
			case 28:
				day_count_2 = 90;
				break;
			default:
				return false;
			}


			unsigned _id1 = itemid - 1 - itemid_disp;
			if (_id1 < itemlist.size() && itemlist[_id1].valid)
			{
				unsigned b_avatar = itemlist[_id1].avatar - 2;
				if (b_avatar >= 4)
					return false;


				if (slot < 0)
					for (slot = 0; slot < items[b_avatar].size(); ++slot)
						if (!items[b_avatar][slot].valid())
							break;

				time_t expiration = 0;
				bool found = false;
				unsigned price = 0;
				bool is_shop = false;

				for (unsigned i = 0; i < itemlist[_id1].prices.size(); ++i)
				{
					if (day_count == itemlist[_id1].prices[i].day_count)
					{
						expiration = time(0) + 60 * 60 * 24 * day_count_2;
						price = itemlist[_id1].prices[i].price;
						found = true;
						is_shop = itemlist[_id1].prices[i].is_shop == 2;
						break;
					}
				}

				if (!found)
					return false;

				Network::session_id_t session_id = this->session_id;

				if (is_shop)
				{
					inc_read_lock();
					query<std::tuple<long long> >("SELECT buy_item(" + to_query(dbid, itemid, slot, expiration, price) + ")", [session_id, b_avatar](std::vector<std::tuple<long long> > const & data, bool success)
					{
						if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
						{
							if (success && data.size() > 0)
							{
								long long id = std::get<0>(data[0]);
								if (id > 0)
								{
									query<fashion_tuple>("SELECT * FROM fashion WHERE id = " + to_query(id), [session_id, b_avatar](std::vector<fashion_tuple> const & data, bool success)
									{
										if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
										{
											if (client->dec_read_lock())
												shopPacketHandler->callPostponed(session_id);
											if (success && data.size() > 0)
											{
												unsigned const i = 0;

												int slot = std::get<3>(data[0]);
												if (slot < 0)
												{
													slot = 0;
													for (; slot < (int)client->items[b_avatar].size(); ++slot)
														if (!client->items[b_avatar][slot].valid())
															break;
												}

												if (client->items[b_avatar].size() <= (unsigned)slot)
													client->items[b_avatar].resize(slot + 1);
												fashion_item_t & it = client->items[b_avatar][slot];
												it.set(data[i], slot);

												shopPacketHandler->send_items(*client, 0);
											}
											else
											{
												//												message(client->clientid, "Failed to purchase item");
												logger << LIGHT_RED << "Failed to load purchased item!";
											}
										}
									});
									return;
								}
								else
								{
									message(client->clientid, "Failed to purchase item");
								}
							}
							else
							{
								message(client->clientid, "Failed to purchase item");
								//								logger << LIGHT_RED << "Failed to create item";
							}
							if (client->dec_read_lock())
								shopPacketHandler->callPostponed(session_id);
						}

					});
				}
				else
				{
					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
					{
						if (player->pro >= price)
						{
							player->pro -= price;
							query("UPDATE characters SET pro = " + to_query(player->pro) + " WHERE id = " + to_query(dbid));
						}
						else
							return false;
					}
					else
						return false;

					inc_read_lock();
					query<std::tuple<long long> >("SELECT create_item(" + to_query(dbid, itemid, slot, expiration) + ")", [session_id, b_avatar](std::vector<std::tuple<long long> > const & data, bool success)
					{
						if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
						{
							if (success && data.size() > 0)
							{
								long long id = std::get<0>(data[0]);
								if (id > 0)
								{
									query<fashion_tuple>("SELECT * FROM fashion WHERE id = " + to_query(id), [session_id, b_avatar](std::vector<fashion_tuple> const & data, bool success)
									{
										if (ShopPacketHandler::connection_t * client = shopPacketHandler->get_client(session_id))
										{
											if (client->dec_read_lock())
												shopPacketHandler->callPostponed(session_id);
											if (success && data.size() > 0)
											{
												unsigned const i = 0;

												int slot = std::get<3>(data[0]);
												if (slot < 0)
												{
													slot = 0;
													for (; slot < (int)client->items[b_avatar].size(); ++slot)
														if (!client->items[b_avatar][slot].valid())
															break;
												}

												if (client->items[b_avatar].size() <= (unsigned)slot)
													client->items[b_avatar].resize(slot + 1);
												fashion_item_t & it = client->items[b_avatar][slot];
												it.set(data[i], slot);

												shopPacketHandler->send_items(*client, 0);
											}
											else
												logger << LIGHT_RED << "Failed to load created item";
										}
									});
									return;
								}
							}
							else
								logger << LIGHT_RED << "Failed to create item";
							if (client->dec_read_lock())
								shopPacketHandler->callPostponed(session_id);
						}

					});
				}
				return true;
			}
			return false;
		}

		void add_gem(unsigned id)
		{
			unsigned r = ~0;

			for (unsigned i = 0; i < gems.size(); ++i)
			{
				if (gems[i].first == id)
				{
					if (gems[i].second + 1 < 10000)
					{
						++gems[i].second;
						r = i;
					}
					else
						r = i;	//prevent multiple stacks
					break;
				}
			}

			if (r == ~0)
				gems.emplace_back(id, 1);

			std::sort(gems.begin(), gems.end());
			for (r = 0; r < gems.size(); ++r)
				if (gems[r].first == id)
					break;
			gems_changed = true;
			shopPacketHandler->send_gems(*this, r);
		}

		void add_cube(unsigned id, unsigned count = 1)
		{
			unsigned r = ~0;

			for (unsigned i = 0; i < cubes.size(); ++i)
			{
				if (cubes[i].first == id)
				{
					if (cubes[i].second + count < 10000)
					{
						cubes[i].second += count;
						r = i;
					}
					break;
				}
			}

			if (r == ~0)
				cubes.emplace_back(id, 1);

			std::sort(cubes.begin(), cubes.end());
			for (r = 0; r < cubes.size(); ++r)
				if (cubes[r].first == id)
					break;
			cubes_changed = true;
			shopPacketHandler->send_cubes(*this, r);
		}
	};

	void send_equips(connection_t & client)
	{
		unsigned b_avatar = client.avatar - 2;
		if (b_avatar >= 4)
			return;

		std::vector<cloth_item_data_t> clothes;

		clothes.reserve(client.equips[b_avatar].size());

		unsigned max_i = 0;
		for (unsigned i = 0; i < client.equips[b_avatar].size(); ++i)
		{
			if (client.equips[b_avatar][i] >= client.items[b_avatar].size())
				continue;
			fashion_item_t const & it = client.items[b_avatar][client.equips[b_avatar][i]];
			if (!it.valid())
				continue;

			clothes.emplace_back(it);
			max_i = i + 1;
		}

		sendTo(client.session_id, Network::packet(0x0422842a, (char)0, (char)client.avatar, (char)1, (short)0, (short)client.avatar, (short)max_i, clothes));
	}

	void send_items(connection_t & client, unsigned start)
	{
		unsigned b_avatar = client.avatar - 2;
		if (b_avatar >= 4)
			return;
		std::vector<cloth_item_data_t> clothes;

		clothes.reserve(18);

//		unsigned max_i = 0;
		start -= start % 18;
		for (unsigned i = 0; i < 18; ++i)
		{
			if (start + i >= client.items[b_avatar].size())
				break;

			fashion_item_t const & it = client.items[b_avatar][start + i];
			if (!it.valid())
				continue;

			clothes.emplace_back(it);
//			max_i = i + 1;
		}

		unsigned short end = (unsigned short)std::min<unsigned>(0xffff, client.items[b_avatar].size());
		sendTo(client.session_id, Network::packet(0x0422842a, (char)0, (char)client.avatar, (char)0, (short)start, (short)client.avatar, (short)end, clothes));
	}

	void send_gems(connection_t & client, unsigned start)
	{
		std::vector<item_data_t> v;
		v.reserve(12);
		start -= start % 12;

		if (start >= client.gems.size())
		{
			start = client.gems.size();
			if (start > 0)
			{
				--start;
				start -= start % 12;
			}
		}
		unsigned end = start + 12;
		for (unsigned i = start; i < end && i < client.gems.size(); ++i)
		{
			if (client.gems[i].second > 0)
				v.push_back(item_data_t{ (unsigned short)i, client.gems[i].first, client.gems[i].second });
		}
		end = (unsigned short)std::min<unsigned>(0xffff, client.gems.size());
		sendTo(client.session_id, Network::packet(0x0c44bd6a, (short)start, (short)client.avatar, (short)end, v));
	}

	void send_cubes(connection_t & client, unsigned start)
	{
		std::vector<item_data_t> v;
		v.reserve(3);
		start -= start % 3;

		if (start >= client.cubes.size())
		{
			start = client.cubes.size();
			if (start > 0)
			{
				--start;
				start -= start % 3;
			}
		}

		unsigned end = start + 3;
		for (unsigned i = start; i < end && i < client.cubes.size(); ++i)
		{
			if (client.cubes[i].second > 0)
				v.push_back(item_data_t{ (unsigned short)i, client.cubes[i].first, client.cubes[i].second });
		}
		end = (unsigned short)std::min<unsigned>(0xffff, client.cubes.size());
		sendTo(client.session_id, Network::packet(0x0365729a, (short)start, (short)client.avatar, (short)end, v));
	}

	void update_room_clothes(GamePacketHandler::connection_t * player, ShopPacketHandler::connection_t * client, bool send_to_self = false);

	connection_t * get_client(Network::session_id_t session_id)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return nullptr;
		if (!_i->second.valid())
			return nullptr;
		return &_i->second;
	}

	std::map<Network::session_id_t, connection_t> connections;
	std::map<unsigned, connection_t*> connections_by_id;

	ShopPacketHandler()
        : save_all(0)
        , disconnect_all(0)
    {}

	virtual ~ShopPacketHandler()
	{
		impl_lock_guard(lock, timer_mutex);
		this->_destroyed = true;
	}
};

void add_gem(unsigned clientid, unsigned gem_id)
{
	auto i = shopPacketHandler->connections_by_id.find(clientid);
	if (i != shopPacketHandler->connections_by_id.end())
		i->second->add_gem(gem_id);
}


unsigned add_pro(unsigned clientid, unsigned pro)
{
	auto i = gamePacketHandler->connections_by_id.find(clientid);
	if (i != gamePacketHandler->connections_by_id.end())
	{
		if (~0 - i->second->pro >= pro)
			i->second->pro += pro;
		else
			i->second->pro = ~0;

		return i->second->pro;
	}
	return 0;
}

struct guild_data_t
{
	unsigned id;
	std::string name;
	std::string msg;
	std::string rule;
	std::string master_name;
	unsigned crew_point = 0;
	unsigned limit = 10;
	unsigned count = 10;

	void operator << (BinaryWriter & bw) const
	{
		bw << id << name << "" << limit << count;
		bw << 0 << 0;
		bw << 0ULL << 0ULL;
		bw << 0 << msg << rule << 0 << master_name << 0;
		bw << (unsigned long long)crew_point;
		bw << 0 << 0;
	}
};

class GuildPacketHandler
	: public Network::NetworkEventHandler
{
	virtual std::string getConnectionData(Network::session_id_t id) const
	{
		auto i = connections.find(id);
		if (i != connections.end())
			return i->second.name;
		return std::string();
	}
	virtual DisconnectKind onConnect(Network::session_id_t session_id, std::string && ip, unsigned port)
	{
		unsigned dwip = Network::string2ipv4(ip);
		{
			impl_lock_guard(lock, connection_count_mutex);
			if (!connection_count[dwip].inc())
				return DisconnectKind::IMMEDIATE_DC;
		}

		connection_t & client = connections[session_id];
		client.session_id = session_id;
		client.ip = std::move(ip);
		client.port = port;
		client.dwip = dwip;
		client.ptime = get_tick();
		return DisconnectKind::ACCEPT;
	}
	virtual void onRecieve(Network::session_id_t session_id, const char * data, unsigned length)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return;
		connection_t & client = _i->second;
		client.check_valid();
		client.ptime = get_tick();

		BinaryReader br(data, length);
		br.skip(4);
		unsigned packetid = br.read<unsigned>();

		switch (log_inpacket_ids)
		{
		case 1:
			if (packetid != 0x0b63816a)
				logger.print("guild inpacket %08x\r\n", packetid);
			break;
		case 2:
			logger.print("guild inpacket %08x\r\n", packetid);
			break;
		}
		if (log_inpackets)
			Network::dump(data, length, "guild");

		if (client.clientid == 0 && packetid != 0x0a506cea && packetid != 0x0b63816a)
		{
			client.disconnect(DisconnectKind::IMMEDIATE_DC);
			return;
		}

		switch (packetid)
		{
		case 0x0b63816a:	//repeated (keepalive?)
			d_dump(data, length, "guild");
			break;
		case 0x0a506cea:	//login req
		{
			if (client.clientid != 0)
				break;

			unsigned clientid;
			std::string _charName;
			unsigned level;

			if (use_launcher)
				br >> client.auth_token;

			//br >> clientid >> charName >> level;
			br.read(clientid, _charName, level);
			client.clientid = clientid;
			client.name = std::move(_charName);

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
			{
				onLogin(client, player);
				return;
			}
			else
			{
				login_req.emplace_back(session_id, clientid);
				client.inc_read_lock();
				return;
			}
		}
		break;

		case 0x0217baba:
		{
			/*
			stream_array<char> charName;
			unsigned level, roomid;
			br >> charName;
			br >> level;
			br >> roomid;

			//client.level = level;
			//client.room_id = roomid;
			*/
		}
		break;
		case 0x01ac1b6a:	//add to bulletin
			if (client.name.size())
			{
				std::string msg;
				br >> msg;
				bulletin.emplace_back(++bulletin_seq_id, client.name, std::move(msg));
				query("INSERT INTO bulletin VALUES(" + to_query(bulletin.back().id, 0, client.name, bulletin.back().msg, 0) + ")");
			}
			break;
		case 0x0eafedaa:	//delete from bulletin
		{
			long long _id;
			br >> _id;
			unsigned id = _id;
			for (unsigned i = 0; i < bulletin.size(); ++i)
			{
				if (bulletin[i].id == id)
				{
					if (bulletin[i].name == client.name)
					{
						bulletin[i].is_deleted = true;
						repply(Network::packet(0x0fa2666a, 0));
						query("UPDATE bulletin SET is_deleted = 1 WHERE id = " + to_query(id));
					}
					break;
				}
			}
		}
		break;
		case 0x0ecbb39a:	//bulletin
		{
			unsigned start, startpage, count_limit;
			//br >> startpage >> count_limit;
			br.read(startpage, count_limit);

			--startpage;
			if (count_limit == 0)
				count_limit = 11;

			BinaryWriter bw;
			bw << Network::packet_header_t();
			bw << 0x093dceaa;
			bw << 0;

			unsigned send_count_pos = bw.allocate_pos<unsigned>();
			unsigned send_count = 0;

			unsigned max_page = bulletin.size() / count_limit;

			if (max_page > 0 && bulletin.size() % count_limit == 0)
				--max_page;

			if (startpage <= max_page)
			{
				start = (max_page - startpage) * count_limit;

				for (unsigned i = start; i < bulletin.size() && send_count < count_limit; ++i, ++send_count)
					bw << bulletin[i];

				if (send_count < count_limit && startpage == 0)
				{
					bulletin_entry_t dummy(~0, "", "");
					for (; send_count < count_limit; ++send_count)
						bw << dummy;
				}

			}

			bw.at<unsigned>(send_count_pos) = send_count;

			repply(std::move(bw.data));
		}
		break;

		case 0x04882c3a:	//add to guild bulletin
			if (client.name.size() && client.guild)
			{
				std::string msg;
				br >> msg;

				client.guild->bulletin.emplace_back(++bulletin_seq_id, client.name, std::move(msg));
				client.guild->distribute2(Network::packet(0x096b164a, 0));
				query("INSERT INTO bulletin VALUES(" + to_query(client.guild->bulletin.back().id, client.guild->guild_id, client.name, client.guild->bulletin.back().msg, 0) + ")");
			}
			break;

		case 0x036bc09a:	//delete guild from bulletin
			if (client.guild)
			{
				long long _id;
				br >> _id;
				unsigned id = _id;
				for (unsigned i = 0; i < client.guild->bulletin.size(); ++i)
				{
					if (client.guild->bulletin[i].id == id)
					{
						if (client.guild->bulletin[i].name == client.name)
						{
							client.guild->bulletin[i].is_deleted = true;
							client.guild->distribute2(Network::packet(0x096b164a, 0));
							query("UPDATE bulletin SET is_deleted = 1 WHERE id = " + to_query(id));
						}
						break;
					}
				}
			}
			break;

		case 0x09be846a:	//guild bulletin
			if (client.guild)
			{
				unsigned start, startpage, count_limit;
				//br >> startpage >> count_limit;
				br.read(startpage, count_limit);

				--startpage;
				if (count_limit == 0)
					count_limit = 11;
				/*7*/

				BinaryWriter bw;
				bw << Network::packet_header_t();
				bw << 0x0082149a;
				bw << client.guild->guild_id;

				unsigned send_count_pos = bw.allocate_pos<unsigned>();
				unsigned send_count = 0;

				unsigned max_page = client.guild->bulletin.size() / count_limit;

				if (max_page > 0 && client.guild->bulletin.size() % count_limit == 0)
					--max_page;

				if (startpage <= max_page)
				{
					start = (max_page - startpage) * count_limit;

					for (unsigned i = start; i < client.guild->bulletin.size() && send_count < count_limit; ++i, ++send_count)
						bw << client.guild->bulletin[i];

					if (send_count < count_limit && startpage == 0)
					{
						bulletin_entry_t dummy(~0, "", "");
						for (; send_count < count_limit; ++send_count)
							bw << dummy;
					}

				}

				bw.at<unsigned>(send_count_pos) = send_count;

				repply(std::move(bw.data));
			}
			break;
		case 0x05706e8a:	//cafe online member list
		{
			unsigned start, startpage, count_limit;
			//br >> startpage >> count_limit;
			br.read(startpage, count_limit);

			start = (startpage - 1) * count_limit;

			//			repply(Network::packet(0x07d011da, 0, 1, "Online: " + std::to_string(online_count), 1, 0, 0));
			//			break;

			bool gm_char = false;
			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
				gm_char = player->player_rank >= 2;

			BinaryWriter bw;
			bw << Network::packet_header_t();
			bw << 0x07d011da;
			bw << 0;

			unsigned send_count_pos = bw.allocate_pos<unsigned>();
			unsigned send_count = 0;
			unsigned it_count = 0;
			for (auto i = gamePacketHandler->connections_by_name.begin(); i != gamePacketHandler->connections_by_name.end(); ++i)
			{
				if (it_count >= start)
				{
					bw << i->second->name;
					bw << i->second->level;
					if (i->second->room != nullptr && i->second->room->room_state == GamePacketHandler::room_t::waiting &&
						((!gm_char && i->second->room->players.size() < i->second->room->player_limit && i->second->room->password.size() == 0) || (gm_char && i->second->room->players.size() < 8)))
					{
						bw << 1 << (i->second->room->room_id + room_id_disp);
					}
					else
					{
						bw << 0 << 0;
					}
					++send_count;
				}

				if (send_count >= count_limit)
					break;

				++it_count;
			}

			bw.at<unsigned>(send_count_pos) = send_count;

			repply(std::move(bw.data));
		}
		break;
		case 0x0fa0cd6a:	//guild applicant list
			if (client.guild)
				client.guild->send_applicant_list(client);
			break;
		case 0x0eb7015a:	//deny aplicant
			if (client.guild && client.guild->is_master(client))
			{
				unsigned id;
				stream_array<char> name;
				unsigned unk1;
				unsigned unk2;
				//br >> id >> name >> unk1 >> unk2;
				br.read(id, name, unk1, unk2);
				client.guild->deny(client, name);
			}
			break;
		case 0x0de8e6da:	//accept aplicant
			if (client.guild && client.guild->is_master(client))
			{
				unsigned id;
				stream_array<char> name;
				unsigned unk1;
				unsigned unk2;
				//br >> id >> name >> unk1 >> unk2;
				br.read(id, name, unk1, unk2);
				client.guild->accept(client, name);
			}
			break;
		case 0x02f8193a:	//send invitation
			if (client.guild && client.guild->is_master(client))
			{
				std::string name;
				br >> name;

				auto i = connections_by_name.find(name);
				if (i != connections_by_name.end())
				{
					if (i->second->guild == client.guild)
						repply(Network::packet(0x0870b6ca, 1));
					else
					{
						if (i->second->guild && i->second->guild->is_master(*i->second))
							repply(Network::packet(0x0870b6ca, 1));
						else
						{
							send_message(name, 10, "", "", client.guild->name, "");
							repply(Network::packet(0x0870b6ca, 0));
						}
					}
				}
				else
				{
					std::string guild_name = client.guild->name;
					query<std::tuple<std::string> >("SELECT master_name FROM guilds WHERE id = (SELECT guildid FROM characters WHERE name = " + to_query(name) + ")", [name, session_id, guild_name](std::vector<std::tuple<std::string> > const & data, bool success)
					{
						GuildPacketHandler::connection_t * client = guildPacketHandler->get_client(session_id);
						if (success)
						{
							if (data.size() == 0)
							{
								if (client)
									guildPacketHandler->sendTo(session_id, Network::packet(0x0870b6ca, 1));
							}
							else
							{
								if (name == std::get<0>(data[0]))
								{
									if (client)
										guildPacketHandler->sendTo(session_id, Network::packet(0x0870b6ca, 1));
								}
								else
								{
									if (client)
										guildPacketHandler->sendTo(session_id, Network::packet(0x0870b6ca, 0));
									send_message(name, 10, "", "", guild_name, "");
								}
							}
						}
						else
						{
							if (client)
								guildPacketHandler->sendTo(session_id, Network::packet(0x0870b6ca, 1));
						}
					});
				}
			}
			break;
		case 0x0550662a:	//guild member list
		{
			unsigned start, startpage, count_limit = 8;
			br >> startpage;
			start = (startpage - 1) * count_limit;

			if (client.guild)
			{
				BinaryWriter bw;
				bw << Network::packet_header_t();
				bw << 0x0757e78a;
				//				bw << client.guild->guild_id;

				unsigned send_count_pos = bw.allocate_pos<unsigned>();
				unsigned send_count = 0;
//				unsigned it_count = 0;
				//for (auto j = client.guild->members.begin(); j != client.guild->members.end(); ++j)
				for (unsigned j = start, je = client.guild->members.size(); j < je && send_count < count_limit; ++j, ++send_count)
				{
					auto const & member = client.guild->members[j];
					unsigned id = 0;
					auto k = gamePacketHandler->connections_by_dbid.find(member.id);
					if (k != gamePacketHandler->connections_by_dbid.end())
						id = k->second->id;
					else
						id = member.id;
					bw << id;
					bw << member.name;
					bw << member.level;
					bw << tv[0];
					bw << tv[1];
					bw << member.point;
					bw << member.rank;
				}

				bw.at<unsigned>(send_count_pos) = send_count;

				repply(std::move(bw.data));
			}
		}
		break;
		case 0x075c979a:	//guild online member list
		{
			unsigned start, startpage, count_limit;
			//br >> startpage >> count_limit;
			br.read(startpage, count_limit);

			start = (startpage - 1) * count_limit;
			if (client.guild)
			{
				BinaryWriter bw;
				bw << Network::packet_header_t();
				bw << 0x05e72ffa;
				bw << client.guild->guild_id;


				unsigned send_count_pos = bw.allocate_pos<unsigned>();
				unsigned send_count = 0;

				bool gm_char = false;
				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(client.clientid))
					gm_char = player->player_rank >= 2;


				//				bw << (std::string)"Disabled" << 1 << 0 << 0;
				//				++send_count;

				for (unsigned j2 = start, e = client.guild->online_members_by_clientid.size(); j2 < e && send_count < count_limit; ++j2)
				{
					auto i = gamePacketHandler->connections_by_id.find(client.guild->online_members_by_clientid[j2]);
					if (i == gamePacketHandler->connections_by_id.end())
						continue;

					bw << i->second->name;
					bw << i->second->level;
					if (i->second->room != nullptr && i->second->room->room_state == GamePacketHandler::room_t::waiting &&
						((!gm_char && i->second->room->players.size() < i->second->room->player_limit && i->second->room->password.size() == 0) || (gm_char && i->second->room->players.size() < 8)))
					{
						bw << 1 << (i->second->room->room_id + room_id_disp);
					}
					else
					{
						bw << 0 << 0;
					}
					++send_count;
				}

				bw.at<unsigned>(send_count_pos) = send_count;

				repply(std::move(bw.data));
			}
		}
		break;
		case 0x03b0da4a:	//guild/cafe list
		{
			d_dump(data, length, "guild");
			unsigned is_guild;
			unsigned focus;

			unsigned start, startpage, count_limit = 6;
			//br >> is_guild >> focus >> startpage;
			br.read(is_guild, focus, startpage);

			start = (startpage - 1) * count_limit;


			if (is_guild == 0)
			{
				repply(Network::packet(0x025a4e2a, 0, 0, 0, 0));
			}
			else
			{

				guild_data_t guild_data;

				BinaryWriter bw;
				bw << Network::packet_header_t();
				bw << 0x025a4e2a;
				bw << 0 << 0 << 0;

				unsigned send_count_pos = bw.allocate_pos<unsigned>();
				unsigned send_count = 0;
				unsigned it_count = 0;
				for (auto i = guilds.begin(); i != guilds.end(); ++i)
				{
					if (i->second->master_name.size() > 0 && i->second->members.size() > 0)
					{
						if (it_count >= start)
						{
							guild_data.id = i->second->guild_id;
							guild_data.name = i->second->name;
							guild_data.master_name = i->second->master_name;
							//							guild_data.limit = i->second->members.size();
							//							guild_data.count = i->second->online_members.size();
							bw << guild_data;
							++send_count;
						}
						if (send_count >= count_limit)
							break;
						++it_count;
					}
				}

				bw.at<unsigned>(send_count_pos) = send_count;

				repply(std::move(bw.data));
			}
		}
		break;
		case 0x062e9eda:	//show crew stats
		{
			unsigned guild_id;
			br >> guild_id;
			auto i = guilds.find(guild_id);
			if (i != guilds.end())
			{
				guild_data_t guild_data;
				guild_data.id = guild_id;
				guild_data.name = i->second->name;
				guild_data.msg = i->second->msg;
				guild_data.rule = i->second->rule;
				guild_data.crew_point = i->second->points;
				guild_data.master_name = i->second->master_name;
				guild_data.count = i->second->members.size();
				guild_data.limit = i->second->online_members.size();

				repply(Network::packet(0x00f6aa3a, 0, guild_data));
			}
		}
		break;
		case 0x0b3c522a:	//join cafe
							/*
							{
							unsigned id;
							unsigned level;
							std::string name;
							br >> id >> level >> name;
							if (client.name == name)
							{

							}
							}
							*/
			break;
		case 0x01755eea:	//join crew
		{
			d_dump(data, length, "guild");

			unsigned guild_id;
			unsigned clientid;
			stream_array<char> name;
			br >> guild_id >> clientid >> name;
			bool found = false;
			if (client.name == name)
			{
				if (guild_id == 0)
				{
					auto k = trickPacketHandler->connections_by_id.find(clientid);
					if (k != trickPacketHandler->connections_by_id.end())
					{
						if (k->second->accepted_guild_name.size() > 0)
						{
							auto i = guilds_by_name.find(k->second->accepted_guild_name);
							if (i != guilds_by_name.end())
							{
								guild_id = i->second->guild_id;
								found = true;
							}
						}
					}
				}

				auto i = guilds.find(guild_id);
				if (i != guilds.end())
				{
					if (found)
					{
						i->second->join(client);
					}
					else
					{
						found = false;
						for (unsigned k = 0; k < i->second->aplicants.size(); ++k)
						{
							if (i->second->aplicants[k].name == client.name)
							{
								found = true;
								break;
							}
						}
						if (!found)
						{
							auto j = gamePacketHandler->connections_by_name.find(client.name);
							if (j != gamePacketHandler->connections_by_name.end())
							{
								guild_aplicant_t app;
								app.id = j->second->dbid;
								app.name = client.name;
								app.level = j->second->level;
								if (client.guild)
									app.previous_guild = client.guild->name;
								i->second->aplicants.push_back(app);
								repply(Network::packet(0x0cc1d3ea, 0, i->second->guild_id, clientid));
							}
						}
						else
						{
							repply(Network::packet(0x0cc1d3ea, 0, i->second->guild_id, clientid));
						}
					}
				}
			}
		}
		break;
		case 0x06d190da:	//cafe chat
		{

			stream_array<char> name, msg;
			br >> name >> msg;

			if (name == client.name && msg.size() > 0)
			{
				if (test_chatban(client.name, to_string(msg)))
					break;

				if (show_chat)
					logger << '[' << client.name << "]: " << msg;

				for (char ch : msg)
				{
					if (ch == ' ' || ch == 9)
						continue;
					if (ch == '/' || ch == '.')
					{
						std::string str = to_string(msg);
						do_command(client.name, str.c_str(), str.size());
						return;
					}
					break;
				}

				distribute(Network::packet(0x06d190da, client.name, msg));
			}
		}
		break;
		case 0x0cb4863a:	//crew chat
		{
			//std::string name, msg;
			stream_array<char> name, msg;
			br >> name >> msg;

			if (name == client.name && msg.size() > 0)
			{
				if (test_chatban(client.name, to_string(msg)))
					break;

				if (show_chat)
					logger << '[' << client.name << "]: " << msg;

				for (char ch : msg)
				{
					if (ch == ' ' || ch == 9)
						continue;
					if (ch == '/' || ch == '.')
					{
						std::string str = to_string(msg);
						do_command(client.name, str.c_str(), str.size());
						return;
					}
					break;
				}

				if (client.guild)
					client.guild->distribute(Network::packet(0x0cb4863a, client.name, msg));
			}
		}
		break;
		case 0x09916fda:	//login crew
		{
			unsigned clientid;
			stream_array<char> name;
			unsigned unk2;
			unsigned unk3;
			br >> clientid >> name >> unk2 >> unk3;

			if (client.name.size() == 0)
			{
				client.name = to_string(name);
				/*
				client.level = 1;
				auto i = gamePacketHandler->connections_by_name.find(client.name);
				if (i != gamePacketHandler->connections_by_name.end())
				client.level = i->second->level;
				client.room_id = 0;
				*/
			}

			auto i = gamePacketHandler->connections_by_id.find(client.clientid);
			if (i != gamePacketHandler->connections_by_id.end())
			{
				unsigned guild_id = i->second->guild_id;
				if (guild_id != (unsigned)~0)
				{
					auto j = guilds.find(guild_id);
					if (j != guilds.end())
					{
						j->second->login(client);
						if (client.guild)
						{
							i->second->guild_name = client.guild->name;
							//							repply(Network::packet(0x0c31ddea, clientid, unk2, unk3));
						}
					}
				}
			}
		}
		break;
		case 0x0e3c45ba:	//disband crew
			if (client.guild && client.guild->is_master(client))
				client.guild->disband(client);
			break;
		case 0x01a07e2a:	//leave crew
			if (client.guild && !client.guild->is_master(client))
				client.guild->leave(client);
			break;
		case 0x0980e7ea:	//ban from crew
			if (client.guild && client.guild->is_master(client))
			{
				unsigned unk1;
				std::string name;
				br >> unk1 >> name;
				client.guild->kick(client, name);
			}
			break;
		case 0x0a61653a:	//transfer master
			if (client.guild && client.guild->is_master(client))
			{
				unsigned unk1;
				std::string name;
				br >> unk1 >> name;
				client.guild->transfer(client, name);
			}
			break;
		case 0x01ff408a:	//create crew
		{
			d_dump(data, length, "guild");

			unsigned unk1;
			std::string crewname;
			unsigned unk2;
			unsigned focus;
			std::string hellomsg;
			std::string crewrule;
			br >> unk1 >> crewname >> unk2 >> focus >> hellomsg >> crewrule;

			if (client.name.size() == 0)
			{
				repply(Network::packet(0x0c45ca9a, 3, 0));
			}
			else
			{

				if (client.guild)
				{
					if (client.guild->master_name == client.name)
					{
						repply(Network::packet(0x0c45ca9a, 1, 0));
						break;
					}
					if (!client.guild->leave(client))
						break;
				}

				if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client.name))
				{
					if (player->pro < 30000)
					{
						player->message("You need 30000 pro to create a new crew");
						break;
					}
				}
				else
					break;

				unsigned guild_id = 1;
				unsigned last_guild_id = 0;
				bool name_found = false;
				auto i = guilds.begin();
				for (; i != guilds.end(); ++i)
				{
					if (i->second->name == crewname)
						name_found = true;

					//					if (i->second->guild_id != last_guild_id + 1)
					//						break;
					//					else
					//						last_guild_id = i->second->guild_id;
					last_guild_id = std::max<unsigned>(last_guild_id, i->second->guild_id);
				}
				guild_id = last_guild_id + 1;

				//				for (; i != guilds.end() && !name_found; ++i)
				//				{
				//					if (i->second->name == crewname)
				//						name_found = true;
				//				}

				if (name_found)
				{
					repply(Network::packet(0x0c45ca9a, 2, 0));
				}
				else
				{
					guild_t * guild = cnew guild_t(client, guild_id, std::move(crewname), focus, std::move(hellomsg), std::move(crewrule));
					guilds[guild_id] = guild;
					guilds_by_name[guild->name] = guild;
					client.guild = guild;
					repply(Network::packet(0x0c45ca9a, 0, guild_id));
					query("INSERT INTO guilds VALUES(" + to_query(guild->guild_id, guild->name, guild->msg, guild->rule, guild->master_name, guild->focus, 0) + ")");
					query("UPDATE characters SET guildid = " + to_query(guild->guild_id) + " WHERE name = " + to_query(guild->master_name));

					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client.name))
					{
						player->pro -= 30000;
						query("UPDATE characters SET pro = " + to_query(player->pro) + " WHERE id = " + to_query(player->dbid));
					}

				}
			}
		}
		break;
		default:
			d_dump(data, length, "guild");
		}
		return;
	}
	virtual void onDisconnect(Network::session_id_t session_id)
	{
		auto i = connections.find(session_id);
		if (i != connections.end())
		{
			{
				impl_lock_guard(lock, connection_count_mutex);
				if (!connection_count[i->second.dwip].dec())
					connection_count.erase(i->second.dwip);
			}

			i->second.invalidate();
			connections.erase(i);
		}
	}
	virtual void onInit()
	{
		if (!query<guilds_tuple>("SELECT * FROM guilds", [this](std::vector<guilds_tuple> const & data, bool success)
		{
			if (success)
			{
				for (unsigned i = 0; i < data.size(); ++i)
				{
					guild_t * guild = cnew guild_t(data[i]);
					this->guilds[guild->guild_id] = guild;
					this->guilds_by_name[guild->name] = guild;
				}

				if (!query<std::tuple<unsigned, std::string, unsigned, unsigned, unsigned, unsigned> >("SELECT id, name, level, guild_points, guild_rank, guildid FROM characters;", [this](std::vector<std::tuple<unsigned, std::string, unsigned, unsigned, unsigned, unsigned> > const & data, bool success)
				{
					if (success)
					{
						GuildPacketHandler::guild_member_t m;
						for (unsigned i = 0; i < data.size(); ++i)
						{
							auto const & t = data[i];
							if (unsigned guildid = std::get<5>(t))
							{
								m.id = std::get<0>(t);
								m.name = std::get<1>(t);
								m.level = std::get<2>(t);
								m.point = std::get<3>(t);
								m.rank = std::get<4>(t);
								auto j = this->guilds.find(guildid);
								if (j != this->guilds.end())
								{
									GuildPacketHandler::guild_t * guild = j->second;
									guild->members.push_back(std::move(m));
								}
							}
						}

						this->initialized |= 2;

					}
					else
						this->initialized = ~0;
				}))
				{
					this->initialized = ~0;
				}

				if (!query<bulletin_tuple>("SELECT * FROM bulletin", [this](std::vector<bulletin_tuple> const & data, bool success)
				{
					if (success)
					{
						for (unsigned i = 0; i < data.size(); ++i)
						{
							bulletin_tuple const & t = data[i];

							this->bulletin_seq_id = std::max<unsigned>(this->bulletin_seq_id, std::get<0>(t));

							bulletin_entry_t b(std::get<0>(t), std::get<2>(t), std::string(std::get<3>(t)));
							if (std::get<4>(t) != 0)
								b.is_deleted = true;

							unsigned guild_id = std::get<1>(t);
							auto j = guild_id ? this->guilds.find(guild_id) : this->guilds.end();
							if (j != this->guilds.end())
								j->second->bulletin.push_back(std::move(b));
							else
								this->bulletin.push_back(std::move(b));
						}
						this->initialized |= 1;
					}
					else
						this->initialized = ~0;
				}))
				{
					this->initialized = ~0;
				}
			}
			else
				this->initialized = ~0;
		}))
		{
			initialized = ~0;
		}

		//		default_color = LIGHT_BLUE;
	}


public:
	struct connection_t;
private:
	bool onLogin(connection_t & client, GamePacketHandler::connection_t * player)
	{
		if (player->name != client.name || (use_launcher && player->auth_token != client.auth_token))
		{
			client.disconnect(DisconnectKind::IMMEDIATE_DC);
			return false;
		}
		//		Network::session_id_t session_id = client.session_id;
		client.clientid = player->id;
		client.dbid = player->dbid;
		//---------

		repply(Network::packet(0x0f40d25a, 0, player->guild_id, client.clientid));
		repply(Network::packet(0x0bb5f0fa, 0, 1, "0"));

		connections_by_name[client.name] = &client;
		std::string login_msg = get_config("login_message");
		if (login_msg.size() > 0)
		{
			//repply(Network::packet(0x0d68a8aa, "", login_msg));
			repply(Network::packet(0x02bda09a, "", "", login_msg));/*6*/
		}
		client.dbid = player->dbid;
		//repply(Network::packet(0x0d68a8aa, "", "Your UDP ports: " + std::to_string(player->private_recv_port) + ", " + std::to_string(player->private_send_port) + ", " + std::to_string(player->public_recv_port) + ", " + std::to_string(player->public_send_port)));
		connections_by_dbid[client.dbid] = &client;
		connections_by_id[client.clientid] = &client;

		return true;
	}

	std::vector<std::pair<Network::session_id_t, unsigned> > login_req;
	unsigned last_client_check = get_tick();
	mutex_t timer_mutex;
	virtual void onTimer()
	{
		if (this->destroyed())
			return;
		impl_lock_guard(lock, timer_mutex);
		unsigned tick = get_tick();
		for (unsigned i = 0; i < login_req.size();)
		{
			auto j = connections.find(login_req[i].first);
			if (j == connections.end())
			{
				login_req[i] = login_req.back();
				login_req.pop_back();
				continue;
			}
			if (tick - j->second.ptime > max_client_idle_time)
			{
				connection_t * p = &j->second;
				login_req[i] = login_req.back();
				login_req.pop_back();
				p->disconnect(DisconnectKind::IMMEDIATE_DC);
				continue;
			}

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(login_req[i].second))
			{
				try
				{
					if (onLogin(j->second, player))
						if (j->second.dec_read_lock())
							this->callPostponed(j->second.session_id);
					//						enableRecv(login_req[i].first);
				}
				LOG_CATCH();
				login_req[i] = login_req.back();
				login_req.pop_back();
			}
			else
				++i;
		}
		if (tick - last_client_check > client_check_interval)
		{
			last_client_check = tick;
			distribute(Network::packet(0));
		}
		unsigned dc_all = disconnect_all;
		switch (dc_all)
		{
		case 1:
			for (auto i = connections.begin(); i != connections.end(); ++i)
				disconnect(i->second.session_id);
			if (connections.size() == 0)
				disconnect_all = 0;
			else
				disconnect_all = 2;
			break;
		case 2:
			if (connections.size() == 0)
				disconnect_all = 0;
			break;
		}

	}


public:
	unsigned bulletin_seq_id = 0;
	std::atomic<int> initialized;
	std::atomic<unsigned> disconnect_all;

	struct bulletin_entry_t
	{
		unsigned id;
		std::string name;
		std::string msg;
		bool is_deleted;
		bulletin_entry_t(unsigned id, std::string const & name, std::string && msg)
			: id(id)
			, name(name)
			, msg(std::move(msg))
			, is_deleted(false)
		{
		}
		void operator <<(BinaryWriter & bw) const
		{
			bw.write((long long)id, 0, is_deleted);
			bw << name << 0 << 0 << msg;
			//bw << (long long)id << 0 << (is_deleted ? (char)1 : (char)0) << name << 0 << "" << msg;
		}
	};

	std::vector<bulletin_entry_t> bulletin;

	struct guild_t;

	struct connection_t
		: public Network::NetworkClientBase
	{
		Network::session_id_t session_id;
		std::string ip;
		std::string auth_token;
		unsigned port;
		std::string name;
		unsigned clientid = 0;
		unsigned dbid = 0;
		unsigned level = 1;
		unsigned room_id = 0;
		unsigned dwip;
		unsigned ptime;
		guild_t * guild;

		virtual void invalidate()
		{
			if (_valid)
			{
				_valid = false;
				if (guild) try
				{
					guild->logout(*this);
				}
				LOG_CATCH();

				if (name.size() > 0)
					guildPacketHandler->connections_by_name.erase(name);
				guildPacketHandler->connections_by_dbid.erase(dbid);
				guildPacketHandler->connections_by_id.erase(clientid);
			}
		}
		void disconnect(DisconnectKind disconnectKind = DisconnectKind::SOFT_DC)
		{
			invalidate();
			guildPacketHandler->disconnect(session_id, disconnectKind);
		}
	};

	connection_t * get_client(Network::session_id_t session_id)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return nullptr;
		if (!_i->second.valid())
			return nullptr;
		return &_i->second;
	}

	struct guild_member_t
	{
		unsigned id;
		std::string name;
		unsigned level;
		unsigned point;
		unsigned rank;
	};

	struct guild_aplicant_t
	{
		unsigned id;
		std::string name;
		unsigned level;
		std::string previous_guild;
		void operator << (BinaryWriter & bw) const
		{
			bw << id << name << level << "" << 0 << previous_guild;
		}
	};

	struct guild_t
	{
		unsigned guild_id;
		unsigned focus;
		unsigned points;
		std::string name;
		std::string msg;
		std::string rule;
		std::string master_name;

		std::vector<bulletin_entry_t> bulletin;
		std::vector<guild_member_t> members;
		std::vector<Network::session_id_t> online_members;
		std::vector<unsigned> online_members_by_clientid;
		std::vector<guild_aplicant_t> aplicants;

		bool is_master(connection_t & client) const
		{
			return client.name == master_name;
		}

		bool is_member(unsigned dbid)
		{
			if (master_name.size() == 0)
				return false;
			for (unsigned i = 0; i < members.size(); ++i)
			{
				if (members[i].id == dbid)
					return true;
			}
			return false;
		}

		bool join(connection_t & client)
		{
			if (master_name.size() == 0)
				return false;

			if (client.guild)
				if (!client.guild->leave(client))
				{
					guildPacketHandler->sendTo(client.session_id, Network::packet(0x0cc1d3ea, 1, 0, 0));
					return false;
				}

			std::string const & name = client.name;

			auto k = gamePacketHandler->connections_by_name.find(name);
			if (k != gamePacketHandler->connections_by_name.end())
			{
				auto j = guildPacketHandler->connections_by_name.find(name);
				if (j != guildPacketHandler->connections_by_name.end())
				{
					guildPacketHandler->sendTo(j->second->session_id, Network::packet(0x0f40d25a, 0, guild_id, k->second->id));

					guild_member_t member;
					member.id = k->second->dbid;
					member.name = name;
					member.level = k->second->level;
					member.point = 0;
					member.rank = 0;
					members.push_back(member);
					online_members.push_back(j->second->session_id);
					online_members_by_clientid.push_back(j->second->clientid);

					distribute(Network::packet(0x02bda09a, "", "", name + " has joined the crew"), j->second->session_id);
					guildPacketHandler->sendTo(j->second->session_id, Network::packet(0x0f8d93da, name, guild_id));

				}
				k->second->guild_id = guild_id;
				k->second->guild_name = name;
				k->second->guild_rank = 1;
				k->second->guild_points = 0;

				send_message(name, 11, "", "", this->name, "");

				query("UPDATE characters SET guildid = " + to_query(guild_id) + ", guild_points = 0, guild_rank = 0 WHERE name = " + to_query(name));

				for (unsigned i = 0; i < aplicants.size(); ++i)
				{
					if (aplicants[i].name == name)
					{
						aplicants[i] = aplicants.back();
						aplicants.pop_back();
						break;
					}
				}

				{
					auto k2 = trickPacketHandler->connections_by_id.find(k->second->id);
					if (k2 != trickPacketHandler->connections_by_id.end())
						k2->second->accepted_guild_name.clear();
				}

				return true;
			}

			return false;
		}

		void send_applicant_list(connection_t & client2)
		{
			BinaryWriter bw;
			bw << Network::packet_header_t();
			bw << 0x0354e48a;
			bw << (unsigned)this->aplicants.size();
			for (unsigned i = 0, e = this->aplicants.size(); i < e; ++i)
			{
				auto const & app = this->aplicants[i];
				unsigned level = app.level;
				auto j = gamePacketHandler->connections_by_name.find(app.name);
				if (j != gamePacketHandler->connections_by_name.end())
				{
					bw << j->second->id;
					level = j->second->level;
				}
				else
					bw << app.id;
				bw << app.name << level << "" << 0 << app.previous_guild;

			}

			//guildPacketHandler->sendTo(client2.session_id, Network::packet(0x0354e48a, this->aplicants));
			guildPacketHandler->sendTo(client2.session_id, std::move(bw.data));
		}

		void accept(connection_t & acceptor, stream_array<char> const & _name)
		{
			if (master_name.size() == 0)
				return;
			for (unsigned i = 0; i < aplicants.size(); ++i)
			{
				if (aplicants[i].name == _name)
				{
					std::string name = std::move(aplicants[i].name);
					unsigned level = aplicants[i].level;
					unsigned id = aplicants[i].id;

					aplicants[i] = aplicants.back();
					aplicants.pop_back();

					//auto k = gamePacketHandler->connections_by_name.find(name);

					//auto j = guildPacketHandler->connections_by_name.find(name);
					//if (j == guildPacketHandler->connections_by_name.end())
					{
						/*
						for (auto i = guildPacketHandler->guilds.begin(); i != guildPacketHandler->guilds.end(); ++i)
						{
						if (i->second->name == name)
						{
						return;
						}
						}
						*/

						Network::session_id_t acceptor_session_id = acceptor.session_id;
						unsigned guild_id = this->guild_id;
						query<std::tuple<> >("UPDATE characters SET guildid = " + to_query(this->guild_id) + ", guild_points = 0, guild_rank = 1 WHERE name = " + to_query(name) + " AND (SELECT count(*) FROM guilds WHERE guilds.id = guildid AND master_name = characters.name) < 1", [acceptor_session_id, id, name, level, guild_id](std::vector<std::tuple<> > const & data, bool success)
						{
							if (success && affected_rows > 0)
							{
								auto i = guildPacketHandler->guilds.find(guild_id);
								if (i != guildPacketHandler->guilds.end())
								{
									GuildPacketHandler::guild_t * guild = i->second;
									GuildPacketHandler::guild_member_t member;
									member.id = id;
									member.name = name;
									member.level = level;
									auto k = gamePacketHandler->connections_by_name.find(name);
									if (k != gamePacketHandler->connections_by_name.end())
										member.level = k->second->level;
									member.point = 0;
									member.rank = 0;
									guild->members.push_back(member);
									Network::session_id_t accepted_player_session_id{ 0, 0 };
									auto j = guildPacketHandler->connections_by_name.find(name);
									if (j != guildPacketHandler->connections_by_name.end())
									{
										accepted_player_session_id = j->second->session_id;
										guild->online_members.push_back(j->second->session_id);
										guild->online_members_by_clientid.push_back(j->second->clientid);
										guildPacketHandler->sendTo(j->second->session_id, Network::packet(0x0f8d93da, name, guild_id));
									}
									guild->distribute(Network::packet(0x02bda09a, "", "", name + " has joined the crew"), accepted_player_session_id);

									if (k != gamePacketHandler->connections_by_name.end())
									{
										guildPacketHandler->sendTo(j->second->session_id, Network::packet(0x0f40d25a, 0, guild_id, k->second->id));
										k->second->guild_id = guild->guild_id;
										k->second->guild_name = guild->name;
										k->second->guild_rank = 1;
										k->second->guild_points = 0;

										{
											auto k2 = trickPacketHandler->connections_by_id.find(k->second->id);
											if (k2 != trickPacketHandler->connections_by_id.end())
												k2->second->accepted_guild_name.clear();
										}
									}
									send_message(name, 11, "", "", guild->name, "");


									if (GuildPacketHandler::connection_t * client = guildPacketHandler->get_client(acceptor_session_id))
										guild->send_applicant_list(*client);
								}
							}
							else
							{
								if (GuildPacketHandler::connection_t * client = guildPacketHandler->get_client(acceptor_session_id))
								{
									guildPacketHandler->sendTo(acceptor_session_id, Network::packet(0x0c20c79a, 1));
									if (client->guild)
										client->guild->send_applicant_list(*client);
								}
							}
						});
					}

					break;
				}
			}
		}

		void deny(connection_t & client, stream_array<char> const & _name)
		{
			if (master_name.size() == 0)
				return;
			for (unsigned i = 0; i < aplicants.size(); ++i)
			{
				if (aplicants[i].name == _name)
				{
					std::string name = std::move(aplicants[i].name);

					unsigned id = aplicants[i].id;
					aplicants[i] = aplicants.back();
					aplicants.pop_back();

					guildPacketHandler->sendTo(client.session_id, Network::packet(0x0ab84daa, id));

					send_message(name, 12, "", "", this->name, "");
					send_applicant_list(client);
					break;
				}
			}
		}

		void transfer(connection_t & master, std::string const & name)
		{
			if (master_name == name || master_name.size() == 0)
				return;

			if (master.name != master_name)
			{
				guildPacketHandler->sendTo(master.session_id, Network::packet(0x0608acda, 1));
				return;
			}

			for (unsigned i = 0; i < members.size(); ++i)
			{
				if (members[i].name == name)
				{
					std::string old_master_name = std::move(master_name);
					master_name = name;
					query("UPDATE guilds SET master_name = " + to_query(name) + " WHERE id = " + to_query(guild_id));

					distribute(Network::packet(0x0e5b4aba, old_master_name, master_name));
					distribute(Network::packet(0x02bda09a, "", "", old_master_name + " has appointed " + master_name + " as crew master"));
					guildPacketHandler->sendTo(master.session_id, Network::packet(0x0608acda, 0));
					return;
				}
			}

			guildPacketHandler->sendTo(master.session_id, Network::packet(0x0608acda, 1));
		}


		void kick(connection_t & master, std::string const & name)
		{
			if (master.name != master_name)
				return;
			if (master_name == name)
			{
				guildPacketHandler->sendTo(master.session_id, Network::packet(0x0ebee5fa, 2));
				return;
			}

			bool found = false;
			unsigned old_member_pos = ~0;
			for (unsigned i = 0; i < members.size(); ++i)
			{
				if (members[i].name == name)
				{
					old_member_pos = i;
					members[i] = members.back();
					members.pop_back();
					found = true;
					break;
				}
			}

			if (!found)
			{
				guildPacketHandler->sendTo(master.session_id, Network::packet(0x053edf8a, 1));
				return;
			}

			{
				auto j = guildPacketHandler->connections_by_name.find(name);
				if (j != guildPacketHandler->connections_by_name.end())
				{
					for (unsigned i = 0; i < online_members.size(); ++i)
					{
						if (online_members[i] == j->second->session_id)
						{
							online_members[i] = online_members.back();
							online_members.pop_back();
							online_members_by_clientid[i] = online_members_by_clientid.back();
							online_members_by_clientid.pop_back();
							break;
						}
					}

					guildPacketHandler->sendTo(j->second->session_id, Network::packet(0x01e2bcda, name));
					j->second->guild = nullptr;
				}
			}
			{
				auto j = gamePacketHandler->connections_by_name.find(name);
				if (j != gamePacketHandler->connections_by_name.end())
				{
					j->second->guild_id = 0;
					j->second->guild_name.clear();
				}

				send_message(name, 13, "", "", this->name, "");
			}

			distribute(Network::packet(0x01e2bcda, name));

			query("UPDATE characters SET guildid = 0 WHERE name = " + to_query(name));


			//unsigned start, startpage, count_limit = 8;
			unsigned count_limit = 8;
			//br >> startpage;
			unsigned startpage = (old_member_pos / count_limit * count_limit) + 1;
			unsigned start = (startpage - 1) * count_limit;


			{
				BinaryWriter bw;
				bw << Network::packet_header_t();
				bw << 0x0757e78a;
				//				bw << client.guild->guild_id;

				unsigned send_count_pos = bw.allocate_pos<unsigned>();
				unsigned send_count = 0;
//				unsigned it_count = 0;
				//for (auto j = client.guild->members.begin(); j != client.guild->members.end(); ++j)
				for (unsigned j = start, je = this->members.size(); j < je && send_count < count_limit; ++j, ++send_count)
				{
					auto const & member = this->members[j];
					unsigned id = 0;
					auto k = gamePacketHandler->connections_by_dbid.find(member.id);
					if (k != gamePacketHandler->connections_by_dbid.end())
						id = k->second->id;
					else
						id = member.id;
					bw << id;
					bw << member.name;
					bw << member.level;
					bw << tv[0];
					bw << tv[1];
					bw << member.point;
					bw << member.rank;
				}

				bw.at<unsigned>(send_count_pos) = send_count;

				guildPacketHandler->sendTo(master.session_id, std::move(bw.data));
			}
		}

		bool leave(connection_t & client)
		{
			if (master_name.size() == 0)
				return false;
			if (client.name == master_name)
			{
				guildPacketHandler->sendTo(client.session_id, Network::packet(0x0ebee5fa, 2));
				return false;
			}

			bool found = false;
			for (unsigned i = 0; i < members.size(); ++i)
			{
				if (members[i].name == client.name)
				{
					members[i] = members.back();
					members.pop_back();
					found = true;
					break;
				}
			}

			if (!found)
				return false;

			for (unsigned i = 0; i < online_members.size(); ++i)
			{
				if (online_members[i] == client.session_id)
				{
					online_members[i] = online_members.back();
					online_members.pop_back();
					online_members_by_clientid[i] = online_members_by_clientid.back();
					online_members_by_clientid.pop_back();
					break;
				}
			}

			auto j = gamePacketHandler->connections_by_name.find(client.name);
			if (j != gamePacketHandler->connections_by_name.end())
			{
				j->second->guild_id = 0;
				j->second->guild_name.clear();
			}

			send_message(client.name, 13, "", "", this->name, "");
			client.guild = nullptr;

			distribute(Network::packet(0x01e2bcda, client.name));
			guildPacketHandler->sendTo(client.session_id, Network::packet(0x01e2bcda, client.name));

			query("UPDATE characters SET guildid = 0 WHERE name = " + to_query(client.name));
			return true;
		}

		void logout(connection_t & client)
		{
			for (unsigned i = 0; i < online_members.size(); ++i)
			{
				if (online_members[i] == client.session_id)
				{
					online_members[i] = online_members.back();
					online_members.pop_back();
					online_members_by_clientid[i] = online_members_by_clientid.back();
					online_members_by_clientid.pop_back();
					break;
				}
			}
		}


		void login(connection_t & client)
		{
			if (master_name.size() == 0)
				return;
			unsigned i = 0;
			for (; i < online_members.size(); ++i)
				if (online_members[i] == client.session_id)
					break;
			if (i == online_members.size())
			{
				online_members.push_back(client.session_id);
				online_members_by_clientid.push_back(client.clientid);
			}

			client.guild = this;
		}

		guild_t(guilds_tuple const & t)
			: guild_id(std::get<0>(t))
			, focus(std::get<5>(t))
			, points(std::get<6>(t))
			, name(std::get<1>(t))
			, msg(std::get<2>(t))
			, rule(std::get<3>(t))
			, master_name(std::get<4>(t))
		{
		}

		guild_t(connection_t & client, unsigned id, std::string && crewname, unsigned focus, std::string && hellomsg, std::string && rule)
			: guild_id(id)
			, focus(focus)
			, points(0)
			, name(std::move(crewname))
			, msg(std::move(hellomsg))
			, rule(std::move(rule))
			, master_name(client.name)
		{
			guild_member_t member;
			member.id = 0;
			{
				auto i = gamePacketHandler->connections_by_name.find(client.name);
				if (i != gamePacketHandler->connections_by_name.end())
				{
					member.id = i->second->dbid;
				}
			}
			member.name = client.name;
			member.level = client.level;
			member.point = 0;
			member.rank = 0;
			members.push_back(member);
			online_members.emplace_back(client.session_id);
			online_members_by_clientid.push_back(client.clientid);
			auto i = gamePacketHandler->connections_by_name.find(client.name);
			if (i != gamePacketHandler->connections_by_name.end())
			{
				i->second->guild_id = guild_id;
				i->second->guild_name = this->name;
				i->second->guild_points = 0;
				i->second->guild_rank = 1;
				query("UPDATE characters SET guildid = " + to_query(guild_id) + ", guild_points = 0, guild_rank = 1 WHERE id = " + to_query(i->second->dbid));
			}

			{
				std::string msg = master_name + " has established the " + name + " crew";
				guildPacketHandler->distribute(Network::packet(0x02bda09a, "", "", msg));
			}
		}

		void disband(connection_t & client)
		{
			if (client.name == master_name)
			{
				for (unsigned i = 0; i < members.size(); ++i)
				{
					send_message(members[i].name, 14, "", "", this->name, "");
				}

				distribute(Network::packet(0x053edf8a, 0));
				for (unsigned i = 0; i < online_members.size(); ++i)
				{
					{
						auto j = guildPacketHandler->connections.find(online_members[i]);
						if (j != guildPacketHandler->connections.end())
						{
							j->second.guild = nullptr;
							auto k = gamePacketHandler->connections_by_id.find(online_members_by_clientid[i]);
							if (k != gamePacketHandler->connections_by_id.end())
							{
								k->second->guild_id = 0;
								k->second->guild_name.clear();
							}
						}
					}

				}


				//				guildPacketHandler->guilds.erase(guild_id);
				query("UPDATE characters SET guildid = 0 WHERE guildid = " + to_query(guild_id));
				query("UPDATE guilds SET master_name = '' WHERE id = " + to_query(guild_id));

				{
					std::string msg = master_name + " has disbanded the " + name + " crew";
					guildPacketHandler->distribute(Network::packet(0x02bda09a, "", "", msg));
				}

				master_name.clear();
				members.clear();
				this->online_members.clear();
				this->online_members_by_clientid.clear();
				this->aplicants.clear();
				this->bulletin.clear();
				//				this->msg.clear();
				//				this->focus.clear();
				//				this->

				//				delete this;
			}
			else
			{
				guildPacketHandler->sendTo(client.session_id, Network::packet(0x053edf8a, 2));
			}
		}

		inline bool distribute(std::vector<char> && data, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			return guildPacketHandler->sendTo(online_members, std::move(data), skipId);
		}
		inline bool distribute(BinaryWriter && bw, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			return guildPacketHandler->sendTo(online_members, std::move(bw.data), skipId);
		}

		inline bool distribute2(std::vector<char> && data, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			std::vector<Network::session_id_t> targets;
			targets.reserve(online_members.size());
			for (Network::session_id_t id : online_members)
			{
				if (id != skipId && guildPacketHandler->connections[id].room_id == 0)
					targets.push_back(id);
			}
			if (targets.size() > 0)
				return guildPacketHandler->sendTo(targets, std::move(data), skipId);
			else
				return true;
		}
		inline bool distribute2(BinaryWriter && bw, Network::session_id_t skipId = Network::session_id_t{ 0, 0 })
		{
			std::vector<Network::session_id_t> targets;
			targets.reserve(online_members.size());
			for (Network::session_id_t id : online_members)
			{
				if (id != skipId && guildPacketHandler->connections[id].room_id == 0)
					targets.push_back(id);
			}
			if (targets.size() > 0)
				return guildPacketHandler->sendTo(targets, std::move(bw.data), skipId);
			else
				return true;
		}
	};


	std::map<unsigned, guild_t*> guilds;
	std::unordered_map<std::string, guild_t*> guilds_by_name;

	std::map<Network::session_id_t, connection_t> connections;
	std::unordered_map<std::string, connection_t*> connections_by_name;
	std::map<unsigned, connection_t*> connections_by_dbid;
	std::map<unsigned, connection_t*> connections_by_id;

	GuildPacketHandler()
		: initialized(0)
		, disconnect_all(0)
	{}
	virtual ~GuildPacketHandler()
	{
		impl_lock_guard(lock, timer_mutex);
		this->_destroyed = true;

		for (auto i = guilds.begin(); i != guilds.end(); ++i)
			delete i->second;
		guilds.clear();
	}
};

std::string getGuildName(unsigned guild_id)
{
	if (guild_id != 0)
	{
		auto i = guildPacketHandler->guilds.find(guild_id);
		if (i != guildPacketHandler->guilds.end())
			return i->second->name;
	}
	return std::string();
}

void set_level_in_guild(unsigned guild_id, std::string const & name, unsigned level)
{
	if (guild_id != 0)
	{
		auto i = guildPacketHandler->guilds.find(guild_id);
		if (i != guildPacketHandler->guilds.end())
		{
			for (unsigned j = 0, e = i->second->members.size(); j < e; ++j)
			{
				if (i->second->members[j].name == name)
				{
					i->second->members[j].level = level;
					break;
				}
			}
		}
	}
}


class MessengerPacketHandler
	: public Network::NetworkEventHandler
{
	virtual std::string getConnectionData(Network::session_id_t id) const
	{
		auto i = connections.find(id);
		if (i != connections.end())
			return i->second.name;
		return std::string();
	}
	virtual DisconnectKind onConnect(Network::session_id_t session_id, std::string && ip, unsigned port)
	{
		unsigned dwip = Network::string2ipv4(ip);
		{
			impl_lock_guard(lock, connection_count_mutex);
			if (!connection_count[dwip].inc())
				return DisconnectKind::IMMEDIATE_DC;
		}

		connection_t & client = connections[session_id];
		client.session_id = session_id;
		client.ip = std::move(ip);
		client.port = port;
		client.dwip = dwip;
		client.ptime = get_tick();
		return DisconnectKind::ACCEPT;
	}
	virtual void onRecieve(Network::session_id_t session_id, const char * data, unsigned length)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return;
		connection_t & client = _i->second;
		client.check_valid();
		client.ptime = get_tick();

		BinaryReader br(data, length);
		br.skip(4);
		unsigned packetid = br.read<unsigned>();

		if (log_inpacket_ids)
			logger.print("messenger inpacket %08x\r\n", packetid);

		if (log_inpackets)
			Network::dump(data, length, "messenger");

		//#ifdef _DEBUG
		//		logger.print("messenger packet %08x\r\n", packetid);
		//#endif

		if (client.clientid == 0 && packetid != 0x0217fc2a)
		{
			client.disconnect(DisconnectKind::IMMEDIATE_DC);
			return;
		}

		switch (packetid)
		{
		case 0x0217fc2a:	//1
		{
			if (client.clientid != 0)
				break;
			d_dump(data, length, "messenger");
			unsigned unk1, clientid, unk2, dbid;
			std::array<char, 21> name, charName;
			name[20] = 0;

			if (use_launcher)
				br >> client.auth_token;

			br >> unk1 >> clientid >> name >> charName;
			br >> unk2;	//0x0d
			br.skip(2);
			br.skip(8 * 16 + 12);
			br >> dbid;

			client.name = &name[0];
			client.clientid = clientid;
			client.dbid = dbid;

			/*8*/

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(clientid))
			{
				onLogin(client, player);
				return;
			}
			else
			{
				login_req.emplace_back(session_id, clientid);
				client.inc_read_lock();
				return;
			}


		}
		break;

		case 0x0183e3ba:	//add friend request
		{
			std::array<char, 21> name;
			br >> name;
			name[20] = 0;

			auto i = connections_by_name.find(&name[0]);
			if (i != connections_by_name.end())
			{
				for (unsigned k = 0; k < client.friends.size(); ++k)
					if (client.friends[k] == i->second->dbid)
						return;

				auto j = gamePacketHandler->connections_by_id.find(client.clientid);
				if (j != gamePacketHandler->connections_by_id.end())
				{
					character_packet_data_t r = GamePacketHandler::conv(*j->second);
					r.clientip = r.dbid;
					sendTo(i->second->session_id, Network::packet(0x0f3be82a, r));
				}
			}
			else
			{
				repply(Network::packet(0x0667e34a, 0, (char)0));
			}
		}
		break;
		case 0x0667e34a:	//deny friend request
		{
			unsigned requester_id;
			unsigned char unk;	//0x44
								//br >> requester_id >> unk;
			br.read(requester_id, unk);

			auto i = connections_by_dbid.find(requester_id);
			if (i != connections_by_dbid.end())
			{
				auto j = gamePacketHandler->connections_by_id.find(client.clientid);
				if (j != gamePacketHandler->connections_by_id.end())
				{
					character_packet_data_t r = GamePacketHandler::conv(*j->second);
					r.clientip = r.dbid;
					sendTo(i->second->session_id, Network::packet(0x01bf8dda, r));
				}
			}

		}
		break;
		case 0x0818c63a:	//friend request accept
		{
			unsigned requester_id;
			br >> requester_id;

			auto i = connections_by_dbid.find(requester_id);
			if (i != connections_by_dbid.end())
			{
				for (unsigned k = 0; k < client.friends.size(); ++k)
					if (client.friends[k] == i->second->dbid)
						return;

				for (unsigned k = 0; k < i->second->friends.size(); ++k)
					if (i->second->friends[k] == client.dbid)
						return;

				auto j = gamePacketHandler->connections_by_id.find(client.clientid);
				if (j != gamePacketHandler->connections_by_id.end())
				{
					character_packet_data_t r = GamePacketHandler::conv(*j->second);
					r.clientip = r.dbid;
					sendTo(i->second->session_id, Network::packet(0x03c4146a, r));
					sendTo(i->second->session_id, Network::packet(0x0d2f17aa, r));
				}

				j = gamePacketHandler->connections_by_id.find(i->second->clientid);
				if (j != gamePacketHandler->connections_by_id.end())
				{
					character_packet_data_t r = GamePacketHandler::conv(*j->second);
					r.clientip = r.dbid;
					repply(Network::packet(0x03c4146a, r));
					repply(Network::packet(0x0d2f17aa, r));
				}


				i->second->friends.push_back(client.dbid);
				i->second->online_friends.push_back(client.session_id);

				client.friends.push_back(i->second->dbid);
				client.online_friends.push_back(i->second->session_id);

				query("INSERT INTO friends VALUES(" + to_query(client.dbid, i->second->dbid) + "),(" + to_query(i->second->dbid, client.dbid) + ")");
			}
			else
			{
				repply(Network::packet(0x0667e34a, 0, (char)0));
			}
		}
		break;
		case 0x0ab0e3da:	//delete friend
		{
			unsigned friend_id;
			br >> friend_id;

			auto i = connections_by_dbid.find(friend_id);
			if (i != connections_by_dbid.end())
			{
				sendTo(i->second->session_id, Network::packet(0x01c0bada, client.dbid));
				erase(i->second->friends, client.dbid);
				erase(i->second->online_friends, client.session_id);
				erase(client.online_friends, i->second->session_id);
			}
			erase(client.friends, friend_id);
			repply(Network::packet(0x01c0bada, friend_id));
			query("delete from friends where id1 = " + to_query(client.dbid) + " and id2 = " + to_query(friend_id));
			query("delete from friends where id1 = " + to_query(friend_id) + " and id2 = " + to_query(client.dbid));
		}
		break;
		case 0x0e137aea:	//friend chat
		{
			unsigned recipient;
			fixed_stream_array<char, 141> text;
			br >> recipient >> text;

			auto j = connections_by_dbid.find(recipient);
			if (j != connections_by_dbid.end())
			{
				log_chat(client.name, j->second->name, to_string(text));
				sendTo(j->second->session_id, Network::packet(0x0f41c6ba, client.dbid, text));
			}
			else
				repply(Network::packet(0x0f41c6ba, recipient, recipient_offline_text));
		}
		break;
		case 0x0e213c7a:	//invite match
		{
			unsigned recipient;
			br >> recipient;
			auto j = connections_by_dbid.find(recipient);
			if (j != connections_by_dbid.end())
			{
				if (client.ptime - j->second->last_invite_time > 20000)
				{
					if (client.state == connection_t::ROOM && j->second->state == connection_t::ROOM && client.room_id == j->second->room_id)
					{
						message(client.clientid, "Player is already in the same room");
					}
					else
					{
						j->second->last_invite_time = client.ptime;
						sendTo(j->second->session_id, Network::packet(0x0863d74a, client.dbid));
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_dbid(recipient))
						{
							player->room_invite_id = client.room_id - room_id_disp;
							player->room_invite_time = client.ptime;
						}
					}
				}
				else
				{
					message(client.clientid, "Please invite later");
				}
			}
		}
		break;
		case 0x0d3985ba:	//enter room
		{
			unsigned char b_is_single;
			unsigned char room_kind;	//0 = race, 1 = battle, 2 = coin
			unsigned char player_limit;
			unsigned char b_password;
			std::array<char, 13> password;
			unsigned unk1;
			unsigned unk2;
			unsigned unk3;
			unsigned room_id;
			unsigned char unk4;
			unsigned mapid;
			unsigned char unk5;
			unsigned unk6;
			unsigned long long unk7;
			unsigned unk8;

			br >> b_is_single;
			br >> room_kind;
			br >> player_limit;
			br >> b_password;
			br >> password;
			br >> unk1;
			br >> unk2;
			br >> unk3;
			br >> room_id;
			br >> unk4;
			br >> mapid;
			br >> unk5;
			br >> unk6;
			br >> unk7;
			br >> unk8;

			if (client.online_friends.size() && (room_id - room_id_disp < gamePacketHandler->rooms.size()))
			{
				client.room_id = room_id;
				client.state = connection_t::ROOM;

				GamePacketHandler::room_t * gameroom = &gamePacketHandler->rooms[room_id - room_id_disp];
				if (gameroom->password.size() > 0)
				{
					b_password = 1;
					strncpy(&password[0], gameroom->password.c_str(), 12);
					password[12] = 0;
				}
				sendTo(client.online_friends, Network::packet(0x0e71918a, client.dbid, b_is_single, room_kind, player_limit, b_password, password, unk1, unk2, unk3, room_id, unk4, mapid, unk5, unk6, unk7, unk8));
			}
		}
		break;
		case 0x063fe0fa:
			client.state = connection_t::GAME;
			if (client.online_friends.size())
				sendTo(client.online_friends, Network::packet(0x0f5095ea, client.dbid));
			break;
		case 0x0afac0ea:	//return to lobby
			if (client.state != connection_t::IDLE)
			{
				client.state = connection_t::IDLE;
				if (client.online_friends.size())
					sendTo(client.online_friends, Network::packet(0x0743a6ea, client.dbid));
			}
			break;
		default:
			d_dump(data, length, "msn");
		}
		return;
	}
	virtual void onDisconnect(Network::session_id_t session_id)
	{
		auto i = connections.find(session_id);
		if (i != connections.end())
		{
			{
				impl_lock_guard(lock, connection_count_mutex);
				if (!connection_count[i->second.dwip].dec())
					connection_count.erase(i->second.dwip);
			}

			i->second.invalidate();
			connections.erase(i);
		}
	}
	virtual void onInit()
	{
		//		default_color = CYAN;
	}

public:
	struct connection_t;
private:
	bool onLogin(connection_t & client, GamePacketHandler::connection_t * player)
	{
		if (use_launcher && player->auth_token != client.auth_token)
		{
			client.disconnect();
			return false;
		}
		Network::session_id_t session_id = client.session_id;
		client.clientid = player->id;
		client.dbid = player->dbid;
		connections_by_id[client.clientid] = &client;

		query<std::tuple<unsigned, std::string, unsigned> >("SELECT friends.id2, characters.name, characters.level FROM friends, characters WHERE friends.id1 = " + to_query(client.dbid) + " AND characters.id = friends.id2", [session_id](std::vector<std::tuple<unsigned, std::string, unsigned> > const & data, bool success)
		{
			if (MessengerPacketHandler::connection_t * client = messengerPacketHandler->get_client(session_id))
			{
				if (success)
				{
					auto j2 = gamePacketHandler->connections_by_id.find(client->clientid);
					if (j2 == gamePacketHandler->connections_by_id.end())
						return;

					client->friends.reserve(data.size());

					character_packet_data_t r_this = GamePacketHandler::conv(*j2->second);
					r_this.clientip = client->dbid;

					for (unsigned i = 0; i < data.size(); ++i)
					{
						unsigned f = std::get<0>(data[i]);
						client->friends.push_back(f);

						character_packet_data_t r;


						auto j = gamePacketHandler->connections_by_dbid.find(f);
						if (j != gamePacketHandler->connections_by_dbid.end())
						{
							r = GamePacketHandler::conv(*j->second);
							r.clientip = r.dbid;

							messengerPacketHandler->sendTo(session_id, Network::packet(0x03c4146a, r));

							auto k = messengerPacketHandler->connections_by_dbid.find(f);
							if (k != messengerPacketHandler->connections_by_dbid.end())
							{
								messengerPacketHandler->sendTo(session_id, Network::packet(0x0d2f17aa, r));
								messengerPacketHandler->sendTo(k->second->session_id, Network::packet(0x0d2f17aa, r_this));
								k->second->online_friends.push_back(session_id);
								client->online_friends.push_back(k->second->session_id);
								switch (k->second->state)
								{
								case connection_t::ROOM:
									//sendTo(session_id, Network::packet(0x0e71918a, k->second->dbid, b_is_single, room_kind, player_limit, b_password, password, unk1, unk2, unk3, room_id, unk4, mapid, unk5, unk6, unk7, unk8));
									if (j->second->room)
										messengerPacketHandler->sendTo(session_id, Network::packet(0x0e71918a, j->second->dbid, j->second->room->get_data()));
									break;
								case connection_t::GAME:
									messengerPacketHandler->sendTo(session_id, Network::packet(0x0f5095ea, k->second->dbid));
									break;
								case connection_t::IDLE:
								case connection_t::OFFLINE:
									break;
								}

								//gamePacketHandler->sendTo(j->second->session_id, Network::packet(0x0bffbd2a, 0, j2->second->name + " has logged in", 0));
								auto k2 = guildPacketHandler->connections_by_dbid.find(f);
								if (k2 != guildPacketHandler->connections_by_dbid.end())
									guildPacketHandler->sendTo(k2->second->session_id, Network::packet(0x0d68a8aa, 0, j2->second->name + " has logged in"));

							}
						}
						else
						{
							r.dbid = f;
							r.clientip = r.dbid;
							r.name = std::get<1>(data[i]);
							r.level = std::get<2>(data[i]);
							messengerPacketHandler->sendTo(session_id, Network::packet(0x03c4146a, r));
						}

					}
				}
				else
				{
					logger << "Failed to load friendlist";
				}
			}
		});

		connections_by_name[client.name] = &client;
		connections_by_id[client.clientid] = &client;
		connections_by_dbid[client.dbid] = &client;

		return true;
	}

	std::vector<std::pair<Network::session_id_t, unsigned> > login_req;
	unsigned last_client_check = get_tick();
	virtual void onTimer()
	{
		unsigned tick = get_tick();
		for (unsigned i = 0; i < login_req.size();)
		{
			auto j = connections.find(login_req[i].first);
			if (j == connections.end())
			{
				login_req[i] = login_req.back();
				login_req.pop_back();
				continue;
			}
			if (tick - j->second.ptime > max_client_idle_time)
			{
				connection_t * p = &j->second;
				login_req[i] = login_req.back();
				login_req.pop_back();
				p->disconnect(DisconnectKind::IMMEDIATE_DC);
				continue;
			}

			if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(login_req[i].second))
			{
				try
				{
					if (onLogin(j->second, player))
						if (j->second.dec_read_lock())
							this->callPostponed(j->second.session_id);
					//						enableRecv(login_req[i].first);
				}
				LOG_CATCH();
				login_req[i] = login_req.back();
				login_req.pop_back();
			}
			else
				++i;
		}

		if (tick - last_client_check > client_check_interval)
		{
			last_client_check = tick;
			distribute(Network::packet(0));
		}

		unsigned dc_all = disconnect_all;
		switch (dc_all)
		{
		case 1:
			for (auto i = connections.begin(); i != connections.end(); ++i)
				disconnect(i->second.session_id);
			if (connections.size() == 0)
				disconnect_all = 0;
			else
				disconnect_all = 2;
			break;
		case 2:
			if (connections.size() == 0)
				disconnect_all = 0;
			break;
		}

	}


public:

	std::atomic<unsigned> disconnect_all;

	struct connection_t
		: public Network::NetworkClientBase
	{
		enum state_t { OFFLINE, IDLE, ROOM, GAME } state = IDLE;
		Network::session_id_t session_id;
		std::string ip;
		std::string auth_token;
		unsigned port;
		unsigned dwip;
		unsigned clientid = 0;
		unsigned dbid;
		unsigned ptime;
		unsigned last_invite_time = 0;
		unsigned room_id = 0;
		std::string name;
		std::vector<unsigned> friends;
		std::vector<Network::session_id_t> online_friends;

		virtual void invalidate()
		{
			if (_valid)
			{
				_valid = false;
				if (online_friends.size()) try
				{
					messengerPacketHandler->sendTo(online_friends, Network::packet(0x095c769a, dbid));
					for (Network::session_id_t id : online_friends)
					{
						auto j = messengerPacketHandler->connections.find(id);
						if (j != messengerPacketHandler->connections.end())
							erase(j->second.online_friends, session_id);
					}
				}
				LOG_CATCH();

				messengerPacketHandler->connections_by_id.erase(clientid);
				messengerPacketHandler->connections_by_dbid.erase(dbid);
				messengerPacketHandler->connections_by_name.erase(name);
			}
		}
		void disconnect(DisconnectKind disconnectKind = DisconnectKind::SOFT_DC)
		{
			invalidate();
			messengerPacketHandler->disconnect(session_id, disconnectKind);
		}
	};

	connection_t * get_client(Network::session_id_t session_id)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return nullptr;
		if (!_i->second.valid())
			return nullptr;
		return &_i->second;
	}

	std::map<Network::session_id_t, connection_t> connections;
	std::map<unsigned, connection_t*> connections_by_id;
	std::map<unsigned, connection_t*> connections_by_dbid;
	std::unordered_map<std::string, connection_t*> connections_by_name;

	MessengerPacketHandler()
        : disconnect_all(0)
    {}

	virtual ~MessengerPacketHandler() {}
};

class RoomPacketHandler
	: public Network::NetworkEventHandler
{
	virtual DisconnectKind onConnect(Network::session_id_t session_id, std::string && ip, unsigned port)
	{
		unsigned dwip = Network::string2ipv4(ip);
		{
			impl_lock_guard(lock, connection_count_mutex);
			if (!connection_count[dwip].inc())
				return DisconnectKind::IMMEDIATE_DC;
		}

		connection_t & client = connections[session_id];
		client.session_id = session_id;
		client.ip = std::move(ip);
		client.port = port;
		client.dwip = dwip;
		client.ptime = get_tick();
		return DisconnectKind::ACCEPT;
	}
	virtual void onRecieve(Network::session_id_t session_id, const char * data, unsigned length)
	{
		auto _i = connections.find(session_id);
		if (_i == connections.end())
			return;
		connection_t & client = _i->second;
		client.check_valid();
		client.ptime = get_tick();

		BinaryReader br(data, length);
		br.skip(4);
		unsigned packetid = br.read<unsigned>();

		if (log_inpacket_ids)
			logger.print("room inpacket %08x\r\n", packetid);
		if (log_inpackets)
			Network::dump(data, length, "room");

		//#ifdef _DEBUG
		//		logger.print("room packet %08x\r\n", packetid);
		//#endif
		switch (packetid)
		{
		case 0x0abd63da:	//show room list
		{
			d_dump(data, length, "room");
			unsigned char kind;
			unsigned char start_page;
			unsigned char unk1;
			unsigned char unk2;
			unsigned char unk3;
			//br >> kind >> start_page >> unk1 >> unk2 >> unk3;
			br.read(kind, start_page, unk1, unk2, unk3);

			unsigned start_count = start_page * 9;

			room_list_data_t r;
			BinaryWriter bw;
			bw.emplace<Network::packet_header_t>();
			bw << 0x02278b3a;

			//bw << (gamePacketHandler->recruitting_rooms.size() + gamePacketHandler->racing_rooms.size());
			unsigned written_count_pos = bw.allocate_pos<unsigned>();
			unsigned written_count = 0;
			written_count = 0;
			unsigned count = 0;

			//			for (unsigned i = 0; i < 20; ++i)
			for (unsigned i : gamePacketHandler->recruitting_rooms)
			{
				if (written_count > 9)
					break;

				GamePacketHandler::room_t & room = gamePacketHandler->rooms[i];
				switch (kind)
				{
				case 6:	//race
					if (room.kind != 0)
						continue;
					break;
				case 7:	//battle
					if (room.kind != 1)
						continue;
					break;
				case 8:	//coin
					if (room.kind != 2)
						continue;
					break;
				}

				if (count >= start_count)
				{
					r.id = room.room_id + room_id_disp;
					r.players_max = room.player_limit;
					r.current_players = (unsigned char)room.players.size();
					r.kind = (room.kind * 2) + (1 - room.is_single);
					r.mapid = room.mapid;
					r.joinable = 1;
					r.has_pw = room.password.size() > 0 ? 1 : 0;
					r.unk1 = 0;
					bw << r;
					++written_count;
				}

				++count;
			}
			for (unsigned i : gamePacketHandler->racing_rooms)
			{
				if (written_count > 9)
					break;

				GamePacketHandler::room_t & room = gamePacketHandler->rooms[i];
				switch (kind)
				{
				case 6:	//race
					if (room.kind != 0)
						continue;
					break;
				case 7:	//battle
					if (room.kind != 1)
						continue;
					break;
				case 8:	//coin
					if (room.kind != 2)
						continue;
					break;
				}

				if (count >= start_count)
				{
					r.id = room.room_id + room_id_disp;
					r.players_max = room.player_limit;
					r.current_players = (unsigned char)room.players.size();
					r.kind = (room.kind * 2) + (1 - room.is_single);
					r.mapid = room.mapid;
					r.joinable = 0;
					r.has_pw = room.password.size() > 0 ? 1 : 0;
					r.unk1 = 0;
					bw << r;
					++written_count;
				}
				++count;
			}

			bw.at<unsigned>(written_count_pos) = written_count;

			repply(std::move(bw.data));
		}
		break;
		case 0x02756c0a:	//create room
		{
			d_dump(data, length, "room");
			unsigned char kind;
			unsigned dbid;
			//br >> kind >> dbid;
			br.read(kind, dbid);

			unsigned char b_is_single = (kind & 1) == 0 ? 1 : 0;
			kind >>= 1;

			//			repply(Network::packet(0x02cdc85a, room_id_disp));	//join existing room
			//			break;

			bool found = false;
			for (unsigned room_id : gamePacketHandler->recruitting_rooms)
			{
				if (room_id < gamePacketHandler->rooms.size())
				{
					GamePacketHandler::room_t & room = gamePacketHandler->rooms[room_id];
					if (room.players.size() > 0 && room.players.size() < room.player_limit && room.is_single == b_is_single && room.kind == kind && room.password.size() == 0)
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_dbid(dbid))
						{
							player->mm_room_id = room.room_id;
							repply(Network::packet(0x02cdc85a, room.room_id + room_id_disp));	//join existing room
							found = true;
						}
						break;
					}
				}
			}

			if (!found && gamePacketHandler->can_create_room(dbid))
				repply(Network::packet(0x0b6db4ba));	//create new room
		}
		break;
		default:
			d_dump(data, length, "room");
		}
		return;
	}
	virtual void onDisconnect(Network::session_id_t session_id)
	{
		auto i = connections.find(session_id);
		if (i != connections.end())
		{
			{
				impl_lock_guard(lock, connection_count_mutex);
				if (!connection_count[i->second.dwip].dec())
					connection_count.erase(i->second.dwip);
			}

			connections.erase(i);
		}
	}
	virtual void onInit()
	{
		//		default_color = PINK;
	}

	unsigned last_client_check = get_tick();
	virtual void onTimer()
	{
		unsigned tick = get_tick();
		if (tick - last_client_check > client_check_interval)
		{
			last_client_check = tick;
			distribute(Network::packet(0));
		}

		unsigned dc_all = disconnect_all;
		switch (dc_all)
		{
		case 1:
			for (auto i = connections.begin(); i != connections.end(); ++i)
				disconnect(i->second.session_id);
			if (connections.size() == 0)
				disconnect_all = 0;
			else
				disconnect_all = 2;
			break;
		case 2:
			if (connections.size() == 0)
				disconnect_all = 0;
			break;
		}

	}
public:

	std::atomic<unsigned> disconnect_all;

	struct connection_t
		: public Network::NetworkClientBase
	{
		Network::session_id_t session_id;
		std::string ip;
		unsigned port;
		unsigned dwip;
		unsigned ptime;
		//		unsigned clientid;

		virtual void invalidate() {}
	};

	std::map<Network::session_id_t, connection_t> connections;

	RoomPacketHandler()
        : disconnect_all(0)
    {}

	virtual ~RoomPacketHandler() {}
};

void ShopPacketHandler::update_room_clothes(GamePacketHandler::connection_t * player, ShopPacketHandler::connection_t * client, bool send_to_self)
{
	send_to_self = true;
	unsigned b_avatar = player->avatar - 2;
	if (b_avatar >= 4 || player->room == nullptr)
		return;
	/*7*/
	std::vector<player_equip_data_t> player_data;

	{
		player_data.push_back(player_equip_data_t());
		player_data.back().clientId = player->id;
		player_data.back().avatar = player->avatar;
		player_data.back().haircolor = player->hair_color[b_avatar];

		player_data.back().items.reserve(client->equips[b_avatar].size());
		for (unsigned i = 0; i < client->equips[b_avatar].size(); ++i)
		{
			if (client->equips[b_avatar][i] >= client->items[b_avatar].size())
				continue;
			fashion_item_t const & it = client->items[b_avatar][client->equips[b_avatar][i]];
			if (!it.valid())
				continue;
			player_item_data_t itemdata = player_item_data_t::make(it);
			if (itemdata.valid())
				player_data.back().items.push_back(itemdata);
		}

	}

	for (Network::session_id_t sid : player->room->players)
	{
		if (sid != player->session_id || send_to_self)
		{
			if (GamePacketHandler::connection_t * player2 = gamePacketHandler->get_client(sid))
			{
				auto j = connections_by_id.find(player2->id);
				if (j != connections_by_id.end())
					shopPacketHandler->sendTo(j->second->session_id, Network::packet(0x0cf9a10a, player_data));
			}
		}
	}
}


void report(std::string const & client_name, std::string const & target, std::string const & reason, unsigned send_gms, std::shared_ptr<replay_data_t> rd, std::shared_ptr<std::string> csv, std::shared_ptr<std::string> html, unsigned playerid, int mapid)
{
	query<std::tuple<> >("INSERT INTO reports SET reporter = " + to_query(client_name) + ", target_player = " + to_query(target) + ", reason = " + to_query(reason) + (playerid ? (", playerid = " + to_query(playerid)) : "") + (mapid ? (", mapid = " + to_query(mapid)) : ""),
		[client_name, target, reason, send_gms, rd, playerid, mapid, csv, html](std::vector<std::tuple<> > const & data, bool success)
	{
		if (send_gms > 0)
		{
			std::string report_msg;
			bool has_replay = false;
			if (success)
			{
				report_msg = "Report #" + std::to_string(last_insert_id) + ": ";
				if (rd)
				{
					BinaryWriter bw;
					bw << *rd;
					std::string filename = "report_" + std::to_string(last_insert_id);
					for (char & ch : filename)
						if (ch == '[' || ch == ']')
							ch = '_';
					save_file("replays/" + filename + ".rpl", "PSFRPL", std::move(bw.data));
					has_replay = true;
				}
				if (csv)
				{
					std::string filename = "report_" + std::to_string(last_insert_id);
					for (char & ch : filename)
						if (ch == '[' || ch == ']')
							ch = '_';
					save_file("replays/" + filename + ".csv", "", std::vector<char>(csv->begin(), csv->end()));
				}
				if (html)
				{
					std::string html_dir;
					{
						impl_lock_guard(l, version_string_mutex);
						html_dir = ::html_dir;
					}
					std::string filename = "report_" + std::to_string(last_insert_id);
					for (char & ch : filename)
						if (ch == '[' || ch == ']')
							ch = '_';
					save_file(html_dir + "/" + filename + ".html", "", std::vector<char>(html->begin(), html->end()));
				}
			}

			report_msg += client_name + " has reported " + target + ", reason: " + reason;
			report_msg += "| replay: ";
			report_msg += (has_replay ? "yes" : "no");
			report_msg += " playerid: " + (playerid ? std::to_string(playerid) : "NULL");
			report_msg += " mapid: " + (mapid ? std::to_string(mapid) : "NULL");

			logger << LIGHT_RED << report_msg;

			query<std::tuple<std::string> >("SELECT name FROM characters WHERE player_rank >= " + std::to_string(send_gms), [report_msg](std::vector<std::tuple<std::string> > const & data, bool success)
			{
				if (success)
				{
					for (unsigned i = 0; i < data.size(); ++i)
						send_message2(std::get<0>(data[i]), report_msg);
				}
			});
		}
		else if (success)
		{
			if (rd)
			{
				BinaryWriter bw;
				bw << *rd;
				std::string filename = "report_" + std::to_string(last_insert_id);
				for (char & ch : filename)
					if (ch == '[' || ch == ']')
						ch = '_';
				save_file("replays/" + filename + ".rpl", "PSFRPL", std::move(bw.data));
			}
			if (csv)
			{
				std::string filename = "report_" + std::to_string(last_insert_id);
				for (char & ch : filename)
					if (ch == '[' || ch == ']')
						ch = '_';
				save_file("replays/" + filename + ".csv", "", std::vector<char>(csv->begin(), csv->end()));
			}
			if (html)
			{
				std::string html_dir;
				{
					impl_lock_guard(l, version_string_mutex);
					html_dir = ::html_dir;
				}
				std::string filename = "report_" + std::to_string(last_insert_id);
				for (char & ch : filename)
					if (ch == '[' || ch == ']')
						ch = '_';
				save_file(html_dir + "/" + filename + ".html", "", std::vector<char>(html->begin(), html->end()));
			}

		}

	});

}

unsigned get_friend_count(std::string const & name)
{
	unsigned r = 0;
	auto i = messengerPacketHandler->connections_by_name.find(name);
	if (i != messengerPacketHandler->connections_by_name.end())
		r = i->second->friends.size();
	return r;
}

unsigned get_friend_count(unsigned id)
{
	unsigned r = 0;
	auto i = messengerPacketHandler->connections_by_id.find(id);
	if (i != messengerPacketHandler->connections_by_id.end())
		r = i->second->friends.size();
	return r;
}

void add_guild_points(unsigned guild_id, int add_points, unsigned dbid)
{
	if (add_points > 0)
	{
		auto i = guildPacketHandler->guilds.find(guild_id);
		if (i != guildPacketHandler->guilds.end())
		{
			GuildPacketHandler::guild_t * guild = i->second;
			guild->points += add_points;
			query("UPDATE guilds SET points = points + " + to_query(add_points) + " WHERE id = " + to_query(guild_id));
			for (unsigned j = 0; j < guild->members.size(); ++j)
			{
				if (guild->members[j].id == dbid)
				{
					guild->members[j].point += add_points;
					query("UPDATE characters SET guild_points = guild_points + " + to_query(add_points) + "WHERE id = " + to_query(dbid));
					break;
				}
			}
		}
	}
}

std::string get_guild_name(unsigned guildid)
{
	std::string r;
	auto i = guildPacketHandler->guilds.find(guildid);
	if (i != guildPacketHandler->guilds.end())
		r = i->second->name;
	return r;
}

bool get_replay_item_data(unsigned player_id, replay_data_t & rd)
{
	auto j = shopPacketHandler->connections_by_id.find(player_id);
	if (j != shopPacketHandler->connections_by_id.end())
	{
		unsigned avatar = j->second->avatar - 2;
		if (avatar < 4)
		{
			for (unsigned id : j->second->equips[j->second->avatar - 2])
			{
				if (id < j->second->items[j->second->avatar - 2].size())
				{
					auto const & item = j->second->items[j->second->avatar - 2][id];
					rd.item_data.emplace_back();
					rd.item_data.back().id = item.id;
					rd.item_data.back().color = item.color;
					rd.item_data.back().gems = item.gems;
				}
			}
		}
		return true;
	}
	return false;
}


bool get_stats(unsigned player_id, ingame_charinfo_t & data)
{
	for (unsigned i = 0; i < data.stats.size(); ++i)
		data.stats[i] = 0;

	auto j = shopPacketHandler->connections_by_id.find(player_id);
	if (j != shopPacketHandler->connections_by_id.end())
	{
		unsigned b_avatar = j->second->avatar - 2;
		if (b_avatar >= 4)
			return false;

		for (unsigned k = 0; k < j->second->equips[b_avatar].size(); ++k)
		{
			unsigned slot = j->second->equips[b_avatar][k];
			if (slot < j->second->items[b_avatar].size())
			{
				for (unsigned short gem : j->second->items[b_avatar][slot].gems)
				{
					unsigned kind = get_gem_kind(gem);
					if (kind < 8)
					{
						data.stats[kind] += get_gem_bonus(gem) / 100.0f;
					}
				}
			}
		}

		return true;
	}
	return false;
}



void do_command(std::string const & client_name, const char * msg, unsigned const e)
{
	unsigned i = 0;
	for (; i < e && msg[i] != 0; ++i)
	{
		if (msg[i] == ' ' || msg[i] == 9)
			continue;
		break;
	}
	if (i >= e || msg[i] == 0 || (msg[i] != '/' && msg[i] != '.'))
		return;

	++i;

	std::string cmd;
	cmd.reserve(16);
	for (; i < e && msg[i] != 0; ++i)
	{
		if (msg[i] == ' ' || msg[i] == 9)
			break;
		cmd += msg[i];
	}

	if (cmd.size() == 0)
		return;

	for (; i < e && msg[i] != 0; ++i)
		if (msg[i] != ' ' && msg[i] != 9)
			break;

	if (cmd == "w" || cmd == "W")
	{
		auto k = guildPacketHandler->connections_by_name.find(client_name);
		if (k == guildPacketHandler->connections_by_name.end())
			return;

		std::string target;
		unsigned target_start = i;
		for (; i < e && msg[i] != 0; ++i)
			if (msg[i] == ' ' || msg[i] == 9)
				break;

		if (target_start == i)
			return;

		target.assign(&msg[target_start], i - target_start);

		if (target == client_name)
			return;

		for (; i < e && msg[i] != 0; ++i)
			if (msg[i] != ' ' && msg[i] != 9)
				break;

		if (i < e && msg[i] != 0)
		{
			auto j = guildPacketHandler->connections_by_name.find(target);
			if (j != guildPacketHandler->connections_by_name.end())
			{
				guildPacketHandler->sendTo(k->second->session_id, std::move(Network::packet(0x0d68a8aa, target + " <<<", std::string(&msg[i], e - i))));
				guildPacketHandler->sendTo(j->second->session_id, std::move(Network::packet(0x0d68a8aa, client_name + " >>>", std::string(&msg[i], e - i))));
			}
		}
		return;
	}


	if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
	{
		if (cmd == "endrace")
		{
			if (player->room && player->room->room_state == GamePacketHandler::room_t::running/* && !player->room->uses_relay && player->room->players.size() == 1*/)
			{
				player->room->stop_run(*player);
			}
		}
		else if (cmd == "random")
		{
			if (player->is_host() && player->room->room_state == GamePacketHandler::room_t::waiting)
				player->room->set_map(0x03e9);
		}
		else if (cmd == "forcestart")
		{
			if (player->is_host() && player->room->room_state == GamePacketHandler::room_t::waiting)
				player->room->start_run(true);
		}
		else if (cmd == "time")
		{
			time_t rawtime;
			time(&rawtime);
			{
				auto j = guildPacketHandler->connections_by_name.find(client_name);
				if (j != guildPacketHandler->connections_by_name.end())
				{
					guildPacketHandler->sendTo(j->second->session_id, (Network::packet(0x0d68a8aa, (std::string)"Server Time: " + datetime2str(rawtime))));
					player->message(ctime(&rawtime));
				}
			}
		}
		else if (cmd == "haircolor")
		{
			for (; i < e && msg[i] != 0; ++i)
				if (msg[i] != ' ' && msg[i] != 9)
					break;

			unsigned r = ~0, g = ~0, b = ~0;
			sscanf(&msg[i], "%u %u %u", &r, &g, &b);
			if (b != ~0 && player->avatar >= 2 && player->avatar <= 5 && r < 256 && g < 256 && b < 256)
			{
				auto i = shopPacketHandler->connections_by_id.find(player->id);
				if (i != shopPacketHandler->connections_by_id.end())
				{
					unsigned color = b + g * 256 + r * 256 * 256;
					player->hair_color[player->avatar - 2] = color;
					query("UPDATE characters SET hair" + std::to_string(player->avatar) + " = " + to_query(color) + " WHERE id = " + to_query(player->dbid));
					i->second->send_avatars();
					if (player->room)
						shopPacketHandler->update_room_clothes(player, i->second);
				}
			}
		}
		else if (cmd == "help")
		{
			auto j = guildPacketHandler->connections_by_name.find(client_name);
			if (j != guildPacketHandler->connections_by_name.end())
			{
				for (unsigned i = 0; i < help_text.size(); ++i)
					guildPacketHandler->sendTo(j->second->session_id, std::move(Network::packet(0x0d68a8aa, help_text[i].first, help_text[i].second)));
			}
		}
		else if (cmd == "rules")
		{
			auto j = guildPacketHandler->connections_by_name.find(client_name);
			if (j != guildPacketHandler->connections_by_name.end())
			{
				for (unsigned i = 0; i < rules_text.size(); ++i)
					guildPacketHandler->sendTo(j->second->session_id, std::move(Network::packet(0x0d68a8aa, rules_text[i].first, rules_text[i].second)));
			}
		}
		else if (cmd == "save_replay")
		{
			std::string replay_name = player->name + "_" + std::string(&msg[i], e - i);
			bool allgood = true;
			for (char ch : replay_name)
			{
				if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
					continue;
				else
				{
					allgood = false;
					break;
				}
			}

			if (allgood)
			{
				Network::session_id_t session_id = player->session_id;
				rename_file("replays/" + player->name + "_last.rpl", "replays/" + replay_name + ".rpl", [session_id, replay_name](bool success)
				{
					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(session_id))
					{
						if (success)
							player->message("Successfully saved last replay as " + replay_name);
						else
							player->message("Failed to save last replay as " + replay_name);
					}
				});
			}
			else
			{
				player->message("Failed to save last replay as " + replay_name);
			}
		}
		else if (cmd == "replay")
		{
			if (player->room == nullptr && i < e)
			{
				std::string replay_name = std::string(&msg[i], e - i);
				if (replay_name.substr(0, 6) == "report")
				{
					if (player->player_rank < 1)
						return;
				}

				if (std::shared_ptr<replay_data_t> rd = gamePacketHandler->find_replay(replay_name))
				{
					gamePacketHandler->create_room(*player, rd);
				}
				else
				{
					Network::session_id_t session_id = player->session_id;
					if (!load_file("replays/" + replay_name + ".rpl", "PSFRPL", [session_id, replay_name](std::vector<char> const & data, bool success)
					{
						if (success)
						{
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(session_id))
							{
								if (player->room == nullptr)
								{
									std::shared_ptr<replay_data_t> rd = std::make_shared<replay_data_t>();
									BinaryReader br(data.data(), data.size());
									br.seek(6);
									br >> *rd;
									gamePacketHandler->add_replay(replay_name, rd);
									gamePacketHandler->create_room(*player, rd);
								}

							}
						}
						else
						{
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client(session_id))
								player->message("Failed to load replay");
						}
					}))
					{
						player->message("Failed to load replay");
					}
				}
			}
			else
			{
				player->message("Please start replay from lobby");
			}
		}
		else if (cmd == "itemcolor")
		{
			if (player->room == nullptr)
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				unsigned r = ~0, g = ~0, b = ~0;
				sscanf(&msg[i], "%u %u %u", &r, &g, &b);
				if (b != ~0 && player->avatar >= 2 && player->avatar <= 5 && r < 256 && g < 256 && b < 256)
				{
					auto i = shopPacketHandler->connections_by_id.find(player->id);
					if (i != shopPacketHandler->connections_by_id.end())
					{
						if (i->second->last_equipped_item < i->second->items[player->avatar - 2].size())
						{
							auto & it = i->second->items[player->avatar - 2][i->second->last_equipped_item];
							if (it.valid())
							{
								unsigned color = b + g * 256 + r * 256 * 256;
								it.color = color;

								query("UPDATE fashion SET color = " + to_query(color) + " WHERE id = " + to_query(it.dbid)) + " AND character_id = " + to_query(player->dbid);

								shopPacketHandler->send_equips(*i->second);
								if (player->room)
								{
									shopPacketHandler->update_room_clothes(player, i->second, true);
									//								player->room->re_enter(*player);
								}
							}
						}
						else
						{
							player->message("Please (re)equip the item you wish to change the color of");
						}
					}
				}
			}
			else
			{
				player->message("Cannot be used in room");
			}
		}
		else if (cmd == "allitemcolor" || cmd == "allitemcolors")
		{
			if (player->room == nullptr)
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				unsigned r = ~0, g = ~0, b = ~0;
				sscanf(&msg[i], "%u %u %u", &r, &g, &b);
				if (b != ~0 && player->avatar >= 2 && player->avatar <= 5 && r < 256 && g < 256 && b < 256)
				{
					auto i = shopPacketHandler->connections_by_id.find(player->id);
					if (i != shopPacketHandler->connections_by_id.end())
					{
						bool changed = false;
						for (unsigned j = 0; j < i->second->equips[player->avatar - 2].size(); ++j)
						{
							if (i->second->equips[player->avatar - 2][j] < i->second->items[player->avatar - 2].size())
							{
								auto & it = i->second->items[player->avatar - 2][i->second->equips[player->avatar - 2][j]];
								if (it.valid())
								{
									unsigned color = b + g * 256 + r * 256 * 256;
									it.color = color;

									query("UPDATE fashion SET color = " + to_query(color) + " WHERE id = " + to_query(it.dbid)) + " AND character_id = " + to_query(player->dbid);
									changed = true;
								}
							}
						}
						//11
						if (changed)
						{
							shopPacketHandler->send_equips(*i->second);
							if (player->room)
							{
								shopPacketHandler->update_room_clothes(player, i->second, true);
								//							player->room->re_enter(*player);
							}
						}
					}
				}
			}
			else
			{
				player->message("Cannot be used in room");
			}
		}
		else if (cmd == "invite" && player->is_host())
		{
			std::string target;
			unsigned target_start = i;
			for (; i < e && msg[i] != 0; ++i)
				if (msg[i] == ' ' || msg[i] == 9)
					break;

			if (target_start == i)
				return;

			target.assign(&msg[target_start], i - target_start);

			auto j = messengerPacketHandler->connections_by_name.find(target);
			if (j != messengerPacketHandler->connections_by_name.end())
			{
				unsigned tick = get_tick();
				if (tick - j->second->last_invite_time > 20000)
				{
					if (player->room != nullptr && j->second->state == MessengerPacketHandler::connection_t::ROOM && player->room->room_id == j->second->room_id - room_id_disp)
					{
						message(player->id, "Player is already in the same room");
					}
					else
					{
						j->second->last_invite_time = tick;
						messengerPacketHandler->sendTo(j->second->session_id, Network::packet(0x0863d74a, player->dbid));
						if (GamePacketHandler::connection_t * player2 = gamePacketHandler->get_client_by_name(target))
						{
							player2->room_invite_id = player->room->room_id;
							player2->room_invite_time = tick;
						}
						message(player->id, "Invite sent");
					}
				}
				else
				{
					message(player->id, "Please invite later");
				}
			}
			else
			{
				message(player->id, "Cannot find " + target);
			}
		}

		if (player->player_rank >= 1)
		{
			if (cmd == "report")
			{
				std::string target;
				unsigned target_start = i;
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				if (target_start == i)
					return;

				target.assign(&msg[target_start], i - target_start);

				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				std::string reason(&msg[i], e - i);

				reason = trim(reason);

				if (reason.size() == 0)
					player->message("Please specify a reason");
				else
				{
					report(client_name, target, reason);
				}
			}
			else if (cmd == "cmd")
			{
				auto j = guildPacketHandler->connections_by_name.find(client_name);
				if (j != guildPacketHandler->connections_by_name.end())
				{
					for (unsigned i = 0; i < cmd_text.size(); ++i)
						guildPacketHandler->sendTo(j->second->session_id, std::move(Network::packet(0x0d68a8aa, cmd_text[i].first, cmd_text[i].second)));
				}
			}
			else if (cmd == "server" || cmd == "info")
			{
				auto j = guildPacketHandler->connections_by_name.find(client_name);
				if (j != guildPacketHandler->connections_by_name.end())
				{
					std::string str_player = std::to_string((int)online_count);
					std::string str_exp = std::to_string((int)exp_mult);
					std::string str_pro = std::to_string((int)pro_mult);
					guildPacketHandler->sendTo(j->second->session_id, (Network::packet(0x0d68a8aa, "Server", "\nPlayer Online: " + str_player + "\nEXP: x" + str_exp + "\nCoin: x" + str_pro)));
				}
			}
			else if (cmd == "notice" || cmd == "announce")
			{
				std::string str = trim(std::string(&msg[i], e - i));
				if (str.size() > 0)
					announce(str);
			}
			else if (cmd == "clear_equips")
			{
				auto j = shopPacketHandler->connections_by_id.find(player->id);
				if (j != shopPacketHandler->connections_by_id.end())
				{
					for (unsigned i = 0; i < j->second->equips.size(); ++i)
						j->second->equips[i].clear();
					shopPacketHandler->send_equips(*j->second);
				}
			}

		}
		if (player->player_rank >= 2)
		{
			if (cmd == "mute")
			{
				int minutes = 0;
				std::string target;
				unsigned target_start = i;
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				if (target_start == i)
					return;

				target.assign(&msg[target_start], i - target_start);

				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i >= e)
					return;

				sscanf(&msg[i], "%d", &minutes);

				if (minutes > 0 && target.size() > 0)
				{
					add_chatban(target, minutes);
					message(client_name, "Successfully muted " + target + " for " + std::to_string(minutes) + " minutes");
				}
			}
			else if (cmd == "unmute")
			{
				std::string target;
				unsigned target_start = i;
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				if (target_start == i)
					return;

				target.assign(&msg[target_start], i - target_start);

				if (target.size())
				{
					if (remove_chatban(target))
						message(client_name, "Unmuted " + target);
					else
						message(client_name, target + " was not muted");
				}
			}
			else if (cmd == "flushchat")
			{
				flush_chatlog();
			}
			else if (cmd == "message")
			{
				std::string target;

				unsigned target_start = i;
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				if (target_start == i)
					return;

				target.assign(&msg[target_start], i - target_start);

				std::string message(&msg[i], e - i);


				if (message.size())
					send_message2(target, message);
			}
			else if (cmd == "delete_messages")
			{
				query("DELETE * FROM messages WHERE char_id = " + to_query(player->dbid));
				auto i = trickPacketHandler->connections_by_dbid.find(player->dbid);
				if (i != trickPacketHandler->connections_by_dbid.end())
				{
					i->second->messages.clear();
				}

			}
			else if (cmd == "reload_event_config")
			{
				init_event();
			}
			else if (cmd == "gift")
			{
				std::string player_name;
				unsigned itemid = 0;
				unsigned daycount = 0;
				std::string target;
				unsigned target_start = i;
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				if (target_start == i)
					return;

				target.assign(&msg[target_start], i - target_start);

				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i >= e)
					return;

				sscanf(&msg[i], "%d", &itemid);

				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i >= e)
					return;

				sscanf(&msg[i], "%d", &daycount);

				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				std::string message(&msg[i], e - i);

				if (itemid - 1 - itemid_disp < itemlist.size() && itemlist[itemid - 1 - itemid_disp].valid)
				{
					query<std::tuple<> >("INSERT INTO gifts SET PlayerID = (SELECT id FROM characters WHERE name = " + to_query(target) + "), ItemID = " + to_query(itemid) + ", message = " + to_query(message) + ", sender = " + to_query(client_name) + ", DayCount = " + to_query(daycount), [client_name](std::vector<std::tuple<> > const &, bool success)
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
						{
							if (!success)
								player->message("Failed to send gift");
							else
								player->message("Gift sent");
						}
					});
				}
				else
					player->message("Invalid item or id");
			}
			else if (cmd == "join")
			{
				std::string target;
				unsigned target_start = i;
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				if (target_start == i)
					return;

				target.assign(&msg[target_start], i - target_start);
				unsigned room_id = ~0;
				{
					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(target))
						if (player->room != nullptr)
							room_id = player->room->room_id;
				}
				if (room_id < gamePacketHandler->rooms.size())
				{
					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
						if (player->room == nullptr || (player->room != nullptr && player->room->room_state == GamePacketHandler::room_t::waiting))
							gamePacketHandler->rooms[room_id].join(*player);
				}

			}
			else if (cmd == "join_room")
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i < e && msg[i] != 0)
				{
					unsigned room_id = 0;
					sscanf(&msg[i], "%d", &room_id);

					room_id -= room_id_disp;
					if (room_id < gamePacketHandler->rooms.size())
					{
						if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
							if (player->room == nullptr || (player->room != nullptr && player->room->room_state == GamePacketHandler::room_t::waiting))
								gamePacketHandler->rooms[room_id].join(*player);
					}
				}
			}
			else if (cmd == "reload_records")
			{
				query<std::tuple<unsigned, float> >("CALL get_global_records()", [](std::vector<std::tuple<unsigned, float> > const & data, bool success)
				{
					if (success)
					{
						global_map_records.clear();
						for (unsigned i = 0; i < data.size(); ++i)
							global_map_records.emplace(std::get<0>(data[i]), std::get<1>(data[i]));
					}
				});
				query<std::tuple<unsigned, float> >("CALL get_seasonal_records()", [](std::vector<std::tuple<unsigned, float> > const & data, bool success)
				{
					if (success)
					{
						seasonal_map_records.clear();
						for (unsigned i = 0; i < data.size(); ++i)
							seasonal_map_records.emplace(std::get<0>(data[i]), std::get<1>(data[i]));
					}
				});
			}
			else if (cmd == "level")
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i < e && msg[i] != 0)
				{
					int new_level = 0;
					sscanf(&msg[i], "%d", &new_level);

					if (new_level > 0 && (unsigned)new_level <= exp_list.size())
					{
						player->level = new_level;
						player->exp = 0;
						if (new_level > 1)
							player->exp = exp_list[new_level - 2];
						query("UPDATE characters SET level = " + to_query(new_level) + ", experience = " + to_query(player->exp) + " WHERE name = " + to_query(client_name));
					}
				}
			}
			else if (cmd == "exp")
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i < e && msg[i] != 0)
				{
					int new_exp = 0;
					sscanf(&msg[i], "%d", &new_exp);

					player->exp = new_exp;
					player->message("Exp is set");
					query("UPDATE characters SET experience = " + to_query(player->exp) + " WHERE name = " + to_query(client_name));
				}
			}
			else if (cmd == "cash")
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i < e && msg[i] != 0)
				{
					unsigned new_cash = 0;
					sscanf(&msg[i], "%u", &new_cash);

					player->message("Set cash");
					player->cash = new_cash;
					query("UPDATE characters SET cash = " + to_query(new_cash) + " WHERE name = " + to_query(client_name));
				}
			}
			else if (cmd == "gem")
			{
				std::string target;
				unsigned target_start = i;
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				if (target_start == i)
					return;

				target.assign(&msg[target_start], i - target_start);

				if (i < e && msg[i] != 0)
				{
					unsigned gem = ~0;
					sscanf(&msg[i], "%u", &gem);

					if (gem != ~0)
					{
						if (GamePacketHandler::connection_t * target_player = gamePacketHandler->get_client_by_name(target))
						{
							add_gem(target_player->id, gem);
							announce_packet(target_player->id, "<Format,2,Color,0,0,255,0> You have acquired a gem");
							player->message("Successfully added gem");
						}
						else
							player->message("Can't find player " + target);
					}
					else
						player->message("Please specify a gem id");
				}
			}
			else if (cmd == "kick")
			{
				std::string target;
				unsigned target_start = i;
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				if (target_start == i)
					return;

				target.assign(&msg[target_start], i - target_start);
				if (::kick(target))
					player->message("Successfully kicked " + target);
				else
					player->message(target + " is not online");

			}
			else if (cmd == "clear_record")
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;
				if (i >= e || msg[i] == 0)
					return;

				unsigned report_id = ~0;
				sscanf(&msg[i], "%u", &report_id);
				if (report_id != ~0)
				{
					query<std::tuple<unsigned, int, unsigned, std::string> >("SELECT playerid, mapid, handled, (SELECT name FROM characters WHERE characters.id = reports.playerid) FROM reports WHERE id = " + to_query(report_id), [client_name, report_id](std::vector<std::tuple<unsigned, int, unsigned, std::string> > const & data, bool success)
					{
						if (success && data.size() > 0)
						{
							unsigned playerid = std::get<0>(data[0]);
							int mapid = std::get<1>(data[0]);
							unsigned handled = std::get<2>(data[0]);
							std::string target = std::get<3>(data[0]);

							if (playerid == 0)
							{
								if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
									player->message("Selected report has no playerid associated");
								return;
							}

							if (handled != 0)
							{
								if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
									player->message("Selected report has already been handled: " + std::to_string(handled));
								return;
							}

							if (mapid >= 0)
							{

								auto k = trickPacketHandler->connections_by_dbid.find(playerid);
								if (k != trickPacketHandler->connections_by_dbid.end())
								{
									k->second->records.erase(mapid);
								}

								query<std::tuple<> >("DELETE FROM current_season_records WHERE season_id = " + to_query(current_season_id) + " AND char_id = " + to_query(playerid) + " AND mapid = " + to_query(mapid), [client_name, mapid, playerid, report_id](std::vector<std::tuple<> > const & data, bool success)
								{
									unsigned handler_id = ~0;
									if (success)
										send_message2(playerid, "Your record on map " + get_map_name(mapid) + " has been cleared");

									if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
									{
										handler_id = player->dbid;
										if (success)
											player->message("Clear record succeded");
										else
											player->message("Clear record failed");
									}

									if (success)
										query("UPDATE reports SET handled = " + to_query(handler_id) + " WHERE id = " + to_query(report_id));

									query<std::tuple<unsigned, float> >("CALL get_global_records()", [](std::vector<std::tuple<unsigned, float> > const & data, bool success)
									{
										if (success)
										{
											global_map_records.clear();
											for (unsigned i = 0; i < data.size(); ++i)
												global_map_records.emplace(std::get<0>(data[i]), std::get<1>(data[i]));
										}
									});
									query<std::tuple<unsigned, float> >("CALL get_seasonal_records()", [](std::vector<std::tuple<unsigned, float> > const & data, bool success)
									{
										if (success)
										{
											seasonal_map_records.clear();
											for (unsigned i = 0; i < data.size(); ++i)
												seasonal_map_records.emplace(std::get<0>(data[i]), std::get<1>(data[i]));
										}
									});
								});
							}
							else
							{
								if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
									player->message("No map is associated with that report");
							}
						}
						else
						{
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
								player->message("Clear record failed");
						}
					});
				}
				else
					player->message("Please specify report id");
			}
			else if (cmd == "ban_report")
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;
				if (i >= e || msg[i] == 0)
					return;

				unsigned report_id = ~0;
				unsigned daycount = 0;
				sscanf(&msg[i], "%u %u", &report_id, &daycount);
				if (report_id != ~0 && daycount > 0)
				{
					std::string reason_str;
					for (unsigned j = 0; j < 2; ++j)
					{
						for (; i < e && msg[i] != 0; ++i)
							if (msg[i] == ' ' || msg[i] == 9)
								break;

						for (; i < e && msg[i] != 0; ++i)
							if (msg[i] != ' ' && msg[i] != 9)
								break;
					}

					if (i < e && msg[i] != 0)
					{
						reason_str.assign(&msg[i], e - i);
						reason_str = trim(reason_str);
					}

					query<std::tuple<unsigned, int, unsigned, std::string> >("SELECT playerid, mapid, handled, (SELECT name FROM characters WHERE characters.id = reports.playerid) FROM reports WHERE id = " + to_query(report_id), [client_name, daycount, report_id, reason_str](std::vector<std::tuple<unsigned, int, unsigned, std::string> > const & data, bool success)
					{
						if (success && data.size() > 0)
						{
							unsigned playerid = std::get<0>(data[0]);
							int mapid = std::get<1>(data[0]);
							unsigned handled = std::get<2>(data[0]);
							std::string target = std::get<3>(data[0]);

							if (playerid == 0)
							{
								if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
									player->message("Selected report has no playerid associated");
								return;
							}

							if (handled != 0)
							{
								if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
									player->message("Selected report has already been handled: " + std::to_string(handled));
								return;
							}

							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_dbid(playerid))
								::kick(player->name);

							unsigned long long ban_time = (unsigned long long)time(0) + (unsigned long long)daycount * 24 * 60 * 60;

							std::string reason = "report id " + std::to_string(report_id) + ": " + reason_str;

							query<std::tuple<> >("UPDATE accounts SET banned = " + to_query(ban_time) + ", ban_reason = " + to_query(reason) + " WHERE character_name = " + to_query(target), [client_name, target, reason, daycount, playerid, mapid, report_id](std::vector<std::tuple<> > const & data, bool success)
							{
								unsigned handler_id = ~0;

								if (!success || affected_rows < 1)
								{
									logger << "Ban failed";
								}
								else
								{
									if (success)
										send_message2(playerid, "Your have been banned for " + std::to_string(daycount) + ": " + reason);

									logger << client_name << " banned " << target << ", reason: " << reason << " for " << daycount << " days";
								}

								if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
								{
									handler_id = player->dbid;
									if (!success || affected_rows < 1)
										player->message("Ban failed");
									else
										player->message("Ban succeeded for character " + target + " for " + std::to_string(daycount) + " days");
								}

								if (success)
									query("UPDATE reports SET handled = " + to_query(handler_id) + " WHERE id = " + to_query(report_id));
							});

							if (mapid >= 0)
							{
								auto k = trickPacketHandler->connections_by_dbid.find(playerid);
								if (k != trickPacketHandler->connections_by_dbid.end())
								{
									k->second->records.erase(mapid);
								}

								query<std::tuple<> >("DELETE FROM current_season_records WHERE season_id = " + to_query(current_season_id) + " char_id = " + to_query(playerid) + " AND mapid = " + to_query(mapid), [client_name, mapid, playerid](std::vector<std::tuple<> > const & data, bool success)
								{
									if (success)
										send_message2(playerid, "Your record on map " + get_map_name(mapid) + " has been cleared");
									if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
									{
										if (success)
											player->message("Clear record succeded");
										else
											player->message("Clear record failed");
									}

									query<std::tuple<unsigned, float> >("CALL get_global_records()", [](std::vector<std::tuple<unsigned, float> > const & data, bool success)
									{
										if (success)
										{
											global_map_records.clear();
											for (unsigned i = 0; i < data.size(); ++i)
												global_map_records.emplace(std::get<0>(data[i]), std::get<1>(data[i]));
										}
									});
									query<std::tuple<unsigned, float> >("CALL get_seasonal_records()", [](std::vector<std::tuple<unsigned, float> > const & data, bool success)
									{
										if (success)
										{
											seasonal_map_records.clear();
											for (unsigned i = 0; i < data.size(); ++i)
												seasonal_map_records.emplace(std::get<0>(data[i]), std::get<1>(data[i]));
										}
									});
								});
							}
						}
						else
						{
							if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
								player->message("Ban failed");
						}
					});
				}
				else
					player->message("Please specify report id and day count");
			}
			else if (cmd == "ban")
			{
				std::string target;
				unsigned target_start = i;
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;

				if (target_start == i)
					return;

				target.assign(&msg[target_start], i - target_start);

				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				int daycount = 0;
				sscanf(&msg[i], "%d", &daycount);

				if (daycount < 1)
				{
					player->message("Ban day count must be greater than 0");
					return;
				}

				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] == ' ' || msg[i] == 9)
						break;


				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i >= e)
					return;

				std::string reason(&msg[i], e - i);

				reason = trim(reason);

				if (reason.size() == 0)
				{
					player->message("Please specify a ban reason");
					return;
				}

				::kick(target);

				unsigned long long ban_time = (unsigned long long)time(0) + (unsigned long long)daycount * 24 * 60 * 60;

				query<std::tuple<> >("UPDATE accounts SET banned = " + to_query(ban_time) + ", ban_reason = " + to_query(reason) + " WHERE character_name = " + to_query(target), [client_name, target, reason, daycount](std::vector<std::tuple<> > const & data, bool success)
				{
					if (!success || affected_rows < 1)
					{
						logger << "Ban failed";
					}
					else
					{
						if (success)
							send_message2(target, "Your have been banned for " + std::to_string(daycount) + ": " + reason);
						logger << client_name << " banned " << target << ", reason: " << reason << " for " << daycount << " days";
					}

					if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(client_name))
					{
						if (!success)
							player->message("Ban failed");
						else
							player->message("Ban succeeded for character " + target + " for " + std::to_string(daycount) + " days");
					}

				});
			}

		}
		if (player->player_rank >= 3)
		{
			if (cmd == "exp_mult")
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i < e && msg[i] != 0)
				{
					float new_rate = 0;
					sscanf(&msg[i], "%f", &new_rate);
					if (new_rate > 0)
					{
						set_config("exp_mult", std::to_string(new_rate));
						std::string str = std::to_string((int)exp_mult) + '.' + (char)('0' + (int)(exp_mult * 10) % 10) + (char)('0' + (int)(exp_mult * 100) % 10) + 'x';
						announce("Exp multiplier is " + str);
					}
				}
			}
			else if (cmd == "pro_mult")
			{
				for (; i < e && msg[i] != 0; ++i)
					if (msg[i] != ' ' && msg[i] != 9)
						break;

				if (i < e && msg[i] != 0)
				{
					float new_rate = 0;
					sscanf(&msg[i], "%f", &new_rate);
					if (new_rate > 0)
					{
						set_config("pro_mult", std::to_string(new_rate));
						std::string str = std::to_string((int)pro_mult) + '.' + (char)('0' + (int)(pro_mult * 10) % 10) + (char)('0' + (int)(pro_mult * 100) % 10) + 'x';
						announce("Gold multiplier is " + str);
					}
				}
			}
		}

	}
}

void announce_packet(unsigned id, std::string const & str)
{
	auto i = guildPacketHandler->connections_by_id.find(id);
	if (i != guildPacketHandler->connections_by_id.end())
		guildPacketHandler->sendTo(i->second->session_id, Network::packet(0x02bda09a, "", "System", str));
}

void message(unsigned id, std::string const & str)
{
	if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_id(id))
		player->message(str);
}

void message(std::string const & name, std::string const & str)
{
	if (GamePacketHandler::connection_t * player = gamePacketHandler->get_client_by_name(name))
		player->message(str);
}

void add_chatban(std::string const & name, unsigned minutes)
{
	if (name.size() > 0)
	{
		time_t t = time(0) + 60 * minutes;
		impl_lock_guard(lock, chatban_mutex);
		chatban[name] = t;
		message(name, "You have been muted for " + std::to_string(minutes) + "minutes");
	}
}

bool test_chatban_2(std::string const & name)
{
	time_t t = time(0);
	auto i = chatban.find(name);
	if (i != chatban.end())
	{
		if (t < i->second)
			return true;
		chatban.erase(i);
	}
	return false;
}


bool remove_chatban(std::string const & name)
{
	impl_lock_guard(lock, chatban_mutex);
	auto i = chatban.find(name);
	if (i != chatban.end())
	{
		bool r = test_chatban_2(name);
		chatban.erase(i);
		message(name, "Mute has been removed");
		return r;
	}
	return false;
}



void send_message(std::string const & name, unsigned msgid, std::string const & str1, std::string const & str2, std::string const & str3, std::string const & str4)
{
	auto i = gamePacketHandler->connections_by_name.find(name);
	if (i != gamePacketHandler->connections_by_name.end())
	{
		auto j = trickPacketHandler->connections_by_id.find(i->second->id);
		if (j != trickPacketHandler->connections_by_id.end())
		{
			unsigned long long msg_seq_id = ++trickPacketHandler->msg_seq_id;
			/*4*/
			trickPacketHandler->sendTo(j->second->session_id, Network::packet(0x05d2d8aa, (char)j->second->today_message_id, msg_seq_id, msgid, str1, str2, str3, str4));
			++j->second->today_message_id;
			TrickPacketHandler::message_data_t msg;
			msg.id = msgid;
			msg.seq_id = msg_seq_id;
			msg.str1 = str1;
			msg.str2 = str2;
			msg.str3 = str3;
			msg.str4 = str4;
			j->second->messages.push_back(std::move(msg));
			query("REPLACE INTO messages VALUES(" + to_query(i->second->dbid, msg_seq_id, msgid, str1, str2, str3, str4) + ", unix_timestamp())");
			return;
		}
	}


	{
		unsigned long long msg_seq_id = ++trickPacketHandler->msg_seq_id;
		query("REPLACE INTO messages VALUES((SELECT id FROM characters WHERE name = " + to_query(name) + "), " + to_query(msg_seq_id, msgid, str1, str2, str3, str4) + ", unix_timestamp())");
	}
}

void send_message(unsigned dbid, unsigned msgid, std::string const & str1, std::string const & str2, std::string const & str3, std::string const & str4)
{
	auto j = trickPacketHandler->connections_by_dbid.find(dbid);
	if (j != trickPacketHandler->connections_by_dbid.end())
	{

		unsigned long long msg_seq_id = ++trickPacketHandler->msg_seq_id;
		/*4*/
		trickPacketHandler->sendTo(j->second->session_id, Network::packet(0x05d2d8aa, (char)j->second->today_message_id, msg_seq_id, msgid, str1, str2, str3, str4));
		++j->second->today_message_id;
		TrickPacketHandler::message_data_t msg;
		msg.id = msgid;
		msg.seq_id = msg_seq_id;
		msg.str1 = str1;
		msg.str2 = str2;
		msg.str3 = str3;
		msg.str4 = str4;
		j->second->messages.push_back(std::move(msg));
		query("REPLACE INTO messages VALUES(" + to_query(dbid, msg_seq_id, msgid, str1, str2, str3, str4) + ", unix_timestamp())");
		return;
	}


	{
		unsigned long long msg_seq_id = ++trickPacketHandler->msg_seq_id;
		query("REPLACE INTO messages VALUES(" + to_query(dbid, msg_seq_id, msgid, str1, str2, str3, str4) + ", unix_timestamp())");
	}
}


void send_message2(std::string const & name, std::string const & str)
{
	send_message(name, 5, std::string(), std::string(), std::string(), str);
}

void send_message2(unsigned dbid, std::string const & str)
{
	send_message(dbid, 5, std::string(), std::string(), std::string(), str);
}

bool kick(std::string const & name)
{
	bool r = false;
	unsigned clientid = 0;
	if (auto * player = gamePacketHandler->get_client_by_name(name))
	{
		clientid = player->id;
		player->send(Network::packet(0x01c5d08a, 0));
		player->disconnect();

		logger << "Kicked " << name;
		r = true;
	}
	{
		auto i = guildPacketHandler->connections_by_name.find(name);
		if (i != guildPacketHandler->connections_by_name.end())
		{
			if (i->second->clientid)
				clientid = i->second->clientid;
			i->second->disconnect();
		}
	}
	{
		auto i = messengerPacketHandler->connections_by_name.find(name);
		if (i != messengerPacketHandler->connections_by_name.end())
		{
			if (i->second->clientid)
				clientid = i->second->clientid;
			i->second->disconnect();
		}
	}
	if (clientid != 0)
	{
		{
			auto i = shopPacketHandler->connections_by_id.find(clientid);
			if (i != shopPacketHandler->connections_by_id.find(clientid))
				i->second->disconnect();
		}
		{
			auto i = trickPacketHandler->connections_by_id.find(clientid);
			if (i != trickPacketHandler->connections_by_id.end())
				i->second->disconnect();
		}
	}
	return r;
}

bool getPacketFromCommands(std::vector<std::string> const & v, BinaryWriter & bw)
{
	bw.emplace<Network::packet_header_t>();
	bool b_error = false;
	unsigned long long value;
	for (unsigned i = 1; i < v.size() && !b_error; ++i)
	{
		value = 0;
		unsigned s = v[i].size();
		if (s > 0 && v[i][0] == '"')
		{
			if (s == 1 || v[i].back() != '"')
				b_error = true;
			else
			{
				bw << s - 2;
				for (unsigned j = 1; j < s - 1; ++j)
					bw << v[i][j];
			}
		}
		else if (s > 0 && v[i][0] == 'F')
		{
			float f1 = 0.0f;
			sscanf(&v[i].c_str()[1], "%f", &f1);
			bw << f1;
		}
		else if (s > 1 && v[i][0] == 'L' && v[i][1] == 'F')
		{
			double f1 = 0.0;
			sscanf(&v[i].c_str()[2], "%lf", &f1);
			bw << f1;
		}
		else if (s > 0 && (v[i][0] == 'q' || v[i][0] == 'l'))
		{
			long long value = 0;
			sscanf(&v[i].c_str()[1], "%lld", &value);
			bw << value;
		}
		else if (s > 0 && (/*v[i][0] == 'd' || */v[i][0] == 'i'))
		{
			int value = 0;
			sscanf(&v[i].c_str()[1], "%d", &value);
			bw << value;
		}
		else if (s > 0 && (v[i][0] == 'w' || v[i][0] == 's'))
		{
			int value = 0;
			sscanf(&v[i].c_str()[1], "%d", &value);
			bw << (short)value;
		}
		/*
		else if (s > 0 && (v[i][0] == 'b' || v[i][0] == 'c'))
		{
		int value = 0;
		sscanf(&v[i].c_str()[1], "%d", &value);
		bw << (signed char)value;
		}*/
		else if (s > 0 && v[i][0] == '\'')
		{
			int len = 0;
			sscanf(&v[i].c_str()[1], "%d", &len);
			if (len >= 65536 || len < 1)
				b_error = true;
			else
			{
				unsigned j = 1;
				for (; j < s; ++j)
					if (v[i][j] == 32)
						break;
				++j;
				if (j < s)
				{
					len += j;
					for (; j < s - 1 && j < (unsigned)len; ++j)
						bw << v[i][j];
					for (; j < (unsigned)len; ++j)
						bw << (char)0;
				}
				else
				{
					for (j = 0; j < (unsigned)len; ++j)
						bw << (char)0;
				}
			}
		}
		else if (s <= 8)
		{
			if ((s % 2) == 0 && s > 0)
			{
				sscanf(v[i].c_str(), "%llx", &value);
				switch (s / 2)
				{
				case 0:
				case 1:
					bw << (unsigned char)value;
					break;
				case 2:
					bw << (unsigned short)value;
					break;
				case 4:
					bw << (unsigned int)value;
					break;
				case 8:
					bw << value;
					break;
				}
			}
			else
				b_error = true;
		}
		else
			b_error = true;
	}


	return !b_error;
}

void announce(std::string const & str)
{
	if (guildPacketHandler)
	{
		//gamePacketHandler->distribute(Network::packet(0x0bffbd2a, 0, str, 0));
		guildPacketHandler->distribute(Network::packet(0x02bda09a, "", "System", str));
	}
	logger << "[announcement] " << str;
}

void announce_cafe(std::string const & str)
{
	if (guildPacketHandler)
		guildPacketHandler->distribute(Network::packet(0x06d190da, "System", str));
	logger << "[announcement] " << str;
}

void announce_guild(std::string const & str)
{
	if (guildPacketHandler)
		guildPacketHandler->distribute(Network::packet(0x0cb4863a, "System", str));
	logger << "[announcement] " << str;
}

void announce_whisper(std::string const & str)
{
	if (guildPacketHandler)
		guildPacketHandler->distribute(Network::packet(0x0d68a8aa, "System", str));
	logger << "[announcement] " << str;
}

void send_whisper(unsigned dbid, std::string const & name, std::string const & msg)
{
	auto k2 = guildPacketHandler->connections_by_dbid.find(dbid);
	if (k2 != guildPacketHandler->connections_by_dbid.end())
		guildPacketHandler->sendTo(k2->second->session_id, Network::packet(0x0d68a8aa, name, msg));
}



std::string input_str;

void handle_input(std::atomic<unsigned> & running)
{
	if (logger.handle_input(input_str))
	{
		std::vector<std::string> v = split2(input_str, ' ');
		if (v.size() > 0)
		{
			if (v[0] == "notice")
				announce(trim(input_str.substr(6)));
			else if (v[0] == "announce")
				announce(trim(input_str.substr(8)));
			else if ((v[0] == "maintance" || v[0] == "maintentance") && v.size() > 2)
			{
				unsigned down_mins = 0;
				unsigned expect_mins = 0;
				sscanf(v[1].c_str(), "%u", &down_mins);
				sscanf(v[2].c_str(), "%u", &expect_mins);
				if (down_mins > 0 && expect_mins > 0)
				{
					announce("Server is going down for maintenance in " + std::to_string(down_mins) + " minutes. Expected downtime is " + std::to_string(expect_mins) + " minutes");
					timed_shutdown = 1;
					shutdown_time = get_tick() + down_mins * 1000 * 60;
					show_chat = true;
				}
				else
					logger << "Wrong parameters";
			}
			else if (v[0] == "show_chat" && v.size() > 1)
			{
				unsigned sc = 0;
				sscanf(v[1].c_str(), "%u", &sc);
				show_chat = sc;
			}
			else if (v[0] == "sha_license")
			{
				logger << "License for sha256.cpp and sha256.h:";
				std::string old_prefix = logger.reset_prefix();
				logger << sha_license;
				logger.set_prefix(std::move(old_prefix));
			}
			else if (v[0] == "kick")
			{
				if (v.size() > 1 && gamePacketHandler)
					gamePacketHandler->dispatch(Network::packet(0, v[1]));
			}
			else if (v[0] == "ban")
			{
				if (v.size() > 1 && gamePacketHandler)
					gamePacketHandler->dispatch(Network::packet(1, v[1]));
			}
			else if (v[0] == "create_account")
			{
				if (v.size() > 4)
				{
					query<std::tuple<int> >("SELECT register(" + to_query(v[1], v[2], v[3], v[4]) + ")", [](std::vector<std::tuple<int> > const & data, bool success)
					{
						if (!success)
							logger << "Call to register failed";
						else
						{
							if (data.size() < 1)
							{
								logger << "Call error";
							}
							else
							{
								int r = std::get<0>(data[0]);
								switch (r)
								{
								case -1:
									logger << r << ": Account or character name already in use";
									break;
								case -2:
								case -6:
									logger << r << ": String length error";
									break;
								case 1:
									logger << r << ": Successfully registered new account";
									break;
								case -3:
									logger << r << ": Invalid character(s) in string";
									break;
								default:
									logger << r << ": Unspecified error";
								}
							}
						}
					});
				}
				else
					logger << "Usage: register_account [account_name] [password] [character_name] [email]";
			}
			else if (v[0] == "get_config")
			{
				if (v.size() > 1)
					logger << v[1] << " = " << get_config(v[1]);
			}
			else if (v[0] == "set_config")
			{
				if (v.size() > 2)
				{
					set_config(v[1], v[2]);
					logger << v[1] << " = " << get_config(v[1]);
				}
			}
			else if (v[0] == "reload_event_config")
			{
				init_event();
			}
			else if (v[0] == "cls")
			{
				logger.cls();
			}
			else if (v[0] == "t")
			{
				if (v.size() > 2)
				{
					unsigned a = convert<unsigned>(v[1]);
					if (a < tv.size())
					{
						tv[a] = convert<int>(v[2]);
						logger << "tv[" << a << "] = " << tv[a];
					}
				}

			}
			else if (v[0] == "print_t")
			{
				if (v.size() > 1)
				{
					unsigned a = convert<unsigned>(v[1]);
					if (a < tv.size())
						logger << "tv[" << a << "] = " << tv[a];
				}
			}
			else if (v[0] == "ts")
			{
				if (v.size() > 2)
				{
					unsigned a = convert<unsigned>(v[1]);
					if (a < ts.size())
					{
						ts[a] = v[2];
						logger << "ts[" << a << "] = \"" << ts[a] << '"';
					}
				}
				else if (v.size() > 1)
				{
					unsigned a = convert<unsigned>(v[1]);
					if (a < ts.size())
					{
						ts[a].clear();
						logger << "ts[" << a << "] = \"" << ts[a] << '"';
					}
				}
			}
			else if (v[0] == "print_ts")
			{
				if (v.size() > 1)
				{
					unsigned a = convert<unsigned>(v[1]);
					if (a < ts.size())
						logger << "ts[" << a << "] = \"" << ts[a] << '"';
				}
			}
			else if (v[0] == "tsx")
			{
				if (v.size() > 2)
				{
					unsigned a = convert<unsigned>(v[1]);
					if (a < ts.size())
					{
						std::string & str = ts[a];
						unsigned int v1;
						str.clear();
						str.reserve(v.size() - 2);
						for (unsigned i = 2; i < v.size(); ++i)
						{
							v1 = 0;
							sscanf(v[i].c_str(), "%x", &v1);
							str.push_back(v1);
						}
						logger << "ts[" << a << "] = \"" << ts[a] << '"';
					}
				}
			}
			else if (v[0] == "tx")
			{
				if (v.size() > 2)
				{
					unsigned a = convert<unsigned>(v[1]);
					if (a < tv.size())
					{
						tv[a] = 0;
						sscanf(v[2].c_str(), "%x", &tv[a]);
						logger << "tv[" << a << "] = " << tv[a];
					}
				}
			}
			else if (v[0] == "tf")
			{
				if (v.size() > 2)
				{
					unsigned a = convert<unsigned>(v[1]);
					if (a < tv.size())
					{
						tv[a] = 0;
						sscanf(v[2].c_str(), "%f", (float*)&tv[a]);
						logger << "tv[" << a << "] = " << *(float*)&tv[a];
					}
				}
			}
			else if (v[0] == "shop" || v[0] == "tis" || v[0] == "game" || v[0] == "guild" || v[0] == "msn" || v[0] == "room" || v[0] == "relay")
			{
				//send test packet
				if (v.size() > 1)
				{
					BinaryWriter bw;

					if (!getPacketFromCommands(v, bw))
						logger << "Incorrect parameters\r\n";
					else
					{
						bw.at<unsigned>(0) = bw.data.size() - 4;
						Network::dump(bw.data);
						if (v[0] == "shop")
							shopPacketHandler->distribute(std::move(bw.data));
						else if (v[0] == "tis")
							trickPacketHandler->distribute(std::move(bw.data));
						else if (v[0] == "game")
							gamePacketHandler->distribute(std::move(bw.data));
						else if (v[0] == "guild")
							guildPacketHandler->distribute(std::move(bw.data));
						else if (v[0] == "room")
							roomPacketHandler->distribute(std::move(bw.data));
						else if (v[0] == "msn")
							messengerPacketHandler->distribute(std::move(bw.data));
						else if (v[0] == "relay")
							relayPacketHandler->distribute(std::move(bw.data));

						logger.print("Sent\r\n");
					}

				}
			}
			if (v[0] == "exit")
				running = 0;
		}
	}
}

unsigned last_gameip_update_time = 0;
bool game_ip_need_update = false;
unsigned last_game_ip = 0;
void update_game_ip()
{
	last_gameip_update_time = get_tick();
	std::string ip_str = trim(get_config("game_ip"));
	game_ip = Network::string2ipv4(ip_str);
	game_ip_need_update = ip_str != Network::ipv42string(game_ip);
	if (game_ip != last_game_ip)
	{
		last_game_ip = game_ip;
		logger << "Game ip changed: " << get_config("game_ip") << '(' << Network::ipv42string(game_ip) << ')';
	}
}

std::atomic<unsigned> running(1);

int pp_main(int argc, char * argv[])
{
#ifdef _WIN32
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	sh_tick = get_tick();

//	assert(mapids_battle.size() > 0);
//	assert(mapids_coin.size() > 0);
//	assert(mapids.size() > 0);

	if (argc > 1 && strcmp(argv[1], "crashed") == 0)
	{
		logger << "Restarted after crash\r\n";
	}


	database_error_reporter = [](std::string const & msg)
	{
		report("Database", "NONE", msg, 0);
		syslog_error("Database: " + msg);
		db_error_count.add();
	};

	file_io_error_reporter = [](std::string const & msg, file_io_error_kind_t errkind)
	{
		if (errkind != file_io_error_kind_t::OPEN_READ)
		{
			report("File_io", "NONE", msg, 0);
			syslog_error("File_io: " + msg);
			file_io_error_count.add();
		}
	};


	memset(&recipient_offline_text[0], 0, recipient_offline_text.size());
	strcpy(&recipient_offline_text[0], "Recipient is offline");

//	for (unsigned mapid : mapids)
//		max_mapid = std::max<unsigned>(max_mapid, mapid);

	logger.print("ProjectSnowflake %d.%d\r\n", version / 100, version % 100);

	try
	{
		if (FILE * fchat = fopen("chat.log", "rb"))
		{
			if (FILE * fchat_old = fopen("chat_old.log", "a+b"))
			{
				fseek(fchat, 0, SEEK_END);
				unsigned filesize = ftell(fchat);
				fseek(fchat, 0, SEEK_SET);
				std::unique_ptr<std::array<char, 4096> > puffer(cnew std::array<char, 4096>());
				int r, r2;
				for (unsigned i = 0; i < filesize; )
				{
					unsigned count = std::min<unsigned>(4096, filesize - i);
					r = fread(&(*puffer)[0], 1, count, fchat);
					if (r <= 0)
					{
						std::cerr << "Failed to read chat.log\r\n";
						sleep_for(1000);
						continue;
					}

					for (int j = 0; j < r; )
					{
						r2 = fwrite(&(*puffer)[j], 1, r - j, fchat_old);
						if (r2 <= 0)
						{
							std::cerr << "Failed to write chat_old.log\r\n";
							sleep_for(1000);
							continue;
						}
						j += r2;
					}
					i += (unsigned)r;
				}
				fclose(fchat_old);
			}
			fclose(fchat);
		}
		if (FILE * fchat = fopen("chat.log", "w"))
			fclose(fchat);

	}
	LOG_CATCH();

	try
	{
		if (FILE * f1 = fopen("rules.txt", "rb"))
		{
			fseek(f1, 0, SEEK_END);
			unsigned filesize = ftell(f1);
			fseek(f1, 0, SEEK_SET);
			if (filesize > 0)
			{
				std::string data;
				data.resize(filesize);
				if (fread(&data[0], filesize, 1, f1) < 1)
				{
					logger << "Failed to read rules.txt" << std::endl;
				}

				std::vector<std::string> lines = split(data, 13);
				rules_text.clear();
				unsigned i = 0;
				for (std::string const & _line : lines)
				{
					std::string line = trim(_line);
					if (line.size() > 0)
					{
						++i;
						rules_text.emplace_back(std::to_string(i), line);
					}
				}
				logger << "Rules have been loaded from rules.txt: " << i << std::endl;
			}
			fclose(f1);
		}
	}
	LOG_CATCH();

	try
	{
		if (FILE * f1 = fopen("online_count.dat", "rb"))
		{
			fseek(f1, 0, SEEK_END);
			unsigned filesize = ftell(f1);
			fseek(f1, 0, SEEK_SET);
			if (filesize > 0)
			{
				std::vector<char> data;
				data.resize(filesize);
				if (fread(&data[0], filesize, 1, f1) < 1)
				{
					logger << "Failed to read online_count.dat" << std::endl;
				}
				BinaryReader br(data);
				int version = 0;
				br >> version;
				if (version == 1)
				{
					br >> online_count_list;
				}
				logger << "Loaded " << online_count_list.size() << " online_count entires";
			}
			fclose(f1);
		}
	}
	LOG_CATCH();

	try
	{
		std::vector<std::vector<std::string> > itemlist_data = from_csv(open_file("itemlist.csv"));
		itemlist.reserve(itemlist_data.size());
		{
			itemlist.emplace_back();
			for (unsigned i = 0; i < itemlist_data.size(); ++i)
			{
				try
				{
					item_list_data_t it(itemlist_data[i]);
					itemlist.emplace_back(std::move(it));
				}
				catch (std::out_of_range &)
				{
				}
				LOG_CATCH();
			}
		}
	}
	LOG_CATCH();

	init_database();
	std::shared_ptr<Network::ServiceThread> db_thread, file_io_thread;
	try
	{
		init_platform_specific("ProjectSnowflake");
		init_config(argc, argv);
		logger.set_prefix("[main ] ");
		Network::init();

		last_game_ip = Network::string2ipv4(trim(get_config("game_ip")));

		set_config_handler("exp_mult", [](std::string const & key, std::string const & value) {exp_mult = convert<float>(value); logger.print("Exp rate: %fx\r\n", exp_mult); }, true);
		set_config_handler("pro_mult", [](std::string const & key, std::string const & value) {pro_mult = convert<float>(value); logger.print("Pro rate: %fx\r\n", pro_mult); }, true);
		set_config_handler("enable_all_tricks", [](std::string const & key, std::string const & value) {enable_all_tricks = convert<unsigned>(value); }, true);
		set_config_handler("max_db_query_count", [](std::string const & key, std::string const & value) {db_query_limiter.set_max(convert<unsigned>(value)); }, true);
		set_config_handler("max_file_io_count", [](std::string const & key, std::string const & value) {file_io_limiter.set_max(convert<unsigned>(value)); }, true);
		set_config_handler("game_ip", [](std::string const & key, std::string const & value) { update_game_ip(); }, true);

		set_config_handler("log_network_events", [](std::string const & key, std::string const & value) {Network::log_network_events = convert<unsigned>(value); }, true);

		set_config_handler("log_inpacket_ids", [](std::string const & key, std::string const & value) {log_inpacket_ids = convert<unsigned>(value); }, true);
		set_config_handler("log_inpackets", [](std::string const & key, std::string const & value) {log_inpackets = convert<unsigned>(value); }, true);
		//		set_config_handler("log_outpacket_ids", [](std::string const & key, std::string const & value) {log_outpacket_ids = convert<unsigned>(value); }, true);
		//		set_config_handler("log_outpackets", [](std::string const & key, std::string const & value) {log_outpackets = convert<unsigned>(value); }, true);

		set_config_handler("send_timeout", [](std::string const & key, std::string const & value) {Network::send_timeout = convert<unsigned>(value); }, true);

		set_config_handler("use_launcher", [](std::string const & key, std::string const & value) {use_launcher = convert<unsigned>(value); }, true);
		set_config_handler("check_version_string", [](std::string const & key, std::string const & value) {check_version_string = convert<unsigned>(value); }, true);
		set_config_handler("runtime_treshold", [](std::string const & key, std::string const & value) {runtime_treshold = convert<float>(value); }, true);
		set_config_handler("min_runtime", [](std::string const & key, std::string const & value) {min_runtime = convert<float>(value); }, true);
		set_config_handler("tangent_treshold", [](std::string const & key, std::string const & value) {tangent_treshold = convert<float>(value); }, true);
		set_config_handler("spike_detection_treshold", [](std::string const & key, std::string const & value) {spike_detection_treshold = convert<float>(value) * 1000.0f; }, true);
		set_config_handler("spike_score_min", [](std::string const & key, std::string const & value) {spike_score_min = convert<unsigned>(value); }, true);
		set_config_handler("report_spikes", [](std::string const & key, std::string const & value) {report_spikes = convert<unsigned>(value); }, true);
		set_config_handler("min_spike_ms", [](std::string const & key, std::string const & value) {min_spike_ms = convert<unsigned>(value); }, true);
		set_config_handler("min_spike_report", [](std::string const & key, std::string const & value) {min_spike_report = convert<unsigned>(value); }, true);


		set_config_handler("race_result_timeout", [](std::string const & key, std::string const & value) {race_result_timeout = convert<unsigned>(value) * 1000; }, true);
		set_config_handler("relay_port", [](std::string const & key, std::string const & value) {relay_port = convert<unsigned short>(value); }, true);
		set_config_handler("return_gems", [](std::string const & key, std::string const & value) {return_gems = convert<unsigned>(value); }, true);
		set_config_handler("query_time_warning", [](std::string const & key, std::string const & value) {query_time_warning = std::max<float>(0.01f, convert<float>(value)); }, true);
		set_config_handler("daily_cash", [](std::string const & key, std::string const & value) {daily_cash = (unsigned)std::max<int>(0, convert<int>(value)); }, true);
		set_config_handler("season_daycount", [](std::string const & key, std::string const & value) {season_daycount = std::max<float>(0, convert<float>(value)); }, true);

		set_config_handler("pw_room_on_kick", [](std::string const & key, std::string const & value) {pw_room_on_kick = (unsigned)convert<int>(value); }, true);

		set_config_handler("drop_event", [](std::string const & key, std::string const & value) {drop_event = convert<unsigned>(value); }, true);


		set_config_handler("map_names", [](std::string const & key, std::string const & value)
		{
			impl_lock_guard(l, mapids_mutex);
			map_id_to_name.clear();
			std::vector<std::string> d = split(value, ',');
			//for (std::string const & str1 : d)
			for(unsigned i = 0; i < d.size(); i += 2)
			{
				std::string str_id = trim(d[i]);
				std::string str_name = trim(d[i + 1]);
				if (str_id.size() > 0)
				{
					try
					{
						map_id_to_name[std::stoi(str_id)] = std::move(str_name);
					}
					LOG_CATCH();
				}
			}
		}, true);

		set_config_handler("mapids", [](std::string const & key, std::string const & value)
		{
			impl_lock_guard(l, mapids_mutex);
			mapids.clear();
			std::vector<std::string> d = split(value, ',');
			for (std::string const & str1 : d)
			{
				std::string str = trim(str1);
				if (str.size() > 0)
				{
					try
					{
						mapids.push_back(std::stoi(str));
					}
					LOG_CATCH();
				}
			}
		}, true);

		set_config_handler("mapids_battle", [](std::string const & key, std::string const & value)
		{
			impl_lock_guard(l, mapids_mutex);
			mapids_battle.clear();
			std::vector<std::string> d = split(value, ',');
			for (std::string const & str1 : d)
			{
				std::string str = trim(str1);
				if (str.size() > 0)
				{
					try
					{
						mapids_battle.push_back(std::stoi(str));
					}
					LOG_CATCH();
				}
			}
		}, true);

		set_config_handler("mapids_coin", [](std::string const & key, std::string const & value)
		{
			impl_lock_guard(l, mapids_mutex);
			mapids_coin.clear();
			std::vector<std::string> d = split(value, ',');
			for (std::string const & str1 : d)
			{
				std::string str = trim(str1);
				if (str.size() > 0)
				{
					try
					{
						mapids_coin.push_back(std::stoi(str));
					}
					LOG_CATCH();
				}
			}
		}, true);

		set_config_handler("client_version_string", [](std::string const & key, std::string const & value)
		{
			impl_lock_guard(lock, version_string_mutex);
			client_version_string = value;
		}, true);

//		set_config_handler("records_filename", [](std::string const & key, std::string const & value)
//		{
//			impl_lock_guard(lock, version_string_mutex);
//			records_filename = value;
//		}, true);

		set_config_handler("html_dir", [](std::string const & key, std::string const & value)
		{
			impl_lock_guard(lock, version_string_mutex);
			html_dir = value;
			while (html_dir.size())
			{
				if (html_dir.back() == '\\' || html_dir.back() == '/')
					html_dir.pop_back();
				else
					break;
			}
		}, true);

		//{ "use_launcher", "0"},
		//{ "check_version_string", "1" },
		//{ "client_version_string", "test_string" },

		config_to_global2(game_ip_update_interval);


		game_port = get_config<unsigned>("game_port");
		//use_ip_as_id = get_config<unsigned>("use_ip_as_id") != 0;

		config_to_global2(private_port_to_public);
		config_to_global2(null_ips);
		config_to_global2(rs_mode);
		config_to_global2(disable_relay);
		config_to_global2(disable_relay_2);
		config_to_global2(log_relay);

		init_event();
		
//		do
//		{
//			std::string _filename;
//			{
//				impl_lock_guard(lock, version_string_mutex);
//				_filename = records_filename;
//			}

//			records_file = fopen(_filename.c_str(), "a+b");
//			if (!records_file)
//			{
//				logger << LIGHT_RED << "Failed to open records.csv";
//				sleep_for(10);
//				handle_input(running);
//			}
//		} while (records_file == nullptr && running);
		
		if (!running)
			return 0;

		logger << "Game ip: " << get_config("game_ip") << '(' << Network::ipv42string(game_ip) << ')';

		file_io_thread = start_file_io();
		if (!file_io_thread)
		{
			logger.print("Failed to start file_io thread\r\n");
			return -1;
		}

		db_thread = start_db();
		if (!db_thread)
		{
			logger.print("Failed to start database thread\r\n");
			deinit_database();
			return -1;
		}

		while (!mysql_state)
			sleep_for(100);

		if (mysql_state && running)
		{
			std::atomic<unsigned> _flag(0);

			query("UPDATE characters SET is_online = 0");
			query("REPLACE INTO online_player_count VALUES(0, 0, NOW())");
			query<std::tuple<long long> >("select ifnull(max(id), 0) from run;", [&_flag/*, &running*/](std::vector<std::tuple<long long> > const & data, bool success)
			{
				if (success && data.size() > 0)
				{
					max_run_id = std::get<0>(data[0]);
				}
				else
				{
					logger << LIGHT_RED << "Failed to get max id from run table";
					running = false;
				}
				_flag |= 2;
			});



			query<std::tuple<unsigned long long> >("SELECT max(season_id) from seasons;", [&_flag/*, &running*/](std::vector<std::tuple<unsigned long long> > const & data, bool success)
			{
				if (success)
				{
					if (data.size() > 0 && std::get<0>(data[0]) > 0)
					{
						current_season_id = std::get<0>(data[0]);
						_flag |= 1;
					}
					else
					{
						query<std::tuple<unsigned long long> >("select start_season(unix_timestamp(date(now())))", [&_flag/*, &running*/](std::vector<std::tuple<unsigned long long> > const & data, bool success)
						{
							if (success && data.size() > 0 && std::get<0>(data[0]) > 0)
							{
								current_season_id = std::get<0>(data[0]);
								logger << YELLOW << "Season switch ok, current season id is: " << current_season_id << std::endl;
								_flag |= 1;
								return;
							}
							running = false;
							_flag |= 1;//2;
							logger << LIGHT_RED << "FAILURE at switch season, current season id is: " << current_season_id << std::endl;
							report("SYSTEM", "Season switch", "Failure at executing start_season(" + std::to_string(current_season_id) + ")", 2);
						});
					}
				}
				else
				{
					running = false;
					_flag |= 1;
				}
			});


			while (_flag != 3)
				sleep_for(100);

			logger << YELLOW << "Current season id is: " << current_season_id << std::endl;

		}
		sinkPacketHandler = Network::start<SinkPacketHandler>("sink ", "", 2424, nullptr, false);
		if (!sinkPacketHandler)
		{
			logger.print("Failed to start server on port 2424\r\n");
			return -1;
		}

		std::shared_ptr<Network::ServiceThread> thread2 = std::make_shared<Network::ServiceThread>();

		ctpPacketHandlerSend = Network::start<CtpPacketHandler>("CTP S", 40000, nullptr, 40000);
		if (!ctpPacketHandlerSend)
		{
			logger.print("Failed to start server on port 40000\r\n");
			deinit_database();
			return -1;
		}

		ctpPacketHandlerRecv = Network::start<CtpPacketHandler>("CTP R", 40001, nullptr, 40001);
		if (!ctpPacketHandlerRecv)
		{
			logger.print("Failed to start server on port 40001\r\n");
			deinit_database();
			return -1;
		}

		ctpPacketHandlerSend->other = ctpPacketHandlerRecv.get();
		ctpPacketHandlerRecv->other = ctpPacketHandlerSend.get();

		authPacketHandler = Network::start<AuthPacketHandler>("auth ", "", 11011, nullptr, false);
		if (!authPacketHandler)
		{
			logger.print("Failed to start server on port 11011\r\n");
			deinit_database();
			return -1;
		}

		std::shared_ptr<Network::ServiceThread> thread1 = std::make_shared<Network::ServiceThread>();

		gamePacketHandler = Network::start<GamePacketHandler>("game ", "", get_config<unsigned>("game_port"), thread1, false);
		if (!gamePacketHandler)
		{
			logger.print("Failed to start server on port %u\r\n", get_config<unsigned>("game_port"));
			deinit_database();
			return -1;
		}

		relayPacketHandler = Network::start<RelayPacketHandler>("relay", "", get_config<unsigned>("relay_port"), nullptr, get_config<unsigned>("relay_nodelay") != 0);
		if (!relayPacketHandler)
		{
			logger.print("Failed to start server on port %u\r\n", get_config<unsigned>("relay_port"));
			return -1;
		}

		trickPacketHandler = Network::start<TrickPacketHandler>("trick", "", 10331, thread1, false);
		if (!trickPacketHandler)
		{
			logger.print("Failed to start server on port %u\r\n", 10331);
			deinit_database();
			return -1;
		}
		shopPacketHandler = Network::start<ShopPacketHandler>("shop ", "", 10486, thread1, false);
		if (!shopPacketHandler)
		{
			logger.print("Failed to start server on port %u\r\n", 10486);
			deinit_database();
			return -1;
		}
		//10231
		guildPacketHandler = Network::start<GuildPacketHandler>("guild", "", 10050, thread1, false);
		if (!guildPacketHandler)
		{
			logger.print("Failed to start server on port %u\r\n", 10050);
			return -1;
		}
		messengerPacketHandler = Network::start<MessengerPacketHandler>(" msn ", "", 10219, thread1, false);
		if (!messengerPacketHandler)
		{
			logger.print("Failed to start server on port %u\r\n", 10219);
			deinit_database();
			return -1;
		}
		roomPacketHandler = Network::start<RoomPacketHandler>("room ", "", 11901, thread1, false);
		if (!roomPacketHandler)
		{
			logger.print("Failed to start server on port %u\r\n", 11901);
			deinit_database();
			return -1;
		}

		while (guildPacketHandler->initialized != 3 && guildPacketHandler->initialized >= 0 && running)
		{
			sleep_for(10);
			handle_input(running);
		}
		while (trickPacketHandler->initialized != 7 && trickPacketHandler->initialized >= 0 && running)
		{
			sleep_for(10);
			handle_input(running);
		}

		if (guildPacketHandler->initialized < 0 || trickPacketHandler->initialized < 0)
			throw std::runtime_error("Initialization error");

	}
	catch (std::exception & e)
	{
		catch_message(e);
		ctpPacketHandlerSend.reset();
		ctpPacketHandlerRecv.reset();
		sinkPacketHandler.reset();
		authPacketHandler.reset();
		gamePacketHandler.reset();
		relayPacketHandler.reset();
		trickPacketHandler.reset();
		shopPacketHandler.reset();
		guildPacketHandler.reset();
		messengerPacketHandler.reset();
		roomPacketHandler.reset();

		deinit_database();
		return -1;
	}



	bool started = false;
	if (running)
	{
		register_counter(db_error_count);
		register_counter(db_queue_length);
		register_counter(file_io_error_count);
		register_counter(file_io_length);
		register_counter(login_count);
		register_counter(logout_count);
		register_counter(race_start_count);
		register_counter(race_end_count);
		register_counter(solo_race_count);
		register_counter(race_mode_count);
		register_counter(battle_mode_count);
		register_counter(coin_mode_count);
		register_counter(gem_acquire_count);
		register_counter(coins_collected);
		register_counter(boxes_collected);
		register_counter(trick_success);
		register_counter(trick_fail);
		register_counter(relay_pings);
		register_counter(replay_count);
		register_counter(online_count_s, true);

		started = true;
		sinkPacketHandler->open();
		authPacketHandler->open();
		gamePacketHandler->open();
		relayPacketHandler->open();
		trickPacketHandler->open();
		shopPacketHandler->open();
		guildPacketHandler->open();
		messengerPacketHandler->open();
		roomPacketHandler->open();

	}

	unsigned last_counter_log = get_tick();
	unsigned last_counter_tick = last_counter_log;

	while (running)
	{
		unsigned tick = get_tick();
		sh_tick = tick;
		if ((game_ip_need_update && tick - last_gameip_update_time > game_ip_update_interval * 60000) || game_ip == 0)
			update_game_ip();

		if (timed_shutdown)
		{
			if (tick - shutdown_time < 0x80000000)
				running = false;
		}

		sleep_for(10);
		handle_input(running);

		if (tick - last_counter_tick >= 1000)
		{
			last_counter_tick = tick;
			counter_tick();
		}

		if (tick - last_counter_log >= 60 * 1000)
		{
			last_counter_log = tick;
			log_counters();
		}

	}

	close_counters();

	if (started)
	{
		logger << "Closing...";
		flush_chatlog();
		sinkPacketHandler->close();
		authPacketHandler->close();
		gamePacketHandler->close();
		relayPacketHandler->close();
		trickPacketHandler->close();
		shopPacketHandler->close();
		guildPacketHandler->close();
		messengerPacketHandler->close();
		roomPacketHandler->close();

		query("UPDATE characters SET is_online = 0;");

		for (shopPacketHandler->save_all = 1; shopPacketHandler->save_all == 1;)
			sleep_for(10);

		authPacketHandler->disconnect_all = 1;
		gamePacketHandler->disconnect_all = 1;
		relayPacketHandler->disconnect_all = 1;
		trickPacketHandler->disconnect_all = 1;
		shopPacketHandler->disconnect_all = 1;
		guildPacketHandler->disconnect_all = 1;
		messengerPacketHandler->disconnect_all = 1;
		roomPacketHandler->disconnect_all = 1;

		logger << "Closing auth";
		while (authPacketHandler->disconnect_all != 0)
			sleep_for(10);
		logger << "Closing game";
		while (gamePacketHandler->disconnect_all != 0)
			sleep_for(10);
		logger << "Closing relay";
		while (relayPacketHandler->disconnect_all != 0)
			sleep_for(10);
		logger << "Closing trick";
		while (trickPacketHandler->disconnect_all != 0)
			sleep_for(10);
		logger << "Closing shop";
		while (shopPacketHandler->disconnect_all != 0)
			sleep_for(10);
		logger << "Closing guild";
		while (guildPacketHandler->disconnect_all != 0)
			sleep_for(10);
		logger << "Closing room";
		while (roomPacketHandler->disconnect_all != 0)
			sleep_for(10);

		logger << "All closed";
	}

//	if (records_file)
//	{
//		fclose(records_file);
//		records_file = nullptr;
//	}

	LOG_EX(ctpPacketHandlerSend.reset());
	LOG_EX(ctpPacketHandlerRecv.reset());
	LOG_EX(sinkPacketHandler.reset());
	LOG_EX(authPacketHandler.reset());
	LOG_EX(gamePacketHandler.reset());
	LOG_EX(relayPacketHandler.reset());
	LOG_EX(trickPacketHandler.reset());
	LOG_EX(shopPacketHandler.reset());
	LOG_EX(guildPacketHandler.reset());
	LOG_EX(messengerPacketHandler.reset());
	LOG_EX(roomPacketHandler.reset());

	if (mysql_state)
	{
		logger << "Updating rankings";
		query("call update_rankings()");

		{
			std::atomic<unsigned> f(0);
			query<std::tuple<> >("REPLACE INTO online_player_count VALUES(0, 0, 0)", [&f](std::vector<std::tuple<> > const & data, bool success)
			{
				f = 1;
			});

			while (f == 0)
				sleep_for(10);
		}
	}

	logger << "Closing database";

	db_thread->stop();
	file_io_thread->stop();

	if (fchat)
	{
		fflush(fchat);
		fclose(fchat);
		fchat = nullptr;
	}

	deinit_database();
	return 0;
}

void on_crash()
{
	if (fchat)
	{
		fflush(fchat);
		fclose(fchat);
		fchat = nullptr;
	}
}


#ifdef AS_DAEMON

int pidFilehandle = -1;

void signal_handler(int sig)
{
	switch (sig)
	{
	case SIGUSR1:
		announce("Server is going down for maintenance in 5 minutes.");
		timed_shutdown = 1;
		shutdown_time = sh_tick + 5 * 1000 * 60;
		show_chat = true;

		break;
	case SIGHUP:
		syslog(LOG_WARNING, "Received SIGHUP signal.");
		break;
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
		running = false;
		if (pidFilehandle != -1)
			close(pidFilehandle);
		break;
	case SIGILL:
	case SIGABRT:
	case SIGSEGV:
	case SIGPIPE:
		on_crash();
		syslog(LOG_ERR, "Receieved signal %s", strsignal(sig));
		if (pidFilehandle != -1)
			close(pidFilehandle);
		exit(EXIT_FAILURE);
		break;
	default:
		syslog(LOG_WARNING, "Unhandled signal %s", strsignal(sig));
		break;
	}
}

void daemonize(char *rundir, char *pidfile)
{
	int pid, sid, i;
	char str[10];
	struct sigaction newSigAction;
	sigset_t newSigSet;

	/* Check if parent process id is set */
	if (getppid() == 1)
	{
		/* PPID exists, therefore we are already a daemon */
		return;
	}

	/* Set signal mask - signals we want to block */
	sigemptyset(&newSigSet);
	sigaddset(&newSigSet, SIGCHLD);  /* ignore child - i.e. we don't need to wait for it */
	sigaddset(&newSigSet, SIGTSTP);  /* ignore Tty stop signals */
	sigaddset(&newSigSet, SIGTTOU);  /* ignore Tty background writes */
	sigaddset(&newSigSet, SIGTTIN);  /* ignore Tty background reads */
	sigprocmask(SIG_BLOCK, &newSigSet, NULL);   /* Block the above specified signals */

												/* Set up a signal handler */
	newSigAction.sa_handler = signal_handler;
	sigemptyset(&newSigAction.sa_mask);
	newSigAction.sa_flags = 0;

	/* Signals to handle */
	sigaction(SIGHUP, &newSigAction, NULL);     /* catch hangup signal */
	sigaction(SIGTERM, &newSigAction, NULL);    /* catch term signal */
	sigaction(SIGINT, &newSigAction, NULL);     /* catch interrupt signal */
	sigaction(SIGUSR1, &newSigAction, NULL);    /* catch interrupt signal */


												/* Fork*/
	pid = fork();

	if (pid < 0)
	{
		/* Could not fork */
		exit(EXIT_FAILURE);
	}

	if (pid > 0)
	{
		/* Child created ok, so exit parent process */
		//printf("Child process created: %d\n", pid);
		exit(EXIT_SUCCESS);
	}

	/* Child continues */

	umask(027); /* Set file permissions 750 */

				/* Get a new process group */
	sid = setsid();

	if (sid < 0)
	{
		exit(EXIT_FAILURE);
	}

	/* close all descriptors */
	for (i = getdtablesize(); i >= 0; --i)
	{
		close(i);
	}

	/* Route I/O connections */

	/* Open STDIN */
	i = open("/dev/null", O_RDWR);

	/* STDOUT */
	dup(i);

	/* STDERR */
	dup(i);

	chdir(rundir); /* change running directory */

				   /* Ensure only one copy */
	pidFilehandle = open(pidfile, O_RDWR | O_CREAT, 0600);

	if (pidFilehandle == -1)
	{
		/* Couldn't open lock file */
		syslog(LOG_INFO, "Could not open PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	/* Try to lock file */
	if (lockf(pidFilehandle, F_TLOCK, 0) == -1)
	{
		/* Couldn't get lock on lock file */
		syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}


	/* Get and format PID */
	sprintf(str, "%d\n", getpid());

	/* write pid to lockfile */
	write(pidFilehandle, str, strlen(str));
}
#endif

int main(int argc, char * argv[])
{
	int r = 0;
#ifdef AS_DAEMON
	setlogmask(LOG_UPTO(LOG_INFO));
	openlog("psbo", LOG_CONS | LOG_PERROR, LOG_USER);

//	syslog(LOG_INFO, "Psbo daemon starting up");

	/* Deamonize */
	daemonize("/var/PS/", "/tmp/psbo_daemon.pid");

	syslog(LOG_INFO, "Psbo daemon running");
#endif

	r = pp_main(argc, argv);
	logger << "Psbo stopped";

#ifdef AS_DAEMON
	syslog(LOG_NOTICE, "Psbo daemon terminated.");
	closelog();
#endif
	return r;
}


