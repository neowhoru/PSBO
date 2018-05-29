#include "stdafx.h"
#ifdef WIN32
#include <mysql.h>
#else
#include <mysql/mysql.h>
#endif
#include "database.h"
#include "config.h"
#include "counters.h"
#include "platform_specific.h"


std::atomic<unsigned> mysql_state(0);
std::atomic<float> query_time_warning(1.0f);
namespace
{
	FILE * db_log = nullptr;
	bool written_to_db_log = false;
	unsigned last_db_log_save_time = 0;
	std::weak_ptr<Network::ServiceThread> sql_thread;
	thread_local MYSQL * mysqlConnection = nullptr;
	thread_local unsigned last_query = 0;
	unsigned errnum = 0;
	void connect()
	{
		sleep_for(1000);
		mysqlConnection = mysql_init(0);
		if (!mysql_real_connect(mysqlConnection, get_config("mysql_host").c_str(), get_config("mysql_user").c_str(), get_config("mysql_password").c_str(), get_config("mysql_database").c_str(), get_config<unsigned>("mysql_port"), (char*)0, CLIENT_MULTI_RESULTS))
		{
			logger << LIGHT_RED << "Connecting to mysql on host " << get_config("mysql_host") << " database " << get_config("mysql_database") << "failed for user " << get_config("mysql_user") << endl;

			errnum = mysql_errno(mysqlConnection);
			if (errnum != 0)
				logger << LIGHT_RED << errnum << ": " << mysql_error(mysqlConnection) << endl;

			mysql_close(mysqlConnection);
			mysqlConnection = nullptr;
		}
		else
		{
			mysql_state = 1;
			logger << "Connected to mysql" << endl;
		}
	}
}

//std::atomic<unsigned> mysql_started = 0;
//std::atomic<unsigned> mysql_ok = 0;
thread_local unsigned long long last_insert_id;
thread_local unsigned long long affected_rows;
//std::atomic<unsigned> db_query_count(0);
//std::atomic<unsigned> max_db_query_count(1000);
queue_limiter_t db_query_limiter(1000);
void(*database_error_reporter)(std::string const & msg) = nullptr;

sampler_t<unsigned> db_queue_length("db_queue_length", [](perf_counter_t * c)
{
	c->add(db_query_limiter.get());
});


void database_error(std::string const & msg)
{
	logger << LIGHT_RED << msg;
	if (database_error_reporter)
	{
		try
		{
			database_error_reporter(msg);
		}
		LOG_CATCH();
	}
}

void init_database()
{
	if (mysql_library_init(-1, NULL, NULL))
	{
		logger << LIGHT_RED << "Could not initialize MySQL library" << endl;
		throw std::runtime_error("Could not initialize MySQL library");
		return;
	}
	db_log = fopen("queries.log", "a+b");
}



void db_sleep(boost::asio::deadline_timer * timer)
{
	timer->expires_at(timer->expires_at() + boost::posix_time::milliseconds(10));
	timer->async_wait(std::bind(db_sleep, timer));
	unsigned tick = get_tick();

	if (db_log != nullptr && written_to_db_log && last_db_log_save_time - tick > 5000)
	{
		written_to_db_log = false;
		last_db_log_save_time = tick;
		fflush(db_log);
	}
}

void database_main(std::shared_ptr<Network::ServiceThread> thread)
{
	try
	{
		Network::currentServiceThread = thread;

		if (!thread->db_inited)
		{
			mysql_thread_init();
			thread->db_inited = true;
		}

		try
		{
			logger.set_prefix("[mysql] ");
			boost::asio::deadline_timer timer(thread->io_service, boost::posix_time::milliseconds(10));
			timer.async_wait(std::bind(db_sleep, &timer));


			logger << "Mysql threadsafe: " << ((mysql_thread_safe() == 0) ? "no" : "yes") << endl;
			logger << "Mysql client version: " << mysql_get_client_version() << endl;
			logger << "Mysql client information: " << mysql_get_client_info() << endl;

			connect();

			//		mysql_ok = mysqlConnection ? 1 : 0;
			//		mysql_started = 1;

			if (mysqlConnection)
			{
				logger << "Mysql server version: " << mysql_get_server_version(mysqlConnection) << endl;
				logger << "Mysql server information: " << mysql_get_server_info(mysqlConnection) << endl;
				logger << "Mysql host information: " << mysql_get_host_info(mysqlConnection) << endl;
				//			logger << "Mysql protocol information: " << mysql_get_proto_info(mysqlConnection) << endl;
				//			logger << "Mysql ssl chipper: " << mysql_get_ssl_cipher(mysqlConnection) << endl;
			}

			while (thread->running())
			{
				try
				{
					if (mysqlConnection == nullptr)
						connect();

					if (mysqlConnection)
					{
						thread->io_service.reset();
						thread->io_service.run();
					}
				}
				LOG_CATCH();
			}

		}
		LOG_CATCH();
		//	mysql_started = 1;

		if (mysqlConnection != nullptr)
		{
			mysql_close(mysqlConnection);
			mysqlConnection = nullptr;
		}


		if (thread->db_inited)
		{
			thread->db_inited = false;
			mysql_thread_end();
		}
	}
	LOG_CATCH();
}

