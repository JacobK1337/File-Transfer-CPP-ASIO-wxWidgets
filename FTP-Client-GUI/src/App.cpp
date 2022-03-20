#include"../include/App.h"
wxIMPLEMENT_APP(App);

App::App()
{
	
}
App::~App()
{
	
}

bool App::OnInit()
{
	m_main_frame = new MainWin();
	m_main_frame->Show();

	return true;
}

