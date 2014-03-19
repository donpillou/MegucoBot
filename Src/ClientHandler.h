
#pragma once

#include "Tools/Server.h"
#include "BotProtocol.h"

class ServerHandler;

class ClientHandler : public Server::Client::Listener
{
public:
  ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client);

  uint64_t getId() const {return id;}

private:
  uint64_t id;
  uint32_t clientAddr;
  ServerHandler& serverHandler;
  Server::Client& client;
  byte_t loginkey[64];

  void_t handleMessage(const BotProtocol::Header& messageHeader, byte_t* data, size_t size);

  void_t handleLogin(uint64_t source, BotProtocol::LoginRequest& loginRequest);

  void_t sendErrorResponse(BotProtocol::MessageType messageType, uint64_t destination, const String& errorMessage);

private: // Server::Client::Listener
  virtual size_t handle(byte_t* data, size_t size);
  virtual void_t write() {};
};
