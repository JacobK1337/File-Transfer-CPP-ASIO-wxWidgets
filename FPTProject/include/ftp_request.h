#pragma once
#include<asio.hpp>
#include<asio/ts/internet.hpp>
#include<vector>


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
		LS,
		CD,

		//verification
		COLLECT_DATA,

		//server verification responses
		SERVER_OK,
		SERVER_ERROR
	};

	ftp_operation operation;
	uint64_t request_size = 0;

};


struct ftp_request
{
	ftp_request_header header;
	std::vector<unsigned char> mem_buffer;
	std::shared_ptr<ftp_connection> sender = nullptr;

	//std::vector<uint8_t> request_body_cache;

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
	void ExtractStringFromBuffer(std::string& str, const std::size_t str_length)
	{
		
		std::vector<unsigned char> str_binary(mem_buffer.cbegin(), mem_buffer.cbegin() + str_length);
		std::string extract_str(str_binary.cbegin(), str_binary.cend());

		mem_buffer.erase(mem_buffer.cbegin(), mem_buffer.cbegin() + str_length);

		header.request_size = GetSize();
		str = extract_str;
	}

	void InsertStringToBuffer(std::string& str)
	{
		std::vector<unsigned char> str_binary(str.cbegin(), str.cend());
		std::copy(str_binary.cbegin(), str_binary.cend(), std::back_inserter(mem_buffer));

		header.request_size = GetSize();
	}

	template<typename VecArg>
	void CopyFromVector(std::vector<VecArg>& copy_from)
	{
		std::copy(
			copy_from.cbegin(),
			copy_from.cend(),
			std::back_inserter(mem_buffer)
		);

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
	public:
		std::string file_name;
		std::size_t file_size;
		file_type type;
		FileDetails() {}
		FileDetails(std::string& t_file_name, std::size_t t_file_size, file_type t_type)
			: file_name(std::move(t_file_name)), file_size(t_file_size), type(t_type) {}
	};
}