
#pragma once

#include "Tools/Server.h"
#include "BotProtocol.h"

class ServerHandler;
class User;
class Session;
class Market;

class ClientHandler : public Server::Client::Listener
{
public:
  ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client);
  ~ClientHandler();

  uint64_t getId() const {return id;}
  
  void_t deselectSession();
  void_t deselectMarket();

  void_t sendMessage(BotProtocol::MessageType type, const void_t* data, size_t size);
  void_t sendEntity(BotProtocol::EntityType type, uint32_t id, const void_t* data, size_t size);
  void_t removeEntity(BotProtocol::EntityType type, uint32_t id);

private:
  enum State
  {
    newState,
    loginState,
    userState,
    botState,
    adapterState,
  };

private:
  uint64_t id;
  uint32_t clientAddr;
  ServerHandler& serverHandler;
  Server::Client& client;
  State state;
  User* user;
  Session* session;
  Market* market;
  byte_t loginkey[64];

private:
  void_t handleMessage(const BotProtocol::Header& messageHeader, byte_t* data, size_t size);

  void_t handleLogin(BotProtocol::LoginRequest& loginRequest);
  void_t handleAuth(BotProtocol::AuthRequest& authRequest);
  void_t handleRegisterBot(BotProtocol::RegisterBotRequest& registerBotRequest);
  void_t handleRegisterMarket(BotProtocol::RegisterMarketRequest& registerMarketRequest);

  void_t handlePing(const byte_t* data, size_t size);

  void_t handleCreateEntity(BotProtocol::EntityType type, byte_t* data, size_t size);
  void_t handleRemoveEntity(BotProtocol::EntityType type, uint32_t id);
  void_t handleControlEntity(BotProtocol::EntityType type, uint32_t id, byte_t* data, size_t size);

  void_t handleCreateMarket(BotProtocol::CreateMarketArgs& createMarketArgs);
  void_t handleRemoveMarket(uint32_t id);

  void_t handleCreateSession(BotProtocol::CreateSessionArgs& createSessionArgs);
  void_t handleRemoveSession(uint32_t id);

  void_t handleCreateTransaction(BotProtocol::CreateTransactionArgs& createTransactionArgs);
  void_t handleRemoveTransaction(uint32_t id);

  void_t handleCreateOrder(BotProtocol::CreateOrderArgs& createOrderArgs);
  void_t handleRemoveOrder(uint32_t id);

  void_t handleControlSession(uint32_t id, BotProtocol::ControlSessionArgs& controlSessionArgs);

  void_t sendError(const String& errorMessage);

private: // Server::Client::Listener
  virtual size_t handle(byte_t* data, size_t size);
  virtual void_t write() {};
};
