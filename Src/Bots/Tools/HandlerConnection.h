
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>
#include <nstd/Buffer.h>

#include "Tools/Socket.h"
#include "BotProtocol.h"

class HandlerConnection
{
public:
  class Callback
  {
  public:
    virtual void_t handleMessage(const BotProtocol::Header& header, byte_t* data, size_t size) = 0;
  };

public:
  HandlerConnection() : simulation(true) {}

  bool_t connect(uint16_t port);
  void_t close() {socket.close();}

  const String& getErrorString() const {return error;}
  Socket& getSocket() {return socket;}

  bool_t process(Callback& callback);

  const String& getMarketAdapterName() const {return marketAdapterName;}
  const String& getCurrencyBase() const {return currencyBase;}
  const String& getCurrencyComm() const {return currencyComm;}
  bool isSimulation() const {return simulation;}

  bool_t sendMessage(BotProtocol::MessageType type, uint32_t requestId, const void_t* data, size_t size);
  bool_t sendMessageHeader(BotProtocol::MessageType type, uint32_t requestId, size_t dataSize);
  bool_t sendMessageData(const void_t* data, size_t size);
  bool_t sendErrorResponse(BotProtocol::MessageType messageType, uint32_t requestId, const BotProtocol::Entity* entity, const String& errorMessage);

private:
  Socket socket;
  String error;
  Buffer recvBuffer;

  String marketAdapterName;
  String currencyBase;
  String currencyComm;
  bool simulation;

private:
  bool_t receiveMessage(BotProtocol::Header& header, byte_t*& data, size_t& size);
};
