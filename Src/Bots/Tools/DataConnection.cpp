
#include <nstd/Time.h>
#include <nstd/Math.h>

#include "DataConnection.h"

bool DataConnection::connect(uint32_t ip, uint16_t port)
{
  socket.close();
  recvBuffer.clear();

  if(!socket.open() ||
     !socket.connect(ip, port) ||
     !socket.setNoDelay())
  {
    error = socket.getLastErrorString();
    return false;
  }

  // request server time
  DataProtocol::Header header;
  header.size = sizeof(header);
  header.destination = header.source = 0;
  header.messageType = DataProtocol::timeRequest;
  if(socket.send((byte_t*)&header, sizeof(header)) != sizeof(header))
  {
    error = socket.getLastErrorString();
    return false;
  }
  int64_t localRequestTime = Time::time();
  byte_t recvBuffer[sizeof(DataProtocol::Header) + sizeof(DataProtocol::TimeResponse)];
  if(socket.recv(recvBuffer, sizeof(recvBuffer), sizeof(recvBuffer)) != sizeof(recvBuffer))
  {
    error = socket.getLastErrorString();
    return false;
  }
  timestamp_t localResponseTime = Time::time();
  {
    DataProtocol::Header* header = (DataProtocol::Header*)recvBuffer;
    DataProtocol::TimeResponse* timeResponse2 = (DataProtocol::TimeResponse*)(header + 1);
    if(header->size != sizeof(DataProtocol::Header) + sizeof(DataProtocol::TimeResponse))
    {
      error = "Received invalid data.";
      return false;
    }
    if(header->messageType != DataProtocol::timeResponse)
    {
      error = "Could not request server time.";
      return false;
    }
    serverTimeToLocalTime = (localResponseTime - localRequestTime) / 2 + localRequestTime - timeResponse2->time;
  }

  return true;
}

bool DataConnection::process(Callback& callback)
{
  this->callback = &callback;

  size_t bufferSize = recvBuffer.size();
  recvBuffer.resize(bufferSize + 1500);
  ssize_t bytesRead = socket.recv(recvBuffer + bufferSize, recvBuffer.size() - bufferSize);
  if(bytesRead <= 0)
  {
    error = socket.getLastErrorString();
    return false;
  }

  bufferSize += bytesRead;
  recvBuffer.resize(bufferSize);
  byte_t* buffer = recvBuffer;

  for(;;)
  {
    if(bufferSize >= sizeof(DataProtocol::Header))
    {
      DataProtocol::Header* header = (DataProtocol::Header*)buffer;
      if(header->size < sizeof(DataProtocol::Header))
      {
        error = "Received invalid data.";
        return false;
      }
      if(bufferSize >= header->size)
      {
        handleMessage((DataProtocol::MessageType)header->messageType, (char*)(header + 1), header->size - sizeof(DataProtocol::Header));
        buffer += header->size;
        bufferSize -= header->size;
        continue;
      }
    }
    break;
  }
  if(buffer > (byte_t*)recvBuffer)
    recvBuffer.removeFront(buffer - (byte_t*)recvBuffer);
  if(recvBuffer.size() > 4000)
  {
    error = "Received invalid data.";
    return false;
  }
  return true;
}

void DataConnection::handleMessage(DataProtocol::MessageType messageType, char* data, unsigned int size)
{
  switch(messageType)
  {
  case DataProtocol::channelResponse:
    {
      int count = size / sizeof(DataProtocol::Channel);
      DataProtocol::Channel* channel = (DataProtocol::Channel*)data;
      for(int i = 0; i < count; ++i, ++channel)
      {
        channel->channel[sizeof(channel->channel) - 1] = '\0';
        String channelName;
        channelName.attach(channel->channel, String::length(channel->channel));
        callback->receivedChannelInfo(channelName);
      }
    }
    break;
  case DataProtocol::subscribeResponse:
    if(size >= sizeof(DataProtocol::SubscribeResponse))
    {
      DataProtocol::SubscribeResponse* subscribeResponse = (DataProtocol::SubscribeResponse*)data;
      subscribeResponse->channel[sizeof(subscribeResponse->channel) - 1] = '\0';
      String channelName;
      channelName.attach(subscribeResponse->channel, String::length(subscribeResponse->channel));
      callback->receivedSubscribeResponse(channelName, subscribeResponse->channelId);
    }
    break;
  case DataProtocol::unsubscribeResponse:
    if(size >= sizeof(DataProtocol::SubscribeResponse))
    {
      DataProtocol::SubscribeResponse* unsubscribeResponse = (DataProtocol::SubscribeResponse*)data;
      unsubscribeResponse->channel[sizeof(unsubscribeResponse->channel) - 1] = '\0';
      String channelName;
      channelName.attach(unsubscribeResponse->channel, String::length(unsubscribeResponse->channel));
      callback->receivedUnsubscribeResponse(channelName, unsubscribeResponse->channelId);
    }
    break;
  case DataProtocol::tradeMessage:
    if(size >= sizeof(DataProtocol::TradeMessage))
    {
      DataProtocol::TradeMessage* tradeMessage = (DataProtocol::TradeMessage*)data;
      tradeMessage->trade.time += serverTimeToLocalTime;
      callback->receivedTrade(tradeMessage->channelId, tradeMessage->trade);
    }
    break;
  case DataProtocol::tickerMessage:
    {
      DataProtocol::TickerMessage* tickerMessage = (DataProtocol::TickerMessage*)data;
      tickerMessage->ticker.time += serverTimeToLocalTime;
      callback->receivedTicker(tickerMessage->channelId, tickerMessage->ticker);
    }
    break;
  case DataProtocol::errorResponse:
    if(size >= sizeof(DataProtocol::ErrorResponse))
    {
      DataProtocol::ErrorResponse* errorResponse = (DataProtocol::ErrorResponse*)data;
      errorResponse->errorMessage[sizeof(errorResponse->errorMessage) - 1] = '\0';
      String errorMessage;
      errorMessage.attach(errorResponse->errorMessage, String::length(errorResponse->errorMessage));
      callback->receivedErrorResponse(errorMessage);
    }
    break;
  default:
    break;
  }
}

