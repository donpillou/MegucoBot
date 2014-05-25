
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
  ClientHandler(uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client);
  ~ClientHandler();
  
  void_t deselectSession();
  void_t deselectMarket();

  void_t sendMessage(BotProtocol::MessageType type, uint32_t requestId, const void_t* data, size_t size);
  void_t sendEntity(uint32_t requestId, const void_t* data, size_t size);
  void_t removeEntity(uint32_t requestId, BotProtocol::EntityType type, uint32_t id);

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
  uint32_t clientAddr;
  ServerHandler& serverHandler;
  Server::Client& client;
  State state;
  User* user;
  Session* session;
  Market* market;
  byte_t loginkey[64];

private:
  void_t handleMessage(const BotProtocol::Header& header, byte_t* data, size_t size);

  void_t handleLogin(uint32_t requestId, BotProtocol::LoginRequest& loginRequest);
  void_t handleAuth(uint32_t requestId, BotProtocol::AuthRequest& authRequest);
  void_t handleRegisterBot(uint32_t requestId, BotProtocol::RegisterBotRequest& registerBotRequest);
  void_t handleRegisterMarket(uint32_t requestId, BotProtocol::RegisterMarketRequest& registerMarketRequest);

  void_t handlePing(const byte_t* data, size_t size);

  void_t handleCreateEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size);
  void_t handleRemoveEntity(uint32_t requestId, const BotProtocol::Entity& entity);
  void_t handleControlEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size);
  void_t handleUpdateEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size);
  void_t handleResponse(BotProtocol::MessageType messageType, uint32_t requestId, const BotProtocol::Entity& response, size_t size);

  void_t handleUserCreateMarket(uint32_t requestId, BotProtocol::Market& market);
  void_t handleUserRemoveMarket(uint32_t requestId, const BotProtocol::Entity& entity);
  void_t handleUserControlMarket(uint32_t requestId, BotProtocol::ControlMarket& controlMarket);

  void_t handleUserCreateSession(uint32_t requestId, BotProtocol::Session& session);
  void_t handleUserRemoveSession(uint32_t requestId, const BotProtocol::Entity& entity);
  void_t handleUserControlSession(uint32_t requestId, BotProtocol::ControlSession& controlSession);
  void_t handleBotControlSession(uint32_t requestId, BotProtocol::ControlSession& controlSession);
  void_t handleBotControlMarket(uint32_t requestId, BotProtocol::ControlMarket& controlMarket);

  void_t handleBotCreateSessionTransaction(uint32_t requestId, BotProtocol::Transaction& transaction);
  void_t handleBotUpdateSessionTransaction(uint32_t requestId, BotProtocol::Transaction& transaction);
  void_t handleBotRemoveSessionTransaction(uint32_t requestId, const BotProtocol::Entity& entity);

  void_t handleBotCreateSessionOrder(uint32_t requestId, BotProtocol::Order& order);
  void_t handleBotRemoveSessionOrder(uint32_t requestId, const BotProtocol::Entity& entity);

  void_t handleBotCreateSessionMarker(uint32_t requestId, BotProtocol::Marker& marker);

  void_t handleBotCreateSessionLogMessage(uint32_t requestId, BotProtocol::SessionLogMessage& logMessage);

  void_t handleMarketUpdateMarketTransaction(uint32_t requestId, BotProtocol::Transaction& transaction);
  void_t handleMarketRemoveMarketTransaction(uint32_t requestId, const BotProtocol::Entity& entity);

  void_t handleMarketUpdateMarketOrder(uint32_t requestId, BotProtocol::Order& order);
  void_t handleMarketRemoveMarketOrder(uint32_t requestId, const BotProtocol::Entity& entity);

  void_t handleMarketUpdateMarketBalance(uint32_t requestId, BotProtocol::MarketBalance& balance);

  void_t handleUserCreateMarketOrder(uint32_t requestId, BotProtocol::Order& order);
  void_t handleUserUpdateMarketOrder(uint32_t requestId, BotProtocol::Order& order);
  void_t handleUserRemoveMarketOrder(uint32_t requestId, const BotProtocol::Entity& entity);

  void_t sendMessageHeader(BotProtocol::MessageType type, uint32_t requestId, size_t dataSize);
  void_t sendMessageData(const void_t* data, size_t size);
  void_t sendErrorResponse(BotProtocol::MessageType messageType, uint32_t requestId, const BotProtocol::Entity* entity, const String& errorMessage);

private: // Server::Client::Listener
  virtual size_t handle(byte_t* data, size_t size);
  virtual void_t write() {};
};
