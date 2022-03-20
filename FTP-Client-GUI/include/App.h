#pragma once
#include<wx/wx.h>
#include"MainWin.h"

DEFINE_EVENT_TYPE(wxEVT_SERVER_RESPONSE)

class App : public wxApp
{
public:
	App();
	~App();
	virtual bool OnInit() override;

private:
	MainWin* m_main_frame = nullptr;

};