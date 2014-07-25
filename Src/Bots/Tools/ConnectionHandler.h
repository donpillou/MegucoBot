
#pragma once

#include "DataConnection.h"
#include "BotConnection.h"
#include "HandlerConnection.h"
#include "Broker.h"

class ConnectionHandler : public HandlerConnection::Callback, public DataConnection::Callback
{
public:
  ConnectionHandler() : broker(0), botSession(0), lastReceivedTradeId(0) {}
  ~ConnectionHandler() {delete broker;}

  bool_t connect(uint16_t botPort, uint32_t dataIp, uint16_t dataPort);
  const String& getLastError() {return error;}

  bool_t process();

private:
  uint32_t dataIp;
  uint16_t dataPort;
  String error;
  BotConnection botConnection;
  HandlerConnection handlerConnection;
  DataConnection dataConnection;
  Broker* broker;
  Bot::Session* botSession;

  Socket* sessionHandlerSocket;

  uint64_t lastReceivedTradeId;

private:
  void_t handleCreateEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size);
  void_t handleUpdateEntity(uint32_t requestId, const BotProtocol::Entity& entity, size_t size);
  void_t handleRemoveEntity(uint32_t requestId, const BotProtocol::Entity& entity);
  void_t handleControlEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size);

  void_t handleCreateSessionItem(uint32_t requestId, BotProtocol::SessionItem& sessionItem);
  void_t handleUpdateSessionItem(uint32_t requestId, const BotProtocol::SessionItem& sessionItem);
  void_t handleRemoveSessionItem(uint32_t requestId, const BotProtocol::Entity& entity);

  void_t handleUpdateSessionProperty(uint32_t requestId, BotProtocol::SessionProperty& sessionProperty);

private: // HandlerConnection::Callback
  virtual void_t handleMessage(const BotProtocol::Header& header, byte_t* data, size_t size);

private: // DataConnection::Callback
  virtual void_t receivedChannelInfo(const String& channelName) {}
  virtual void_t receivedSubscribeResponse(const String& channelName, uint64_t channelId) {}
  virtual void_t receivedUnsubscribeResponse(const String& channelName, uint64_t channelId) {}
  virtual void_t receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade);
  virtual void_t receivedTicker(uint64_t channelId, const DataProtocol::Ticker& ticker) {}
  virtual void_t receivedErrorResponse(const String& message) {}
};
