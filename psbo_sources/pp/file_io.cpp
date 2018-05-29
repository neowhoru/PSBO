#include "stdafx.h"
#include "file_io.h"
#include "config.h"
#include "counters.h"
#include "network.h"
#ifndef _WIN32
#include <sys/stat.h>
#endif


namespace
{
	std::weak_ptr<Network::ServiceThread> file_io_thread;
	unsigned const max_read_chunk = 0x10000;
	unsigned const max_write_chunk = 0x10000;;
	unsigned const max_retry_count = 3;
}

//std::atomic<unsigned> file_io_count(0);
//std::atomic<unsigned> max_file_io_count(1000);
queue_limiter_t file_io_limiter(1000);

void(*file_io_error_reporter)(std::string const & msg, file_io_error_kind_t) = nullptr;

sampler_t<unsigned> file_io_length("file_io_length", [](perf_counter_t * c)
{
	c->add(file_io_limiter.get());
});


void file_io_error(std::string const & msg, file_io_error_kind_t errkind)
{
	logger << LIGHT_RED << msg;
	if (file_io_error_reporter)
		file_io_error_reporter(msg, errkind);
}


void file_io_sleep(boost::asio::deadline_timer * timer)
{
	timer->expires_at(timer->expires_at() + boost::posix_time::milliseconds(10));
	timer->async_wait(std::bind(file_io_sleep, timer));
}

void file_io_main(std::shared_ptr<Network::ServiceThread> thread)
{
	try
	{
		Network::currentServiceThread = thread;

		try
		{
			logger.set_prefix("[file_io] ");
			boost::asio::deadline_timer timer(thread->io_service, boost::posix_time::milliseconds(10));
			timer.async_wait(std::bind(file_io_sleep, &timer));

			while (thread->running())
			{
				try
				{
					thread->io_service.reset();
					thread->io_service.run();
				}
				LOG_CATCH();
			}

		}
		LOG_CATCH();

	}
	LOG_CATCH();
}

std::shared_ptr<Network::ServiceThread> start_file_io()
{
	std::shared_ptr<Network::ServiceThread> thread = file_io_thread.lock();
	if (!thread)
	{
		thread = std::make_shared<Network::ServiceThread>();
		file_io_thread = thread;
		thread->start(std::move(thread_t(file_io_main, thread)));
	}
	return thread;
}

void io_handler_t::write_finish(std::string & filename, unsigned retry, FILE * f1, std::string & sign, unsigned write_pos, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread)
{
	queue_limiter_dec_guard_t _g(file_io_limiter);
	unsigned write_count = std::min<unsigned>(sign.size() - write_pos, max_write_chunk);
	unsigned r = fwrite(&sign.data()[write_pos], 1, write_count, f1);

	if (r > 0 && fflush(f1) != 0)
	{
		r = 0;
		retry = 0;
	}

	if (r > 0)
	{
		write_pos += r;
		if (write_pos != sign.size() && thread->running())
		{
			_g.reset();
			thread->io_service.post(std::bind(&io_handler_t::write_finish, std::move(filename), max_retry_count, f1, std::move(sign), write_pos, handler, thread));
			return;
		}
	}
	else
	{
		if (retry > 0 && thread->running())
		{
			_g.reset();
			thread->io_service.post(std::bind(&io_handler_t::write_finish, std::move(filename), retry - 1, f1, std::move(sign), write_pos, handler, thread));
			return;
		}
	}
	fclose(f1);
	if (write_pos != sign.size())
		file_io_error("Failed to write signature to " + filename + (thread->running() ? "" : " because of thread exit"), file_io_error_kind_t::WRITE);
	else
	{
		//used for linux build
		//chmod(filename.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH);
	}
	if (handler) try
	{
		handler->on_ready(write_pos == sign.size());
	}
	LOG_CATCH();

}

