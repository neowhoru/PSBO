#include "stdafx.h"
#include "common.h"
#include <vector>
#include "counters.h"

namespace
{
	bool perf_counters_changed = true;
	std::vector<std::pair<perf_counter_t *, bool> > perf_counters;
	//FILE * counters_log = nullptr;
	std::ofstream outfile;
	const char separator = ';';
}

void register_counter(perf_counter_t * counter, bool need_reset)
{
	assert(counter != nullptr);
	if (counter == nullptr)
		return;
	try
	{
		unsigned i = 0;
		for (; i < perf_counters.size(); ++i)
			if (perf_counters[i].first == counter)
				return;

		perf_counters.push_back(std::make_pair(counter, need_reset));
		perf_counters_changed = true;
	}
	LOG_CATCH();
}

void close_counters()
{
	if (outfile.is_open())
		outfile.close();
}

void counter_tick()
{
	for (unsigned i = 0; i < perf_counters.size(); ++i)
		perf_counters[i].first->onTick();
}

void log_counters()
{
	try
	{
		if(!outfile.is_open())
		{
			std::string filename = "counters " + datetime2str(time(0), '-', '_') + ".csv";
			outfile.open(filename, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
		}
		if (outfile.is_open())
		{
			if (perf_counters_changed)
			{
				perf_counters_changed = false;

				outfile << "//";

				std::vector<std::string> names;
				names.reserve(perf_counters.size() * 3 + 1);
				names.emplace_back("//");

				for (unsigned i = 0; i < perf_counters.size(); ++i)
					perf_counters[i].first->add_names(outfile, separator);

				outfile << "\r\n";
			}

			outfile << datetime2str();

			for (unsigned i = 0; i < perf_counters.size(); ++i)
			{
				perf_counters[i].first->add_data(outfile, separator);
				if (perf_counters[i].second)
					perf_counters[i].first->v_reset();
			}

			outfile << "\r\n";
			outfile.flush();
		}
	}
	LOG_CATCH();
}

