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
	std::deque<request> m_receivedControlRequests;
	std::deque<request> m_receivedDataRequests;
	//
	std::deque<std::shared_ptr<ftp_connection>> m_establishedControlConnections;
	std::deque<std::shared_ptr<ftp_connection>> m_establishedDataConnections;

	asio::io_context m_serverContext;
	std::thread m_threadServerContext;

	asio::ip::tcp::acceptor m_serverAcceptor;

	uint32_t m_clientIdCounter = 1000;
	ftp_connection::conn_type m_newConnectionType = ftp_connection::conn_type::control;
	std::string default_server_path = "C:/Users/kubcz/Desktop";
	

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
				if(!ec)
				{

					std::cout << "[SERVER] New connection on: " << socket.remote_endpoint() << "\n";

					switch(m_newConnectionType)
					{

					case ftp_connection::conn_type::control:
						{
						std::shared_ptr<ftp_connection> new_control_con =
							std::make_shared<ftp_connection>(
								ftp_connection::conn_type::control,
								ftp_connection::conn_founder::server,
								std::move(socket),
								m_serverContext,
								m_receivedControlRequests
								);

						if (OnControlConnectionEstablish(new_control_con))
						{
							m_establishedControlConnections.push_back(std::move(new_control_con));
							m_establishedControlConnections.back()->ListenToClient(m_clientIdCounter++);

							std::cout << "[" << m_establishedControlConnections.back()->GetId() << "] Connection approved! \n";
						}

						else
							std::cout << "Connection denied \n";

						break;
						}
					case ftp_connection::conn_type::data:
						{
						std::shared_ptr<ftp_connection> new_data_con =
							std::make_shared<ftp_connection>(
								ftp_connection::conn_type::data,
								ftp_connection::conn_founder::server,
								std::move(socket),
								m_serverContext,
								m_receivedDataRequests
								);

						if (OnDataConnectionEstablish(new_data_con))
						{
							m_establishedDataConnections.push_back(std::move(new_data_con));
							m_establishedDataConnections.back()->ListenToClient(m_clientIdCounter++);

							std::cout << "[" << m_establishedDataConnections.back()->GetId() << "] Connection approved! \n";
						}

						else
							std::cout << "Connection denied \n";

						break;
						}
					}

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
		while(!m_receivedControlRequests.empty() || !m_receivedDataRequests.empty())
		{
			if(!m_receivedControlRequests.empty())
			{
				auto new_control_request = m_receivedControlRequests.front();
				m_receivedControlRequests.pop_front();

				OnControlRequest(new_control_request.sender, new_control_request);

			}

			if(!m_receivedDataRequests.empty())
			{
				auto new_data_request = m_receivedDataRequests.front();
				m_receivedControlRequests.pop_front();

				OnDataRequest(new_data_request.sender, new_data_request);
			}

		}
	}
private:
	bool OnDataConnectionEstablish(std::shared_ptr<ftp_connection> client)
	{
		std::cout << "Data connection has been established...\n";
		return true;
	}
	bool OnControlConnectionEstablish(std::shared_ptr<ftp_connection> client)
	{
		std::cout << "Control connection has been established... \n";
		return true;
	}
	void OnClientDisconnect(std::shared_ptr<ftp_connection> client)
	{
		
	}

	void OnControlRequest(std::shared_ptr<ftp_connection> client, request& req)
	{
		switch(req.request_header.operation)
		{
		
		case request_code::operation_types::ASSIGN_DATA_CONNECTION:
			std::cout << "Client [" << client->GetId() << "] requested data stream connection \n";
			std::cout << "No errors, accepting request. \n";
			client->Send(req);
			break;

		case request_code::operation_types::STOR:
			{
			const char* file_name;
			req.ExtractTrivialFromBuffer(file_name);


			std::cout << client->GetId() << "Received file:  \n";
			std::cout << file_name << "\n";
			request new_request;
			new_request.request_header.operation = request_code::operation_types::STOR;
			new_request.InsertTrivialToBuffer(file_name);

			client->Send(new_request);
			break;
			}
			

		case request_code::operation_types::LS:
			{
			std::cout << "User [" << client->GetId() << "] requested listing of files: \n";

			std::size_t user_path_length;
			std::string user_path;
			req.ExtractTrivialFromBuffer(user_path_length);
			req.ExtractStringFromBuffer(user_path, user_path_length);
			request response;
			response.request_header.operation = request_code::operation_types::LS;
			//response.CopyFromVectorBuffer(file_buffer);
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

			std::cout << "Final buffer size in server: " << response.request_body.size() << "\n";
			client->Send(response);
			break;
				
			}
		case request_code::operation_types::RETR:
			{
			std::string file_name;
			std::size_t file_name_length;
			req.ExtractTrivialFromBuffer(file_name_length);
			req.ExtractStringFromBuffer(file_name, file_name_length);
			std::string path = "C:/Users/kubcz/Desktop";
			std::string ifstream_path = "C:\\Users\kubcz\\Desktop\\";
			path += "/" + file_name;
			ifstream_path += file_name;
			//std::cout << std::filesystem::exists(path);

			request response;
			response.request_header.operation = request_code::operation_types::RETR;
				if(std::filesystem::exists(path))
				{
					std::ifstream input(path, std::ios::binary);
					//Inserting trivial boolean set to true -> file exists, can save
					
					bool file_exists = true;
					
					response.InsertTrivialToBuffer(file_exists);
					response.InsertTrivialToBuffer(file_name_length);
					response.InsertStringToBuffer(file_name);

					std::vector<unsigned char> file_buffer(std::istreambuf_iterator<char>(input), {});
					std::copy(file_buffer.cbegin(), file_buffer.cend(), std::back_inserter(response.request_body));
					response.request_header.request_size = response.request_body.size();

					 
				}
				else
				{
					bool file_exists = false;
					response.InsertTrivialToBuffer(file_exists);

					std::string not_such_file = "There is no such file!";
					const std::size_t str_length = not_such_file.length();
					response.InsertTrivialToBuffer(str_length);
					response.InsertStringToBuffer(not_such_file);
				}

				client->Send(response);
			break;
			}
		case request_code::operation_types::CD:
			{
			std::string path = "C:/Users/kubcz/Desktop/";
			std::size_t user_path_size;
			std::string user_path;
			req.ExtractTrivialFromBuffer(user_path_size);
			req.ExtractStringFromBuffer(user_path, user_path_size);

			path += user_path;
			request response;
			std::cout << path << "\n";
			response.request_header.operation = request_code::operation_types::CD;
				if(std::filesystem::is_directory(path))
				{
					std::cout << "Is a directory";
				}
				else
				{
					std::cout << "Not a directory";
				}
				break;
			}
		}
	}

	void OnDataRequest(std::shared_ptr<ftp_connection> client, request& req)
	{
		switch(req.request_header.operation)
		{
		case request_code::operation_types::STOR:
			std::cout << client->GetId() << " Stor recevied as data req \n";
			break;
		}
	}
};