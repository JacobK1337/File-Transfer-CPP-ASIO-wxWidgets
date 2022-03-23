#include"../include/FtpClientWin.h"
wxBEGIN_EVENT_TABLE(FtpClientWin, wxFrame)
	EVT_LIST_ITEM_ACTIVATED(wxID_ANY, FtpClientWin::OnFileActivate)
	EVT_CLOSE(FtpClientWin::OnClose)
wxEND_EVENT_TABLE()

FtpClientWin::FtpClientWin() : wxFrame(nullptr, wxID_ANY, "FTP Client", wxPoint(30, 30), wxSize(800, 600))
{
	//Connect server response thread event
	Connect(evt_id::SERVER_RESPONSE_ID, wxEVT_SERVER_RESPONSE, wxThreadEventHandler(FtpClientWin::OnServerResponse));

	//Establish connection with the server
	client.EstablishControlConnection("127.0.0.1", 60000);
	client.EstablishDataConnection("127.0.0.1", 60000);
	FtpClientWin::SetupToolbar();

	//Panel contains all items in the window
	m_panel = new wxPanel(this, wxID_ANY);

	//Box sizer to align items in a vertical direction
	wxBoxSizer* panel_sizer = new wxBoxSizer(wxVERTICAL);

	m_panel->SetSizer(panel_sizer);

	FtpClientWin::SetupFilesList();
	FtpClientWin::SetupLogsList();

	panel_sizer->AddStretchSpacer();

	m_panel->Layout();


	FtpClientWin::ChangeDirectory(user_directory);
}

FtpClientWin::~FtpClientWin(){}

void FtpClientWin::SetupToolbar()
{
	m_toolbar = this->CreateToolBar(wxTB_HORIZONTAL, wxID_ANY);

	//Saving directory picker
	std::string current_path = std::filesystem::current_path().string();
	m_dir_picker = new wxDirPickerCtrl(m_toolbar, wxID_ANY, current_path, "Choose saving directory");
	m_dir_picker->SetSize(wxSize(300, 50));

	//Saving directory label
	m_dir_picker_label = new wxStaticText(m_toolbar, wxID_ANY, "Set saving directory");

	m_toolbar->AddControl(m_dir_picker_label);
	m_toolbar->AddControl(m_dir_picker);

	//Save button
	m_save_button = new wxButton(m_toolbar, wxID_ANY, "Save selected files");
	m_save_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FtpClientWin::OnSaveButtonClick, this);

	m_toolbar->AddControl(m_save_button);
	m_toolbar->Realize();
}

void FtpClientWin::SetupFilesList()
{
	auto* server_files_label = new wxStaticText(m_panel, wxID_ANY, "Server files at: " + user_directory);
	const auto panel_sizer = dynamic_cast<wxBoxSizer*> (m_panel->GetSizer());

	panel_sizer->Add(server_files_label, 0, wxLEFT, 10);

	m_server_files_list = new wxListCtrl(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);

	const std::vector<std::string> file_desc_columns =
	{
		"File name",
		"File size",
		"File type"
	};

	for (auto& column_text : file_desc_columns)
	{
		wxListItem new_col;
		new_col.SetId(m_server_files_list->GetColumnCount());
		new_col.SetText(column_text);

		m_server_files_list->InsertColumn(m_server_files_list->GetColumnCount(), new_col);
		m_server_files_list->SetColumnWidth(new_col, 100);
	}

	panel_sizer->Add(m_server_files_list, 1, wxEXPAND | wxALL, 10);
}

