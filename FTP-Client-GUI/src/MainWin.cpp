#include"../include/MainWin.h"

MainWin::MainWin() : wxFrame(nullptr, wxID_ANY, "FTP Project", wxPoint(30, 30), wxSize(800, 600))
{
	m_panel = new wxPanel(this, wxID_ANY);
	m_panel->SetBackgroundColour(wxColour(0, 0, 204));
	const wxFont button_font(20, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
	const wxFont header_font(40, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);

	auto* panel_sizer = new wxBoxSizer(wxVERTICAL);

	panel_sizer->AddStretchSpacer();

	auto* m_header = new wxStaticText(m_panel, wxID_ANY, "FTP Client");
	m_header->SetFont(header_font);
	m_header->SetForegroundColour(wxColour(255, 255, 255));

	
	//
	panel_sizer->Add(m_header, 0, wxALL | wxALIGN_CENTER, 50);

	m_ftp_connect_button = new wxButton(m_panel, 10000, "Connect to FTP Server");
	m_ftp_connect_button->SetFont(button_font);
	m_ftp_connect_button->SetForegroundColour(wxColour(255, 255, 255));

	m_ftp_connect_button->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &MainWin::OnFtpConnectButton, this);

	panel_sizer->Add(m_ftp_connect_button, 0, wxALL | wxALIGN_CENTER, 50);
	panel_sizer->AddStretchSpacer();

	m_panel->SetSizer(panel_sizer);
}

MainWin::~MainWin()
{
	
}

void MainWin::OnFtpConnectButton(wxCommandEvent& evt)
{
	m_ftp_client_win = new FtpClientWin();
	m_ftp_client_win->Show();
}

