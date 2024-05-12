#include "TcpServer.hpp"
#include <iostream>
#include <future>



int main()
{

    auto server = std::make_unique<TcpServer>("127.0.0.1", 8080);
    
    std::getchar();
    server.get()->stopListening = true;
    while (!server.get()->listenWorker.joinable()) {}
    server.get()->ServerShutdown();
}

