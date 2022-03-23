
#include <iostream>
#include"include/request.h"
#include<filesystem>
#include"include/ftpclient.h"



int main()
{
	ftp_client test;
	test.EstablishControlConnection("127.0.0.1", 60000);

	while(true)
	{
		
	}
	
}


