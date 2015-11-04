
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
    virtual void_t receivedChannelInfo(const String& channelName) = 0;
    virtual void_t receivedSubscribeResponse(const String& channelName, uint64_t channelId) = 0;
    virtual void_t receivedUnsubscribeResponse(const String& channelName, uint64_t channelId) = 0;
    virtual void_t receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade) = 0;
    virtual void_t receivedTicker(uint64_t channelId, const DataProtocol::Ticker& ticker) = 0;
    virtual void_t receivedErrorResponse(const String& message) = 0;
  };

public:
  bool_t connect(uint32_t ip, uint16_t port);
  void_t close();

  const String& getLastError() {return error;}
  Socket& getSocket() {return socket;}

  bool_t process(Callback& callback);

  bool_t loadChannelList();

  bool_t subscribe(const String& channel, uint64_t lastReceivedTradeId, int64_t maxAge);
  bool_t unsubscribe(const String& channel);

  //bool readTrade(uint64_t& channelId, DataProtocol::Trade& trade);

private:
  Socket socket;
  Buffer recvBuffer;
  String error;
  Callback* callback;
  int64_t serverTimeToLocalTime;

private:
  void_t handleMessage(DataProtocol::MessageType messageType, char_t* data, uint_t dataSize);
};
