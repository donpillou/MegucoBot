
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

  uint64_t getId() const {return __id;}
  
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
  uint64_t __id;
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
  void_t handleCreateEntityResponse(BotProtocol::CreateEntityResponse& entity);
  void_t handleErrorResponse(BotProtocol::ErrorResponse& errorResponse);

  void_t handleUserCreateMarket(BotProtocol::Market& market);
  void_t handleUserRemoveMarket(const BotProtocol::Entity& entity);
  void_t handleUserControlMarket(BotProtocol::ControlMarket& controlMarket);

  void_t handleUserCreateSession(BotProtocol::Session& session);
  void_t handleUserRemoveSession(const BotProtocol::Entity& entity);
  void_t handleUserControlSession(BotProtocol::ControlSession& controlSession);
  void_t handleBotControlSession(BotProtocol::ControlSession& controlSession);

  void_t handleBotCreateSessionTransaction(BotProtocol::Transaction& transaction);
  void_t handleBotRemoveSessionTransaction(const BotProtocol::Entity& entity);

  void_t handleBotCreateSessionOrder(BotProtocol::Order& order);
  void_t handleBotRemoveSessionOrder(const BotProtocol::Entity& entity);

  void_t handleBotCreateSessionLogMessage(BotProtocol::SessionLogMessage& logMessage);

  void_t handleMarketUpdateMarketTransaction(BotProtocol::Transaction& transaction);
  void_t handleMarketRemoveMarketTransaction(uint32_t id);

  void_t handleMarketUpdateMarketOrder(BotProtocol::Order& order);
  void_t handleMarketRemoveMarketOrder(uint32_t id);

  void_t handleMarketUpdateMarketBalance(BotProtocol::MarketBalance& balance);

  void_t handleUserCreateMarketOrder(BotProtocol::Order& order);
  void_t handleUserUpdateMarketOrder(BotProtocol::Order& order);
  void_t handleUserRemoveMarketOrder(const BotProtocol::Entity& entity);

  void_t sendErrorResponse(const BotProtocol::Entity& entity, const String& errorMessage);

private: // Server::Client::Listener
  virtual size_t handle(byte_t* data, size_t size);
  virtual void_t write() {};
};