void _deinit_db_thread()
{
	if (mysqlConnection != nullptr)
	{
		mysql_close(mysqlConnection);
		mysqlConnection = nullptr;
	}
	mysql_thread_end();
}

void deinit_database()
{
	mysql_library_end();
	if (db_log)
	{
		fclose(db_log);
		db_log = nullptr;
	}
}

std::shared_ptr<Network::ServiceThread> start_db()
{
	std::shared_ptr<Network::ServiceThread> thread = sql_thread.lock();
	if (!thread)
	{
		thread = std::make_shared<Network::ServiceThread>();
		sql_thread = thread;
		thread->start(std::move(thread_t(database_main, thread)));
	}
	return thread;
}

void result_handler_t::execute_query(std::string const & query, std::shared_ptr<result_handler_t> handler)
{
	db_query_limiter.dec();
	if (_execute_query(query, handler, true))
		_execute_query("SHOW WARNINGS", nullptr, false);
}

bool result_handler_t::_execute_query(std::string const & query, std::shared_ptr<result_handler_t> handler, bool check_warnings)
{
	bool success = false;
	bool sql_gone = false;
	bool had_exception = false;
	unsigned wc = 0;

	MYSQL * mysqlConnection = ::mysqlConnection;

	if (!mysqlConnection)
	{
		if (std::shared_ptr<Network::ServiceThread> thread = sql_thread.lock())
		{
			db_query_limiter.inc();
			thread->io_service.post(std::bind(&result_handler_t::execute_query, query, handler));
			thread->io_service.stop();
		}
		else
		{
			if (handler) try
			{
				handler->_on_finished(false);
			}
			LOG_CATCH();
		}
		return false;
	}

	{
		unsigned tick = get_tick();
		if (tick - last_query >= 60000)
		{
			int r = mysql_ping(mysqlConnection);
			if (r != 0)
			{
				errnum = mysql_errno(mysqlConnection);
				//sql_gone |= errnum == 2006 || errnum == 2013 || r == 2000 || r == 2006 || r == 2013 || r == 2014;
				database_error("mysql ping failed: " + std::to_string(r) + ", " + std::to_string(errnum) + ": " + mysql_error(mysqlConnection) + "\r\nWhile executing query " + query + "\r\n");

				mysql_close(mysqlConnection);
				::mysqlConnection = nullptr;
				mysql_state = 0;
				errnum = 0;

				if (std::shared_ptr<Network::ServiceThread> thread = sql_thread.lock())
				{
					thread->io_service.post(std::bind(&result_handler_t::execute_query, query, handler));
					thread->io_service.stop();
				}
				else
				{
					if (handler) try
					{
						handler->_on_finished(false);
					}
					LOG_CATCH();
				}
				return false;
			}
		}
		last_query = tick;
	}

	unsigned query_start_time = get_tick();
	unsigned query_end_time;
	unsigned result_count = -1;

	if (query.size() > 0 && db_log != nullptr)
	{
		std::string t = datetime2str();
		t = "\r\n" + t + ":\t";

		fwrite(t.data(), t.size(), 1, db_log);
		fwrite(query.data(), query.size(), 1, db_log);
		written_to_db_log = true;
	}

	if (mysql_query(mysqlConnection, query.c_str()) == 0)
	{
		int multi_status = 0;
		do
		{
			++result_count;
			MYSQL_RES * result = mysql_store_result(mysqlConnection);
			if (result != nullptr)
			{
				unsigned row_count = mysql_num_rows(result);
				if (handler)
				{
					try
					{
						handler->_set_insert_id(mysql_insert_id(mysqlConnection), mysql_affected_rows(mysqlConnection), result_count);
						handler->_row_count(row_count, result_count);
					}
					catch (std::exception & e)
					{
						catch_message(e);
						had_exception = true;
					}
				}

				MYSQL_ROW row;
				for (unsigned j = 0;; ++j)
				{
					unsigned fieldCount = mysql_num_fields(result);
					row = mysql_fetch_row(result);
					if (row == 0)
						break;
					if (handler)
					{
						unsigned long const * const lengths = mysql_fetch_lengths(result);

						try
						{
							handler->_add_result(row, lengths, fieldCount, result_count);
						}
						catch (std::exception & e)
						{
							catch_message(e);
							had_exception = true;
						}
					}
				}

				mysql_free_result(result);
			}
			else
			{
				if (mysql_field_count(mysqlConnection) != 0)
				{
					errnum = mysql_errno(mysqlConnection);
					sql_gone |= errnum == 2006 || errnum == 2013;
					database_error(std::to_string(errnum) + ": " + mysql_error(mysqlConnection) + "\r\nWhile executing query " + query + "\r\n");
					while(multi_status == 0)
						multi_status = mysql_next_result(mysqlConnection);

					break;
				}
				else
				{
					if (handler)
					{
						try
						{
							handler->_set_insert_id(mysql_insert_id(mysqlConnection), mysql_affected_rows(mysqlConnection), result_count);
						}
						catch (std::exception & e)
						{
							catch_message(e);
							had_exception = true;
						}
					}
				}
			}

			multi_status = mysql_next_result(mysqlConnection);
			if (multi_status == -1)
				success = !had_exception;
			else if (multi_status > 0)
				success = false;

		} while (multi_status == 0);
		query_end_time = get_tick();
	}
	else
	{
		query_end_time = get_tick();
		errnum = mysql_errno(mysqlConnection);
		sql_gone |= errnum == 2006 || errnum == 2013;
		if (errnum)
		{
			database_error(std::to_string(errnum) + ": " + mysql_error(mysqlConnection) + "\r\nWhile executing query " + query + "\r\n");
		}
	}

	if (!success)
	{
		if (errnum != 0)
		{
			if (errnum == 1205 ||	//lock wait timeout
				errnum == 1213 ||	//deadlock
				errnum == 2006 ||	//server gone
				errnum == 2013
				)
			{
				if (handler)
				{
					if (handler->retry > 0)
					{
						--handler->retry;
						if (std::shared_ptr<Network::ServiceThread> thread = sql_thread.lock())
						{
							db_query_limiter.inc();
							thread->io_service.post(std::bind(&result_handler_t::execute_query, query, handler));
						}
						else try
						{
							handler->_on_finished(false);
						}
						LOG_CATCH();
					}
					else try
					{
						handler->_on_finished(false);
					}
					LOG_CATCH();
				}
			}
			else if (handler) try
			{
				handler->_on_finished(false);
			}
			LOG_CATCH();
		}
		else if (handler) try
		{
			handler->_on_finished(false);
		}
		LOG_CATCH();
	}
	else
	{
		if (check_warnings)
		{
			wc = mysql_warning_count(mysqlConnection);
			if (wc > 0)
			{
				logger << YELLOW << "Warning count " << wc << "\r\n" << query << "\r\n";
				syslog_warning("Mysql warning count: " + std::to_string(wc) + ", query: " + query);
			}
		}

		if (handler) try
		{
			handler->_on_finished(true);
		}
		LOG_CATCH();
	}

	float query_time = (query_end_time - query_start_time) / 1000.0f;
	float handler_time = (get_tick() - query_end_time) / 1000.0f;

	if (query_time > 1 || handler_time > 1)
	{
		std::ostringstream oss;
		oss << "Query time: " << query_time << " handler time: " << handler_time << " queue length: " << db_query_limiter.get() << " for query: " << query;
		std::string ossstr = oss.str();
		logger << YELLOW << ossstr;
		syslog_warning(ossstr);
	}

	if (sql_gone)
	{
		mysql_close(mysqlConnection);
		::mysqlConnection = nullptr;
		mysql_state = 0;
		errnum = 0;

		if (std::shared_ptr<Network::ServiceThread> thread = sql_thread.lock())
			thread->io_service.stop();

		return false;
	}

	return wc > 0;
}


