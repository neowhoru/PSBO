#include "stdafx.h"
#include "config.h"
#include <unordered_map>
#include <mutex>
#include "platform_specific.h"

bool const debug_build =
#ifdef _DEBUG
true
#else
false
#endif
;

namespace
{
	mutex_t config_mutex;
	std::unordered_map<std::string, std::string> config;
	std::unordered_map<std::string, void(*)(std::string const &, std::string const &)> handlers;
}

void _init_config()
{
	config = std::unordered_map<std::string, std::string>{

		{ "join_message", "Welcome" },
		{ "login_message", "Welcome" },

		{ "game_ip", "127.0.0.1" },
		{ "game_ip_update_interval", "5" },

		{ "game_port", "10001" },
		{ "relay_port", "10002" },
		{ "relay_nodelay", "0" },	//Nagle's algorithm
		{ "exp_mult", "1" },
		{ "pro_mult", "1" },
		{ "gem_mult", "1" },
		{ "gem_add", "0" },
		{ "return_gems", "1" },

		{ "daily_cash", "75"},

		{ "use_launcher", "1" },
		{ "check_version_string", "0" },
		{ "client_version_string", "2.0" },
		{ "runtime_treshold", "20" },	//speedhack timediff limit
		{ "min_runtime", "80"},
		{ "tangent_treshold", "5" },
		{ "spike_detection_treshold", "1"},
		{ "spike_score_min", "5000" },
		{ "report_spikes", "1" },
		{ "min_spike_ms", "350" },
		{ "min_spike_report", "2" },

		{ "send_timeout", "60000" },
		{ "race_result_timeout", "8" },

		{ "mysql_host", "localhost" },
		{ "mysql_user", "root" },
		{ "mysql_password", "" },
		{ "mysql_database", "snowflake" },
		{ "mysql_port", "0" },
		{ "max_db_query_count", "1000" },
		{ "max_file_io_count", "1000" },
		{ "query_time_warning", "1" },

		{ "enable_all_tricks", "0" },

		{ "log_network_events", "0" },
		{ "log_inpacket_ids", "0" },
		{ "log_inpackets", "0" },

		{ "season_daycount", "30" },

		{ "mapids", "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 32, 33, 34, 35"},
		{ "mapids_battle", "10, 0, 3, 7, 11, 8, 4, 5, 2"},
		{ "mapids_coin", "19, 21, 17, 22, 20, 18, 23"},
		{ "map_names", "0, \"Oblivion 1\",1, \"Eisen Watercourse 1\",2, \"Eisen Watercourse 2\",3, \"Oblivion 2\",4, \"Chachapoyas 1\",5, \"Santa Claus 1\",6, \"Jansen's Forest 1\",7, \"Smallpox 1\",8, \"Smallpox 2\",9, \"Equilibrium 1\",10, \"Chachapoyas 2\",11, \"Smallpox 3\",12, \"Jansen's Forest 2\",13, \"Eisen Watercourse 3\",14, \"Oblivion 3\",15, \"Chagall 1\",16, \"Chagall 2\",17, \"Smallpox 1 Mirror\",18, \"Santa Claus 1 Mirror\",19, \"Chagall 1 Mirror\",20, \"Chachapoyas 1 Mirror\",21, \"Oblivion 2 Mirror\",22, \"Smallpox 3 Mirror\",23, \"Smallpox 2 Mirror\",24, \"Chachapoyas 2 Mirror\",25, \"Chagall 2 Mirror\",26, \"Equilibrium 1 Mirror\",27, \"Jansen's Forest 1 Mirror\",28, \"Jansen's Forest 2 Mirror\",29, \"Eisen Watercourse 1 Mirror\",30, \"Eisen Watercourse 2 Mirror\",32, \"Chachapoyas 3\",33, \"Sand Madness\",34, \"Shangri - La\",35, \"Giant Ruin\"" },

		//following are for testing misc stuff
		{ "private_port_to_public", "0"},
		{ "null_ips", "0" },
		//{ "use_ip_as_id", "0" },
		{ "rs_mode", "0" },
		{ "disable_relay", "0" },
		{ "disable_relay_2", "0" },
		{ "log_relay", "0" },
		{ "pw_room_on_kick", "0"},

		{ "drop_event", "0" },

//		{ "records_filename", "C:\\xampp\\htdocs\\ranked\\records.csv" },
		{ "html_dir", "C:\\xampp\\htdocs\\reports" },

//		{ "service_count", "128" },
//		{ "in_packet_limit", "512" },
		{ "", "" },
	};
}

