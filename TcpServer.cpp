#include "TcpServer.hpp"

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <array>
#include <future>


TcpServer::TcpServer(__in const std::string& hostIpAddress, __in std::uint_least16_t hostPort) : ipAddress(hostIpAddress), port(hostPort), serverMessage(ConstructResponse())
{
	///initialization
	this->ourSocket = std::make_shared<SOCKET>();
	this->newSocket = std::make_shared<SOCKET>();
	this->socketAddress = std::make_shared<sockaddr_in>();
	this->wsaData = std::make_shared<WSADATA>();

	///setup
	this->socketAddress.get()->sin_family = AF_INET;
	this->socketAddress.get()->sin_port = htons(this->port);

	
	if (inet_pton(AF_INET, this->ipAddress.c_str(), &this->socketAddress.get()->sin_addr) != 1)
		this->ShutdownWithError("[!]Failed to Convert IP Address For Socket");

	///startup
	this->ServerStartup();
	this->listenWorker = std::jthread(&TcpServer::startListen, this);
}




void TcpServer::ServerStartup()
{
	auto pWsaData = std::weak_ptr(this->wsaData);
	auto pOurSocket = std::weak_ptr(this->ourSocket);
	auto pSocketAddress = std::weak_ptr(this->socketAddress);
	
	
	if (WSAStartup(MAKEWORD(2, 0), pWsaData.lock().get()) != 0)
	{
		this->ShutdownWithError("[!]WSAStartup Failed");
	}

	*pOurSocket.lock().get() = socket(AF_INET, SOCK_STREAM, 0);
	if (!*pOurSocket.lock().get())
	{
		this->ShutdownWithError("[!]Failed To Create Socket");
	}

	if (bind(*pOurSocket.lock().get(), (sockaddr*)pSocketAddress.lock().get(), this->szSocketAddress) < 0)
	{
		this->ShutdownWithError("[!]Failed To Connect Socket With Address");
	}
}


[[noreturn]] void TcpServer::ServerShutdown()
{
	auto pOurSocket = std::weak_ptr(this->ourSocket);
	auto pNewSocket = std::weak_ptr(this->newSocket);
	
	if (pOurSocket.lock().get())
	closesocket(*pOurSocket.lock().get());
	if (pNewSocket.lock().get())
	closesocket(*pNewSocket.lock().get());

	WSACleanup();
	_exit(0x0);
}



static void ConnectionHelper(__in std::weak_ptr<SOCKET> socket, __in char* recvBuffer, __out std::int_fast32_t* bytesReceived)
{
		*bytesReceived = recv(*socket.lock().get(), recvBuffer, BUFFER_SIZE, 0);
}


void TcpServer::startListen()
{
	auto pNewSocket = std::weak_ptr(this->newSocket);
	auto pOurSocket = std::weak_ptr(this->ourSocket);
	auto pSocketAddress = std::weak_ptr(this->socketAddress);
	std::ostringstream ss;
	char errorCode = { 0 };
	socklen_t szErrorCode = sizeof(errorCode);
	auto internalBuffer = std::make_unique<std::array<char, BUFFER_SIZE>>();
	std::int_fast32_t dwBytes = 0;

	
	
	if (listen(*pOurSocket.lock().get(), 20) < 0)
	{
		this->ShutdownWithError("[!]Failed To Peek Socket");
	}

	std::string sinAddrCov; sinAddrCov.reserve(MAX_PATH);
	inet_ntop(AF_INET, &pSocketAddress.lock()->sin_addr, &sinAddrCov[0], MAX_PATH);
	if (!sinAddrCov.empty())
	{
		
		ss << h_errno;
		this->ShutdownWithError(ss.str());
	}

	

	while (true)
	{

		if (this->stopListening == true) break;
		this->InternalLog("====== Waiting for a new connection ======\n\n");

		this->IncomingConnection(*pNewSocket.lock().get());
		
		getsockopt(*pNewSocket.lock().get(), SOL_SOCKET, SO_ERROR, &errorCode, &szErrorCode);
		if (errorCode == 0)
		{
			auto recvResult = std::async(std::launch::async, ConnectionHelper, pNewSocket, internalBuffer.get()->data(), &dwBytes);		
			if (dwBytes < 0)
			{
				this->ShutdownWithError("[!]Failed To Receive Request From Client");
			}
			else
			{
				this->InternalLog("\n\n------Received Request from client------\n\n");

				this->DeliverResponse();
			}	
		}

		closesocket(*pNewSocket.lock().get());
	}
}


void TcpServer::IncomingConnection(__in SOCKET &incomingSocket)
{
	auto pOurSocket = std::weak_ptr(this->ourSocket);
	auto pSocketAddress = std::weak_ptr(this->socketAddress);
	

	incomingSocket = accept(*pOurSocket.lock().get(), (sockaddr*)pSocketAddress.lock().get(), &this->szSocketAddress);
	if (!incomingSocket)
	{
		std::string sinAddrCov; sinAddrCov.reserve(MAX_PATH);
		inet_ntop(AF_INET, &pSocketAddress.lock()->sin_addr, &sinAddrCov[0], MAX_PATH);
		if (!sinAddrCov.empty())
		{
			std::ostringstream errorSS;
			errorSS << h_errno;
			this->ShutdownWithError(errorSS.str());
		}
		std::ostringstream ss;
		ss << "[!]Server Failed To Accept Incoming Connection From : " << sinAddrCov.c_str() << "; Port : " << ntohs(pSocketAddress.lock()->sin_port);
		this->ShutdownWithError(ss.str());
	}
}

const std::string TcpServer::ConstructResponse()
{
	std::string htmlFile = "<!DOCTYPE html><html lang=\"en\"><body><h1> HOME </h1><p> YOU FOUND ME </p></body></html>";

	std::ostringstream ss;
	ss << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " << htmlFile.size() << "\n\n" << htmlFile;

	return ss.str();
}


void TcpServer::DeliverResponse()
{
	auto pNewSocket = std::weak_ptr(this->newSocket);
	std::int_fast32_t bytesSent = 0;
	std::int_fast32_t dwBytesSent = 0;

	while (dwBytesSent < this->serverMessage.size())
	{
		bytesSent = send(*pNewSocket.lock().get(), this->serverMessage.c_str(), (std::int_fast32_t)this->serverMessage.size(), 0);
		if (bytesSent < 0)break;

		dwBytesSent += bytesSent;
	}

	if (dwBytesSent == this->serverMessage.size())
	{
		this->InternalLog("------ Server Response Sent To Client ------\n\n");
	}
	else
		this->InternalLog("[!]Failed To Deliver Response");
}


[[noreturn]] void TcpServer::ShutdownWithError(__in const std::string &errorMessage)
{
	auto pOurSocket = std::weak_ptr(this->ourSocket);
	auto pNewSocket = std::weak_ptr(this->newSocket);
	
	std::cout << h_errno << std::endl;
	this->InternalLog("ERROR: " + errorMessage);

	if (pOurSocket.lock().get())
		closesocket(*pOurSocket.lock().get());
	if (pNewSocket.lock().get())
		closesocket(*pNewSocket.lock().get());

	WSACleanup();
	_exit(1);
}

const void TcpServer::InternalLog(__in const std::string& message)
{
	std::cout << message << std::endl;
}
