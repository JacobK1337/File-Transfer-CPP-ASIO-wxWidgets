#pragma once
#include<asio.hpp>
#include<asio/ts/buffer.hpp>
#include<asio/ts/internet.hpp>
#include<deque>
#include"request.h"
#include"ftpconnection.h"
class ftp_client
{
private:
	asio::io_context m_controlContext;
	asio::io_context m_dataContext;

	std::thread m_threadControlContext;
	std::thread m_threadDataContext;

	asio::ip::tcp::socket m_controlSocket;
	asio::ip::tcp::socket m_dataSocket;

	asio::ip::tcp::resolver::results_type m_controlConnectionEndpoints;
	asio::ip::tcp::resolver::results_type m_dataConnectionEndpoints;

	std::unique_ptr<ftp_connection> m_controlConnection;
	std::unique_ptr<ftp_connection> m_dataConnection;

	std::deque<request> m_controlRequests;
	std::deque<request> m_dataRequests;

	std::string working_directory;
public:
	ftp_client() : m_controlSocket(m_controlContext), m_dataSocket(m_dataContext)
	{
		
	}

	~ftp_client()
	{
		
	}

	bool EstablishControlConnection(const std::string& host, const uint16_t port)
	{

		try
		{
			asio::ip::tcp::resolver resolver(m_controlContext);
			auto endpoints = resolver.resolve(host, std::to_string(port));

			m_controlConnection = std::make_unique<ftp_connection>(
				ftp_connection::conn_type::control,
				ftp_connection::conn_founder::client,
				std::move(m_controlSocket),
				m_controlContext,
				m_controlRequests
				);


			m_controlConnection->ListenToServer(endpoints->endpoint());

			m_threadControlContext = std::thread(
				[this]() -> void
				{
					m_controlContext.run();
				});
		}

		catch(std::exception& e)
		{
			std::cout << "Exception: " << e.what() << "\n";
			return false;
		}

		return true;
	}


	bool EstablishDataConnection(const std::string& host, const uint16_t port)
	{
		try
		{
			asio::ip::tcp::resolver resolver(m_dataContext);
			auto endpoints = resolver.resolve(host, std::to_string(port));

			m_dataConnection = std::make_unique<ftp_connection>(
				ftp_connection::conn_type::data,
				ftp_connection::conn_founder::client,
				std::move(m_dataSocket),
				m_dataContext,
				m_dataRequests
				);

			//std::cout << "Listening to the server on data stream [" << m_dataSocket.local_endpoint() << "]\n";
			m_dataConnection->ListenToServer(endpoints->endpoint());

			m_threadDataContext = std::thread(
				[this]() -> void
				{
					m_dataContext.run();
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


	void ControlStreamDisconnect()
	{
		if (IsControlStreamConnected())
			m_controlConnection->Disconnect();

		m_controlContext.stop();
		if (m_threadControlContext.joinable())
			m_threadControlContext.join();
	}

	void DataStreamDisconnect()
	{
		if (IsDataStreamConnected())
			m_dataConnection->Disconnect();
	}

	bool IsControlStreamConnected()
	{
		if (m_controlConnection)
			return m_controlConnection->IsSocketOpen();

		else
			return false;
	}

	bool IsDataStreamConnected()
	{
		if (m_dataConnection)
			return m_dataConnection->IsSocketOpen();

		else
			return false;
	}

	void SendRequest(const request& req)
	{

		if(IsControlStreamConnected())
		{
			m_controlConnection->Send(req);
		}
		 
			
	}

	void SendFile(const request& req)
	{
		if (IsDataStreamConnected())
		{
			m_dataConnection->Send(req);
		}
	}


	
	std::deque<request>& IncomingControlRequests()
	{
		return m_controlRequests;
	}

	std::deque<request>& IncomingDataRequests()
	{
		return m_dataRequests;
	}
};

