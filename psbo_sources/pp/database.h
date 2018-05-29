#pragma once
#include "network.h"
#include "error.h"
//#include "reflect.h"

void init_database();
void deinit_database();
void _deinit_db_thread();

//extern std::atomic<unsigned> mysql_started;
//extern std::atomic<unsigned> mysql_ok;
extern thread_local unsigned long long last_insert_id;
extern thread_local unsigned long long affected_rows;
extern queue_limiter_t db_query_limiter;
extern std::atomic<unsigned> mysql_state;
extern std::atomic<float> query_time_warning;
extern void(*database_error_reporter)(std::string const & msg);

std::shared_ptr<Network::ServiceThread> start_db();
std::string escape(const char * str, unsigned len);
inline std::string escape(std::string const & str)
{
	return escape(str.data(), str.size());
}

class result_handler_t;

bool query(std::string const & query, std::shared_ptr<result_handler_t> handler = nullptr);

template <class TT, class T>
std::shared_ptr<result_handler_t> query(std::string const & querytext, T handler);


class result_handler_t
	: public std::enable_shared_from_this<result_handler_t>
{
	friend bool query(std::string const & query, std::shared_ptr<result_handler_t> handler);

	template <class TT, class T>
	friend std::shared_ptr<result_handler_t> query(std::string const & querytext, T handler);
	static bool _execute_query(std::string const & query, std::shared_ptr<result_handler_t> handler, bool check_warnings);
	static void execute_query(std::string const & query, std::shared_ptr<result_handler_t> handler);
	unsigned retry = 5;
protected:
	volatile unsigned _need_sync = false;
	std::atomic<unsigned> _finished;
	static unsigned _call_result(std::shared_ptr<result_handler_t> result)
	{
		return result->call_result();
	}
	result_handler_t()
		: _finished(0)
	{}
	virtual void process_result() = 0;
public:
	inline bool need_sync() const
	{
		return _need_sync;
	}
	inline bool finished() const
	{
		return _finished != 0;
	}
	unsigned call_result()
	{
		unsigned _exp = 1;
		if (_finished.compare_exchange_weak(_exp, 2))
		{
			process_result();
			return 2;
		}
		return _exp;
	}
	virtual void _row_count(unsigned count, unsigned result_id) {}
	virtual void _set_insert_id(unsigned long long id, unsigned long long affected_rows, unsigned result_id) {}
	virtual void _add_result(const char * const * row, unsigned long const * lengths, unsigned count, unsigned result_id) = 0;
	virtual void _on_finished(bool success) = 0;
	virtual void _failed() {}
	virtual ~result_handler_t() {}
};

namespace detail
{
	template <class T>
	struct my
	{
		static inline T convert(const char * p, unsigned l)
		{
			T r;
			if (p)
			{
				//std::istringstream iss(std::string(p, l));
				//iss >> r;
				BinaryReader br(p, l);
				br >> r;
			}
			return r;
		}
	};
	template <>
	struct my<std::string>
	{
		static inline std::string convert(const char * p, unsigned l)
		{
			return p ? std::string(p, l) : std::string();
		}
	};
#define decl_my_convert(TYPE, MODE) \
	template <> \
	struct my< TYPE > \
	{ \
		static inline TYPE convert(const char * p, unsigned l) \
		{ \
			TYPE r = 0; \
			if (p) \
				sscanf(p, MODE, &r); \
			return r; \
		} \
	};


	decl_my_convert(unsigned char, "%hhu");
	decl_my_convert(signed char, "%hhd");
	decl_my_convert(unsigned short, "%hu");
	decl_my_convert(short, "%hd");
	decl_my_convert(unsigned, "%u");
	decl_my_convert(int, "%d");
	decl_my_convert(unsigned long, "%lu");
	decl_my_convert(long, "%ld");
	decl_my_convert(unsigned long long, "%llu");
	decl_my_convert(long long, "%lld");
	decl_my_convert(float, "%f");
	decl_my_convert(double, "%lf");
	decl_my_convert(long double, "%Lf");
#undef decl_my_convert

	template <>
	struct my< char >
	{
		static inline char convert(const char * p, unsigned l)
		{
			char r = 0;
			if (p)
				r = *p;
			return r;
		}
	};

	template <class T2>
	struct my< std::unique_ptr<T2> >
	{
		static inline std::unique_ptr<T2> convert(const char * p, unsigned l)
		{
			std::unique_ptr<T2> r;
			if (p)
				r = cnew T2(my<T2>::convert(p, l));
			return r;
		}
	};

	template <class T2>
	struct my< std::shared_ptr<T2> >
	{
		static inline std::shared_ptr<T2> convert(const char * p, unsigned l)
		{
			std::shared_ptr<T2> r;
			if (p)
				r = cnew T2(my<T2>::convert(p, l));
			return r;
		}
	};

	template <unsigned i, unsigned e, class T>
	struct get
	{
		static inline void data(T & t, const char * const * row, unsigned long const * lengths)
		{
			typedef typename std::tuple_element<i, T>::type ARG;
			std::get<i>(t) = my<ARG>::convert(row[i], lengths[i]);
			get<i + 1, e, T>::data(t, row, lengths);
		}
	};

