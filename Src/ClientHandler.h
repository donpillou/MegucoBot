
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
  void_t sendEntity(const void_t* data, size_t size);
  void_t removeEntity(BotProtocol::EntityType type, uint32_t id);

private:
  enum State
  {
    newState,
    loginState,
    userState,
    botState,
    marketState,
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

  void_t handleCreateEntity(BotProtocol::Entity& entity, size_t size);
  void_t handleRemoveEntity(const BotProtocol::Entity& entity);
  void_t handleControlEntity(BotProtocol::Entity& entity, size_t size);
  void_t handleUpdateEntity(BotProtocol::Entity& entity, size_t size);

  void_t handleCreateMarket(BotProtocol::CreateMarketArgs& createMarketArgs);
  void_t handleRemoveMarket(uint32_t id);
  void_t handleControlMarket(BotProtocol::ControlMarketArgs& controlMarketArgs);

  void_t handleCreateSession(BotProtocol::CreateSessionArgs& createSessionArgs);
  void_t handleRemoveSession(uint32_t id);
  void_t handleControlSession(BotProtocol::ControlSessionArgs& controlSessionArgs);

  void_t handleCreateSessionTransaction(BotProtocol::CreateTransactionArgs& createTransactionArgs);
  void_t handleRemoveSessionTransaction(uint32_t id);

  void_t handleCreateSessionOrder(BotProtocol::CreateOrderArgs& createOrderArgs);
  void_t handleRemoveSessionOrder(uint32_t id);

  void_t handleUpdateMarketTransaction(BotProtocol::Transaction& transaction);
  void_t handleRemoveMarketTransaction(uint32_t id);

  void_t handleUpdateMarketOrder(BotProtocol::Order& order);
  void_t handleRemoveMarketOrder(uint32_t id);

  void_t sendError(const String& errorMessage);
  void_t sendControlEntityResponse(const void_t* data, size_t size);

private: // Server::Client::Listener
  virtual size_t handle(byte_t* data, size_t size);
  virtual void_t write() {};
};
