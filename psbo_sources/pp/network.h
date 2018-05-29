#pragma once

#include <string>
#include "binarystream.h"

enum class DisconnectKind { ACCEPT, SOFT_DC, IMMEDIATE_DC };

namespace Network
{
	class postpone
	{
	};

	struct session_id_t
	{
		unsigned id;
		unsigned seq_id;

		explicit operator bool() const
		{
			return id != 0;
		}

		bool operator ==(const session_id_t & b) const
		{
			return id == b.id && seq_id == b.seq_id;
		}
		bool operator !=(const session_id_t & b) const
		{
			return id != b.id || seq_id != b.seq_id;
		}
		bool operator <(const session_id_t & b) const
		{
			return id < b.id || (id == b.id && seq_id < b.seq_id);
		}
		bool operator <=(const session_id_t & b) const
		{
			return id < b.id || (id == b.id && seq_id <= b.seq_id);
		}
		bool operator >(const session_id_t & b) const
		{
			return id > b.id || (id == b.id && seq_id > b.seq_id);
		}
		bool operator >=(const session_id_t & b) const
		{
			return id > b.id || (id == b.id && seq_id >= b.seq_id);
		}
	};

	typedef std::shared_ptr<class Server> channel_id_t;
	extern thread_local session_id_t current_client_id;
	extern thread_local boost::asio::ip::udp::endpoint current_udp_endpoint;
	extern thread_local std::atomic<unsigned> timer_msecs;

	extern volatile bool log_network_errors;
	extern volatile bool log_network_events;
	extern std::atomic<unsigned> send_timeout;


	void init();

	class ServiceThread
		: public std::enable_shared_from_this<ServiceThread>
	{
	private:
		mutex_t mutex;
		std::list<std::shared_ptr<class Server> > servers;
		static void thread_main(std::shared_ptr<ServiceThread> ths);
		std::atomic<unsigned> threadRunning;
	public:
		bool db_inited = false;
		boost::asio::io_service io_service;
		thread_t networkThread;
		void start(thread_t && t);
		void start(std::shared_ptr<class Server>);
		void remove(std::shared_ptr<class Server>);
		void stop();
		inline bool running() const
		{
			return threadRunning != 0;
		}
		ServiceThread()
			: threadRunning(0)
		{}
		~ServiceThread()
		{
			stop();
		}
	};

	extern thread_local std::weak_ptr<ServiceThread> currentServiceThread;
	/*
	class dispatch_base_data_t
	{
	protected:
		dispatch_base_data_t() {}
	public:
		virtual ~dispatch_base_data_t() {}
		virtual void * get_ptr() = 0;
	};

	template <class T>
	class dispatch_data_t
		: public dispatch_base_data_t
	{
	public:
		T data;
		dispatch_data_t(T && _data)
			: data(std::move(_data))
		{}
		virtual ~dispatch_data_t() {}
		virtual void * get_ptr()
		{
			return (void*)&data;
		}
	};
	*/
	class NetworkEventHandler;
	class UdpNetworkEventHandler;

	void stopNetwork(channel_id_t & c_addr);
	void stop(channel_id_t c_addr);
	bool sendTo(channel_id_t cid, session_id_t id, std::vector<char> && data);
	bool inline sendTo(channel_id_t const & cid, session_id_t id, BinaryWriter && bw)
	{
		return sendTo(cid, id, std::move(bw.data));
	}
	bool sendToDropable(channel_id_t cid, session_id_t id, std::vector<char> && data);
	bool inline sendToDropable(channel_id_t const & cid, session_id_t id, BinaryWriter && bw)
	{
		return sendToDropable(cid, id, std::move(bw.data));
	}


	bool dispatch(channel_id_t cid, std::vector<char> && data);

	inline bool dispatch(channel_id_t const & cid, BinaryWriter && bw)
	{
		return dispatch(cid, std::move(bw.data));
	}

