#pragma once
#include<asio.hpp>
#include<asio/ts/buffer.hpp>
#include<asio/ts/internet.hpp>
#include<filesystem>
#include"ftpconnection.h"
#include<fstream>


class ftp_server
{

private:
	std::deque<request> m_received_requests;
	std::deque<std::shared_ptr<ftp_connection>> m_established_connections;

	asio::io_context m_serverContext;
	std::thread m_threadServerContext;

	asio::ip::tcp::acceptor m_serverAcceptor;

	uint32_t m_clientIdCounter = 1000;
	//ftp_connection::conn_type m_newConnectionType; 
	std::string default_server_path = std::filesystem::current_path().string();
	

	std::map<unsigned long, request> data_request_details;

public:
	ftp_server(uint16_t port)
		:m_serverAcceptor(m_serverContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
	{
		
	}
	~ftp_server()
	{
		Stop();
	}

	void Start()
	{
		try
		{
			AsyncAcceptClient();

			m_threadServerContext = std::thread(
				[this]() -> void
				{
					m_serverContext.run();
				}
			);
		}
		catch(std::exception& e)
		{
			std::cout << "An error occured while starting server. \n";
			return;
		}

		std::cout << "Server started successfully. \n";
	}

	void Stop()
	{
		m_serverContext.stop();

		if (m_threadServerContext.joinable())
			m_threadServerContext.join();

		std::cout << "Server stopped \n";

	}

	void AsyncAcceptClient()
	{
		m_serverAcceptor.async_accept(
			[this](std::error_code ec, asio::ip::tcp::socket socket) -> void
			{
				if (!ec)
				{

					std::cout << "[SERVER] New connection on: " << socket.remote_endpoint() << "\n";

					std::shared_ptr<ftp_connection> new_connection =
						std::make_shared<ftp_connection>(
							ftp_connection::conn_type::server_remote,
							ftp_connection::conn_founder::server,
							std::move(socket),
							m_serverContext,
							m_received_requests
							);

					if (OnConnectionEstablish(new_connection))
					{
						m_established_connections.push_back(std::move(new_connection));
						m_established_connections.back()->ListenToClient(m_clientIdCounter++);

						std::cout << "[" << m_established_connections.back()->GetId() << "] Connection approved! \n";
					}

					else
						std::cout << "Connection denied \n";


				}
				else
				{
					std::cout << "[SERVER] New connection error: " << ec.message() << "\n";
				}

				AsyncAcceptClient();
			});
	}


	void ServerUpdate()
	{
		while(!m_received_requests.empty())
		{
			if(!m_received_requests.empty())
			{
				auto new_request = m_received_requests.front();
				m_received_requests.pop_front();


				if (new_request.request_header.operation == request_code::operation_types::COLLECT_DATA)
					OnDataRequest(new_request.sender, new_request);

				else
					OnControlRequest(new_request.sender, new_request);

			}


		}
	}
private:
	
	bool OnConnectionEstablish(std::shared_ptr<ftp_connection> client)
	{
		std::cout << "Connection has been established \n";
		return true;
	}
	void OnClientDisconnect(std::shared_ptr<ftp_connection> client)
	{
		
	}

	void OnControlRequest(std::shared_ptr<ftp_connection> client, request& req)
	{

		//saving requested data
		unsigned long request_hash = ftp_server::HashRequest(client->GetId());
		data_request_details.insert({ request_hash, std::move(req)});

		request response;
		response.request_header.operation = request_code::operation_types::SERVER_OK;
		response.InsertTrivialToBuffer(request_hash);
		client->Send(response);
		
	}

	void OnDataRequest(std::shared_ptr<ftp_connection> client, request& req)
	{
		unsigned long data_hash;
		req.ExtractTrivialFromBuffer(data_hash);
		auto& data_request = data_request_details[data_hash];

		switch(data_request.request_header.operation)
		{
		case request_code::operation_types::LS:
			{

			std::size_t user_path_length;
			std::string user_path;
			data_request.ExtractTrivialFromBuffer(user_path_length);
			data_request.ExtractStringFromBuffer(user_path, user_path_length);

			request response;
			response.request_header.operation = request_code::operation_types::LS;

			for (const auto& file : std::filesystem::directory_iterator(default_server_path + user_path))
			{

				std::string file_path = file.path().filename().string();
				const std::size_t file_path_length = file_path.length();
				const bool type = file.is_directory();
				const std::size_t file_size = file.file_size();

				response.InsertTrivialToBuffer(file_path_length);
				response.InsertStringToBuffer(file_path);
				response.InsertTrivialToBuffer(file_size);
				response.InsertTrivialToBuffer(type);
			}

			data_request_details.erase(data_hash);
			client->Send(response);
			break;
			}

		case request_code::operation_types::RETR:
			{
			std::size_t user_path_length;
			std::string user_path;

			data_request.ExtractTrivialFromBuffer(user_path_length);
			data_request.ExtractStringFromBuffer(user_path, user_path_length);
			
			request response;
			response.request_header.operation = request_code::operation_types::RETR;
			while (!data_request.request_body.empty())
			{
				std::string file_name;
				std::size_t file_name_length;
				data_request.ExtractTrivialFromBuffer(file_name_length);
				data_request.ExtractStringFromBuffer(file_name, file_name_length);

				std::ifstream file_binary(default_server_path + user_path + "\\" + file_name, std::ios::binary);

				response.InsertTrivialToBuffer(file_name_length);
				response.InsertStringToBuffer(file_name);
				std::vector<unsigned char> file_binary_buffer(std::istreambuf_iterator<char>(file_binary), {});
				auto file_binary_size = file_binary_buffer.size();


				response.InsertTrivialToBuffer(file_binary_size);
				std::copy(
					file_binary_buffer.cbegin(), 
					file_binary_buffer.cend(), 
					std::back_inserter(response.request_body)
				);

				response.request_header.request_size = response.request_body.size();
			}

			client->Send(response);
			break;
			}

		case request_code::operation_types::DELE:
			{

			break;
			}
		}
	}
	static unsigned int HashRequest(uint32_t client_id)
	{
		 return client_id ^ 0xB16B00B5;
	}
};