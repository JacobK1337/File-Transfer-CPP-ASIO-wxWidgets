#include"../include/FtpClientWin.h"
wxBEGIN_EVENT_TABLE(FtpClientWin, wxFrame)
	EVT_LIST_ITEM_ACTIVATED(wxID_ANY, FtpClientWin::onActivate)
	EVT_CLOSE(FtpClientWin::OnClose)
wxEND_EVENT_TABLE()

FtpClientWin::FtpClientWin() : wxFrame(nullptr, wxID_ANY, "FTP Client", wxPoint(30, 30), wxSize(800, 600))
{
	//Panel contains all items in the window
	m_panel = new wxPanel(this, wxID_ANY);

	Connect(evt_id::SERVER_RESPONSE_ID, wxEVT_SERVER_RESPONSE, wxThreadEventHandler(FtpClientWin::OnServerResponse));
	client.EstablishControlConnection("127.0.0.1", 60000);

	FtpClientWin::SetupToolbar();

	wxBoxSizer* panel_sizer = new wxBoxSizer(wxVERTICAL);

	//FILE LIST
	
	auto* some_text = new wxStaticText(m_panel, wxID_ANY, "Server files at: " + user_directory);
	
	panel_sizer->Add(some_text, 0, wxLEFT, 10);
	 

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


	auto* console_label = new wxStaticText(m_panel, wxID_ANY, "Logs: ");
	panel_sizer->Add(console_label);

	//LOGS LIST
	m_logs_list = new wxListCtrl(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);
	m_logs_list->InsertColumn(0, "", wxLIST_FORMAT_LEFT, 400);
	panel_sizer->Add(m_logs_list, 1, wxEXPAND | wxALL, 10);

	panel_sizer->AddStretchSpacer();

	m_panel->SetSizer(panel_sizer);
	m_panel->Layout();

	FtpClientWin::SetupLogsList();

}
FtpClientWin::~FtpClientWin()
{
	
}


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

	//Some button*
	test_button = new wxButton(m_toolbar, wxID_ANY, "Click me");
	test_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FtpClientWin::onClick, this);

	m_toolbar->AddControl(test_button);

	m_toolbar->Realize();
}

void FtpClientWin::SetupFileList()
{
	
}

void FtpClientWin::SetupLogsList()
{
	
}

void FtpClientWin::onClick(wxCommandEvent& evt)
{

	request temp_request;
	temp_request.request_header.operation = request_code::operation_types::LS;
	std::size_t directory_length = user_directory.length();

	temp_request.InsertTrivialToBuffer(directory_length);
	temp_request.InsertStringToBuffer(user_directory);

	m_request_thread = new RequestHandlerThread(this, client, temp_request);
	
	m_request_thread->Create();
	m_request_thread->Run();
}

void FtpClientWin::onActivate(wxListEvent& evt)
{

	std::string list_item_text = evt.GetItem().GetText().ToStdString();

	
	if(m_file_details[evt.GetItem()] -> is_directory)
	{

	if (list_item_text == "<----")
	{
		auto last_dir = user_directory.find_last_of("/");
		user_directory.erase(user_directory.cbegin() + last_dir, user_directory.cend());
	}

	else
		user_directory += "/" + list_item_text;


	FtpClientWin::DisplayLog("[YOU]: changing directory to : home" + user_directory, wxColour(0, 255, 255));

		request temp_request;
		temp_request.request_header.operation = request_code::operation_types::LS;
		std::size_t directory_length = user_directory.length();

		temp_request.InsertTrivialToBuffer(directory_length);
		temp_request.InsertStringToBuffer(user_directory);


		//running the side-thread to handle request sending and receiving
		m_request_thread = new RequestHandlerThread(this, client, temp_request);
		m_request_thread->Create();
		m_request_thread->Run();
	}

	else
	{
		FtpClientWin::DisplayLog("Trying to download file: " + list_item_text, wxColour(0, 255, 255));
	}
	
	
}

void FtpClientWin::OnServerResponse(wxThreadEvent& evt)
{
	
	auto response = evt.GetPayload<request>();

	switch(response.request_header.operation)
	{
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

			FtpClientWin::DisplayFile(file_name, file_size, is_directory);

			m_file_details.push_back(
				std::make_shared<FileDetails>(file_name, file_size, is_directory)
			);

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

void FtpClientWin::DisplayFile(const std::string& file_name, const std::size_t file_size, const bool is_directory)
{
	wxListItem new_file;
	new_file.SetId(m_server_files_list->GetItemCount());
	new_file.SetText(file_name);

	m_server_files_list->InsertItem(new_file);
	m_server_files_list->SetItem(new_file, 1, std::to_string(file_size));
	m_server_files_list->SetItem(new_file, 2, is_directory ? "Directory" : "File");
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
		wxListItem prev_dir;
		prev_dir.SetId(m_server_files_list->GetItemCount());
		std::string prev_dir_str = "<----";

		prev_dir.SetText(prev_dir_str);
		m_server_files_list->InsertItem(prev_dir);

		m_file_details.push_back(
			std::make_shared<FileDetails>(prev_dir_str, 0, true)
		);

	}
}
void FtpClientWin::GetServerFileList(std::string& file_path)
{

}

void FtpClientWin::OnClose(wxCloseEvent& evt)
{
	client.ControlStreamDisconnect();

	Destroy();
}