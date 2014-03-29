
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
  
  void_t sendMessage(BotProtocol::MessageType type, const void_t* data, size_t size);
  void_t sendEntity(BotProtocol::EntityType type, uint32_t id, const void_t* data, size_t size);
  void_t removeEntity(BotProtocol::EntityType type, uint32_t id);

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

private:
  void_t handleMessage(const BotProtocol::Header& messageHeader, byte_t* data, size_t size);

  void_t handleLogin(BotProtocol::LoginRequest& loginRequest);
  void_t handleAuth(BotProtocol::AuthRequest& authRequest);
  void_t handleCreateSession(BotProtocol::CreateSessionRequest& createSessionRequest);
  void_t handleRegisterBot(BotProtocol::RegisterBotRequest& registerBotRequest);

  void_t sendError(const String& errorMessage);

  template<size_t N> void_t setString(char_t(&str)[N], const String& value)
  {
    size_t size = value.length() + 1;
    if(size > N - 1)
      size = N - 1;
    Memory::copy(str, (const char_t*)value, size);
    str[N - 1] = '\0';
  }
  
  template<size_t N> String getString(char_t(&str)[N])
  {
    str[N - 1] = '\0';
    String result;
    result.attach(str, String::length(str));
    return result;
  }

private: // Server::Client::Listener
  virtual size_t handle(byte_t* data, size_t size);
  virtual void_t write() {};
};
