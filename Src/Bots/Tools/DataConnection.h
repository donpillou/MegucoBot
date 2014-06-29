
#pragma once

#include <nstd/Buffer.h>

#include "Tools/Socket.h"
#include "DataProtocol.h"

class DataConnection
{
public:
  class Callback
  {
  public:
    virtual void receivedChannelInfo(const String& channelName) = 0;
    virtual void receivedSubscribeResponse(const String& channelName, uint64_t channelId) = 0;
    virtual void receivedUnsubscribeResponse(const String& channelName, uint64_t channelId) = 0;
    virtual void receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade) = 0;
    virtual void receivedTicker(uint64_t channelId, const DataProtocol::Ticker& ticker) = 0;
    virtual void receivedErrorResponse(const String& message) = 0;
  };

public:
  bool connect(uint32_t ip, uint16_t port);
  const String& getLastError() {return error;}

  bool process(Callback& callback);

  bool loadChannelList();

  bool subscribe(const String& channel, uint64_t lastReceivedTradeId);
  bool unsubscribe(const String& channel);

  //bool readTrade(uint64_t& channelId, DataProtocol::Trade& trade);

private:
  Socket socket;
  Buffer recvBuffer;
  String error;
  Callback* callback;
  timestamp_t serverTimeToLocalTime;

private:
  void handleMessage(DataProtocol::MessageType messageType, char* data, unsigned int dataSize);
};
