#pragma once
#include <filesystem>
#include<fstream>
#include<vector>
#include<iostream>

class ftp_connection;

struct ftp_request_header
{
	//Enum class representing defined FTP operation codes.
	enum class ftp_operation : uint32_t
	{
		//FTP operations
		STOR,
		DELE,
		RETR,
		CD,

		//connection operations
		DISCONNECT,

		//verification
		COLLECT_DATA,

		//server verification responses
		SERVER_OK,
		SERVER_ERROR,

		//upload file headers
		UPLOAD_ACCEPT,
		UPLOAD_REJECT,
		UPLOAD_DATA
	};

	ftp_operation operation;
	uint64_t request_size = 0;

};


struct ftp_request
{
	ftp_request_header header;
	std::vector<unsigned char> mem_buffer;
	std::shared_ptr<ftp_connection> sender = nullptr;


	ftp_request() = default;


	auto GetSize() const
	{
		return mem_buffer.size();
	}

	void AssignSender(std::shared_ptr<ftp_connection> t_sender)
	{
		sender = t_sender;
	}
	
	//Variadic template to insert only trivially copyable types of parameters into the ftp_request.
	template<class Arg, class... Data>
	void InsertTrivialToBuffer(Arg& arg, Data&... data)
	{
		
		size_t curr_size = mem_buffer.size();

		mem_buffer.resize(curr_size + sizeof(Arg));

		std::memcpy(mem_buffer.data() + curr_size, &arg, sizeof(Arg));


		header.request_size = GetSize();

		if constexpr (sizeof...(data) > 0u)
			InsertTrivialToBuffer(data...);
	}


	//Variadic template to extract only trivially copyable parameters from the ftp_request.
	//The order of the given parameters must agree with the order of the packed parameters.
	template<class Arg, class... Data>
	void ExtractTrivialFromBuffer(Arg& arg, Data&... data)
	{

		std::memcpy(&arg, mem_buffer.data(), sizeof Arg);

		mem_buffer.erase(mem_buffer.cbegin(), mem_buffer.cbegin() + sizeof Arg);
		
		//mem_buffer = std::vector<uint8_t>{ mem_buffer.cbegin() + sizeof(Arg), mem_buffer.cend() };

		header.request_size = GetSize();

		if constexpr (sizeof...(data) > 0u)
			ExtractTrivialFromBuffer(data...);
	}


	//Extraction and insertion of not trivially copyable structures, ex. string.
	void ExtractStringFromBuffer(std::string& str)
	{
		//extracting string length
		//it should always be inserted BEFORE corresponding string
		//other way, can't determine the length, so extracting is not possible
		std::size_t str_length;
		ExtractTrivialFromBuffer(str_length);


		std::vector<unsigned char> str_binary(mem_buffer.cbegin(), mem_buffer.cbegin() + str_length);
		std::string extract_str(str_binary.cbegin(), str_binary.cend());

		mem_buffer.erase(mem_buffer.cbegin(), mem_buffer.cbegin() + str_length);

		header.request_size = GetSize();
		str = extract_str;
	}

	void InsertStringToBuffer(std::string& str)
	{
		//always inserting string length before actual string.
		auto str_length = str.length();
		InsertTrivialToBuffer(str_length);

		std::vector<unsigned char> str_binary(str.cbegin(), str.cend());
		std::copy(str_binary.cbegin(), 
			str_binary.cend(), 
			std::back_inserter(mem_buffer)
		);

		header.request_size = GetSize();
	}

	template<typename VecArg>
	void CopyFromVector(std::vector<VecArg>& copy_from)
	{

		auto copy_from_size = copy_from.size();
		InsertTrivialToBuffer(copy_from_size);

		std::copy(
			copy_from.cbegin(),
			copy_from.cend(),
			std::back_inserter(mem_buffer)
		);

		header.request_size = GetSize();
	}

	template<typename VecArg>
	void ExtractToVector(std::vector<VecArg>& extract_to)
	{
		std::size_t file_size;
		ExtractTrivialFromBuffer(file_size);

		std::copy(
			mem_buffer.cbegin(),
			mem_buffer.cbegin() + file_size,
			std::back_inserter(extract_to)
		);

		mem_buffer.erase(mem_buffer.cbegin(), mem_buffer.cbegin() + file_size);

		header.request_size = GetSize();
	}
};

namespace File
{
	enum class file_type
	{
		DIR,
		FILE
	};

	struct FileDetails
	{
	
		std::string file_name;
		std::size_t file_size;
		file_type type;
		FileDetails() {}
		FileDetails(std::string& t_file_name, std::size_t t_file_size, file_type t_type)
			: file_name(std::move(t_file_name)), file_size(t_file_size), type(t_type) {}
	};

	struct FileRequest
	{
		std::shared_ptr<std::ofstream> file_dest;
		std::string file_name;
		const std::size_t file_size = 0;
		std::size_t remaining_bytes;
		FileRequest() {}
		FileRequest(std::shared_ptr<std::ofstream> t_file_dest, std::size_t t_file_size, std::string& t_file_name)
		:  file_dest(std::move(t_file_dest)), file_size(t_file_size), remaining_bytes(t_file_size), file_name(t_file_name)
		{
		}
	};

	struct FileResponse
	{
		int client_file_id;
		std::shared_ptr<std::ifstream> file_src;
		std::size_t remaining_bytes;
		std::shared_ptr<ftp_connection> receiver;
		FileResponse() {}
		FileResponse(std::shared_ptr<std::ifstream> t_file_src, std::size_t t_file_size, int t_file_id, std::shared_ptr<ftp_connection> t_rec = nullptr)
		:  file_src(std::move(t_file_src)), remaining_bytes(t_file_size), client_file_id(t_file_id), receiver(std::move(t_rec))
		{
			
		}
	};
	static void InsertFileDetails(ftp_request& request, std::string& file_name, std::size_t& file_size, file_type& f_type)
	{
		request.InsertStringToBuffer(file_name);
		request.InsertTrivialToBuffer(file_size, f_type);
	}

	static void ExtractFileDetails(ftp_request& request, std::string& file_name, std::size_t& file_size, file_type& f_type)
	{
		request.ExtractStringFromBuffer(file_name);
		request.ExtractTrivialFromBuffer(file_size, f_type);
	}
}