unsigned add_config_line(std::string const & str)
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
			config[left] = std::move(right);
			return 2;
		}
		return 0;
	}
	return line.size();
}

void _init_config2(std::vector<std::string> const & args)
{
	config.insert({ "argc", convert<std::string>(args.size()) });

	for (unsigned i = 0; i < args.size(); ++i)
	{
		config.insert({ convert<std::string>(i), args[i] });
		add_config_line(args[i]);
	}
}



void init_config(std::vector<std::string> const & args)
{
//	std::lock_guard<std::mutex> lock(config_mutex);
	impl_lock_guard(lock, config_mutex);
	_init_config();
	_init_config2(args);

	FILE * f1 = fopen("config.ini", "r+b");
	if (f1 != nullptr)
	{
		fseek(f1, 0, SEEK_END);
		long s = ftell(f1);
		fseek(f1, 0, SEEK_SET);
		std::string filedata;
		filedata.resize(s);

		if (fread(&filedata[0], s, 1, f1) == 1)
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

				unsigned line_size = add_config_line(lines[i].substr(0, l));
				if (line_size == 2)
					continue;
				if (line_size == 1 && trim(lines[i].substr(0, l)).size() == 0)
					continue;

				logger << LIGHT_RED << "Invalid config on line " << (i + 1) << ": " << lines[i].substr(0, l) << endl;
			}

		}
		else
		{
			logger << LIGHT_RED << "Failed to read config.ini";
			syslog_error("Failed to read config.ini");
		}

		fclose(f1);
	}
	else
	{
		logger << LIGHT_RED << "Failed to open config.ini";
		syslog_error("Failed to open config.ini");
	}
}


void init_config(int argc, char * argv[])
{
	std::vector<std::string> arr;
	arr.reserve(argc);
	for (int i = 0; i < argc; ++i)
		arr.push_back(argv[i]);
	init_config(arr);
}


std::string get_config(std::string const & key)
{
	{
		impl_lock_guard(lock, config_mutex);
		//std::lock_guard<std::mutex> lock(config_mutex);
		auto i = config.find(key);
		if (i != config.end())
			return i->second;
	}

	logger << YELLOW << "Config not found: " << key << endl;
	return std::string();
}

void set_config(std::string const & key, std::string const & value)
{
	void(*func)(std::string const &, std::string const &) = nullptr;
	{
		impl_lock_guard(lock, config_mutex);
		config[key] = value;
		auto i = handlers.find(key);
		if (i != handlers.end())
			func = i->second;
	}

	if (func) try
	{
		(*func)(key, value);
	}
	LOG_CATCH();
}

bool has_config(std::string const & key)
{
	impl_lock_guard(lock, config_mutex);
	auto i = config.find(key);
	return i != config.end();
}

void set_config_handler(std::string const & key, void(*func)(std::string const &, std::string const &), bool init_call)
{
	bool err = false;
	std::string value;
	{
		impl_lock_guard(lock, config_mutex);
		handlers[key] = func;
		auto i = config.find(key);
		if (i == config.end())
			err = true;
		else
			value = i->second;
	}

	if(err)
		logger << YELLOW << "Handler set for nonexisting config: " << key << endl;
	if (init_call)
		func(key, value);
}
