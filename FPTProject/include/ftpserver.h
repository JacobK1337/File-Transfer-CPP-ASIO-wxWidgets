#pragma once
#include<asio.hpp>
#include<asio/ts/internet.hpp>
#include<filesystem>
#include"ftpconnection.h"
#include<fstream>
#include<map>

class ftp_server
{

private:
	std::deque<ftp_request> m_received_requests;
	std::deque<std::shared_ptr<ftp_connection>> m_established_connections;
	asio::io_context m_server_context;
	std::thread m_context_thread;
	asio::ip::tcp::acceptor m_server_acceptor;

	std::string default_server_path = "C:\\Users\\kubcz\\Desktop";//std::filesystem::current_path().string();

	std::map<unsigned long, ftp_request> data_request_details;

public:
	ftp_server(uint16_t port)
		:m_server_acceptor(m_server_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
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

			m_context_thread = std::thread(
				[this]() -> void
				{
					m_server_context.run();
				}
			);
		}
		catch (std::exception& e)
		{
			std::cout << "An error occured while starting server. \n";
			return;
		}

		std::cout << "Server started successfully. \n";
	}

	void Stop()
	{
		m_server_context.stop();

		if (m_context_thread.joinable())
			m_context_thread.join();

		std::cout << "Server stopped \n";

	}

	void AsyncAcceptClient()
	{
		m_server_acceptor.async_accept(
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
							m_server_context,
							m_received_requests
							);

					
					m_established_connections.push_back(std::move(new_connection));
					
					ftp_server::ListenToClient(m_established_connections.back());

					std::cout << "[" << m_established_connections.back()->GetId() << "] New connection! \n";

				}
				else
				{
					std::cout << "[SERVER] New connection error: " << ec.message() << "\n";
				}

				AsyncAcceptClient();
			});
	}

	static void ListenToClient(std::shared_ptr<ftp_connection> m_client_conn)
	{

		const auto client_ip_ipv4 = m_client_conn->GetSocket().remote_endpoint().address().to_v4().to_ulong();

		m_client_conn->SetId(client_ip_ipv4);
		m_client_conn->StartReading();
	}

	void CheckForRequests()
	{
		while(!m_received_requests.empty())
		{
			if(!m_received_requests.empty())
			{
				auto new_request = m_received_requests.front();
				m_received_requests.pop_front();


				if (new_request.header.operation == ftp_request_header::ftp_operation::COLLECT_DATA)
					OnDataRequest(new_request.sender, new_request);

				else
					OnControlRequest(new_request.sender, new_request);

			}
		}
	}
private:
	void OnControlRequest(std::shared_ptr<ftp_connection> client, ftp_request& req)
	{

		//saving requested data
		unsigned long request_hash = ftp_server::HashRequest(client->GetId());
		data_request_details.insert({ request_hash, std::move(req)});

		ftp_request response;
		response.header.operation = ftp_request_header::ftp_operation::SERVER_OK;
		response.InsertTrivialToBuffer(request_hash);
		client->Write(response);
		
	}

	void OnDataRequest(std::shared_ptr<ftp_connection> client, ftp_request& req)
	{
		unsigned long data_hash;

		req.ExtractTrivialFromBuffer(data_hash);
		auto& data_request = data_request_details[data_hash];

		switch(data_request.header.operation)
		{
		case ftp_request_header::ftp_operation::LS:
			{
			
			std::size_t user_path_length;
			std::string user_path;
			data_request.ExtractTrivialFromBuffer(user_path_length);
			data_request.ExtractStringFromBuffer(user_path, user_path_length);

			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::LS;

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
			client->Write(response);
			break;
			}

		case ftp_request_header::ftp_operation::RETR:
			{
			std::size_t user_path_length;
			std::string user_path;

			data_request.ExtractTrivialFromBuffer(user_path_length);
			data_request.ExtractStringFromBuffer(user_path, user_path_length);
			
			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::RETR;
			while (!data_request.mem_buffer.empty())
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
				/*
				 *
				std::copy(
					file_binary_buffer.cbegin(), 
					file_binary_buffer.cend(), 
					std::back_inserter(response.mem_buffer)
				);

				response.header.request_size = response.mem_buffer.size();
				 */
				response.CopyFromVector(file_binary_buffer);
			}
			data_request_details.erase(data_hash);
			client->Write(response);
			break;
			}

		case ftp_request_header::ftp_operation::STOR:
			{
			//to implement
			break;
			}
		case ftp_request_header::ftp_operation::DELE:
			{

			break;
			}

		}
	}

	static unsigned int HashRequest(uint32_t client_id)
	{
		 return client_id ^ 0xB16B00B5;
	}

	static unsigned int ForgeClientId()
	{
		return 1337;
	}
};