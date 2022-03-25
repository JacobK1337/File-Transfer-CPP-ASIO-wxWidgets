#pragma once
#include<wx/wx.h>
#include"FtpClientWin.h"
class MainWin : public wxFrame
{
public:
	MainWin();
	~MainWin();
	void OnFtpConnectButton(wxCommandEvent& evt);

private:
	wxButton* m_ftp_connect_button = nullptr;
	//wxStaticText* m_header = nullptr;
	wxPanel* m_panel = nullptr;
	//
	FtpClientWin* m_ftp_client_win = nullptr;
};