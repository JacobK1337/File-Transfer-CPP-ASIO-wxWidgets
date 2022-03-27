#pragma once
#include<wx/wx.h>
#include<wx/filepicker.h>
#include<wx/listctrl.h>
#include"ftpclient.h"
#include"RequestHandlerThread.h"
#include<filesystem>
#include<map>
#include<fstream>
#include<mutex>

class FtpClientWin : public wxFrame
{
public:
	FtpClientWin();
	~FtpClientWin();
	wxDECLARE_EVENT_TABLE();
	

private:
	std::atomic_bool running = true;
	std::atomic_bool quit_uploading = false;
	ftp_client client;

	wxPanel* m_panel = nullptr;
	wxToolBar* m_toolbar;
	wxDirPickerCtrl* m_save_dir_picker = nullptr;
	wxFilePickerCtrl* m_send_file_picker = nullptr;
	wxListCtrl* m_server_files_list = nullptr;

	wxImageList* m_file_image_list = nullptr;
	
	wxButton* m_save_button = nullptr;
	wxButton* m_upload_button = nullptr;
	wxListCtrl* m_logs_list = nullptr;

	RequestHandlerThread* m_request_thread = nullptr;
	std::thread m_upload_thread;
	std::mutex m_upload_request_mutex;
	std::condition_variable m_upload_request_cond;

	std::deque <std::shared_ptr<File::FileLocal>> m_files_to_transfer_accepted;
	std::deque <std::shared_ptr<File::FileLocal>> m_files_to_transfer_queued;
	std::deque <std::shared_ptr<File::FileLocal>> m_files_to_transfer_unaccepted;
	std::mutex m_file_upload_mutex;

	std::string user_server_directory = "";
	std::string user_server_directory_pending = "";

	std::vector<std::shared_ptr<File::FileDetails>> m_file_details;
	std::map<File::file_type, int> m_file_icons;

	std::map<unsigned int, std::shared_ptr<File::FileRemote>> m_requested_files;
	int m_req_files_counter = 0;

	const unsigned long long MAX_BANDWIDTH = 100000000; //100MB per packet


	//GUI items setters
	void SetupToolbar();
	void SetupFilesList();
	void SetupLogsList();
	void SetupFileImageList();

	//Action handlers
	void OnSaveButtonClick(wxCommandEvent& evt);
	void OnUploadButtonClick(wxCommandEvent& evt);
	void OnFileActivate(wxListEvent& evt);
	void OnKeyDown(wxKeyEvent& evt);
	void OnServerResponse(wxThreadEvent& evt);
	void OnClose(wxCloseEvent& evt);


	//Display handlers
	void DisplayLog(const std::string&& log_text, const wxColour&& log_colour) const;
	void DisplayFiles() const;
	void ResetFilesList();
	
	//Requests handlers
	void ChangeDirectory(std::string& path);
	void SaveSelectedFiles();
	void RemoveSelectedFiles();
	void UploadFile();
	void SendFileBytes();

	//Thread action
	void SendRequest(ftp_request& new_request, ftp_connection::conn_type conn_type);
};