	bool sendDatagramTo(channel_id_t cid, boost::asio::ip::udp::endpoint const & endpoint, std::vector<char> && data);
	inline bool sendDatagramTo(channel_id_t const & cid, boost::asio::ip::udp::endpoint const & endpoint, BinaryWriter && bw)
	{
		return sendDatagramTo(cid, endpoint, std::move(bw.data));
	}
	bool startNetwork(std::string const & service_name, channel_id_t & c_addr, std::string const & ip, unsigned port, NetworkEventHandler * handler, bool no_delay, std::shared_ptr<ServiceThread> serviceThread = std::make_shared<ServiceThread>());
	bool startNetwork(std::string const & service_name, channel_id_t & c_addr, unsigned port, UdpNetworkEventHandler * handler, std::shared_ptr<ServiceThread> serviceThread = std::make_shared<ServiceThread>());
	unsigned sendTo(channel_id_t cid, std::vector<session_id_t> const & ids, std::vector<char> && data, session_id_t skipId = session_id_t{ 0, 0 });
	inline unsigned sendTo(channel_id_t const & cid, std::vector<session_id_t> const & ids, BinaryWriter && bw, session_id_t skipId = session_id_t{ 0, 0 })
	{
		return sendTo(cid, ids, std::move(bw.data), skipId);
	}
	unsigned sendToDropable(channel_id_t cid, std::vector<session_id_t> const & ids, std::vector<char> && data, session_id_t skipId = session_id_t{ 0, 0 });
	inline unsigned sendToDropable(channel_id_t const & cid, std::vector<session_id_t> const & ids, BinaryWriter && bw, session_id_t skipId = session_id_t{ 0, 0 })
	{
		return sendToDropable(cid, ids, std::move(bw.data), skipId);
	}
	bool set_accept_open(channel_id_t cid, unsigned port);
	bool distribute(channel_id_t cid, std::vector<char> && data, session_id_t skipId = session_id_t{ 0, 0 });
	inline bool distribute(channel_id_t const & cid, BinaryWriter && bw, session_id_t skipId = session_id_t{ 0, 0 })
	{
		return distribute(cid, std::move(bw.data), skipId);
	}
	bool disconnect(channel_id_t cid, session_id_t id, DisconnectKind disconnectKind = DisconnectKind::SOFT_DC);
	void callPostponed(channel_id_t cid, session_id_t id);
	bool addNetworkOneTimeTask(channel_id_t cid, void(*func)());

	class NetworkClientBase
	{
	private:
		unsigned disable_recv = 0;
	protected:
		bool _valid = true;
		NetworkClientBase() {}

	public:

		void check_valid() const
		{
			if (disable_recv != 0 || !_valid)
				throw postpone();
		}

		bool valid() const
		{
			return _valid;
		}

		inline bool dec_read_lock()
		{
			assert(disable_recv > 0);
			--disable_recv;
			return disable_recv == 0;
		} 

		inline void inc_read_lock()
		{
			++disable_recv;
		}

		inline bool can_read() const
		{
			return disable_recv == 0;
		}

		virtual void invalidate() = 0;
		virtual ~NetworkClientBase() {}
	};

	class NetworkEventHandler
	{
		friend bool startNetwork(std::string const & service_name, channel_id_t & c_addr, std::string const & ip, unsigned port, NetworkEventHandler * handler, bool no_delay, std::shared_ptr<ServiceThread> serviceThread);
		channel_id_t channel_addr = 0;
		std::atomic<unsigned short> _port;
		/*
		inline void set_channel_id(unsigned value)
		{
			channel_id._value = value;
		}
		*/
	protected:
		bool _destroyed = false;
		void stopNetwork()
		{
			if (channel_addr)
			{
				Network::stopNetwork(channel_addr);
//				if (channel_addr == Network::channel_addr)
//				{
//					Network::channel_addr = 0;
//					Network::channel_id = ~0;
//				}
				channel_addr = 0;
//				set_channel_id(~0);

			}
		}
	public:
		NetworkEventHandler()
			: _port(0) 
		{}
		/*
		union
		{
			volatile const unsigned value = ~0;
			inline operator unsigned () const { return value; }
		private:
			friend class NetworkEventHandler;
			unsigned _value;
		} channel_id;
		*/
		bool destroyed() const
		{
			return _destroyed;
		}

		virtual ~NetworkEventHandler()
		{
			_destroyed = true;
			stopNetwork();
		}
		virtual DisconnectKind onConnect(session_id_t session_id, std::string && ip, unsigned port) = 0;
		virtual void onRecieve(session_id_t session_id, const char * data, unsigned length) = 0;
		virtual void onDisconnect(session_id_t session_id) = 0;
		virtual void onInit() = 0;
		virtual void onTimer() {}
		virtual std::string getConnectionData(Network::session_id_t id) const 
		{
			return std::string();
		}
		virtual void inPacketTooBig(session_id_t session_id) {}

