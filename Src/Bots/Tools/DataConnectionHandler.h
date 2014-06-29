
#pragma once

#if 0
#include "DataConnection.h"
#include "BotConnection.h"
#include "Broker.h"
#include "TradeHandler.h"

class DataConnectionHandler : private DataConnection::Callback
{
public:
  DataConnectionHandler(BotConnection& botConnection, Broker& broker, Bot::Session& session, bool simulation) :
    botConnection(botConnection), broker(broker), session(session), simulation(simulation), lastReceivedTradeId(0), startTime(0) {}

  bool_t connect(uint32_t ip, uint16_t port) {return dataConnection.connect(ip, port);}
  bool_t subscribe(const String& channel) {return dataConnection.subscribe(channel, lastReceivedTradeId);}
  bool_t process() {return dataConnection.process(*this);}

private:
  BotConnection& botConnection;
  Broker& broker;
  DataConnection dataConnection;
  Bot::Session& session;
  bool simulation;
  TradeHandler tradeHandler;
  uint64_t lastReceivedTradeId;
  timestamp_t startTime;

private: // DataConnection::Callback
  virtual void receivedChannelInfo(const String& channelName) {};
  virtual void receivedSubscribeResponse(const String& channelName, uint64_t channelId) {};
  virtual void receivedUnsubscribeResponse(const String& channelName, uint64_t channelId) {};
  virtual void receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade);
  virtual void receivedTicker(uint64_t channelId, const DataProtocol::Ticker& ticker) {};
  virtual void receivedErrorResponse(const String& message) {};
};
#endif