void io_handler_t::write(std::string & filename, unsigned retry, FILE * f1, std::string & sign, unsigned write_pos, std::vector<char> & data, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread)
{
	queue_limiter_dec_guard_t _g(file_io_limiter);
	unsigned write_count = std::min<unsigned>(data.size() - write_pos, max_write_chunk);
	unsigned r = fwrite(&data[write_pos], 1, write_count, f1);

	if (r > 0 && fflush(f1) != 0)
	{
		r = 0;
		retry = 0;
	}


	if (r > 0)
	{

		write_pos += r;
		if (thread->running())
		{
			if (write_pos != data.size())
			{
				_g.reset();
				thread->io_service.post(std::bind(&io_handler_t::write, std::move(filename), max_retry_count, f1, std::move(sign), write_pos, std::move(data), handler, thread));
				return;
			}
			else
			{
				if (sign.size() > 0)
				{
					fseek(f1, 0, SEEK_SET);
					_g.reset();
					thread->io_service.dispatch(std::bind(&io_handler_t::write_finish, std::move(filename), max_retry_count, f1, std::move(sign), 0, handler, thread));
				}
				else
				{
					fclose(f1);
					if (handler) try
					{
						handler->on_ready(true);
					}
					LOG_CATCH();
				}
				return;
			}
		}
	}
	else
	{
		if (retry > 0 && thread->running())
		{
			_g.reset();
			thread->io_service.post(std::bind(&io_handler_t::write, std::move(filename), retry - 1, f1, std::move(sign), write_pos, std::move(data), handler, thread));
			return;
		}
	}

	fclose(f1);
	if (write_pos != sign.size())
		file_io_error("Failed to write to " + filename + (thread->running() ? "" : " because of thread exit"), file_io_error_kind_t::WRITE);
	if (handler) try
	{
		handler->on_ready(false);
	}
	LOG_CATCH();
}


void io_handler_t::open_write(std::string & filename, std::string & sign, std::vector<char> & data, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread)
{
	queue_limiter_dec_guard_t _g(file_io_limiter);
	FILE * f1 = fopen(filename.c_str(), "w+b");
	if (f1)
	{
		if (sign.size() > 0)
		{
			std::string sign2 = sign;
			for (char & ch : sign2)
				ch = ~ch;
			if (fwrite(sign2.data(), sign2.size(), 1, f1) < 1)
			{
				fclose(f1);
				if (handler) try
				{
					handler->on_ready(false);
				}
				LOG_CATCH();
				return;
			}
		}

		if (data.size() == 0)
		{
			fclose(f1);
			if (handler) try
			{
				handler->on_ready(true);
			}
			LOG_CATCH();
			return;
		}

		_g.reset();
		thread->io_service.dispatch(std::bind(&io_handler_t::write, std::move(filename), max_retry_count, f1, std::move(sign), 0, std::move(data), handler, thread));
	}
	else
	{
		file_io_error("Failed to open " + filename + " for writing", file_io_error_kind_t::OPEN_WRITE);
		if (handler) try
		{
			handler->on_ready(false);
		}
		LOG_CATCH();
	}
}

void io_handler_t::read(std::string & filename, unsigned retry, FILE * f1, unsigned read_pos, unsigned filesize, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread)
{
	queue_limiter_dec_guard_t _g(file_io_limiter);
	unsigned read_count = std::min<unsigned>(filesize - read_pos, max_read_chunk);
	unsigned r = fread(&handler->data[read_pos], 1, read_count, f1);
	if (r > 0)
	{
		read_pos += r;
		if (read_pos != filesize && thread->running())
		{
			_g.reset();
			thread->io_service.post(std::bind(&io_handler_t::read, std::move(filename), max_retry_count, f1, read_pos, filesize, handler, thread));
			return;
		}
	}
	else
	{
		if (retry > 0 && thread->running())
		{
			_g.reset();
			thread->io_service.post(std::bind(&io_handler_t::read, std::move(filename), retry - 1, f1, read_pos, filesize, handler, thread));
			return;
		}
	}

	if (read_pos != filesize)
		file_io_error("Failed to read " + filename + (thread->running() ? "" : " because of thread exit"), file_io_error_kind_t::OPEN_READ);
	fclose(f1);
	try
	{
		handler->on_ready(read_pos == filesize);
	}
	LOG_CATCH();
}