		inline bool sendTo(session_id_t id, std::vector<char> && data)
		{
			return Network::sendTo(channel_addr, id, std::move(data));
		}

		inline bool sendTo(session_id_t id, BinaryWriter && bw)
		{
			return Network::sendTo(channel_addr, id, std::move(bw.data));
		}

		inline bool sendToDropable(session_id_t id, std::vector<char> && data)
		{
			return Network::sendToDropable(channel_addr, id, std::move(data));
		}

		inline bool sendToDropable(session_id_t id, BinaryWriter && bw)
		{
			return Network::sendToDropable(channel_addr, id, std::move(bw.data));
		}

		inline bool dispatch(std::vector<char> && data)
		{
			return Network::dispatch(channel_addr, std::move(data));
		}

		inline bool dispatch(BinaryWriter && bw)
		{
			return Network::dispatch(channel_addr, std::move(bw.data));
		}

		inline bool repply(std::vector<char> && data)
		{
			return Network::sendTo(channel_addr, Network::current_client_id, std::move(data));
		}

		inline bool repply(BinaryWriter && bw)
		{
			return Network::sendTo(channel_addr, Network::current_client_id, std::move(bw.data));
		}

		inline bool repplyDropable(std::vector<char> && data)
		{
			return Network::sendToDropable(channel_addr, Network::current_client_id, std::move(data));
		}

		inline bool repplyDropable(BinaryWriter && bw)
		{
			return Network::sendToDropable(channel_addr, Network::current_client_id, std::move(bw.data));
		}

		inline unsigned sendTo(std::vector<session_id_t> const & ids, std::vector<char> && data, session_id_t skipId = session_id_t{ 0, 0 })
		{
			return Network::sendTo(channel_addr, ids, std::move(data), skipId);
		}

		inline unsigned sendTo(std::vector<session_id_t> const & ids, BinaryWriter && bw, session_id_t skipId = session_id_t{ 0, 0 })
		{
			return Network::sendTo(channel_addr, ids, std::move(bw.data), skipId);
		}

		inline unsigned sendToDropable(std::vector<session_id_t> const & ids, std::vector<char> && data, session_id_t skipId = session_id_t{ 0, 0 })
		{
			return Network::sendToDropable(channel_addr, ids, std::move(data), skipId);
		}

		inline unsigned sendToDropable(std::vector<session_id_t> const & ids, BinaryWriter && bw, session_id_t skipId = session_id_t{ 0, 0 })
		{
			return Network::sendToDropable(channel_addr, ids, std::move(bw.data), skipId);
		}

		inline bool set_accept_open(unsigned port)
		{
			_port = port;
			return Network::set_accept_open(channel_addr, port);
		}

		inline bool open()
		{
			return Network::set_accept_open(channel_addr, _port);
		}

		inline bool close()
		{
			return Network::set_accept_open(channel_addr, 0);
		}

		void callPostponed(session_id_t id)
		{
			Network::callPostponed(channel_addr, id);
		}

		inline bool distribute(std::vector<char> && data, session_id_t skipId = session_id_t{ 0, 0 })
		{
			return Network::distribute(channel_addr, std::move(data), skipId);
		}
		inline bool distribute(BinaryWriter && bw, session_id_t skipId = session_id_t{ 0, 0 })
		{
			return Network::distribute(channel_addr, std::move(bw.data), skipId);
		}
		inline bool disconnect(session_id_t id, DisconnectKind disconnectKind = DisconnectKind::SOFT_DC)
		{
			return Network::disconnect(channel_addr, id, disconnectKind);
		}
		inline bool addNetworkOneTimeTask(void(*func)())
		{
			return Network::addNetworkOneTimeTask(channel_addr, func);
		}

		template <class T, typename ... ARGS>
		static std::unique_ptr<T> start(std::string const & service_name, std::string const & ip, unsigned port, std::shared_ptr<ServiceThread> serviceThread, bool no_delay, ARGS && ... args)
		{
			std::unique_ptr<T> handler(new T(std::forward<ARGS>(args)...));
			if (!Network::startNetwork(service_name, handler->channel_addr, ip, port, handler.get(), no_delay, serviceThread))
				handler.reset();

			return handler;
		}

	};