bool DataConnection::loadChannelList()
{
  DataProtocol::Header header;
  header.size = sizeof(header);
  header.destination = header.source = 0;
  header.messageType = DataProtocol::channelRequest;
  if(socket.send((byte_t*)&header, sizeof(header)) != sizeof(header))
  {
    error = socket.getLastErrorString();
    return false;
  }
  return true;
}

bool DataConnection::subscribe(const String& channel, uint64_t lastReceivedTradeId)
{
  byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::SubscribeRequest)];
  DataProtocol::Header* header = (DataProtocol::Header*)message;
  DataProtocol::SubscribeRequest* subscribeRequest = (DataProtocol::SubscribeRequest*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = DataProtocol::subscribeRequest;
  Memory::copy(subscribeRequest->channel, (const char_t*)channel, Math::min(channel.length() + 1, sizeof(subscribeRequest->channel) - 1));
  subscribeRequest->channel[sizeof(subscribeRequest->channel) - 1] = '\0';
  if(lastReceivedTradeId)
  {
    subscribeRequest->maxAge = 0;
    subscribeRequest->sinceId =  lastReceivedTradeId;
  }
  else
  {
    subscribeRequest->maxAge = 24ULL * 60ULL * 60ULL * 1000ULL * 7ULL;
    //subscribeRequest->maxAge = 60ULL * 60ULL * 1000ULL;
    subscribeRequest->sinceId =  0;
  }
  if(socket.send(message, sizeof(message)) != sizeof(message))
  {
    error = socket.getLastErrorString();
    return false;
  }
  return true;
}

bool DataConnection::unsubscribe(const String& channel)
{
  byte_t message[sizeof(DataProtocol::Header) + sizeof(DataProtocol::UnsubscribeRequest)];
  DataProtocol::Header* header = (DataProtocol::Header*)message;
  DataProtocol::UnsubscribeRequest* unsubscribeRequest = (DataProtocol::UnsubscribeRequest*)(header + 1);
  header->size = sizeof(message);
  header->destination = header->source = 0;
  header->messageType = DataProtocol::unsubscribeRequest;
  Memory::copy(unsubscribeRequest->channel, (const char_t*)channel, Math::min(channel.length() + 1, sizeof(unsubscribeRequest->channel) - 1));
  unsubscribeRequest->channel[sizeof(unsubscribeRequest->channel) - 1] = '\0';
  if(socket.send(message, sizeof(message)) != sizeof(message))
  {
    error = socket.getLastErrorString();
    return false;
  }
  return true;
}

bool DataConnection::readTrade(uint64_t& channelId, DataProtocol::Trade& trade)
{
  struct ReadTradeCallback : public Callback
  {
    uint64_t& channelId;
    DataProtocol::Trade& trade;
    bool finished;

    ReadTradeCallback(uint64_t& channelId, DataProtocol::Trade& trade) : channelId(channelId), trade(trade), finished(false) {}

    virtual void receivedChannelInfo(const String& channelName) {}
    virtual void receivedSubscribeResponse(const String& channelName, uint64_t channelId) {}
    virtual void receivedUnsubscribeResponse(const String& channelName, uint64_t channelId) {}
    virtual void receivedTicker(uint64_t channelId, const DataProtocol::Ticker& ticker) {}
    virtual void receivedErrorResponse(const String& message) {}

    virtual void receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade)
    {
      this->channelId = channelId;
      this->trade = trade;
      finished = true;
    }
  } callback(channelId, trade);
  do
  {
    if(!process(callback))
      return false;
  } while(!callback.finished);
  return true;
}