	template <unsigned i, class T>
	struct get<i, i, T>
	{
		static inline void data(T & t, const char * const * row, unsigned long const * lengths)
		{
		}
	};


	template <class T, unsigned i = 0, unsigned e = T::memberCount>
	struct get_s
	{
		static inline void data(T & value, const char * const * row, unsigned long const * lengths)
		{
			value.*(typename T::template TypeData<i>::getPtr()) = my<typename T::template TypeData<i>::type>::convert(row[i], lengths[i]);
			get_s<T, i + 1, e>::data(value, row, lengths);
		}
	};

	template <class T, unsigned i>
	struct get_s<T, i, i>
	{
		static inline void data(T & t, const char * const * row, unsigned long const * lengths)
		{
		}
	};
};

template <class T, class TT>
class tuple_result_handler_t
	: public result_handler_t
{
	std::vector<TT> data;
	bool success = false;
	unsigned long long lid = 0;
	unsigned long long affected_rows = 0;

	virtual void _row_count(unsigned count, unsigned result_id)
	{
		if (result_id == 0)
			data.reserve(count);
	}

	virtual void _set_insert_id(unsigned long long id, unsigned long long affected_rows, unsigned result_id)
	{
		if (result_id == 0)
		{
			lid = id;
			this->affected_rows = affected_rows;
		}
	}

	virtual void _add_result(const char * const* row, unsigned long const * lengths, unsigned count, unsigned result_id)
	{
		if (result_id == 0)
		{
			assert(std::tuple_size<TT>::value <= count);
			TT t;
			detail::get<0, std::tuple_size<TT>::value, TT>::data(t, row, lengths);
			data.emplace_back(std::move(t));
		}
	}

	virtual void _on_finished(bool success)
	{
		this->success = success;
		this->_finished = 1;
		if (std::shared_ptr<Network::ServiceThread> thread = requester.lock())
			thread->io_service.post(std::bind(_call_result, shared_from_this()));
	}

	virtual void _failed()
	{
		try
		{
			handler(std::move(data), false);
		}
		LOG_CATCH();
	}

	T handler;
	std::weak_ptr<Network::ServiceThread> requester;

	virtual void process_result()
	{
		try
		{
			::last_insert_id = lid;
			::affected_rows = this->affected_rows;
			handler(std::move(data), success);
		}
		LOG_CATCH();
	}

public:
	tuple_result_handler_t(T && handler)
		: handler(std::move(handler))
	{
		if (std::shared_ptr<Network::ServiceThread> t = Network::currentServiceThread.lock())
			this->requester = t;
		else
			this->_need_sync = true;
	}
	virtual ~tuple_result_handler_t() {}
};

/*
template <class T, class TT>
class struct_result_handler_t
	: public result_handler_t
{
	std::vector<TT> data;
	bool success = false;
	unsigned long long lid = 0;
	unsigned long long affected_rows = 0;

	virtual void _row_count(unsigned count, unsigned result_id)
	{
		if (result_id == 0)
			data.reserve(count);
	}

	virtual void _set_insert_id(unsigned long long id, unsigned long long affected_rows, unsigned result_id)
	{
		if (result_id == 0)
		{
			lid = id;
			this->affected_rows = affected_rows;
		}
	}

	virtual void _add_result(const char * const* row, unsigned long const * lengths, unsigned count, unsigned result_id)
	{
		if (result_id == 0)
		{
			assert(TT::memberCount <= count);
			TT t;
			detail::get_s<TT>::data(t, row, lengths);
			data.emplace_back(std::move(t));
		}
	}

	virtual void _on_finished(bool success)
	{
		this->success = success;
		this->_finished = 1;
		if (std::shared_ptr<Network::ServiceThread> thread = requester.lock())
			thread->io_service.post(std::bind(_call_result, shared_from_this()));
	}

	virtual void _failed()
	{
		try
		{
			handler(std::move(data), false);
		}
		LOG_CATCH();
	}

	T handler;
	std::weak_ptr<Network::ServiceThread> requester;

	virtual void process_result()
	{
		try
		{
			::last_insert_id = lid;
			::affected_rows = this->affected_rows;
			handler(std::move(data), success);
		}
		LOG_CATCH();
	}

public:
	struct_result_handler_t(T && handler)
		: handler(std::move(handler))
	{
		if (std::shared_ptr<Network::ServiceThread> t = Network::currentServiceThread.lock())
			this->requester = t;
		else
			this->_need_sync = true;
	}
	virtual ~struct_result_handler_t() {}
};
*/
namespace detail
{
	template <class TT/*, bool is_refl = is_reflectable<TT>::value*/>
	struct get_handler
	{
		template <class T>
		static inline tuple_result_handler_t<T, TT> * get(T handler)
		{
			return cnew tuple_result_handler_t<T, TT>(std::move(handler));
		}
	};
/*
	template <class TT>
	struct get_handler<TT, true>
	{
		template <class T>
		static inline struct_result_handler_t<T, TT> * get(T handler)
		{
			return cnew struct_result_handler_t<T, TT>(std::move(handler));
		}
	};
*/
}

