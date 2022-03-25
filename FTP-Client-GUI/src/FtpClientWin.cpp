#include"../include/FtpClientWin.h"
wxBEGIN_EVENT_TABLE(FtpClientWin, wxFrame)
	EVT_LIST_ITEM_ACTIVATED(wxID_ANY, FtpClientWin::OnFileActivate)
	EVT_CLOSE(FtpClientWin::OnClose)
wxEND_EVENT_TABLE()

FtpClientWin::FtpClientWin() : wxFrame(nullptr, wxID_ANY, "FTP Client", wxPoint(30, 30), wxSize(1280, 768))
{
	//Connect server response thread event
	Connect(evt_id::SERVER_RESPONSE_ID, wxEVT_SERVER_RESPONSE, wxThreadEventHandler(FtpClientWin::OnServerResponse));

	//Establish connection with the server
	client.EstablishControlConnection("127.0.0.1", 60000);
	client.EstablishDataConnection("127.0.0.1", 60000);

	m_panel = new wxPanel(this, wxID_ANY);

	auto* panel_sizer = new wxBoxSizer(wxVERTICAL);

	m_panel->SetSizer(panel_sizer);

	FtpClientWin::SetupToolbar();
	FtpClientWin::SetupFilesList();
	FtpClientWin::SetupLogsList();
	FtpClientWin::SetupFileImageList();

	panel_sizer->AddStretchSpacer();

	m_panel->Layout();

	m_panel->Bind(wxEVT_CHAR_HOOK, &FtpClientWin::OnKeyDown, this);

	FtpClientWin::ChangeDirectory(user_server_directory);
}

FtpClientWin::~FtpClientWin(){}


void FtpClientWin::SetupToolbar()
{
	m_toolbar = this->CreateToolBar(wxTB_HORIZONTAL, wxID_ANY);

	//Saving directory picker
	auto current_path = std::filesystem::current_path().string();
	const wxFont toolbar_labels_font(15, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

	m_save_dir_picker = new wxDirPickerCtrl(m_toolbar, wxID_ANY, current_path);
	m_save_dir_picker->SetSize(wxSize(300, 50));

	//Saving directory label
	auto* m_dir_picker_label = new wxStaticText(m_toolbar, wxID_ANY, "Set saving directory:");
	m_dir_picker_label->SetFont(toolbar_labels_font);

	m_toolbar->AddControl(m_dir_picker_label);
	m_toolbar->AddControl(m_save_dir_picker);

	//Save button
	m_save_button = new wxButton(m_toolbar, wxID_ANY, "Save selected files");
	m_save_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FtpClientWin::OnSaveButtonClick, this);

	m_toolbar->AddControl(m_save_button);

	//User file picker
	m_send_file_picker = new wxFilePickerCtrl(m_toolbar, wxID_ANY, current_path);
	m_send_file_picker->SetSize(wxSize(300, 50));

	m_upload_button = new wxButton(m_toolbar, wxID_ANY, "Upload chosen file");
	m_upload_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &FtpClientWin::OnUploadButtonClick, this);

	auto* m_send_picker_label = new wxStaticText(m_toolbar, wxID_ANY, "Upload file:");
	m_send_picker_label->SetFont(toolbar_labels_font);
	
	m_toolbar->AddStretchableSpace();

	m_toolbar->AddControl(m_send_picker_label);
	m_toolbar->AddControl(m_send_file_picker);
	m_toolbar->AddControl(m_upload_button);

	m_toolbar->Realize();
}

