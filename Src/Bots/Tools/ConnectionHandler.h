
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

private: // HandlerConnection::Callback
  virtual void_t receivedControlEntity(BotProtocol::Entity& entity, size_t size);

private: // DataConnection::Callback
  virtual void receivedChannelInfo(const String& channelName) {}
  virtual void receivedSubscribeResponse(const String& channelName, uint64_t channelId) {}
  virtual void receivedUnsubscribeResponse(const String& channelName, uint64_t channelId) {}
  virtual void receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade);
  virtual void receivedTicker(uint64_t channelId, const DataProtocol::Ticker& ticker) {}
  virtual void receivedErrorResponse(const String& message) {}
};
