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

	std::deque < std::shared_ptr<File::FileLocal>> m_files_to_send;

	std::deque <std::shared_ptr<File::FileLocal>> m_files_to_send_pending;

	std::map <unsigned int, std::shared_ptr<File::FileRemote>> m_files_to_save;
	unsigned int m_files_uploaded_counter = 0;

	std::atomic_bool server_running = false;

	std::atomic_bool quit_sending = false;
	std::mutex m_send_mutex;
	std::condition_variable m_send_cond;

	asio::io_context m_server_context;

	std::thread m_context_thread;

	std::thread m_file_send_thread;

	asio::ip::tcp::acceptor m_server_acceptor;

	std::string default_server_path = std::filesystem::current_path().string();

	std::map<unsigned long, ftp_request> data_request_unverified;

	std::mutex m_file_pending_mutex;

	const unsigned long long MAX_BANDWIDTH = 100000000; //100MB per packet

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

		{
			std::unique_lock<std::mutex> lock(m_send_mutex);
			quit_sending = true;
		}

		m_send_cond.notify_one();
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
			std::unique_lock<std::mutex> check_for_send_lock(m_send_mutex);
			m_send_cond.wait(check_for_send_lock, [this]() -> bool
				{
					return quit_sending || (!m_files_to_send.empty() || !m_files_to_send_pending.empty());
				});


			if (quit_sending)
				return;


			Sleep(100);
			for(auto& curr_file : m_files_to_send)
			{
				
				if(curr_file->receiver->IsSocketOpen())
				{
					ftp_request file_bytes_response;
					file_bytes_response.header.operation = ftp_request_header::ftp_operation::DOWNLOAD_FILE;
					std::vector<char> buffer(std::min(MAX_BANDWIDTH, curr_file->remaining_bytes));

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
						[](std::shared_ptr<File::FileLocal> const entry)
						{
							return entry->remaining_bytes <= 0 || !entry->receiver->IsSocketOpen();
						}),
					m_files_to_send.end()
				);
			}

			//locking the state of the pending vector
			//to add all pending requests and erase them later

			m_file_pending_mutex.lock();

			for(auto& pending_file : m_files_to_send_pending)
			{
				m_files_to_send.push_back(std::move(pending_file));
			}

			if(!m_files_to_send_pending.empty())
			{
				
				m_files_to_send_pending.erase(
					std::remove_if(m_files_to_send_pending.begin(), m_files_to_send_pending.end(),
						[](std::shared_ptr<File::FileLocal> const entry)
						{
							return entry == nullptr;
						}),
					m_files_to_send.end()
							);

			}

			m_file_pending_mutex.unlock();
		}
	}

	void CheckForRequests()
	{
		while(true)
		{

			while (!m_received_requests.empty())
			{

				auto new_request = m_received_requests.front();
				m_received_requests.pop_front();

				if (new_request.header.operation == ftp_request_header::ftp_operation::DATA_STREAM_VERIFIED)
					OnDataRequest(new_request.sender, new_request);

				else if (new_request.header.operation == ftp_request_header::ftp_operation::UPLOAD_DATA)
					OnUpload(new_request.sender, new_request);

				else if (new_request.header.operation == ftp_request_header::ftp_operation::DISCONNECT)
					OnDisconnectRequest(new_request.sender);

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
		data_request_unverified.insert({ request_hash, std::move(req)});

		ftp_request response;
		response.header.operation = ftp_request_header::ftp_operation::SERVER_OK;
		response.InsertTrivialToBuffer(request_hash);
		client->Write(response);
		
	}

	void OnDataRequest(std::shared_ptr<ftp_connection> client, ftp_request& req)
	{
		unsigned long data_hash;

		req.ExtractTrivialFromBuffer(data_hash);
		auto& data_request = data_request_unverified[data_hash];

		switch(data_request.header.operation)
		{
		case ftp_request_header::ftp_operation::CHANGE_DIRECTORY:
			{

			std::string user_path;
			
			data_request.ExtractStringFromBuffer(user_path);

			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::CHANGE_DIRECTORY;

			for (const auto& file : std::filesystem::directory_iterator(default_server_path + user_path))
			{

				std::string file_name = file.path().filename().string();
				File::file_type file_type = file.is_directory() ? File::file_type::DIR : File::file_type::FILE;
				std::size_t file_size = file.file_size();

				File::InsertFileDetails(response, file_name, file_size, file_type);
				
			}

			data_request_unverified.erase(data_hash);
			client->Write(response);
			break;
			}

		case ftp_request_header::ftp_operation::DOWNLOAD_FILE:
			{
			
			std::string user_path;

			data_request.ExtractStringFromBuffer(user_path);
			
			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::DOWNLOAD_FILE;
			while (!data_request.mem_buffer.empty())
			{
				std::string file_name;
				int file_id;
				data_request.ExtractTrivialFromBuffer(file_id);
				data_request.ExtractStringFromBuffer(file_name);


				std::shared_ptr<std::ifstream> file_src = 
					std::make_shared<std::ifstream>(default_server_path + user_path + "\\" + file_name, std::ios::binary);

				auto file_size = std::filesystem::file_size(default_server_path + user_path + "\\" + file_name);

				m_file_pending_mutex.lock();

				m_files_to_send_pending.emplace_back(
					std::make_shared<File::FileLocal>(std::move(file_src), file_size, file_id, client)
				);

				m_file_pending_mutex.unlock();

				m_send_cond.notify_all();
			}

			data_request_unverified.erase(data_hash);
			break;
			}

		case ftp_request_header::ftp_operation::UPLOAD_FILE:
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
							std::make_shared<File::FileRemote>(std::move(file_dest), file_size, file_name, client)
						});
					
					response.InsertTrivialToBuffer(m_files_uploaded_counter);

					m_files_uploaded_counter++;
				}
				data_request_unverified.erase(data_hash);
				client->Write(response);
			break;
			}


		case ftp_request_header::ftp_operation::DELETE_FILE:
			{
			std::string user_path;

			data_request.ExtractStringFromBuffer(user_path);

			ftp_request response;
			response.header.operation = ftp_request_header::ftp_operation::DELETE_FILE;
			while (!data_request.mem_buffer.empty())
			{
				
				std::string file_name;

				data_request.ExtractStringFromBuffer(file_name);

				remove((default_server_path + user_path + "\\" + file_name).c_str());

			}

			data_request_unverified.erase(data_hash);
			std::string server_response = "Files successfully deleted!";
			response.InsertStringToBuffer(server_response);

			client->Write(response);
			break;
			}

		}
	}


	void OnUpload(std::shared_ptr<ftp_connection> client, ftp_request& req)
	{
		
		unsigned int file_id;
		req.ExtractTrivialFromBuffer(file_id);

		std::vector<char> retrieved_file_buffer;
		req.ExtractToVector(retrieved_file_buffer);

		m_files_to_save[file_id]->file_dest->write(retrieved_file_buffer.data(), retrieved_file_buffer.size());
		m_files_to_save[file_id]->remaining_bytes -= retrieved_file_buffer.size();
		std::cout << "File [" << file_id << "]: bytes remaining -> " << m_files_to_save[file_id]->remaining_bytes << "\n";

		if (m_files_to_save[file_id]->remaining_bytes <= 0)
		{
			std::cout << "File uploaded! \n";

			ftp_request upload_finished_response;
			upload_finished_response.header.operation = ftp_request_header::ftp_operation::UPLOAD_FINISHED;
			std::string server_response = "File: " + m_files_to_save[file_id]->file_name + " successfully uploaded!";

			upload_finished_response.InsertStringToBuffer(server_response);

			client->Write(upload_finished_response);

			m_files_to_save.erase(file_id);
		}

	}
	void OnDisconnectRequest(std::shared_ptr<ftp_connection> client)
	{

		std::cout << "[" << client->GetId() << "] Client disconnected\n";

		

		//removing client from established connections
		m_established_connections.erase(
			std::remove(
				m_established_connections.begin(), 
				m_established_connections.end(), client), 
			m_established_connections.end()
		);

		client.reset();

		//if client disconnected during upload, we remove all files that he was uploading.

		auto map_it = m_files_to_save.begin();

		while(map_it != m_files_to_save.end())
		{
			if (!map_it->second->sender->IsSocketOpen())
				map_it = m_files_to_save.erase(map_it);

			else
				++map_it;
		}

		
		

	}
	static unsigned int HashRequest(uint32_t client_id)
	{
		 return client_id ^ 0xB16B00B5;
	}

};