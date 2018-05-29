#include "stdafx.h"
#include "network.h"
#include "binarystream.h"
#include "config.h"
#include "string.h"

#ifdef _MSC_VER
#include <WS2tcpip.h>
#endif

void _deinit_db_thread();

namespace Network
{
	thread_local std::atomic<unsigned> timer_msecs(100);
	volatile bool log_network_errors = true;
	volatile bool log_network_events = false;
	unsigned const maxDatagramSize = 65000;
	unsigned const max_client_count = 1024;
	std::atomic<unsigned> send_timeout(5*60*1000);

	thread_local session_id_t current_client_id = session_id_t{ 0, 0 };
	thread_local boost::asio::ip::udp::endpoint current_udp_endpoint;


	void dump_packet(bool outPacket, std::vector<char> const & packet, unsigned length = ~0)
	{
		auto log = logger << (outPacket ? "Outpacket \r\n" : "Inpacket \r\n");

		for (unsigned i = 0; i < packet.size() && i < length; ++i)
		{
			log << hex << setw(2) << setfill('0') << (unsigned)(unsigned char)packet[i] << ' ';
			if (i % 16 == 0 && i > 0)
				log << endl;
			else if (i % 16 == 8)
				log << ' ';
		}
		log << endl;
	}

	void dump_packet(bool outPacket, const char * packet, unsigned length)
	{
		auto log = logger << (outPacket ? "Outpacket \r\n" : "Inpacket \r\n");

		for (unsigned i = 0; i < length; ++i)
		{
			log << hex << setw(2) << setfill('0') << (unsigned)(unsigned char)packet[i] << ' ';
			if (i % 16 == 0 && i > 0)
				log << endl;
			else if (i % 16 == 8)
				log << ' ';
		}
		log << endl;
	}

	class Server;

	void ServiceThread::stop()
	{
		threadRunning = false;
		io_service.stop();
		if (networkThread.joinable()) try
		{
			networkThread.join();
		}
		LOG_CATCH();
	}


	class Server
		: public std::enable_shared_from_this<Server>
	{
	public:
		std::shared_ptr<ServiceThread> serviceThread;
		boost::asio::deadline_timer * timer = nullptr;
		std::atomic<unsigned> networkRunning;
		std::atomic<unsigned> taskCount;

		std::string service_name;

		Server()
			: taskCount(0)
		{
		}

		bool no_delay = false;
		virtual ~Server() {}
		virtual void cleanup() = 0;
		void stop()
		{
			networkRunning = false;
			if (serviceThread)
			{
				//serverThread->stop();
				serviceThread->remove(shared_from_this());
			}
		}
		std::shared_ptr<ServiceThread> running() const
		{
			std::shared_ptr<ServiceThread> r;
			if (networkRunning)
				r = serviceThread;
			return r;
		}
	};

	void ServiceThread::start(std::shared_ptr<Server> server)
	{
		std::shared_ptr<ServiceThread> st = shared_from_this();

		impl_lock_guard(lock, mutex);
		server->serviceThread = st;
		servers.push_back(server);
		threadRunning = true;
		if (!networkThread.joinable())
			networkThread = std::move(thread_t(thread_main, st));
	}

	void ServiceThread::start(thread_t && t)
	{
		impl_lock_guard(lock, mutex);
		threadRunning = true;
		if (!networkThread.joinable())
			networkThread = std::move(t);
	}

	void ServiceThread::remove(std::shared_ptr<Server> server)
	{
		bool call_stop = false;
		std::shared_ptr<ServiceThread> st = shared_from_this();
		{
			impl_lock_guard(lock, mutex);

			for (auto i = servers.begin(); i != servers.end(); ++i)
			{
				if (*i == server)
				{
					servers.erase(i);
					break;
				}
			}

			while (server->taskCount > 0)
				sleep_for(10);

			if(this->threadRunning)
			{
				std::atomic<unsigned> barrier(0);
				io_service.post([&barrier]() { barrier = 1; });

				while (barrier == 0)
					sleep_for(10);
			}

			if (servers.size() == 0)
				call_stop = true;
		}

		if(call_stop)
			stop();
	}

	thread_local std::weak_ptr<ServiceThread> currentServiceThread;

	void ServiceThread::thread_main(std::shared_ptr<ServiceThread> serverThread)
	{
		try
		{
			currentServiceThread = serverThread;
			logger.set_prefix("[" + serverThread->servers.front()->service_name + "] ");
			current_client_id = session_id_t{ 0, 0 };


			while (serverThread->threadRunning)
			{
				try
				{
					serverThread->io_service.run();
				}
				LOG_CATCH();

				if (serverThread->threadRunning)
					serverThread->io_service.reset();
			}

			{
				impl_lock_guard(lock, serverThread->mutex);

				while (!serverThread->servers.empty())
				{
					try
					{
						std::shared_ptr<Server> server = serverThread->servers.front();
						serverThread->servers.pop_front();
						server->cleanup();
					}
					LOG_CATCH();
				}
			}

			if (serverThread->db_inited)
			{
				serverThread->db_inited = false;
				_deinit_db_thread();
			}
		}
		LOG_CATCH();
	}

	struct Client;
	class TcpServer
		: public Server
	{
	public:
		std::vector<unsigned> free_ids;
		std::vector<unsigned> used_ids;
		std::vector<std::shared_ptr<Client> > clients;
		boost::asio::ip::tcp::acceptor * acceptor = nullptr;
		bool accepting = false;
		std::atomic<unsigned> acceptor_port;
		NetworkEventHandler * eventHandler = nullptr;
		unsigned last_send_check;
		unsigned seq_id = 0;
		virtual ~TcpServer() {}
		virtual void cleanup();
		TcpServer()
			:acceptor_port(0)
		{
			clients.resize(max_client_count + 1);
			free_ids.reserve(max_client_count);
			used_ids.reserve(max_client_count);
			for (unsigned i = 1; i < max_client_count + 1; ++i)
				free_ids.push_back(i);
			last_send_check = get_tick();
		}

		std::shared_ptr<Client> get_client(session_id_t id);
	};

	void _disconnect(std::shared_ptr<TcpServer> entity, session_id_t id, DisconnectKind disconnectKind);