	class UdpNetworkEventHandler
	{
		bool _destroyed = false;
		friend bool startNetwork(std::string const & service_name, channel_id_t & c_addr, unsigned port, UdpNetworkEventHandler * handler, std::shared_ptr<ServiceThread> serviceThread);
		channel_id_t channel_addr;
		/*
		inline void set_channel_id(unsigned value)
		{
			channel_id._value = value;
		}
		*/
	protected:
		void stopNetwork()
		{
			if (channel_addr)
			{
				Network::stopNetwork(channel_addr);
//				set_channel_id(~0);
			}
		}
	public:
		/*
		union
		{
			volatile const unsigned value = ~0;
			inline operator unsigned () const { return value; }
		private:
			friend class UdpNetworkEventHandler;
			unsigned _value;
		} channel_id;
		*/
		bool destroyed() const
		{
			return _destroyed;
		}

		virtual ~UdpNetworkEventHandler()
		{
			_destroyed = true;
			stopNetwork();
		}
		virtual void onRecieveDatagram(const char * data, unsigned len, boost::asio::ip::udp::endpoint const & endpoint) = 0;
		virtual void onInit() = 0;

		inline bool sendDatagramTo(boost::asio::ip::udp::endpoint const & endpoint, std::vector<char> && data)
		{
			return Network::sendDatagramTo(channel_addr, endpoint, std::move(data));
		}

		inline bool sendDatagramTo(boost::asio::ip::udp::endpoint const & endpoint, BinaryWriter && bw)
		{
			return Network::sendDatagramTo(channel_addr, endpoint, std::move(bw.data));
		}

		inline bool repply(std::vector<char> && data)
		{
			return Network::sendDatagramTo(channel_addr, current_udp_endpoint, std::move(data));
		}

		inline bool repply(BinaryWriter && bw)
		{
			return Network::sendDatagramTo(channel_addr, current_udp_endpoint, std::move(bw.data));
		}

		inline bool addNetworkOneTimeTask(void(*func)())
		{
			return Network::addNetworkOneTimeTask(channel_addr, func);
		}

		template <class T, typename ... ARGS>
		static std::unique_ptr<T> start(std::string const & service_name, unsigned port, std::shared_ptr<ServiceThread> serviceThread, ARGS && ... args)
		{
			std::unique_ptr<T> handler(new T(std::forward<ARGS>(args)...));
			if (!Network::startNetwork(service_name, handler->channel_addr, port, handler.get(), serviceThread))
				handler.reset();

			return handler;
		}

	};
	template <class T, typename ... ARGS>
	inline std::unique_ptr<T> start(std::string const & service_name, std::string const & ip, unsigned port, std::shared_ptr<ServiceThread> const & serviceThread, bool no_delay, ARGS && ... args)
	{
		return NetworkEventHandler::start<T, ARGS...>(service_name, ip, port, serviceThread, no_delay, std::forward<ARGS>(args)...);
	}

	template <class T, typename ... ARGS>
	inline std::unique_ptr<T> start(std::string const & service_name,unsigned port, std::shared_ptr<ServiceThread> serviceThread, ARGS && ... args)
	{
		return UdpNetworkEventHandler::start<T, ARGS...>(service_name, port, serviceThread, std::forward<ARGS>(args)...);
	}

	bool connect(channel_id_t cid, std::string const & ip, unsigned port, bool no_delay);

	//boost::asio::io_service * _get_service(channel_id_t cid);

	unsigned string2ipv4(const char * ip);
	inline unsigned string2ipv4(std::string const & ip)
	{
		return string2ipv4(ip.c_str());
	}
	std::string ipv42string(unsigned ip);
	inline unsigned make_ipv4(unsigned char a, unsigned char b, unsigned char c, unsigned char d)
	{
		return (unsigned)a | ((unsigned)b << 8) | ((unsigned)c << 16) | ((unsigned)d << 24);
	}

	void dump(const char * data, unsigned len, const char * desc = nullptr);
	inline void dump(std::vector<char> const & data, const char * desc = nullptr)
	{
		dump(data.data(), data.size(), desc);
	}

	struct packet_header_t
	{
		unsigned size;
	};


	template <typename ... ARGS>
	BinaryWriter packet(unsigned id, ARGS const & ... args)
	{
		BinaryWriter bw;
		bw.write(packet_header_t{0}, id, args...);
		return bw;
	}

	template <typename ... ARGS>
	BinaryWriter datagram(ARGS const & ... args)
	{
		BinaryWriter bw;
		bw.write(args...);
		return bw;
	}

};



