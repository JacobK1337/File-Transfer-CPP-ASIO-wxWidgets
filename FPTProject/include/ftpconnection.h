#pragma once
#include<asio.hpp>
#include<asio/ts/buffer.hpp>
#include<asio/ts/internet.hpp>
#include<deque>
#include"request.h"

class ftp_connection : public std::enable_shared_from_this<ftp_connection>
{

public:
	enum class conn_type
	{
		control,
		data
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
		std::deque<request>& t_founderRequests)
			: m_connSocket(std::move(t_connSocket)), m_connContext(t_connContext), m_toFounderRequests(t_founderRequests)
	{
		m_connType = t_connType;
		m_connFounder = t_connFounder;
	}
	~ftp_connection(){}


	auto GetId() const
	{
		return m_clientId;
	}

	void ListenToServer(const asio::ip::tcp::resolver::endpoint_type& t_serverEndpoint)
	{

		m_connSocket.async_connect(t_serverEndpoint,
			[this](std::error_code ec) -> void
			{
				if(!ec)
				{
					std::cout << "Connection type: ";
					m_connType == conn_type::control ? std::cout << "CONTROL \n" : std::cout << "DATA \n";
					std::cout << "Local endpoint: " << m_connSocket.local_endpoint() << "\n";
					ReadRequestHeader();
				}
			});

	}

	void ListenToClient(uint32_t t_clientId)
	{
		if(m_connSocket.is_open())
		{
			m_clientId = t_clientId;
			ReadRequestHeader();
		}
	}

	void Disconnect()
	{
		asio::post(m_connContext,
			[this]() -> void
			{
				m_connSocket.close();
			});
	}

	bool IsSocketOpen() const
	{
		return m_connSocket.is_open();
	}

public:
	void Send(const request& req)
	{
		asio::post(m_connContext,
			[this, req]() -> void
			{
				auto writing_message = !m_byFounderRequests.empty();
				m_byFounderRequests.push_back(req);

				if(!writing_message)
				{
					WriteRequestHeader();
				}
			});
	}
private:
	void WriteRequestHeader()
	{
		asio::async_write(m_connSocket, asio::buffer(&m_byFounderRequests.front().request_header, sizeof(request_code)), asio::transfer_all(),
		[this](std::error_code ec, std::size_t length) -> void
		{
			if(!ec)
			{
				if(m_byFounderRequests.front().request_header.request_size > 0)
					WriteRequestBody();

				else
				{
					m_byFounderRequests.pop_front();

					if (!m_byFounderRequests.empty())
						WriteRequestHeader();
				}
				 
			}
			else
			{
				std::cout << "Writing request failed. Closing socket. \n";
				m_connSocket.close();
			}
		});
	}

	void WriteRequestBody()
	{
		asio::async_write(m_connSocket, asio::buffer(m_byFounderRequests.front().request_body.data(), m_byFounderRequests.front().request_header.request_size), asio::transfer_all(),
			[this](std::error_code ec, std::size_t length) -> void
			{
				if(!ec)
				{
					m_byFounderRequests.pop_front();

					if (!m_byFounderRequests.empty())
						WriteRequestHeader();
				}

				else
				{
					std::cout << "Writing request failed. Closing socket. \n";
					m_connSocket.close();
				}
			});
	}

	void ReadRequestHeader()
	{
		asio::async_read(m_connSocket, asio::buffer(&m_cacheRequest.request_header, sizeof(request_code)), asio::transfer_all(),
			[this](std::error_code ec, std::size_t length) -> void
			{
				if(!ec)
				{
					if(m_cacheRequest.request_header.request_size > 0)
					{
						m_cacheRequest.request_body.resize(m_cacheRequest.request_header.request_size);
						ReadRequestBody();
					}
					else
					{
						//erasing left-over data from previous requests
						m_cacheRequest.request_body.erase(m_cacheRequest.request_body.cbegin(), m_cacheRequest.request_body.cend());
						SendCacheRequest();
					}
				}
				else
				{
					std::cout << "Reading header failed. \n";
					m_connSocket.close();
				}
			});
	}

	void ReadRequestBody()
	{
		asio::async_read(m_connSocket, asio::buffer(m_cacheRequest.request_body.data(), m_cacheRequest.request_header.request_size), asio::transfer_all(),
			[this](std::error_code ec, std::size_t length) -> void
			{
				if(!ec)
				{
					SendCacheRequest();
				}
				else
				{
					std::cout << "Reading body failed. \n";
					m_connSocket.close();
				}
			});
	}

	void SendCacheRequest()
	{
		if(m_connFounder == conn_founder::server)
			m_cacheRequest.AssignSender(shared_from_this());
		
		else
			m_cacheRequest.AssignSender(nullptr);
		

		m_toFounderRequests.push_back(m_cacheRequest);

		ReadRequestHeader();
	}
private:
	conn_type m_connType;
	conn_founder m_connFounder;
	asio::ip::tcp::socket m_connSocket;
	asio::io_context& m_connContext;

public:
	
	//
private:
	std::deque<request> m_byFounderRequests;
	std::deque<request>& m_toFounderRequests;

	//
	request m_cacheRequest;

	uint32_t m_clientId;
};