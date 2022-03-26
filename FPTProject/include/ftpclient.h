#pragma once
#include<asio.hpp>
#include<deque>
#include"ftp_request.h"
#include"ftpconnection.h"

class ftp_client
{
private:
	asio::io_context m_control_context;
	asio::io_context m_data_context;

	std::thread m_thread_control_context;
	std::thread m_thread_data_context;

	asio::ip::tcp::socket m_control_socket;
	asio::ip::tcp::socket m_data_socket;

	asio::ip::tcp::resolver::results_type m_control_conn_endpoints;
	asio::ip::tcp::resolver::results_type m_data_conn_endpoints;

	std::shared_ptr<ftp_connection> m_control_conn;
	std::shared_ptr<ftp_connection> m_data_conn;

	std::deque<ftp_request> m_control_requests;
	std::deque<ftp_request> m_data_requests;

public:
	ftp_client() : m_control_socket(m_control_context), m_data_socket(m_data_context)
	{
		
	}

	~ftp_client()
	{
		
	}


	bool EstablishControlConnection(const std::string& host, const uint16_t port)
	{

		try
		{

			asio::ip::tcp::resolver resolver(m_control_context);
			auto endpoints = resolver.resolve(host, std::to_string(port));

			m_control_conn = std::make_shared<ftp_connection>(
				ftp_connection::conn_type::control,
				ftp_connection::conn_founder::client,
				std::move(m_control_socket),
				m_control_context,
				m_control_requests
				);

			ListenToServer(endpoints->endpoint(), m_control_conn);

			m_thread_control_context = std::thread(
				[this]() -> void
				{
					m_control_context.run();
				});
		}

		catch(std::exception& e)
		{
			std::cerr << "Exception: " << e.what() << "\n";
			return false;
		}

		return true;
	}


	bool EstablishDataConnection(const std::string& host, const uint16_t port)
	{
		try
		{
			asio::ip::tcp::resolver resolver(m_data_context);
			auto endpoints = resolver.resolve(host, std::to_string(port));

			m_data_conn = std::make_shared<ftp_connection>(
				ftp_connection::conn_type::data,
				ftp_connection::conn_founder::client,
				std::move(m_data_socket),
				m_data_context,
				m_data_requests
				);

			ListenToServer(endpoints->endpoint(), m_data_conn);

			m_thread_data_context = std::thread(
				[this]() -> void
				{
					m_data_context.run();
				}
			);
			 
		}
		catch(std::exception& e)
		{
			std::cerr << "Exception: " << e.what() << "\n";

			return false;
		}

		return true;
	}

	void ListenToServer(const asio::ip::tcp::resolver::endpoint_type& t_serverEndpoint, std::shared_ptr<ftp_connection> t_conn) const
	{
		t_conn->GetSocket().async_connect(t_serverEndpoint,
			[this, t_conn](std::error_code ec) -> void
			{
				if (!ec)
				{
					t_conn->StartReading();
				}
			});
	}

	void ControlStreamDisconnect()
	{
		if (IsControlStreamConnected())
			m_control_conn->Disconnect();

		m_control_context.stop();
		if (m_thread_control_context.joinable())
			m_thread_control_context.join();
		 
	}

	void DataStreamDisconnect()
	{
		if (IsDataStreamConnected())
			m_data_conn->Disconnect();

		m_data_context.stop();
		if (m_thread_data_context.joinable())
			m_thread_data_context.join();
	}

	bool IsControlStreamConnected()
	{
		if (m_control_conn)
			return m_control_conn->IsSocketOpen();

		else
			return false;
	}

	bool IsDataStreamConnected()
	{
		if (m_data_conn)
			return m_data_conn->IsSocketOpen();

		else
			return false;
	}

	void SendControlRequest(const ftp_request& req)
	{

		if(IsControlStreamConnected())
		{
			m_control_conn->Write(req);
		}
		 
			
	}

	void SendDataRequest(const ftp_request& req)
	{
		if (IsDataStreamConnected())
		{
			m_data_conn->Write(req);
		}
	}

	
	std::deque<ftp_request>& ReceivedControlResponses()
	{
		return m_control_requests;
	}

	std::deque<ftp_request>& ReceivedDataResponses()
	{
		return m_data_requests;
	}
};

