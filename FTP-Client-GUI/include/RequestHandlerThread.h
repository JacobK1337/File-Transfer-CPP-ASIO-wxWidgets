#pragma once
#include<wx/wx.h>
#include"ftpclient.h"
#include"declared_events.h"

class RequestHandlerThread : public wxThread
{
public:
	RequestHandlerThread(wxEvtHandler* t_parent, ftp_client& client, std::atomic_bool& running) : wxThread(wxTHREAD_JOINABLE), m_parent(t_parent), m_client(client), running(running)
	{
	}

	wxThread::ExitCode Entry()
	{

		//if there is still a connection, check if there are responses.
		while(running)
		{
			if (m_client.IsControlStreamConnected())
			{
				if (!m_client.ReceivedControlResponses().empty())
				{
					auto response = m_client.ReceivedControlResponses().front();
					m_client.ReceivedControlResponses().pop_front();

					//send response to GUI through inter-thread connection
					SendResponseData(wxEVT_SERVER_RESPONSE, evt_id::SERVER_RESPONSE_ID, response);
				}
				if (!m_client.ReceivedDataResponses().empty())
				{
					auto response = m_client.ReceivedDataResponses().front();
					m_client.ReceivedDataResponses().pop_front();

					//send response to GUI through inter-thread connection
					SendResponseData(wxEVT_SERVER_RESPONSE, evt_id::SERVER_RESPONSE_ID, response);
				}
			}
			wxMilliSleep(25);
		}

		return wxThread::ExitCode();
	}
protected:
	wxEvtHandler* m_parent;
	ftp_client& m_client;
	std::atomic_bool& running;

private:
	 void SendResponseData(const wxEventType& event_type, const int& event_id, const ftp_request& thread_data) const
	{
		wxThreadEvent thread_evt_data(event_type, event_id);
		thread_evt_data.SetPayload(thread_data);
		wxQueueEvent(m_parent, thread_evt_data.Clone());
	}
};