
#pragma once

#include "Tools/Server.h"
#include "BotProtocol.h"

class ServerHandler;
class User;
class Session;

class ClientHandler : public Server::Client::Listener
{
public:
  ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client);
  ~ClientHandler();

  uint64_t getId() const {return id;}
  
  void_t send(const byte_t* data, size_t size) {client.send(data, size);}

private:
  enum State
  {
    newState,
    loginState,
    authedState,
    botState,
  };

private:
  uint64_t id;
  uint32_t clientAddr;
  ServerHandler& serverHandler;
  Server::Client& client;
  State state;
  User* user;
  Session* session;
  byte_t loginkey[64];

  void_t handleMessage(const BotProtocol::Header& messageHeader, byte_t* data, size_t size);

  void_t handleLogin(uint64_t source, BotProtocol::LoginRequest& loginRequest);
  void_t handleAuth(uint64_t source, BotProtocol::AuthRequest& authRequest);
  void_t handleCreateSession(uint64_t source, BotProtocol::CreateSessionRequest& createSessionRequest);
  void_t handleRegisterBot(uint64_t source, BotProtocol::RegisterBotRequest& registerBotRequest);

  void_t sendErrorResponse(BotProtocol::MessageType messageType, uint64_t destination, const String& errorMessage);

private: // Server::Client::Listener
  virtual size_t handle(byte_t* data, size_t size);
  virtual void_t write() {};
};
