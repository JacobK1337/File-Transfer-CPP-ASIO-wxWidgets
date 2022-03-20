
#include <iostream>
#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
#include"../FPTProject/include/ftpserver.h"

int main()
{
    ftp_server server(60000);

    server.Start();
    while(true)
    {
        server.ServerUpdate();
    }
}