	struct UdpServer
		: public Server
	{
		enum socket_state_t { SSF_OK, SSF_SOFT_DC, SSF_IMMEDIATE_DC };
		std::atomic<socket_state_t> sState2;
		boost::asio::ip::udp::socket socket;
		boost::asio::ip::udp::endpoint recv_endpoint;
		std::vector<char> inPacket;
		unsigned port;
		UdpNetworkEventHandler * eventHandler = nullptr;

		std::list<std::tuple<std::shared_ptr<std::vector<char> >, boost::asio::ip::udp::endpoint > > sendQueue;

		virtual void cleanup()
		{
			networkRunning = false;
			sState2 = SSF_SOFT_DC;

			LOG_EX(stop());

			if (timer)
				LOG_EX(safe_delete(timer));
		}

		bool sending() const
		{
			return sendQueue.size() > 0;
		}

		UdpServer(boost::asio::io_service * io_service, unsigned _port)
			: sState2(SSF_OK)
			, socket(*io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), _port))
			, port(_port)
		{
			inPacket.resize(maxDatagramSize);
		}

		~UdpServer()
		{
		}

		void stop()
		{
			if (socket.is_open())
			{
				boost::system::error_code error;
				LOG_EX(socket.shutdown(boost::asio::socket_base::shutdown_both, error));
				if (error && error != boost::asio::error::not_connected)
					print_boost_error(error);
				LOG_EX(socket.close(error));
				if (error && error != boost::asio::error::not_connected)
					print_boost_error(error);
			}

			if (log_network_events)
				logger << "Udp " << port << " stopped" << std::endl;
		}

		static void handle_write(std::shared_ptr<UdpServer> ths, boost::system::error_code const & error, size_t bytes_written)
		{
			if (error || ths->sendQueue.empty())
			{
				if (error != boost::asio::error::eof && error != boost::asio::error::connection_reset && ths->sState2 != Network::UdpServer::SSF_SOFT_DC)
				{
					if (log_network_errors && error && (ths->networkRunning || (error.value() != 995 && !ths->networkRunning)))
						print_boost_error(error);
				}

				if (!ths->socket.is_open())
					ths->sState2 = SSF_IMMEDIATE_DC;

				if (ths->sState2 != SSF_OK)
					return;
			}
			else
			{
				if (!ths->socket.is_open())
					ths->sState2 = SSF_IMMEDIATE_DC;

				if (ths->sState2 != SSF_OK)
					return;
			}

			{
				ths->sendQueue.pop_front();

				if (ths->sendQueue.size() > 0)
				{
					ths->sendNext();
				}
				else if (ths->sState2 == SSF_SOFT_DC)
				{
					ths->sState2 = SSF_IMMEDIATE_DC;
					ths->stop();
				}
			}
		}

		void sendNext()
		{
			if (sendQueue.size() > 0 && socket.is_open())
			{
				std::shared_ptr<UdpServer> ths = std::static_pointer_cast<UdpServer>(this->shared_from_this());

				socket.async_send_to(boost::asio::buffer(&(*std::get<0>(sendQueue.front()))[0], std::get<0>(sendQueue.front())->size()),
					std::get<1>(sendQueue.front()),
					boost::bind(&handle_write, ths,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));

			}
		}

		static void handle_read(std::shared_ptr<UdpServer> ths, boost::system::error_code const & error, size_t bytes_read)
		{
			if (error == boost::asio::error::message_size)
			{
				logger << "Message size " << bytes_read << endl;
			}

			if (error/* && error != boost::asio::error::message_size*/)
			{
				if (error != boost::asio::error::eof && error != boost::asio::error::connection_reset && ths->sState2 != Network::UdpServer::SSF_IMMEDIATE_DC && ths->sState2 != Network::UdpServer::SSF_SOFT_DC)
				{
					if (log_network_errors && error && (ths->networkRunning || (error.value() != 995 && !ths->networkRunning)))
						print_boost_error(error);
				}

				if (!ths->socket.is_open())
					ths->sState2 = SSF_IMMEDIATE_DC;

				if (ths->sState2 != SSF_OK)
					return;
			}
			else
			{
				if (!ths->socket.is_open())
					ths->sState2 = SSF_IMMEDIATE_DC;

				if (ths->sState2 != SSF_OK)
					return;

				assert(bytes_read <= ths->inPacket.size());

				{

					current_udp_endpoint = ths->recv_endpoint;

					try
					{
						ths->eventHandler->onRecieveDatagram(&ths->inPacket[0], bytes_read, ths->recv_endpoint);
					}
					LOG_CATCH();

					current_udp_endpoint = boost::asio::ip::udp::endpoint();

				}

			}

			ths->inPacket.resize(maxDatagramSize);
			ths->socket.async_receive_from(boost::asio::buffer(&ths->inPacket[0], ths->inPacket.size()),
				ths->recv_endpoint,
				boost::bind(&handle_read, ths,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));

		}

		inline void start()
		{
			startRecv(std::static_pointer_cast<UdpServer>(this->shared_from_this()));
		}

		static void startRecv(std::shared_ptr<UdpServer> ths)
		{
			ths->inPacket.resize(maxDatagramSize);
			ths->socket.async_receive_from(boost::asio::buffer(&ths->inPacket[0], ths->inPacket.size()),
				ths->recv_endpoint,
				boost::bind(&handle_read, ths,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
		}
	};

	void init()
	{
	}

	void removeClient(session_id_t id, std::shared_ptr<TcpServer>);

	struct Client
		: public std::enable_shared_from_this<Client>
	{
		enum socket_state_t { SSF_OK, SSF_SOFT_DC, SSF_IMMEDIATE_DC };
		std::atomic<socket_state_t> sState;

		boost::asio::ip::tcp::socket socket;
		session_id_t id;
		unsigned inPos;
		unsigned recvPos;
		bool reciveing;
		bool postponed;
		bool closed;
		unsigned sendStart;
		unsigned sendEnd;
		unsigned last_recieved_time;
		unsigned sendStartTime;
		unsigned sendingSize;
		std::array<char, 64 * 1024> inPacket;
		std::array<char, 256 * 1024> outPacket;

		std::vector<boost::asio::const_buffer> send_buffers;

		void log_client_info(std::string const & userdata = std::string())
		{
			logger << "ID = " << id.id << " Socket is " << this->socket.is_open() << " sState = " << sState << " inPos = " << inPos << " recvPos = " << recvPos << " sendTimeout = " << (sendTimeout(get_tick())) << " sendingSize = " << sendingSize << " sendStart = " << sendStart << " sendEnd = " << sendEnd << " postponed = " << (postponed) << " reciveing = " << (reciveing) << " closed " << (closed) << " userdata: " << userdata;
		}

		int sendTimeout(unsigned tick) const
		{
			if (sendingSize > 0 && tick - sendStartTime > send_timeout)
				return 1;
			return 0;
		}

		Client(boost::asio::io_service & io_service)
			: sState(SSF_OK)
			, socket(io_service)
			, id({0, 0})
			, inPos(0)
			, recvPos(0)
			, reciveing(false)
			, postponed(false)
			, closed(false)
			, sendStart(0)
			, sendEnd(0)
			, sendingSize(0)
		{
			last_recieved_time = get_tick();
			send_buffers.reserve(2);
		}

		~Client()
		{
			if (socket.is_open())
			{
				closed = true;
				boost::system::error_code error;
				LOG_EX(socket.shutdown(boost::asio::socket_base::shutdown_both, error));
				if (error && error != boost::asio::error::not_connected)
					print_boost_error(error);
				LOG_EX(socket.close(error));
				if (error && error != boost::asio::error::not_connected)
					print_boost_error(error);
			}
			send_buffers.clear();
		}

		void set_state(socket_state_t newstate, std::shared_ptr<TcpServer> entity)
		{
			if (newstate != SSF_OK && sState != SSF_IMMEDIATE_DC)
			{
				closed = true;
				try
				{
					if (sState == SSF_OK && newstate == SSF_SOFT_DC)
					{
						if (sendingSize > 0 && socket.is_open())
						{
							sState = SSF_SOFT_DC;
							boost::system::error_code error;
							LOG_EX(socket.shutdown(boost::asio::socket_base::shutdown_receive, error));
							if (error && error != boost::asio::error::not_connected)
							{
								log_client_info();
								print_boost_error(error);
							}
							if (error != boost::asio::error::not_connected)
								return;
						}
					}
				}
				LOG_CATCH();
				sState = SSF_IMMEDIATE_DC;
				try
				{
					if (socket.is_open())
					{
						boost::system::error_code error;
						LOG_EX(socket.shutdown(boost::asio::socket_base::shutdown_both, error));
						if (error && error != boost::asio::error::not_connected)
						{
							log_client_info();
							print_boost_error(error);
						}
						LOG_EX(socket.close(error));
						if (error && error != boost::asio::error::not_connected)
						{
							log_client_info();
							print_boost_error(error);
						}
					}
				}
				LOG_CATCH();

				if (id.id != (unsigned)0)
				{
					if (log_network_events)
						logger << entity->service_name << ": Client " << id.id << " stopped" << std::endl;

					if (entity && entity->networkRunning)
					{
						try
						{
							if (!entity->eventHandler->destroyed())
								entity->eventHandler->onDisconnect(id);
						}
						LOG_CATCH();
					}
					removeClient(id, entity);
				}

				id.id = 0;

			}
		}

		static void handle_write(std::shared_ptr<Client> ths, std::shared_ptr<TcpServer> entity, boost::system::error_code const & error, size_t bytes_written)
		{
			if ((error && error != boost::asio::error::would_block) || bytes_written > ths->sendingSize)
			{
				if (error != boost::asio::error::eof && error != boost::asio::error::connection_reset && ths->sState != Network::Client::SSF_SOFT_DC)
				{
					if (log_network_errors && error && /*(entity->networkRunning || (error.value() != 995 && !entity->networkRunning))*/ (error.value() != 995 || ::debug_build))
					{

						if (entity)
							logger << LIGHT_RED << entity->service_name;
						ths->log_client_info();
						print_boost_error(error);
					}
				}

				ths->set_state(Network::Client::SSF_IMMEDIATE_DC, entity);
				return;
			}
			else
			{
				if (!ths->socket.is_open() && ths->sState != Network::Client::SSF_IMMEDIATE_DC)
				{
					ths->set_state(Network::Client::SSF_IMMEDIATE_DC, entity);
					return;
				}

				if (ths->sState == Network::Client::SSF_IMMEDIATE_DC)
					return;

				ths->sendStart += bytes_written;
				ths->sendStart %= ths->outPacket.size();
				ths->sendingSize = 0;
				ths->sendNext(ths, entity);

				if (ths->sendingSize == 0 && ths->sState == Network::Client::SSF_SOFT_DC)
				{
					ths->set_state(Network::Client::SSF_IMMEDIATE_DC, entity);
					return;
				}
			}
		}
private:

	unsigned send_buffer_size() const
	{
		return (sendEnd >= sendStart) ? (sendEnd - sendStart) : (sendEnd + outPacket.size() - sendStart);
	}

	void sendNext(std::shared_ptr<Client> ths, std::shared_ptr<TcpServer> entity)
	{
		if (sendingSize == 0)
		{
			if (sendEnd != sendStart && socket.is_open())
			{
				assert(sendStart < outPacket.size());
				assert(sendEnd < outPacket.size());
				send_buffers.clear();
				if (sendEnd > sendStart)
				{
					send_buffers.emplace_back<boost::asio::const_buffer>(boost::asio::buffer(&outPacket[sendStart], sendEnd - sendStart));
				}
				else
				{
					if(outPacket.size() - sendStart > 0)
						send_buffers.emplace_back<boost::asio::const_buffer>(boost::asio::buffer(&outPacket[sendStart], outPacket.size() - sendStart));
					if(sendEnd > 0)
						send_buffers.emplace_back<boost::asio::const_buffer>(boost::asio::buffer(&outPacket[0], sendEnd));
				}

				sendingSize = send_buffer_size();
				sendStartTime = get_tick();
				boost::asio::async_write(socket,
					send_buffers,
					boost::bind(&handle_write, ths, entity,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
			}
		}
	}
public:

		void send(std::vector<char> const & data, std::shared_ptr<TcpServer> entity, bool dropable = false)
		{
			unsigned i;
			if (outPacket.size() - 1 - send_buffer_size() < data.size())
			{
				if (log_network_errors)
					logger << LIGHT_RED << "Send buffer overflow";
				set_state(SSF_IMMEDIATE_DC, entity);
				return;
			}
			else
			{
				if (dropable && send_buffer_size() > outPacket.size() * 8 / 10)
					return;

				if (sendEnd + data.size() >= outPacket.size())
				{
					i = 0;
					for (; i < data.size() && sendEnd < outPacket.size(); ++i)
					{
						outPacket[sendEnd] = data[i];
						++sendEnd;
					}
					sendEnd = 0;
					for (; i < data.size(); ++i)
					{
						outPacket[sendEnd] = data[i];
						++sendEnd;
					}
				}
				else
				{
					for (i = 0; i < data.size(); ++i)
					{
						outPacket[sendEnd] = data[i];
						++sendEnd;
					}
				}

				if (sendingSize == 0)
					sendNext(shared_from_this(), entity);

			}

		}

		static void handle_read(std::shared_ptr<Client> ths, std::shared_ptr<TcpServer> entity, boost::system::error_code const & error, size_t bytes_read)
		{
			if (error && error != boost::asio::error::would_block)
			{
				if (error != boost::asio::error::eof && error != boost::asio::error::connection_reset && ths->sState != Network::Client::SSF_IMMEDIATE_DC && ths->sState != Network::Client::SSF_SOFT_DC)
				{
					if (log_network_errors && error && (entity->networkRunning || (error.value() != 995 && !entity->networkRunning)))
					{
						if (entity)
							logger << LIGHT_RED << entity->service_name;
						ths->log_client_info(entity->eventHandler->getConnectionData(ths->id));
						print_boost_error(error);
					}
				}

				ths->set_state(Network::Client::SSF_IMMEDIATE_DC, entity);
				return;
			}
			else
			{
				if (!ths->socket.is_open() || (ths->inPos + bytes_read > ths->inPacket.size()))
				{
					ths->set_state(Network::Client::SSF_IMMEDIATE_DC, entity);
					return;
				}

				if (ths->sState != Network::Client::SSF_OK)
					return;

				ths->inPos += bytes_read;
				ths->reciveing = false;

				bool was_postponed = ths->postponed;

				ths->process_inpacket(ths, entity);

				ths->recvNext(entity, ths, was_postponed && ths->postponed);
			}

		}

	private:
		void process_inpacket(std::shared_ptr<Client> ths2, std::shared_ptr<TcpServer> entity)
		{
			while (inPos >= recvPos + 4 && !postponed)
			{
				unsigned packet_length = *reinterpret_cast<unsigned*>(&inPacket[recvPos]);
				if (inPos >= packet_length + 4 + recvPos)
				{
					current_client_id = id;

					try
					{
						if (!entity->eventHandler->destroyed() && sState == SSF_OK)
							entity->eventHandler->onRecieve(id, &inPacket[recvPos], packet_length + 4);
						last_recieved_time = get_tick();
					}
					catch (postpone &)
					{
						postponed = true;
						current_client_id = session_id_t{ 0, 0 };
						break;
					}
					LOG_CATCH();

					current_client_id = session_id_t{ 0, 0 };

					recvPos += packet_length + 4;
				}
				else
				{
					if (packet_length > inPacket.size() - 4 - recvPos)
					{
						if (recvPos > 0 && !reciveing)
						{
							memmove(&inPacket[0], &inPacket[recvPos], inPos - recvPos);
							inPos -= recvPos;
							recvPos = 0;
						}

						if (packet_length > inPacket.size() - 4 - recvPos)
						{
							bool was_ok = sState == SSF_OK;
							set_state(SSF_SOFT_DC, entity);
							if (was_ok)
							{
								try
								{
									entity->eventHandler->inPacketTooBig(id);
								}
								LOG_CATCH();
							}
						}
					}

					break;
				}
			}

			if (recvPos > 0 && !reciveing)
			{
				memmove(&inPacket[0], &inPacket[recvPos], inPos - recvPos);
				inPos -= recvPos;
				recvPos = 0;
			}
		}

		void recvNext(std::shared_ptr<TcpServer> entity, std::shared_ptr<Client> ths, bool call_postponed = true)
		{
			if (sState == Network::Client::SSF_OK && !reciveing)
			{
				if (postponed && call_postponed)
				{
					postponed = false;
					process_inpacket(ths, entity);
				}

				reciveing = true;
				socket.async_read_some(
					boost::asio::buffer(&inPacket[inPos], inPacket.size() - inPos),
					boost::bind(&handle_read, ths, entity,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
			}
		}
	public:

		void callPostponed(std::shared_ptr<Client> ths, std::shared_ptr<TcpServer> entity)
		{
			if (postponed)
			{
				postponed = false;
				process_inpacket(ths, entity);
			}
		}

		static void startRecv(std::shared_ptr<Client> ths, std::shared_ptr<TcpServer> entity)
		{
			if (ths->sState == Network::Client::SSF_OK && !ths->reciveing)
			{
				if (!ths->postponed)
				{
					ths->inPos = 0;
					ths->recvPos = 0;
				}
				ths->recvNext(entity, ths);
			}
		}
	};

	void TcpServer::cleanup()
	{
		std::shared_ptr<TcpServer> _this = std::static_pointer_cast<TcpServer>(this->shared_from_this());

		try
		{
			if (acceptor)
			{
				if (acceptor_port)
				{
					acceptor_port = 0;
					LOG_EX(acceptor->cancel());
				}
				LOG_EX(acceptor->close());
				LOG_EX(safe_delete(acceptor));
			}
		}
		LOG_CATCH();


		for (unsigned i = 0; i < clients.size(); ++i)
		{
			if (std::shared_ptr<Client> client = clients[i])
			{
				LOG_EX(client->set_state(Network::Client::socket_state_t::SSF_IMMEDIATE_DC, _this));
				client.reset();
			}
		}
		free_ids.clear();
		LOG_EX(clients.clear());
		used_ids.clear();

		if (timer)
			LOG_EX(safe_delete(timer));
	}

	std::shared_ptr<Client> TcpServer::get_client(session_id_t id)
	{
		std::shared_ptr<Client> client;
		if (id.id < clients.size())
		{
			client = clients[id.id];
			if (client && client->id.seq_id != id.seq_id)
				client.reset();
		}
		return client;
	}


	bool start_accept(std::shared_ptr<TcpServer> entity);
	void checkTasks(std::shared_ptr<Server> _entity);

	void startTcpServer(std::string const & ip, unsigned port, NetworkEventHandler * handler, bool volatile * __networkStarted, bool volatile * _networkStartupSuccess, std::shared_ptr<TcpServer> _entity)
	{
		bool volatile & _networkStarted = *__networkStarted;
		bool volatile & networkStartupSuccess = *_networkStartupSuccess;
		try
		{
			bool is_server = ip == "";

			if (is_server)
			{
				if (port != 0)
				{
					try
					{
						_entity->acceptor_port = 0;// port;
						_entity->acceptor = cnew boost::asio::ip::tcp::acceptor(_entity->serviceThread->io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
					}
					catch (std::exception & e)
					{
						catch_message(e);
						networkStartupSuccess = false;
						_networkStarted = true;
						return;
					}
				}
			}
			else
			{
				boost::asio::ip::tcp::resolver::iterator end;

				boost::asio::ip::tcp::resolver resolver(_entity->serviceThread->io_service);
				boost::asio::ip::tcp::resolver::query query((std::string)ip, convert<std::string>(port));

				boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

				if (endpoint_iterator == end)
				{
					networkStartupSuccess = false;
					_networkStarted = true;
					return;
				}

				std::shared_ptr<Client> client = std::make_shared<Client>(_entity->serviceThread->io_service);

				try
				{
					boost::asio::connect(client->socket, endpoint_iterator);
				}
				catch (std::exception & e)
				{
					logger << LIGHT_RED << ERROR_MESSAGE(e) << endl;
					client.reset();
					networkStartupSuccess = false;
					_networkStarted = true;
					return;
				}

				if (!client->socket.is_open())
				{
					client.reset();
					networkStartupSuccess = false;
					_networkStarted = true;
					return;
				}


				if (!_entity->free_ids.empty())
				{
					client->id.id = _entity->free_ids.back();
					client->id.seq_id = ++_entity->seq_id;
					_entity->free_ids.pop_back();
					_entity->used_ids.push_back(client->id.id);
					try
					{
						_entity->clients[client->id.id] = client;
					}
					catch (...)
					{
						_entity->free_ids.push_back(client->id.id);
						_entity->used_ids.pop_back();
						client->id = session_id_t{ 0, 0 };
						throw;
					}
				}
				else
				{
					networkStartupSuccess = false;
					_networkStarted = true;
					return;
				}

				{
					session_id_t sid = client->id;
					DisconnectKind dk = DisconnectKind::ACCEPT;
					try
					{
						if (!_entity->eventHandler->destroyed())
							dk = _entity->eventHandler->onConnect(client->id, std::string(ip), port);
						else
							dk = DisconnectKind::IMMEDIATE_DC;
//						if (dk == DisconnectKind::SOFT_DC)
//							dk = DisconnectKind::ACCEPT;//TODO /*1*/
					}
					LOG_CATCH();
					if (client == _entity->get_client(sid))
					{
						if (dk == DisconnectKind::ACCEPT)
							client->startRecv(client, _entity);
						else
							_disconnect(_entity, sid, dk);
					}
				}
			}

			_entity->timer = cnew boost::asio::deadline_timer(_entity->serviceThread->io_service, boost::posix_time::milliseconds(std::max<unsigned>(1, timer_msecs)));

			++_entity->taskCount;
			_entity->timer->async_wait(std::bind(&checkTasks, _entity));


			networkStartupSuccess = true;
			_networkStarted = true;
			_entity->networkRunning = true;

			LOG_EX(handler->onInit());

			if (port)
				logger << trim(_entity->service_name) << " on port " << port << std::endl;
			else
				logger << trim(_entity->service_name) << "started" << endl;

		}
		catch (std::exception & e)
		{
			catch_message(e);
			networkStartupSuccess = false;
			_networkStarted = true;
			_entity = nullptr;
			return;
		}
	}

	void startUdpServer(unsigned port, UdpNetworkEventHandler * handler, std::shared_ptr<UdpServer> _entity)
	{
		_entity->timer = cnew boost::asio::deadline_timer(_entity->serviceThread->io_service, boost::posix_time::milliseconds(std::max<unsigned>(1, timer_msecs)));

		++_entity->taskCount;
		_entity->timer->async_wait(std::bind(&checkTasks, _entity));

		LOG_EX(handler->onInit());

		_entity->networkRunning = true;

		_entity->start();

		if (port)
			logger << "Network started on port " << port << std::endl;
		else
			logger << "Interface started" << endl;
	}


	bool startNetwork(std::string const & service_name, channel_id_t & c_addr, std::string const & ip, unsigned port, NetworkEventHandler * handler, bool no_delay, std::shared_ptr<ServiceThread> serviceThread)
	{
		assert(handler != nullptr);
		if (handler == nullptr)
			return false;

		if (!serviceThread)
			serviceThread = std::make_shared<ServiceThread>();

		volatile bool networkStarted = false;
		volatile bool networkStartupSuccess = false;
		std::shared_ptr<TcpServer> entity;
		if (c_addr)
		{
			entity = std::dynamic_pointer_cast<TcpServer>(c_addr);
			if (!assert2((bool)entity))
			{
				logger << LIGHT_RED << __FILE__ << ':' << __LINE__ << ": internal server error(2)" << endl;
				return false;
			}
			assert(entity->networkRunning == false);
			if (entity->networkRunning)
				return 0;
		}
		else
		{
			entity = std::make_shared<TcpServer>();
			c_addr = entity;
			handler->_port = port;
			handler->channel_addr = c_addr;
		}

		entity->service_name = service_name;
		entity->no_delay = no_delay;
		entity->eventHandler = handler;

		entity->networkRunning = true;
		entity->serviceThread = serviceThread;
		serviceThread->start(entity);
		serviceThread->io_service.post(std::bind(startTcpServer, ip, port, handler, &networkStarted, &networkStartupSuccess, entity));

		while (!networkStarted)
			sleep_for(10);

		if (networkStartupSuccess)
		{
			return true;
		}
		else
			return 0;
	}

	bool startNetwork(std::string const & service_name, channel_id_t & c_addr, unsigned port, UdpNetworkEventHandler * handler, std::shared_ptr<ServiceThread> serviceThread)
	{
		assert(handler != nullptr);
		if (handler == nullptr)
			return 0;

		std::shared_ptr<UdpServer> entity;
		if (c_addr)
		{
			entity = std::dynamic_pointer_cast<UdpServer>(c_addr);
			if (!assert2((bool)entity))
			{
				logger << LIGHT_RED << __FILE__ << ':' << __LINE__ << ": internal server error(2)" << endl;
				return false;
			}
			assert(entity->networkRunning == false);
			if (entity->networkRunning)
				return false;
		}
		else
		{
			if (!serviceThread)
				serviceThread = std::make_shared<ServiceThread>();

			entity = std::make_shared<UdpServer>(&serviceThread->io_service, port);
			c_addr = entity;
		}

		entity->service_name = service_name;
		entity->eventHandler = handler;

		entity->networkRunning = true;
		serviceThread->start(entity);
		serviceThread->io_service.post(std::bind(startUdpServer, port, handler, entity));

		return true;
	}

	void removeClient(session_id_t id, std::shared_ptr<TcpServer> entity)
	{
		if (std::shared_ptr<Client> client = entity->get_client(id))
		{
			entity->free_ids.push_back(id.id);
			for (unsigned i = 0, e = entity->used_ids.size(); i < e; ++i)
			{
				if (entity->used_ids[i] == id.id)
				{
					entity->used_ids[i] = entity->used_ids.back();
					entity->used_ids.pop_back();
					break;
				}
			}

			entity->clients[id.id].reset();
		}
	}

	void handle_accept(std::shared_ptr<Client> new_connection, std::shared_ptr<TcpServer> entity, const boost::system::error_code& error)
	{
		entity->accepting = false;
		try
		{
			if (!error && entity->acceptor_port && new_connection->socket.is_open() && !entity->free_ids.empty())
			{
				if (entity->no_delay)
				{
					boost::asio::ip::tcp::no_delay option(true);
					LOG_EX(new_connection->socket.set_option(option));
				}

				new_connection->id.id = entity->free_ids.back();
				new_connection->id.seq_id = ++entity->seq_id;
				entity->free_ids.pop_back();
				entity->used_ids.push_back(new_connection->id.id);
				entity->clients[new_connection->id.id] = new_connection;

				boost::asio::ip::tcp::endpoint remote_ep = new_connection->socket.remote_endpoint();
				boost::asio::ip::address remote_ad = remote_ep.address();

				std::string ip_add = remote_ad.to_string();

				if (log_network_events)
					logger << entity->service_name << ": Client " << new_connection->id.id << " connected. Ip = " << ip_add << " port = " << remote_ep.port() << endl;

				{
					session_id_t sid = new_connection->id;
					DisconnectKind dk = DisconnectKind::ACCEPT;
					try
					{
						if (!entity->eventHandler->destroyed())
							dk = entity->eventHandler->onConnect(new_connection->id, std::move(ip_add), remote_ep.port());
						else
							dk = DisconnectKind::IMMEDIATE_DC;
//						if (dk == DisconnectKind::SOFT_DC)
//							dk = DisconnectKind::ACCEPT;//TODO /*1*/
					}
					LOG_CATCH();
					if (new_connection == entity->get_client(sid))
					{
						if (dk == DisconnectKind::ACCEPT)
							new_connection->startRecv(new_connection, entity);
						else
							_disconnect(entity, sid, dk);
					}

				}
			}
			else
			{
				if (error && error != boost::asio::error::eof && error != boost::asio::error::connection_reset)
				{
					if (log_network_errors && error && (entity->networkRunning || (error.value() != 995 && error.value() != 125 && !entity->networkRunning)))
						print_boost_error(error);
				}
				if (new_connection)
					new_connection->set_state(Network::Client::socket_state_t::SSF_IMMEDIATE_DC, entity);
				new_connection.reset();
			}
		}
		catch (std::exception & e)
		{
			catch_message(e);
			if (new_connection)
			{
				try
				{
					new_connection->set_state(Network::Client::socket_state_t::SSF_IMMEDIATE_DC, entity);
					new_connection.reset();
				}
				LOG_CATCH();
			}
		}
		start_accept(entity);
	}

	void _set_accept_open(unsigned value, std::shared_ptr<TcpServer> entity)
	{
		if (entity->acceptor_port != value)
		{
			entity->acceptor_port = value;
			if (entity->acceptor_port)
			{
				start_accept(entity);
			}
			else
			{
				if (entity->acceptor)
				{
					entity->acceptor = nullptr;
				}
			}
		}
	}

	bool start_accept(std::shared_ptr<TcpServer> entity)
	{
		if (entity->networkRunning && entity->acceptor_port && !entity->accepting && !entity->free_ids.empty())
		{
			if (std::shared_ptr<ServiceThread> serviceThread = entity->serviceThread)
			{
				try
				{
					std::shared_ptr<Client> new_connection = std::make_shared<Client>(serviceThread->io_service);
					if (entity->acceptor == nullptr)
						entity->acceptor = cnew boost::asio::ip::tcp::acceptor(serviceThread->io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), entity->acceptor_port));

					if (entity->acceptor)
					{
						try
						{
							entity->acceptor->async_accept(new_connection->socket,
								boost::bind(&handle_accept, new_connection, entity,
									boost::asio::placeholders::error));

							entity->accepting = true;
							return true;
						}
						LOG_CATCH();
					}
				}
				LOG_CATCH();
			}
		}
		return false;
	}

	void checkTasks(std::shared_ptr<Server> _entity)
	{
		if (_entity->networkRunning)
		{
			_entity->timer->expires_at(_entity->timer->expires_at() + boost::posix_time::milliseconds(std::max<unsigned>(1, timer_msecs)));
			_entity->timer->async_wait(std::bind(&checkTasks, _entity));
			if (std::shared_ptr<TcpServer> e = std::dynamic_pointer_cast<TcpServer>(_entity))
			{
				try
				{
					e->eventHandler->onTimer();
				}
				LOG_CATCH();

				unsigned tick = get_tick();

				if (tick - e->last_send_check > 60000)
				{
					e->last_send_check = tick;
					/*2*/
					std::vector<unsigned> check_ids = e->used_ids;
					for (unsigned id : check_ids)
					{
						if (std::shared_ptr<Client> client = e->clients[id])
						{
							if (unsigned reason = client->sendTimeout(tick))
							{
								if (log_network_errors)
								{
									logger << "Client id " << id << (reason == 1 ? " send" : " recv") << " timeout in " << e->service_name;
									client->log_client_info(e->eventHandler->getConnectionData(client->id));
								}
								client->set_state(Network::Client::socket_state_t::SSF_IMMEDIATE_DC, e);
							}
						}
					}
				}

				if (!e->accepting && e->acceptor_port && !e->free_ids.empty())
					start_accept(e);

			}
		}
		else
		{
			LOG_EX(_entity->cleanup());
			--_entity->taskCount;
			logger << trim(_entity->service_name) << " down\r\n";
		}

	}

	void stopNetwork(channel_id_t & cid)
	{
		if (cid)
		{
			if (cid->networkRunning != 0)
				cid->stop();

			cid.reset();
		}
	}

	void _distribute(std::shared_ptr<TcpServer> entity, std::vector<char> const & data, session_id_t skipId)
	{
		for (unsigned i = 0; i < entity->clients.size(); ++i)
		{
			if (std::shared_ptr<Client> client = entity->clients[i])
			{
				if (i == skipId.id)
				{
					if (!client)
						continue;
					else
						if (client->id.seq_id == skipId.seq_id)
							continue;
				}

				LOG_EX(client->send(data, entity));
			}
		}
	}

	bool distribute(channel_id_t cid, std::vector<char> && data, session_id_t skipId)
	{
		if (cid)
		{
			assert(data.size() >= 4);
			if (data.size() < 4)
				return data.size() == 0;
//			if (data.size() < 8)
//				return false;


			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					try
					{
						*reinterpret_cast<unsigned*>(&data[0]) = data.size() - 4;
						st->io_service.post(std::bind(&_distribute, entity, std::move(data), skipId));
						return true;
					}
					LOG_CATCH();
				}
			}
		}
		return false;
	}

	void _sendDatagramTo(std::shared_ptr<UdpServer> entity, boost::asio::ip::udp::endpoint endpoint, std::shared_ptr<std::vector<char> > data)
	{
		entity->sendQueue.push_back(std::make_tuple(data, endpoint));
		if (entity->sendQueue.size() == 1)
			entity->sendNext();
	}

	bool sendDatagramTo(channel_id_t cid, boost::asio::ip::udp::endpoint const & endpoint, std::vector<char> && data)
	{
		if (cid)
		{
//			if (data.size() == 0)
//				return false;

			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<UdpServer> entity = std::dynamic_pointer_cast<UdpServer>(cid))
				{
					//*reinterpret_cast<unsigned*>(&data[0]) = data.size() - 4;
					st->io_service.post(std::bind(&_sendDatagramTo, entity, endpoint, std::make_shared<std::vector<char> >(std::move(data))));
					return true;
				}
			}
		}
		return false;
	}

	void _sendTo(std::shared_ptr<TcpServer> entity, session_id_t id, std::vector<char> const & data)
	{
		if (std::shared_ptr<Client> client = entity->get_client(id))
			client->send(data, entity);
		else
			logger << LIGHT_RED << "Invalid client id: " << id.id << endl;
	}


	bool sendTo(channel_id_t cid, session_id_t id, std::vector<char> && data)
	{
		if (cid)
		{
			assert(data.size() >= 4);
			if (data.size() < 4)
				return data.size() == 0;
//			if (data.size() <= 4)
//				return data.size() == 0;

			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					*reinterpret_cast<unsigned*>(&data[0]) = data.size() - 4;
					try
					{
						st->io_service.dispatch(std::bind(&_sendTo, entity, id, std::move(data)));
						return true;
					}
					LOG_CATCH();
				}
			}
		}
		return false;
	}

	void _sendToDropable(std::shared_ptr<TcpServer> entity, session_id_t id, std::vector<char> const & data)
	{
		if (std::shared_ptr<Client> client = entity->get_client(id))
			client->send(data, entity, true);
		else
			logger << LIGHT_RED << "Invalid client id: " << id.id << endl;
	}


	bool sendToDropable(channel_id_t cid, session_id_t id, std::vector<char> && data)
	{
		if (cid)
		{
			assert(data.size() >= 4);
			if (data.size() < 4)
				return data.size() == 0;
			//			if (data.size() <= 4)
			//				return data.size() == 0;

			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					*reinterpret_cast<unsigned*>(&data[0]) = data.size() - 4;
					try
					{
						st->io_service.dispatch(std::bind(&_sendToDropable, entity, id, std::move(data)));
						return true;
					}
					LOG_CATCH();
				}
			}
		}
		return false;
	}

	void _dispatch(std::shared_ptr<TcpServer> entity, std::shared_ptr<std::vector<char> > data)
	{
		current_client_id = session_id_t{ 0, 0 };

		try
		{
			if(!entity->eventHandler->destroyed())
				entity->eventHandler->onRecieve(session_id_t{0, 0}, data->data(), data->size());
		}
		LOG_CATCH();
	}

	bool dispatch(channel_id_t cid, std::vector<char> && data)
	{
		if (cid)
		{
			assert(data.size() >= 4);
			if (data.size() < 4)
				return false;

			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					*reinterpret_cast<unsigned*>(&data[0]) = data.size() - 4;
					try
					{
						st->io_service.post(std::bind(&_dispatch, entity, std::make_shared<std::vector<char> >(std::move(data))));
						return true;
					}
					LOG_CATCH();
				}
			}
		}
		return false;
	}

	void _sendTo2(std::shared_ptr<TcpServer> entity, std::vector<session_id_t> const & ids, std::vector<char> const & data, session_id_t skipId)
	{
		for (session_id_t id : ids)
		{
			if (id == skipId)
				continue;

			if (std::shared_ptr<Client> client = entity->get_client(id))
				client->send(data, entity);
			else
				logger << LIGHT_RED << "Invalid client id: " << id.id << endl;
		}
	}

	unsigned sendTo(channel_id_t cid, std::vector<session_id_t> const & ids, std::vector<char> && data, session_id_t skipId)
	{
		if (cid)
		{
			assert(data.size() >= 4);
			if (data.size() < 4)
				return data.size() == 0;
			//			if (data.size() <= 4)
			//				return data.size() == 0;

			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					*reinterpret_cast<unsigned*>(&data[0]) = data.size() - 4;
					try
					{
						st->io_service.dispatch(std::bind(&_sendTo2, entity, ids, std::move(data), skipId));
						return true;
					}
					LOG_CATCH();
				}
			}
		}
		return false;
	}


	void _sendTo2Dropable(std::shared_ptr<TcpServer> entity, std::vector<session_id_t> const & ids, std::vector<char> const & data, session_id_t skipId)
	{
		for (session_id_t id : ids)
		{
			if (id == skipId)
				continue;

			if (std::shared_ptr<Client> client = entity->get_client(id))
				client->send(data, entity, true);
			else
				logger << LIGHT_RED << "Invalid client id: " << id.id << endl;
		}
	}

	unsigned sendToDropable(channel_id_t cid, std::vector<session_id_t> const & ids, std::vector<char> && data, session_id_t skipId)
	{
		if (cid)
		{
			assert(data.size() >= 4);
			if (data.size() < 4)
				return data.size() == 0;
			//			if (data.size() <= 4)
			//				return data.size() == 0;

			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					*reinterpret_cast<unsigned*>(&data[0]) = data.size() - 4;
					try
					{
						st->io_service.dispatch(std::bind(&_sendTo2Dropable, entity, ids, std::move(data), skipId));
						return true;
					}
					LOG_CATCH();
				}
			}
		}
		return false;
	}

	void _disconnect(std::shared_ptr<TcpServer> entity, session_id_t id, DisconnectKind disconnectKind)
	{
		try
		{
			if (std::shared_ptr<Client> client = entity->get_client(id))
			{
				if (disconnectKind != DisconnectKind::IMMEDIATE_DC)
					client->set_state(Network::Client::SSF_SOFT_DC, entity);
				else
				{
					client->set_state(Network::Client::SSF_IMMEDIATE_DC, entity);
					return;
				}
			}
		}
		LOG_CATCH();
	}


	bool disconnect(channel_id_t cid, session_id_t id, DisconnectKind disconnectKind)
	{
		if (cid)
		{
			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					try
					{
						st->io_service.post(std::bind(&_disconnect, entity, id, disconnectKind));
						return true;
					}
					LOG_CATCH();
				}
			}
		}
		return false;
	}

	void _callPostponed(std::shared_ptr<TcpServer> entity, session_id_t id)
	{
		try
		{
			if (std::shared_ptr<Client> client = entity->get_client(id))
				client->callPostponed(client, entity);
		}
		LOG_CATCH();
	}

	void callPostponed(channel_id_t cid, session_id_t id)
	{
		if (cid)
		{
			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					try
					{
						st->io_service.post(std::bind(&_callPostponed, entity, id));
					}
					LOG_CATCH();
				}
			}
		}
	}

	bool set_accept_open(channel_id_t cid, unsigned value)
	{
		if (cid)
		{
			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					try
					{
						st->io_service.post(std::bind(&_set_accept_open, value, entity));
						return true;
					}
					LOG_CATCH();
				}
			}
		}
		return false;
	}

	void _addNetworkOneTimeTask(void(*func)(), std::shared_ptr<Server> entity)
	{
		LOG_EX(func());
	}

	bool addNetworkOneTimeTask(channel_id_t cid, void(*func)())
	{
		if (cid)
		{
			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				try
				{
					st->io_service.post(std::bind(&_addNetworkOneTimeTask, func, cid));
					return true;
				}
				LOG_CATCH();
			}
		}
		return false;
	}

	void _connect(std::shared_ptr<TcpServer> entity, std::string const & ip, unsigned port, bool no_delay)
	{
		try
		{
			boost::asio::ip::tcp::resolver::iterator end;

			boost::asio::ip::tcp::resolver resolver(entity->serviceThread->io_service);
			boost::asio::ip::tcp::resolver::query query((std::string)ip, convert<std::string>(port));

			boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

			if (endpoint_iterator != end)
			{
				std::shared_ptr<Client> client = std::make_shared<Client>(entity->serviceThread->io_service);

				try
				{
					boost::asio::connect(client->socket, endpoint_iterator);

					if (client->socket.is_open() && !entity->free_ids.empty())
					{
						client->id.id = entity->free_ids.back();
						client->id.seq_id = ++entity->seq_id;
						entity->free_ids.pop_back();
						entity->used_ids.push_back(client->id.id);
						entity->clients[client->id.id] = client;

						session_id_t sid = client->id;
						DisconnectKind dk = DisconnectKind::ACCEPT;
						try
						{
							if (!entity->eventHandler->destroyed())
								dk = entity->eventHandler->onConnect(client->id, std::string(ip), port);
							else
								dk = DisconnectKind::IMMEDIATE_DC;
//							if (dk == DisconnectKind::SOFT_DC)
//								dk = DisconnectKind::ACCEPT;//TODO /*1*/
						}
						LOG_CATCH();
						if (client == entity->get_client(sid))
						{
							if (dk == DisconnectKind::ACCEPT)
								client->startRecv(client, entity);
							else
								_disconnect(entity, sid, dk);
						}


						return;
					}
				}
				LOG_CATCH();

				client.reset();
			}
		}
		LOG_CATCH();
	}

	bool connect(channel_id_t cid, std::string const & ip, unsigned port, bool no_delay)
	{
		if (cid)
		{
			if (std::shared_ptr<ServiceThread> st = cid->running())
			{
				if (std::shared_ptr<TcpServer> entity = std::dynamic_pointer_cast<TcpServer>(cid))
				{
					try
					{
						st->io_service.post(std::bind(&_connect, entity, ip, port, no_delay));
						return true;
					}
					LOG_CATCH();
				}
			}
		}

		return false;
	}

	void dump(const char * data, unsigned len, const char * desc)
	{
		logger_session log = logger.print("%s packet dump(%d):\r\n", (desc ? desc : ""), len);
		unsigned k, k1;
		for (k = 0; k < len;)
		{

			for (k1 = 0; k1 < 16 && (k + k1) < len; ++k1)
			{
				log.print("%02x ", (unsigned int)((int)data[k + k1] & 0xff));
				if (k1 == 7)
					log.print(" ");
			}
			for (; k1 < 16; ++k1)
			{
				log.print("   ");
				if (k1 == 7)
					log.print(" ");
			}
			for (k1 = 0; k1 < 16 && k < len; ++k1, ++k)
			{
				unsigned char ch = data[k];
				if (ch != 0 && ch != 7 && ch != 8 && ch != 9 && ch != 10 && ch != 13)
					log.print("%c", ch);
				else
					log.print(" ");
			}

			log.print("\r\n");
		}
	}

	unsigned string2ipv4(const char * ip)
	{
		try
		{
			boost::system::error_code error = boost::asio::error::host_not_found;
			boost::asio::ip::address_v4 a = boost::asio::ip::address_v4::from_string(ip, error);
			if(!error)
				return htonl(a.to_ulong());
		}
		catch (boost::exception &)
		{
		}
		try
		{
			boost::asio::io_service io_service;
			boost::asio::ip::tcp::resolver::iterator end;

			boost::asio::ip::tcp::resolver resolver(io_service);
			boost::asio::ip::tcp::resolver::query query((std::string)ip, "0");

			boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

			io_service.run();

			for (; endpoint_iterator != end; ++endpoint_iterator)
			{
				boost::asio::ip::tcp::endpoint ep = endpoint_iterator->endpoint();
				boost::asio::ip::address remote_ad = ep.address();

				std::string ip_add = remote_ad.to_string();

				boost::system::error_code error = boost::asio::error::host_not_found;

				try
				{
					boost::asio::ip::address_v4 a = boost::asio::ip::address_v4::from_string(ip_add, error);
					if(!error)
						return htonl(a.to_ulong());
				}
				catch (boost::exception &)
				{
				}
			}
		}
		catch (boost::exception &)
		{
		}
		return 0;
	}

	std::string ipv42string(unsigned ip)
	{
		char puffer[16];
		sprintf(puffer, "%d.%d.%d.%d", (unsigned)(ip & 0xff), (unsigned)((ip >> 8) & 0xff), (unsigned)((ip >> 16) & 0xff), (unsigned)((ip >> 24) & 0xff));
		return &puffer[0];
	}
}
