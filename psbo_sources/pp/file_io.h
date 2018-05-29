#pragma once
#include "network.h"

class io_handler_t;

enum class file_io_error_kind_t : int {NONE, OPEN_READ, OPEN_WRITE, READ, WRITE, RENAME};

bool load_file(std::string const & filename, std::string const & sign, std::shared_ptr<io_handler_t> handler);
bool save_file(std::string const & filename, std::string const & sign, std::vector<char> && data, std::shared_ptr<io_handler_t> handler = nullptr);
bool rename_file(std::string const & source, std::string const & dest, std::shared_ptr<io_handler_t> handler);
std::shared_ptr<Network::ServiceThread> start_file_io();

extern void(*file_io_error_reporter)(std::string const & msg, file_io_error_kind_t);
extern queue_limiter_t file_io_limiter;

template <class T>
bool load_file(std::string const & filename, std::string const & sign, T handler);
template <class T>
bool save_file(std::string const & filename, std::string const & sign, std::vector<char> && data, T handler);
template <class T>
bool rename_file(std::string const & source, std::string const & dest, T handler);

class io_handler_t
	: public std::enable_shared_from_this<io_handler_t>
{
	template <class T>
	friend bool load_file(std::string const & filename, std::string const & sign, T handler);
	template <class T>
	friend bool save_file(std::string const & filename, std::string const & sign, std::vector<char> && data, T handler);
	template <class T>
	friend bool rename_file(std::string const & source, std::string const & dest, T handler);

	friend bool rename_file(std::string const & source, std::string const & dest, std::shared_ptr<io_handler_t> handler);

	friend bool load_file(std::string const & filename, std::string const & sign, std::shared_ptr<io_handler_t> handler);
	friend bool save_file(std::string const & filename, std::string const & sign, std::vector<char> && data, std::shared_ptr<io_handler_t> handler);
	static void open_read(std::string & filename, std::string const & sign, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread);
	static void read(std::string & filename, unsigned retry, FILE * f1, unsigned read_pos, unsigned filesize, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread);
	static void open_write(std::string & filename, std::string & sign, std::vector<char> & data, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread);
	static void write(std::string & filename, unsigned retry, FILE * f1, std::string & sign, unsigned write_pos, std::vector<char> & data, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread);
	static void write_finish(std::string & filename, unsigned retry, FILE * f1, std::string & sign, unsigned write_pos, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread);
	static void rename_file(std::string const & source, std::string const & dest, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread);
protected:
	std::vector<char> data;
	volatile unsigned _need_sync = false;
	std::atomic<unsigned> _finished;
	virtual void on_ready(bool success) = 0;
	virtual void process_result() = 0;
	io_handler_t()
		: _finished(false)
	{}

	static void call_result(std::shared_ptr<io_handler_t> result)
	{
		result->process_result();
	}

public:
	inline bool need_sync() const
	{
		return _need_sync;
	}
	inline bool finished() const
	{
		return _finished != 0;
	}

	virtual ~io_handler_t() {}
};

template <class T>
class io_handler_callback_t
	: public io_handler_t
{
	bool success;
	T handler;
	std::weak_ptr<Network::ServiceThread> requester;
	virtual void on_ready(bool success)
	{
		this->success = success;
		this->_finished = true;
		if (std::shared_ptr<Network::ServiceThread> thread = requester.lock())
			thread->io_service.post(std::bind(call_result, shared_from_this()));
	}

	virtual void process_result()
	{
		try
		{
			handler(std::move(this->data), success);
		}
		LOG_CATCH();
	}

public:
	io_handler_callback_t(T && handler)
		: handler(std::move(handler))
	{
		if (std::shared_ptr<Network::ServiceThread> t = Network::currentServiceThread.lock())
			this->requester = t;
		else
			this->_need_sync = true;
	}
	virtual ~io_handler_callback_t() {}
};

template <class T>
class io_handler_callback2_t
	: public io_handler_t
{
	bool success;
	T handler;
	std::weak_ptr<Network::ServiceThread> requester;
	virtual void on_ready(bool success)
	{
		this->success = success;
		this->_finished = true;
		if (std::shared_ptr<Network::ServiceThread> thread = requester.lock())
			thread->io_service.post(std::bind(call_result, shared_from_this()));
	}

	virtual void process_result()
	{
		try
		{
			handler(success);
		}
		LOG_CATCH();
	}

public:
	io_handler_callback2_t(T && handler)
		: handler(std::move(handler))
	{
		if (std::shared_ptr<Network::ServiceThread> t = Network::currentServiceThread.lock())
			this->requester = t;
		else
			this->_need_sync = true;
	}
	virtual ~io_handler_callback2_t() {}
};


template <class T>
bool load_file(std::string const & filename, std::string const & sign, T handler)
{
	std::shared_ptr<io_handler_t> rh = std::shared_ptr<io_handler_t>(cnew io_handler_callback_t<T>(std::move(handler)));
	bool r = load_file(filename, sign, rh);
	if (r && rh->need_sync())
	{
		while (!rh->finished())
			sleep_for(10);
		rh->call_result(rh);
	}
	return r;
}

template <class T>
bool save_file(std::string const & filename, std::string const & sign, std::vector<char> && data, T handler)
{
	std::shared_ptr<io_handler_t> rh = std::shared_ptr<io_handler_t>(cnew io_handler_callback_t<T>(std::move(handler)));
	bool r = save_file(filename, sign, std::move(data), rh);
	if (r && rh->need_sync())
	{
		while (!rh->finished())
			sleep_for(10);
		rh->call_result(rh);
	}
	return r;
}


template <class T>
bool rename_file(std::string const & source, std::string const & dest, T handler)
{
	std::shared_ptr<io_handler_t> rh = std::shared_ptr<io_handler_t>(cnew io_handler_callback2_t<T>(std::move(handler)));
	bool r = rename_file(source, dest, rh);
	if (r && rh->need_sync())
	{
		while (!rh->finished())
			sleep_for(10);
		rh->call_result(rh);
	}
	return r;
}