bool query(std::string const & query, std::shared_ptr<result_handler_t> handler)
{
//	bool stalled = false;
	if (std::shared_ptr<Network::ServiceThread> thread = sql_thread.lock())
	{
		if (db_query_limiter.post([query, handler, thread]()
		{
			thread->io_service.post(std::bind(&result_handler_t::execute_query, query + ';', handler));
			return thread->running();
		}, [thread](){ return thread->running(); }))
		{
			return true;
		}

		/*
		for (unsigned exp = db_query_count; thread->running();)
		{
			if (exp >= max_db_query_count)
			{
				if (!stalled)
				{
					stalled = true;
					logger << YELLOW << "Query count reached limit, call blocks";
				}
				sleep_for(10);
				exp = db_query_count;
				continue;
			}
			if (db_query_count.compare_exchange_weak(exp, exp + 1))
			{
				thread->io_service.post(std::bind(&result_handler_t::execute_query, query + ';', handler));
				return true;
			}
#ifndef _NO_X86
			_mm_pause();
#endif
		}
		*/
	}

	if (handler) try
	{
		handler->_failed();
	}
	LOG_CATCH();

	return false;
}


std::string escape(const char * str, unsigned len)
{
	if (std::shared_ptr<Network::ServiceThread> thread = Network::currentServiceThread.lock())
	{
		if (!thread->db_inited)
		{
			mysql_thread_init();
			thread->db_inited = true;
		}
	}


	MYSQL * conn = ::mysqlConnection;
	if (conn == nullptr)
	{
		connect();
		conn = ::mysqlConnection;
		if (conn == nullptr)
			throw std::runtime_error("Failed to connect to database for escape()");
	}

	std::string result;
	result.resize(len * 2 + 1 + 2);
	result[0] = '\'';

	unsigned long result_len = mysql_real_escape_string(conn, &result[1], str, len);

	result[result_len + 1] = '\'';

	result.resize(result_len + 2);
	return result;
}