void io_handler_t::rename_file(std::string const & source, std::string const & dest, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread)
{
	queue_limiter_dec_guard_t _g(file_io_limiter);
	try
	{
		int r = std::rename(source.c_str(), dest.c_str());
		if (r != 0)
		{
			file_io_error("Failed to rename \"" + source + "\" to \"" + dest + "\"\r\n", file_io_error_kind_t::RENAME);
		}
		try
		{
			handler->on_ready(r == 0);
		}
		LOG_CATCH();
		return;
	}
	LOG_CATCH();

	try
	{
		handler->on_ready(false);
	}
	LOG_CATCH();

}


 void io_handler_t::open_read(std::string & filename, std::string const & sign, std::shared_ptr<io_handler_t> handler, std::shared_ptr<Network::ServiceThread> thread)
{
	queue_limiter_dec_guard_t _g(file_io_limiter);
	FILE * f1 = fopen(filename.c_str(), "r+b");
	if (f1)
	{
		unsigned read_pos = 0;
		fseek(f1, 0, SEEK_END);
		unsigned filesize = ftell(f1);
		fseek(f1, 0, SEEK_SET);


		if (filesize < sign.size())
			goto l_error;

		handler->data.resize(filesize);

		if (sign.size() > 0)
		{
			if (fread(&handler->data[0], sign.size(), 1, f1) < 1)
			{
				handler->data.clear();
				goto l_error;
			}
			read_pos += sign.size();

			unsigned i = 0;
			for (; i < sign.size(); ++i)
				if (sign[i] != handler->data[i])
					break;

			if (i < sign.size())
				goto l_error;
		}

		if (read_pos >= filesize)
		{
			fclose(f1);
			try
			{
				handler->on_ready(true);
			}
			LOG_CATCH();
			return;
		}

		_g.reset();
		thread->io_service.dispatch(std::bind(&io_handler_t::read, std::move(filename), max_retry_count, f1, read_pos, filesize, handler, thread));
		return;
	}
	else
	{
		file_io_error("Failed to open " + filename + " for reading", file_io_error_kind_t::OPEN_READ);
		try
		{
			handler->on_ready(false);
		}
		LOG_CATCH();
		return;
	}

l_error:
	file_io_error("Failed to read signature from " + filename, file_io_error_kind_t::READ);
	fclose(f1);
	try
	{
		handler->on_ready(false);
	}
	LOG_CATCH();
 }


bool load_file(std::string const & filename, std::string const & sign, std::shared_ptr<io_handler_t> handler)
{
	if (!handler)
		return false;

//	bool stalled = false;
	if (std::shared_ptr<Network::ServiceThread> thread = file_io_thread.lock())
	{
		if (file_io_limiter.post([thread, filename, sign, handler]() {
			thread->io_service.post(std::bind(&io_handler_t::open_read, filename, sign, handler, thread));
			return thread->running();
		}, [thread]() {return thread->running(); }))
		{
			return true;
		}
	}


	return false;
}

bool save_file(std::string const & filename, std::string const & sign, std::vector<char> && data, std::shared_ptr<io_handler_t> handler)
{
//	bool stalled = false;
	if (std::shared_ptr<Network::ServiceThread> thread = file_io_thread.lock())
	{
		if (file_io_limiter.post([thread, filename, sign, handler, data]() {
			thread->io_service.post(std::bind(&io_handler_t::open_write, filename, sign, std::move(data), handler, thread));
			return true;
		}, [thread]() {return thread->running(); }))
		{
			return true;
		}
	}


	return false;
}

bool rename_file(std::string const & source, std::string const & dest, std::shared_ptr<io_handler_t> handler)
{
//	bool stalled = false;
	if (std::shared_ptr<Network::ServiceThread> thread = file_io_thread.lock())
	{
		if (file_io_limiter.post([thread, source, dest, handler]() {
			thread->io_service.post(std::bind(&io_handler_t::rename_file, source, dest, handler, thread));
			return thread->running();
		}, [thread]() {return thread->running(); }))
		{
			return true;
		}
	}
	return false;
}
