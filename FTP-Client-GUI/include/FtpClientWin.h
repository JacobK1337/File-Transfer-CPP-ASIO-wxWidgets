#pragma once
#include<wx/wx.h>
#include<wx/filepicker.h>
#include<wx/listctrl.h>
#include"ftpclient.h"
#include"RequestHandlerThread.h"
#include<filesystem>

struct FileDetails
{
public:
	std::string file_name;
	std::size_t file_size;
	bool is_directory = false;

	FileDetails(){}
	FileDetails(std::string& t_file_name, std::size_t t_file_size, bool t_is_dir)
	: file_name(std::move(t_file_name)), file_size(t_file_size), is_directory(t_is_dir){}
};


class FtpClientWin : public wxFrame
{
public:
	FtpClientWin();
	~FtpClientWin();
	wxDECLARE_EVENT_TABLE();
	

private:
	wxPanel* m_panel = nullptr;
	wxToolBar* m_toolbar;
	wxDirPickerCtrl* m_dir_picker = nullptr;
	wxStaticText* m_dir_picker_label = nullptr;
	wxListCtrl* m_server_files_list = nullptr;
	wxButton* test_button = nullptr;
	wxListCtrl* m_logs_list = nullptr;
	ftp_client client;
	RequestHandlerThread* m_request_thread = nullptr;
	std::string user_directory = "";

	std::vector<std::shared_ptr<FileDetails>> m_file_details;

	//GUI items setters
	void SetupToolbar();
	void SetupFileList();
	void SetupLogsList();

	//Action handlers
	void onClick(wxCommandEvent& evt);
	void onActivate(wxListEvent& evt);
	void OnServerResponse(wxThreadEvent& evt);
	void GetServerFileList(std::string& file_path);
	void OnClose(wxCloseEvent& evt);


	//Logs handlers

	void DisplayLog(const std::string&& log_text, const wxColour&& log_colour) const;
	void DisplayFile(const std::string& file_name, const std::size_t file_size, const bool is_directory);
	void ResetFilesList();
};