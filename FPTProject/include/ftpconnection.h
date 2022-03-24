#pragma once
#include<asio.hpp>
#include<deque>
#include"ftp_request.h"



class ftp_connection : public std::enable_shared_from_this<ftp_connection>
{

public:
	enum class conn_type
	{
		control,
		data,
		server_remote
	};

	enum class conn_founder
	{
		client,
		server
	};


	ftp_connection(conn_type t_connType, 
		conn_founder t_connFounder, 
		asio::ip::tcp::socket t_connSocket,
		asio::io_context& t_connContext,
		std::deque<ftp_request>& t_founderRequests)
			: m_conn_socket(std::move(t_connSocket)), m_conn_context(t_connContext), m_recieved_requests(t_founderRequests)
	{
		m_conn_type = t_connType;
		m_conn_founder = t_connFounder;
	}


	~ftp_connection(){}


	auto GetId() const
	{
		return m_conn_id;
	}

	auto SetId(uint32_t id)
	{
		m_conn_id = id;
	}

	auto& GetSocket()
	{
		return m_conn_socket;
	}


	void Disconnect()
	{
		asio::post(m_conn_context,
			[this]() -> void
			{
				m_conn_socket.close();
			});
	}

	bool IsSocketOpen() const
	{
		return m_conn_socket.is_open();
	}

	void Write(const ftp_request& req)
	{
		asio::post(m_conn_context,
			[this, req]() -> void
			{
				auto writing_message = !m_written_requests.empty();
				m_written_requests.push_back(req);

				if(!writing_message)
				{
					AsyncWriteHeader();
				}
			});
	}

	void StartReading()
	{
		AsyncReadHeader();
	}

private:
	conn_type m_conn_type;
	conn_founder m_conn_founder;
	asio::ip::tcp::socket m_conn_socket;
	asio::io_context& m_conn_context;
	std::deque<ftp_request> m_written_requests;
	std::deque<ftp_request>& m_recieved_requests;
	ftp_request m_cache_request;
	uint32_t m_conn_id;

	void AsyncWriteHeader()
	{
		asio::async_write(m_conn_socket, asio::buffer(&m_written_requests.front().header, sizeof ftp_request_header), asio::transfer_all(),
		[this](std::error_code ec, std::size_t length) -> void
		{
			if(!ec)
			{
				if(m_written_requests.front().header.request_size > 0)
					AsyncWriteBuffer();

				else
				{
					m_written_requests.pop_front();

					if (!m_written_requests.empty())
						AsyncWriteHeader();
				}
				 
			}
			else
			{
				std::cout << "Writing ftp_request failed. Closing socket. \n";
				m_conn_socket.close();
			}
		});
	}

	void AsyncWriteBuffer()
	{
		asio::async_write(m_conn_socket, asio::buffer(m_written_requests.front().mem_buffer.data(), m_written_requests.front().header.request_size), asio::transfer_all(),
			[this](std::error_code ec, std::size_t length) -> void
			{
				if(!ec)
				{
					m_written_requests.pop_front();

					if (!m_written_requests.empty())
						AsyncWriteHeader();
				}

				else
				{
					std::cout << "Writing ftp_request failed. Closing socket. \n";
					m_conn_socket.close();
				}
			});
	}

	void AsyncReadHeader()
	{
		asio::async_read(m_conn_socket, asio::buffer(&m_cache_request.header, sizeof ftp_request_header), asio::transfer_all(),
			[this](std::error_code ec, std::size_t length) -> void
			{
				if(!ec)
				{
					if(m_cache_request.header.request_size > 0)
					{
						m_cache_request.mem_buffer.resize(m_cache_request.header.request_size);
						AsyncReadBuffer();
					}
					else
					{
						//erasing left-over data from previous requests
						m_cache_request.mem_buffer.erase(m_cache_request.mem_buffer.cbegin(), m_cache_request.mem_buffer.cend());
						WriteCacheRequest();
					}
				}
				else
				{
					std::cout << "Reading header failed. \n";
					m_conn_socket.close();
				}
			});
	}

	void AsyncReadBuffer()
	{
		asio::async_read(m_conn_socket, asio::buffer(m_cache_request.mem_buffer.data(), m_cache_request.header.request_size), asio::transfer_all(),
			[this](std::error_code ec, std::size_t length) -> void
			{
				if(!ec)
				{
					WriteCacheRequest();
				}
				else
				{
					std::cout << "Reading body failed. \n";
					m_conn_socket.close();
				}
			});
	}

	void WriteCacheRequest()
	{
		m_conn_founder == conn_founder::server ? m_cache_request.AssignSender(shared_from_this()) : m_cache_request.AssignSender(nullptr);
		m_recieved_requests.push_back(m_cache_request);
		AsyncReadHeader();
	}


	
};