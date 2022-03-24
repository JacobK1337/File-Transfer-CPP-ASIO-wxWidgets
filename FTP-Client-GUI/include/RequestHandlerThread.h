#pragma once
#include<wx/wx.h>
#include"ftpclient.h"
#include"declared_events.h"

class RequestHandlerThread : public wxThread
{
public:
	RequestHandlerThread(wxEvtHandler* t_parent, ftp_client& client, ftp_connection::conn_type conn_type, ftp_request&& new_request) : wxThread(wxTHREAD_DETACHED), m_parent(t_parent), m_client(client), m_request(new_request), m_conn_type(conn_type)
	{
	}

	wxThread::ExitCode Entry()
	{
		//sending the ftp_request to server
		switch(m_conn_type)
		{

		case ftp_connection::conn_type::control:
			m_client.SendControlRequest(m_request);
			break;
		case ftp_connection::conn_type::data:
			m_client.SendDataRequest(m_request);
			break;

		}

		//wxMilliSleep(1);

		//waiting for response before continuing, or to disconnect
		//checking each 25ms
		while (m_client.ReceivedControlResponses().empty() && m_client.ReceivedDataResponses().empty() && m_client.IsControlStreamConnected())
			wxMilliSleep(25);


		//if there is still a connection, check if there are responses.
		if(m_client.IsControlStreamConnected())
		{
			if(!m_client.ReceivedControlResponses().empty())
			{
				auto response = m_client.ReceivedControlResponses().front();
				m_client.ReceivedControlResponses().pop_front();
				
				//send response to GUI through inter-thread connection
				SendResponseData(wxEVT_SERVER_RESPONSE, evt_id::SERVER_RESPONSE_ID, response);
			}
			if(!m_client.ReceivedDataResponses().empty())
			{
				auto response = m_client.ReceivedDataResponses().front();
				m_client.ReceivedDataResponses().pop_front();

				//send response to GUI through inter-thread connection
				SendResponseData(wxEVT_SERVER_RESPONSE, evt_id::SERVER_RESPONSE_ID, response);
			}
		}

		return wxThread::ExitCode();
	}
protected:
	wxEvtHandler* m_parent;
	ftp_client& m_client;
	const ftp_request m_request;
	const ftp_connection::conn_type m_conn_type;

private:
	 void SendResponseData(const wxEventType& event_type, const int& event_id, const ftp_request& thread_data) const
	{
		wxThreadEvent thread_evt_data(event_type, event_id);
		thread_evt_data.SetPayload(thread_data);
		wxQueueEvent(m_parent, thread_evt_data.Clone());
	}
};