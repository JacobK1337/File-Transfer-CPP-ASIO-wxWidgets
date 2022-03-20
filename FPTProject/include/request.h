#pragma once
#include<asio.hpp>
#include<asio/ts/buffer.hpp>
#include<asio/ts/internet.hpp>
#include<vector>
#include<iostream>
struct request_code
{
	//Enum class representing defined FTP operation codes.
	enum class operation_types : uint32_t
	{
		//operations from client
		STOR,
		DELE,
		RETR,
		PWD,
		LS,
		CD,

		//requests
		ASSIGN_DATA_CONNECTION,

		//server responses
		REQUEST_ACCEPT,
		BAD_REQUEST
	};

	operation_types operation;
	uint64_t request_size = 0;
};

namespace request_code_mapping
{
	static std::unordered_map<std::string, request_code::operation_types> const string_to_operation = {
		{"stor", request_code::operation_types::STOR},
		{"dele", request_code::operation_types::DELE},
		{"retr", request_code::operation_types::RETR},
		{"pwd", request_code::operation_types::PWD},
		{"ls", request_code::operation_types::LS},
		{"cd", request_code::operation_types::CD}
	};
}

class ftp_connection;

struct request
{
	request_code request_header;
	std::vector<uint8_t> request_body;
	std::shared_ptr<ftp_connection> sender = nullptr;

	//std::vector<uint8_t> request_body_cache;

	void AssignSender(std::shared_ptr<ftp_connection> t_sender)
	{
		sender = t_sender;
	}
	
	//Variadic template to insert only trivially copyable types of parameters into the request.
	template<class Arg, class... Data>
	void InsertTrivialToBuffer(Arg& arg, Data&... data)
	{
		
		size_t curr_size = request_body.size();

		request_body.resize(curr_size + sizeof(Arg));

		std::memcpy(request_body.data() + curr_size, &arg, sizeof(Arg));


		request_header.request_size = request_body.size();

		if constexpr (sizeof...(data) > 0u)
			InsertTrivialToBuffer(data...);
	}


	//Variadic template to extract only trivially copyable parameters from the request.
	//The order of the given parameters must agree with the order of the packed parameters.
	template<class Arg, class... Data>
	void ExtractTrivialFromBuffer(Arg& arg, Data&... data)
	{

		std::memcpy(&arg, request_body.data(), sizeof Arg);

		request_body.erase(request_body.cbegin(), request_body.cbegin() + sizeof Arg);
		
		//request_body = std::vector<uint8_t>{ request_body.cbegin() + sizeof(Arg), request_body.cend() };

		request_header.request_size = request_body.size();
		if constexpr (sizeof...(data) > 0u)
			ExtractTrivialFromBuffer(data...);
	}


	//Extraction and insertion of not trivially copyable structures, ex. string.
	void ExtractStringFromBuffer(std::string& str, const std::size_t str_length)
	{
		
		std::vector<uint8_t> str_binary(request_body.cbegin(), request_body.cbegin() + str_length);
		std::string extract_str(str_binary.cbegin(), str_binary.cend());

		request_body.erase(request_body.cbegin(), request_body.cbegin() + str_length);
		request_header.request_size = request_body.size();
		str = extract_str;
	}

	void InsertStringToBuffer(std::string& str)
	{
		std::vector<uint8_t> str_binary(str.cbegin(), str.cend());
		std::copy(str_binary.cbegin(), str_binary.cend(), std::back_inserter(request_body));

		request_header.request_size = request_body.size();
	}

	static std::vector<std::string> tokenize_string(std::string& str, std::string delimiter)
	{
		std::size_t begin = str.find_first_not_of(delimiter);
		std::size_t end = begin;
		std::vector<std::string> string_tokens;

		while (begin != std::string::npos)
		{
			end = str.find(delimiter, begin);
			string_tokens.emplace_back(str.substr(begin, end - begin));
			begin = str.find_first_not_of(delimiter, end);

		}

		return string_tokens;

	}

	


	
};

struct request_handler
{
public:
	static request_code::operation_types FindCommand(std::string& user_command)
	{
		using namespace request_code_mapping;

		auto occur = string_to_operation.find(user_command);

		return occur != string_to_operation.end() ? occur->second : request_code::operation_types::BAD_REQUEST;
	}

	static request PrepareRequest(request_code::operation_types resolved_operation, std::vector<std::string>& user_command_tokenized)
	{

		switch (resolved_operation)
		{
		case request_code::operation_types::LS:
			return request_handler::ls_request(user_command_tokenized);
			break;
		case request_code::operation_types::RETR:
			return request_handler::retr_request(user_command_tokenized);
			break;
		case request_code::operation_types::CD:
			return request_handler::cd_request(user_command_tokenized);
			break;
		default:
			break;
		}

		
	}

private:
	static request ls_request(std::vector<std::string>& user_command_tokenized)
	{
		if (user_command_tokenized.size() > 2)
		{
			std::cout << "Invalid arguments \n";
			request req;
			req.request_header.operation = request_code::operation_types::BAD_REQUEST;
			return req;
		}

		else
		{

			switch (user_command_tokenized.size())
			{
			case 1:
			{
				request req;
				req.request_header.operation = request_code::operation_types::LS;
				return req;
				break;
			}

			case 2:
				request req;
				req.request_header.operation = request_code::operation_types::LS;

				return req;
				break;
			}
		}


	}
	static request retr_request(std::vector<std::string>& user_command_tokenized)
	{
		if(user_command_tokenized.size() > 2 || user_command_tokenized.size() == 1)
		{
			std::cout << "Invalid arguments \n";
			request req;
			req.request_header.operation = request_code::operation_types::BAD_REQUEST;
			return req;
		}

		request req;
		req.request_header.operation = request_code::operation_types::RETR;

		std::string file_name = user_command_tokenized[1];
		const std::size_t file_name_length = file_name.length();
		req.InsertTrivialToBuffer(file_name_length);
		req.InsertStringToBuffer(file_name);

		return req;
	}
	static request cd_request(std::vector<std::string>& user_command_tokenized)
	{
		request req;

		return req;
	}
};