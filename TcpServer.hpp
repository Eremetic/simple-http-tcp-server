#pragma once
#include <thread>
#include <string>
#include <memory>

#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")






constexpr auto BUFFER_SIZE = 0x7800;


class TcpServer
{
private:

	std::string ipAddress;
	std::uint_least16_t port;
	std::shared_ptr<SOCKET> ourSocket;
	std::shared_ptr<SOCKET> newSocket;
	long incomingMessage = 0;
	std::shared_ptr<sockaddr_in> socketAddress;
	std::int_fast32_t szSocketAddress = sizeof(sockaddr_in);
	std::string serverMessage;
	std::shared_ptr<WSADATA> wsaData;

	void ServerStartup();
	void IncomingConnection(__in SOCKET &incomingSocket);
	
	const std::string ConstructResponse();
	void DeliverResponse();
	const void InternalLog(__in const std::string& message);
	void ShutdownWithError(__in const std::string& errorMessage);
	void startListen();



public:
	::TcpServer(__in const std::string& hostIpAddress, __in std::uint_least16_t hostPort);
	std::jthread listenWorker;
	std::atomic_bool stopListening = false;
	void ServerShutdown();
};