void FtpClientWin::SetupFilesList()
{
	auto* server_files_label = new wxStaticText(m_panel, wxID_ANY, "Server files");
	const auto panel_sizer = dynamic_cast<wxBoxSizer*> (m_panel->GetSizer());

	panel_sizer->Add(server_files_label, 0, wxLEFT, 10);

	m_server_files_list = new wxListCtrl(m_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);


	const std::vector<std::string> file_desc_columns =
	{
		"File name",
		"File size",
	};

	for (auto& column_text : file_desc_columns)
	{
		wxListItem new_col;
		new_col.SetId(m_server_files_list->GetColumnCount());
		new_col.SetText(column_text);

		m_server_files_list->InsertColumn(m_server_files_list->GetColumnCount(), new_col);
		m_server_files_list->SetColumnWidth(new_col, 200);
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


void FtpClientWin::SetupFileImageList()
{
	wxInitAllImageHandlers();

	using namespace File;

	m_file_image_list = new wxImageList(32, 32);
	m_server_files_list->AssignImageList(m_file_image_list, wxIMAGE_LIST_SMALL);

	const wxImage dir_img(wxT("./img/dir-icon-small.png"));
	const wxImage file_img(wxT("./img/file-icon-small.png"));

	auto dir_img_id = m_file_image_list->Add(dir_img);
	auto file_img_id = m_file_image_list->Add(file_img);

	m_file_icons.insert({ file_type::DIR, dir_img_id });
	m_file_icons.insert({ file_type::FILE, file_img_id });
	
}

void FtpClientWin::OnSaveButtonClick(wxCommandEvent& evt)
{
	FtpClientWin::SaveSelectedFiles();
}

void FtpClientWin::OnUploadButtonClick(wxCommandEvent& evt)
{
	FtpClientWin::UploadFile();
}

void FtpClientWin::OnFileActivate(wxListEvent& evt)
{

	auto file_name = evt.GetItem().GetText().ToStdString();

	if (m_file_details[evt.GetItem()]->type == File::file_type::DIR)
	{

		//if user is not in the home directory, the first option is to go back to previous directory.
		//change to path for previous directory
		if (!user_server_directory.empty() && evt.GetItem() == 0)
		{
			const auto last_dir = user_server_directory.find_last_of("/");
			user_server_directory.erase(user_server_directory.cbegin() + last_dir, user_server_directory.cend());
		}

		else
		{
			user_server_directory += "/" + file_name;
		}

		FtpClientWin::DisplayLog("[YOU]: changing directory to : home" + user_server_directory, wxColour(0, 255, 255));
		FtpClientWin::ChangeDirectory(user_server_directory);

	}

	else
	{
		FtpClientWin::DisplayLog("Trying to download file: " + file_name, wxColour(0, 255, 255));
		FtpClientWin::SaveSelectedFiles();
	}
	
	
}

void FtpClientWin::OnKeyDown(wxKeyEvent& evt)
{

	if(evt.GetKeyCode() == 127)
	{
		FtpClientWin::RemoveSelectedFiles();
	}
}


void FtpClientWin::OnServerResponse(wxThreadEvent& evt)
{
	
	auto response = evt.GetPayload<ftp_request>();

	switch(response.header.operation)
	{
	case ftp_request_header::ftp_operation::SERVER_OK:
		{
		unsigned long request_hash;
		response.ExtractTrivialFromBuffer(request_hash);
		
		ftp_request data_collect_request;
		data_collect_request.header.operation = ftp_request_header::ftp_operation::COLLECT_DATA;
		data_collect_request.InsertTrivialToBuffer(request_hash);
		
		FtpClientWin::SendRequest(data_collect_request, ftp_connection::conn_type::data);

		break;
		}
	case ftp_request_header::ftp_operation::CD:
		{

		FtpClientWin::DisplayLog("[INFO]: Fetched data from the server.", wxColour(0, 204, 0));
		FtpClientWin::ResetFilesList();
		
		while(response.header.request_size > 0)
		{

			std::string file_name;
			std::size_t file_size;
			File::file_type file_type;
			File::ExtractFileDetails(response,file_name, file_size, file_type);

			m_file_details.push_back(
				std::make_shared<File::FileDetails>(file_name, file_size, file_type)
			);

		}

		FtpClientWin::DisplayFiles();


		break;
		}

	case ftp_request_header::ftp_operation::RETR:
		{
		FtpClientWin::DisplayLog("[INFO]: Fetched files from the server.", wxColour(0, 204, 0));

		auto user_file_path = m_save_dir_picker->GetPath().ToStdString();
		while (!response.mem_buffer.empty())
		{
			
			std::string file_name;

			response.ExtractStringFromBuffer(file_name);

			std::ofstream retrieved_file(user_file_path + "\\" + file_name, std::ios::binary);
			std::vector<unsigned char> retrieved_file_buffer;

			response.ExtractToVector(retrieved_file_buffer);

			std::copy(
				retrieved_file_buffer.cbegin(),
				retrieved_file_buffer.cend(),
				std::ostreambuf_iterator<char>(retrieved_file)
			);


			FtpClientWin::DisplayLog("[INFO]: File: " + file_name + " successfully saved!", wxColour(0, 204, 0));
		}
		break;
		}

	case ftp_request_header::ftp_operation::STOR:
		{
		std::string server_response;
		response.ExtractStringFromBuffer(server_response);
		FtpClientWin::DisplayLog("[SERVER]: " + server_response, wxColour(0, 204, 0));
		FtpClientWin::ChangeDirectory(user_server_directory);
		break;
		}

	case ftp_request_header::ftp_operation::DELE:
		{
		std::string server_response;
		response.ExtractStringFromBuffer(server_response);
		FtpClientWin::DisplayLog("[SERVER]: " + server_response, wxColour(0, 204, 0));
		FtpClientWin::ChangeDirectory(user_server_directory);
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
	using namespace File;

	for(auto& file_detail : m_file_details)
	{
		auto& [file_name, file_size, type] = *file_detail;

		wxListItem new_file;
		new_file.SetId(m_server_files_list->GetItemCount());
		new_file.SetText(file_name);

		m_server_files_list->InsertItem(new_file, file_name, m_file_icons.at(type));
		m_server_files_list->SetItem(new_file, 1,  std::to_string(file_size));
		
	}


}

void FtpClientWin::ResetFilesList()
{
	if (m_server_files_list->GetItemCount() > 0)
	{
		m_file_details.erase(m_file_details.cbegin(), m_file_details.cend());
		m_server_files_list->DeleteAllItems();
	}


	if (!user_server_directory.empty())
	{
		std::string prev_dir_str = "<----";

		m_file_details.push_back(
			std::make_shared<File::FileDetails>(prev_dir_str, 0, File::file_type::DIR)
		);

	}
}
void FtpClientWin::ChangeDirectory(std::string& path)
{
	ftp_request temp_request;
	temp_request.header.operation = ftp_request_header::ftp_operation::CD;
	

	//temp_request.InsertTrivialToBuffer(directory_length);
	temp_request.InsertStringToBuffer(path);

	//running the side-thread to handle ftp_request sending and receiving

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
		ftp_request temp_request;
		temp_request.header.operation = ftp_request_header::ftp_operation::RETR;

		//First, we specify the directory where the files lay.
		//Its determined by the user_server_directory variable.
		temp_request.InsertStringToBuffer(user_server_directory);

		//foreach selected items we insert them into ftp_request...
		FtpClientWin::InsertSelectedToRequest(temp_request);
		FtpClientWin::SendRequest(temp_request, ftp_connection::conn_type::control);
	}
}


void FtpClientWin::RemoveSelectedFiles()
{
	if (m_server_files_list->GetSelectedItemCount() == 0)
	{
		auto* no_selected_files_dialog = new wxMessageDialog(this, "Select files before deleting!");
		no_selected_files_dialog->ShowModal();
	}

	else
	{

		ftp_request temp_request;
		temp_request.header.operation = ftp_request_header::ftp_operation::DELE;

		//First, we specify the directory where the files lay.
		//Its determined by the user_server_directory variable.
		temp_request.InsertStringToBuffer(user_server_directory);

		//foreach selected items we insert them into ftp_request...
		FtpClientWin::InsertSelectedToRequest(temp_request);
		FtpClientWin::SendRequest(temp_request, ftp_connection::conn_type::control);
	}
}

void FtpClientWin::InsertSelectedToRequest(ftp_request& request) const
{
	auto next_item = -1;

	while ((next_item = m_server_files_list->GetNextItem(next_item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != -1)
	{
		//for each selected item, we insert its name into the ftp_request.

		auto file_name = m_file_details[next_item]->file_name;

		//temp_request.InsertTrivialToBuffer(file_name_length);
		request.InsertStringToBuffer(file_name);

	}
}

void FtpClientWin::UploadFile()
{
	ftp_request temp_request;
	temp_request.header.operation = ftp_request_header::ftp_operation::STOR;

	auto file_name = m_send_file_picker->GetFileName().GetFullName().ToStdString();
	

	std::ifstream file_binary(m_send_file_picker->GetPath().ToStdString(), std::ios::binary);

	std::vector<unsigned char> file_binary_buffer(std::istreambuf_iterator<char>(file_binary), {});
	auto file_binary_size = file_binary_buffer.size();

	temp_request.InsertStringToBuffer(file_name);

	//temp_request.InsertTrivialToBuffer(file_binary_size);

	temp_request.CopyFromVector(file_binary_buffer);

	temp_request.InsertStringToBuffer(user_server_directory);

	FtpClientWin::SendRequest(temp_request, ftp_connection::conn_type::control);
}

void FtpClientWin::SendRequest(ftp_request& new_request, ftp_connection::conn_type conn_type)
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