template <class TT, class T>
inline std::shared_ptr<result_handler_t> make_result_handler(T handler)
{
	//return std::shared_ptr<result_handler_t>(detail::template get_handler<TT>::get<T>(handler));
	return std::shared_ptr<result_handler_t>(cnew tuple_result_handler_t<T, TT>(std::move(handler)));
}

template <class TT, class T>
std::shared_ptr<result_handler_t> query(std::string const & querytext, T handler)
{
	std::shared_ptr<result_handler_t> rh = make_result_handler<TT>(std::move(handler));
	bool r = query(querytext, rh);
	if (r)
	{
		if (rh->need_sync())
		{
			while (rh->call_result() == 0)
				sleep_for(10);
		}
		return rh;
	}
	return std::shared_ptr<result_handler_t>();
}

namespace detail
{
	template <class T>
	struct to_query
	{
		static inline std::string helper(T const & data)
		{
			BinaryWriter bw;
			bw << data;
			return escape(bw.data.data(), bw.data.size());
		}
	};


#define decl_my_convert(TYPE, MODE) \
	template <> \
	struct to_query< TYPE > \
	{ \
		static inline std::string helper( TYPE data) \
		{ \
			char p[64]; \
			unsigned len = sprintf(p, "'" MODE "'", data); \
			return std::string(p, len); \
		} \
	};

	decl_my_convert(unsigned char, "%hhu");
	decl_my_convert(signed char, "%hhd");
	decl_my_convert(unsigned short, "%hu");
	decl_my_convert(short, "%hd");
	decl_my_convert(unsigned, "%u");
	decl_my_convert(int, "%d");
	decl_my_convert(unsigned long, "%lu");
	decl_my_convert(long, "%ld");
	decl_my_convert(unsigned long long, "%llu");
	decl_my_convert(long long, "%lld");
	decl_my_convert(float, "%f");
	decl_my_convert(double, "%lf");
	decl_my_convert(long double, "%Lf");

#undef decl_my_convert

	template <>
	struct to_query< char >
	{
		static inline std::string helper(char data)
		{
			return escape(&data, 1);
		}
	};

	template <unsigned l>
	struct to_query< char[l] >
	{
		static_assert(l > 0, "char array lenght is 0");
		static inline std::string helper(const char data[l])
		{
			return escape(&data[0], l - 1);
		}
	};

	template <>
	struct to_query< std::string >
	{
		static inline std::string helper(std::string const & data)
		{
			return escape(data.data(), data.size());
		}
	};

	template <>
	struct to_query< std::vector<char> >
	{
		static inline std::string helper(std::vector<char> const & data)
		{
			return escape(data.data(), data.size());
		}
	};

	template <class T>
	struct to_query< std::atomic<T> >
	{
		static inline std::string helper(std::atomic<T> const & data)
		{
			return to_query<T>::helper((T)data);
		}
	};

	template <unsigned i, unsigned e, class T>
	struct to_query_tuple
	{
		static inline void get(std::ostringstream & oss, T const & t)
		{
			if (i > 0)
				oss << ',';
			oss << to_query<typename std::tuple_element<i, T>::type>::helper(std::get<i>(t));
			to_query_tuple<i + 1, e, T>::get(oss, t);
		}
	};

	template <unsigned i, class T>
	struct to_query_tuple<i, i, T>
	{
		static inline void get(std::ostringstream & oss, T const & t)
		{

		}
	};


	template <typename ... ARGS>
	struct to_query< std::tuple<ARGS...> >
	{
		static inline std::string helper(std::tuple<ARGS...> const & data)
		{
			std::ostringstream oss;
			to_query_tuple<0, std::tuple_size<std::tuple<ARGS...> >::value, std::tuple<ARGS...> >::get(oss, data);
			return oss.str();
		}
	};

	template <typename ... ARGS>
	struct to_query2;

	template <typename ARG, typename ... ARGS>
	struct to_query2<ARG, ARGS...>
	{
		static inline std::string helper(ARG const & data, ARGS const & ... args)
		{
			std::ostringstream oss;
			oss << detail::to_query<ARG>::helper(data);
			to_query2<ARGS...>::helper2(oss, args...);
			return oss.str();
		}

		static inline void helper2(std::ostringstream & oss, ARG const & data, ARGS const & ... args)
		{
			oss << ',' << detail::to_query<ARG>::helper(data);
			to_query2<ARGS...>::helper2(oss, args...);
		}
	};

	template <typename ARG>
	struct to_query2<ARG>
	{
		static inline std::string helper(ARG const & data)
		{
			return detail::to_query<ARG>::helper(data);
		}

		static inline void helper2(std::ostringstream & oss, ARG const & data)
		{
			oss << ',' << detail::to_query<ARG>::helper(data);
		}
	};

	template <>
	struct to_query2<>
	{
		static inline std::string helper()
		{
			return std::string();
		}

		static inline void helper2(std::ostringstream & oss)
		{
		}
	};
}

template <typename ... ARGS>
inline std::string to_query(ARGS const & ... data)
{
	return detail::to_query2<typename std::remove_reference<ARGS>::type ...>::helper(data...);
}
