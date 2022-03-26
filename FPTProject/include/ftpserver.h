#pragma once
#include<asio.hpp>
#include<asio/ts/internet.hpp>
#include<filesystem>
#include"ftpconnection.h"
#include<fstream>
#include<map>
#include<mutex>

class ftp_server
{

private:
	std::deque<ftp_request> m_received_requests;

	std::vector<std::shared_ptr<ftp_connection>> m_established_connections;

	std::deque < std::shared_ptr<File::FileResponse>> m_files_to_send;
	std::deque <std::shared_ptr<File::FileResponse>> m_files_to_send_pending;

	std::map <unsigned int, std::shared_ptr<File::FileRequest>> m_files_to_save;
	unsigned int m_files_uploaded_counter = 0;

	std::atomic_bool server_running = false;

	asio::io_context m_server_context;

	std::thread m_context_thread;

	std::thread m_file_send_thread;

	asio::ip::tcp::acceptor m_server_acceptor;

	std::string default_server_path = std::filesystem::current_path().string();

	std::map<unsigned long, ftp_request> data_request_details;

	std::mutex m_file_pending_mutex;

public:
	ftp_server(uint16_t port)
		:m_server_acceptor(m_server_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
	{}

	~ftp_server()
	{
		Stop();
	}

	void Start()
	{

		try
		{
			server_running = true;
			AsyncAcceptClient();

			m_context_thread = std::thread(
				[this]() -> void
				{
					m_server_context.run();
				}
			);

			m_file_send_thread = std::thread(
				[this]() -> void
				{
					SendFileBytes();
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
		server_running = false;
		m_server_context.stop();

		if (m_context_thread.joinable())
			m_context_thread.join();

		if (m_file_send_thread.joinable())
			m_file_send_thread.join();

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


	void SendFileBytes()
	{
		
		while(server_running)
		{
			for(auto& curr_file : m_files_to_send)
			{
				
				if(curr_file->receiver->IsSocketOpen())
				{
					ftp_request file_bytes_response;
					file_bytes_response.header.operation = ftp_request_header::ftp_operation::RETR;
					std::vector<char> buffer(std::min(static_cast<unsigned long long>(1000000), curr_file->remaining_bytes));

					file_bytes_response.InsertTrivialToBuffer(curr_file->client_file_id);

					curr_file->file_src->read(buffer.data(), buffer.size());

					curr_file->remaining_bytes -= buffer.size();

					file_bytes_response.CopyFromVector(buffer);

					curr_file->receiver->Write(file_bytes_response);
				}
			}


			if(!m_files_to_send.empty())
			{
				m_files_to_send.erase(
					std::remove_if(m_files_to_send.begin(), m_files_to_send.end(),
						[](std::shared_ptr<File::FileResponse> const entry)
						{
							return entry->remaining_bytes <= 0 || !entry->receiver->IsSocketOpen();
						}),
					m_files_to_send.end()
				);
			}

			//locking the state of the pending vector
			//to add all pending requests and erase them later
			//m_file_pending_mutex.lock();

			for(auto& pending_file : m_files_to_send_pending)
			{
				m_files_to_send.push_back(std::move(pending_file));
			}

			if(!m_files_to_send_pending.empty())
			{
				std::cout << "Pending before erase: " << m_files_to_send_pending.size() << "\n";
				m_files_to_send_pending.erase(
					std::remove_if(m_files_to_send_pending.begin(), m_files_to_send_pending.end(),
						[](std::shared_ptr<File::FileResponse> const entry)
						{
							return entry == nullptr;
						}),
					m_files_to_send.end()
							);

				std::cout << "Pending after erase: " << m_files_to_send_pending.size() << "\n";
			}

			//m_file_pending_mutex.unlock();
		}
	}

	void CheckForRequests()
	{
		while(!m_received_requests.empty())
		{
			
			auto new_request = m_received_requests.front();
			m_received_requests.pop_front();


			if (new_request.header.operation == ftp_request_header::ftp_operation::COLLECT_DATA)
				OnDataRequest(new_request.sender, new_request);

			else if (new_request.header.operation == ftp_request_header::ftp_operation::DISCONNECT)
				OnDisconnectRequest(new_request.sender);

			else
				OnControlRequest(new_request.sender, new_request);

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
		case ftp_request_header::ftp_operation::CD:
			{

			std::string user_path;
			
			data_request.ExtractStringFromBuffer(user_path);

			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::CD;

			for (const auto& file : std::filesystem::directory_iterator(default_server_path + user_path))
			{

				std::string file_name = file.path().filename().string();
				File::file_type file_type = file.is_directory() ? File::file_type::DIR : File::file_type::FILE;
				std::size_t file_size = file.file_size();

				File::InsertFileDetails(response, file_name, file_size, file_type);
				
			}

			data_request_details.erase(data_hash);
			client->Write(response);
			break;
			}

		case ftp_request_header::ftp_operation::RETR:
			{
			
			std::string user_path;

			data_request.ExtractStringFromBuffer(user_path);
			
			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::RETR;
			while (!data_request.mem_buffer.empty())
			{
				std::string file_name;
				int file_id;
				data_request.ExtractTrivialFromBuffer(file_id);
				data_request.ExtractStringFromBuffer(file_name);


				std::shared_ptr<std::ifstream> file_src = 
					std::make_shared<std::ifstream>(default_server_path + user_path + "\\" + file_name, std::ios::binary);

				auto file_size = std::filesystem::file_size(default_server_path + user_path + "\\" + file_name);

				m_files_to_send_pending.emplace_back(
					std::make_shared<File::FileResponse>(std::move(file_src), file_size, file_id, client)
				);

			}

			data_request_details.erase(data_hash);
			break;
			}

		case ftp_request_header::ftp_operation::STOR:
			{
			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::UPLOAD_ACCEPT;

			std::string user_path;
			data_request.ExtractStringFromBuffer(user_path);

				while(data_request.header.request_size > 0)
				{
					std::string file_name;
					uintmax_t file_size;
					data_request.ExtractStringFromBuffer(file_name);
					data_request.ExtractTrivialFromBuffer(file_size);

					std::shared_ptr<std::ofstream> file_dest =
						std::make_shared<std::ofstream>(default_server_path + user_path + "\\" + file_name, std::ios::binary);

					m_files_to_save.insert({
						m_files_uploaded_counter,
							std::make_shared<File::FileRequest>(std::move(file_dest), file_size, file_name)
						});

					response.InsertTrivialToBuffer(m_files_uploaded_counter);

					m_files_uploaded_counter++;
				}

				
				/*
				 *
			std::string file_name;
			std::vector<unsigned char> uploaded_file_buffer;
			std::string user_path;

			data_request.ExtractStringFromBuffer(file_name);
			data_request.ExtractToVector(uploaded_file_buffer);
			data_request.ExtractStringFromBuffer(user_path);

			std::ofstream uploaded_file(default_server_path + user_path + "\\" + file_name, std::ios::binary);
			std::copy(
				uploaded_file_buffer.cbegin(),
				uploaded_file_buffer.cend(),
				std::ostreambuf_iterator<char>(uploaded_file)
			);

			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::STOR;
			std::string server_info = "File successfully uploaded!";
			response.InsertStringToBuffer(server_info);

			data_request_details.erase(data_hash);
				 */
			client->Write(response);
			break;
			}


		case ftp_request_header::ftp_operation::DELE:
			{
			std::string user_path;

			data_request.ExtractStringFromBuffer(user_path);

			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::DELE;
			while (!data_request.mem_buffer.empty())
			{
				
				std::string file_name;

				data_request.ExtractStringFromBuffer(file_name);

				remove((default_server_path + user_path + "\\" + file_name).c_str());

			}

			data_request_details.erase(data_hash);
			std::string server_response = "Files successfully deleted!";
			response.InsertStringToBuffer(server_response);

			client->Write(response);
			break;
			}


		case ftp_request_header::ftp_operation::UPLOAD_DATA:
			{
				
			}
		}
	}

	void OnDisconnectRequest(std::shared_ptr<ftp_connection> client)
	{

		std::cout << "[" << client->GetId() << "] Client disconnected\n";

		m_established_connections.erase(
			std::remove(m_established_connections.begin(), m_established_connections.end(), client), m_established_connections.end());

		client.reset();

	}
	static unsigned int HashRequest(uint32_t client_id)
	{
		 return client_id ^ 0xB16B00B5;
	}

};