void FtpClientWin::SetupLogsList()
{
	auto* console_label = new wxStaticText(m_panel, wxID_ANY, "Logs: ");
	const auto panel_sizer = dynamic_cast<wxBoxSizer*> (m_panel->GetSizer());

	panel_sizer->Add(console_label, 0, wxLEFT, 10);

	m_logs_list = new wxListCtrl(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
	m_logs_list->InsertColumn(0, "", wxLIST_FORMAT_LEFT, 400);

	panel_sizer->Add(m_logs_list, 1, wxEXPAND | wxALL, 10);

}

void FtpClientWin::OnSaveButtonClick(wxCommandEvent& evt)
{
	FtpClientWin::SaveSelectedFiles();
}

void FtpClientWin::OnFileActivate(wxListEvent& evt)
{

	const std::string file_name = evt.GetItem().GetText().ToStdString();

	if (m_file_details[evt.GetItem()]->is_directory)
	{

		//if user is not in the home directory, the first option is to go back to previous directory.
		//change to path for previous directory
		if (!user_directory.empty() && evt.GetItem() == 0)
		{
			const auto last_dir = user_directory.find_last_of("/");
			user_directory.erase(user_directory.cbegin() + last_dir, user_directory.cend());
		}

		else
		{
			user_directory += "/" + file_name;
		}

		FtpClientWin::DisplayLog("[YOU]: changing directory to : home" + user_directory, wxColour(0, 255, 255));
		FtpClientWin::ChangeDirectory(user_directory);

	}

	else
	{
		FtpClientWin::DisplayLog("Trying to download file: " + file_name, wxColour(0, 255, 255));
	}
	
	
}

void FtpClientWin::OnServerResponse(wxThreadEvent& evt)
{
	
	auto response = evt.GetPayload<request>();

	switch(response.request_header.operation)
	{
	case request_code::operation_types::SERVER_OK:
		{
		unsigned long request_hash;
		response.ExtractTrivialFromBuffer(request_hash);
		
		request data_collect_request;
		data_collect_request.request_header.operation = request_code::operation_types::COLLECT_DATA;
		data_collect_request.InsertTrivialToBuffer(request_hash);
		//client.EstablishDataConnection("127.0.0.1", 60000);
		
		FtpClientWin::SendRequest(data_collect_request, ftp_connection::conn_type::data);

		break;
		}
	case request_code::operation_types::LS:
		{

		FtpClientWin::DisplayLog("[INFO]: Fetched data from the server.", wxColour(0, 204, 0));
		FtpClientWin::ResetFilesList();

		while(response.request_header.request_size > 0)
		{
			std::size_t file_name_length;
			std::string file_name;
			std::size_t file_size;
			bool is_directory;

			response.ExtractTrivialFromBuffer(file_name_length);
			response.ExtractStringFromBuffer(file_name, file_name_length);
			response.ExtractTrivialFromBuffer(file_size);
			response.ExtractTrivialFromBuffer(is_directory);

			m_file_details.push_back(
				std::make_shared<FileDetails>(file_name, file_size, is_directory)
			);

		}

		FtpClientWin::DisplayFiles();

		//client.DataStreamDisconnect();

		break;
		}

	case request_code::operation_types::RETR:
		{
		FtpClientWin::DisplayLog("[INFO]: Fetched files from the server.", wxColour(0, 204, 0));

		auto user_file_path = m_dir_picker->GetPath().ToStdString();
		while (!response.request_body.empty())
		{
			std::size_t file_name_length;
			std::string file_name;
			std::size_t file_size;

			response.ExtractTrivialFromBuffer(file_name_length);
			response.ExtractStringFromBuffer(file_name, file_name_length);
			response.ExtractTrivialFromBuffer(file_size);
			std::ofstream retrieved_file(user_file_path + "\\" + file_name, std::ios::binary);

			std::copy(
				response.request_body.cbegin(),
				response.request_body.cbegin() + file_size,
				std::ostreambuf_iterator<char>(retrieved_file)
			);
			response.request_body.erase(response.request_body.cbegin(), response.request_body.cbegin() + file_size);

			FtpClientWin::DisplayLog("[INFO]: File: " + file_name + " successfully saved!", wxColour(0, 204, 0));
		}
		break;
		}
	}
}


void FtpClientWin::DisplayLog(const std::string&& log_text, const wxColour&& log_colour) const
{
	wxListItem console_item;
	console_item.SetId(m_logs_list->GetItemCount());
	console_item.SetText(log_text);

	m_logs_list->InsertItem(console_item);
	m_logs_list->SetItemBackgroundColour(m_logs_list->GetItemCount() - 1, log_colour);
}

void FtpClientWin::DisplayFiles() const
{

	for(auto& file_detail : m_file_details)
	{
		auto& [file_name, file_size, is_directory] = *file_detail;

		wxListItem new_file;
		new_file.SetId(m_server_files_list->GetItemCount());
		new_file.SetText(file_name);

		m_server_files_list->InsertItem(new_file);
		m_server_files_list->SetItem(new_file, 1, std::to_string(file_size));
		m_server_files_list->SetItem(new_file, 2, is_directory ? "Directory" : "File");
	}


}

void FtpClientWin::ResetFilesList()
{
	if (m_server_files_list->GetItemCount() > 0)
	{
		m_file_details.erase(m_file_details.cbegin(), m_file_details.cend());
		m_server_files_list->DeleteAllItems();
	}


	if (!user_directory.empty())
	{
		std::string prev_dir_str = "<----";

		m_file_details.push_back(
			std::make_shared<FileDetails>(prev_dir_str, 0, true)
		);

	}
}
void FtpClientWin::ChangeDirectory(std::string& path)
{
	request temp_request;
	temp_request.request_header.operation = request_code::operation_types::LS;
	std::size_t directory_length = path.length();

	temp_request.InsertTrivialToBuffer(directory_length);
	temp_request.InsertStringToBuffer(path);


	//running the side-thread to handle request sending and receiving
	FtpClientWin::SendRequest(temp_request, ftp_connection::conn_type::control);
}

void FtpClientWin::SaveSelectedFiles()
{
	if (m_server_files_list->GetSelectedItemCount() == 0)
	{
		auto* no_selected_files_dialog = new wxMessageDialog(this, "Select files before saving!");
		no_selected_files_dialog->ShowModal();
	}

	else
	{
		auto next_item = -1;
		std::vector<std::string> file_names;

		//First, we specify the directory where the files lay.
		//Its determined by the user_directory variable.

		request temp_request;
		auto user_directory_length = user_directory.length();

		temp_request.InsertTrivialToBuffer(user_directory_length);
		temp_request.InsertStringToBuffer(user_directory);

		//foreach selected items we insert them into request...
		while ((next_item = m_server_files_list->GetNextItem(next_item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1)
		{
			//for each selected item, we insert its name into the request.
			temp_request.request_header.operation = request_code::operation_types::RETR;

			auto file_name = m_file_details[next_item]->file_name;
			auto file_name_length = file_name.length();

			temp_request.InsertTrivialToBuffer(file_name_length);
			temp_request.InsertStringToBuffer(file_name);

		}

		FtpClientWin::SendRequest(temp_request, ftp_connection::conn_type::control);
	}
}




void FtpClientWin::RemoveSelectedFiles()
{
	
}

void FtpClientWin::ImportSelectedFiles()
{
	
}

void FtpClientWin::SendRequest(request& new_request, ftp_connection::conn_type conn_type)
{
	m_request_thread = new RequestHandlerThread(this, client, conn_type, std::move(new_request));
	m_request_thread->Create();
	m_request_thread->Run();
}

void FtpClientWin::OnClose(wxCloseEvent& evt)
{
	client.ControlStreamDisconnect();
	client.DataStreamDisconnect();
	Destroy();
}