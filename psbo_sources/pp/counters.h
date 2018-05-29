#pragma once
#include <string>
#include <atomic>
#include <vector>
#include <tuple>
#include <fstream>

class alignas(128) perf_counter_t
{
protected:
	void(*_onTick)(perf_counter_t *);
	std::string _name;
	bool do_reset;
	perf_counter_t(std::string const & name, void(*_onTick)(perf_counter_t *), bool do_reset)
		: _onTick(_onTick)
		, _name(name)
		, do_reset(do_reset)
	{}
public:

	virtual ~perf_counter_t() {}

	std::string const & name() const
	{
		return _name;
	}

	inline void onTick()
	{
		if (_onTick)
			_onTick(this);
	}

	virtual void add(int value = 1) = 0;
	virtual void add(unsigned value) = 0;
	virtual void add(long long value) = 0;
	virtual void add(unsigned long long value) = 0;
	virtual void add(float value) = 0;
	virtual void add(double value) = 0;

	virtual void add_data(std::ofstream & outfile, char separator) = 0;
	virtual void add_names(std::ofstream & outfile, char separator) = 0;

	virtual void v_reset() = 0;
};

template <class T>
class counter_t
	: public perf_counter_t
{
	std::atomic<T> count;
public:
	counter_t(std::string const & name, void(*onTick)(perf_counter_t *) = nullptr, bool do_reset = true)
		: perf_counter_t(name, onTick, do_reset)
		, count(0)
	{}
	virtual ~counter_t() {}

	virtual void add(int value = 1)
	{
		count += value;
	}

	virtual void add(unsigned value)
	{
		count += value;
	}

	virtual void add(long long value)
	{
		count += value;
	}

	virtual void add(unsigned long long value)
	{
		count += value;
	}

	virtual void add(float value)
	{
		count += value;
	}

	virtual void add(double value)
	{
		count += value;
	}

	T reset()
	{
		return count.exchange(0);
	}

	T get()
	{
		return count.load();
	}

	virtual void add_data(std::ofstream & outfile, char separator)
	{
		T value;
		if (do_reset)
			value = reset();
		else
			value = get();

		outfile << separator << value;
	}
	virtual void add_names(std::ofstream & outfile, char separator)
	{
		outfile << separator << this->_name;
	}

	virtual void v_reset()
	{
		reset();
	}

};


template <class T>
class sampler_t
	: public perf_counter_t
{
	mutex_t mutex;
	T min, max, sum;
	unsigned count;

	void _add(T value)
	{
		impl_lock_guard(lock, mutex);

		if (count != 0)
		{
			min = std::min<T>(min, value);
			max = std::max<T>(max, value);
			sum += value;
			++count;
		}
		else
		{
			count = 1;
			min = value;
			max = value;
			sum = value;
		}
	}

public:
	sampler_t(std::string const & name, void(*onTick)(perf_counter_t *) = nullptr, bool do_reset = true)
		: perf_counter_t(name, onTick, do_reset)
		, min(0)
		, max(0)
		, count(0)
	{}

	virtual ~sampler_t() {}

	virtual void add(int value = 1)
	{
		_add(value);
	}

	virtual void add(unsigned value)
	{
		_add(value);
	}

	virtual void add(long long value)
	{
		_add(value);
	}
	virtual void add(unsigned long long value)
	{
		_add(value);
	}

	virtual void add(float value)
	{
		_add(value);
	}
	virtual void add(double value)
	{
		_add(value);
	}

	std::tuple<T, T, double> reset()
	{
		std::tuple<T, T, double> r;
		unsigned count;
		{
			impl_lock_guard(lock, mutex);
			std::get<0>(r) = this->min;
			std::get<1>(r) = this->max;
			std::get<2>(r) = this->sum;
			count = this->count;
			this->count = 0;
		}

		if(count > 0)
			std::get<2>(r) /= count;
		else
		{
			std::get<0>(r) = 0;
			std::get<1>(r) = 0;
			std::get<2>(r) = 0;
		}

		return r;
	}

	virtual void v_reset()
	{
		reset();
	}

	std::tuple<T, T, double> get()
	{
		std::tuple<T, T, double> r;
		unsigned count;
		{
			impl_lock_guard(lock, mutex);
			std::get<0>(r) = this->min;
			std::get<1>(r) = this->max;
			std::get<2>(r) = this->sum;
			count = this->count;
		}

		if (count > 0)
			std::get<2>(r) /= count;
		else
		{
			std::get<0>(r) = 0;
			std::get<1>(r) = 0;
			std::get<2>(r) = 0;
		}

		return r;
	}

	virtual void add_data(std::ofstream & outfile, char separator)
	{
		std::tuple<T, T, double> r;
		if (do_reset)
			r = reset();
		else
			r = get();

		outfile << separator << std::get<0>(r) << separator << std::get<1>(r) << separator << std::get<2>(r);
	}
	virtual void add_names(std::ofstream & outfile, char separator)
	{
		outfile << separator << _name << " min" << separator << _name << " max" << separator << _name << " avg";
	}
};

void register_counter(perf_counter_t * counter, bool need_reset = false);
inline void register_counter(perf_counter_t & counter, bool need_reset = false)
{
	register_counter(&counter, need_reset);
}
void close_counters();
void log_counters();
void counter_tick();

