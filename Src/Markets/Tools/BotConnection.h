
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>
#include <nstd/Buffer.h>

#include "Tools/Socket.h"
#include "BotProtocol.h"

class BotConnection
{
public:
  BotConnection() {}

  bool_t connect(uint16_t port);
  void_t close() {socket.close();}
  bool_t isOpen() const {return socket.isOpen();}
  const String& getErrorString() const {return error;}

  const String& getUserName() const {return userName;}
  const String& getKey() const {return key;}
  const String& getSecret() const {return secret;}

  bool_t sendMessage(BotProtocol::MessageType type, uint32_t requestId, const void_t* data, size_t size);
  bool_t receiveMessage(BotProtocol::Header& header, byte_t*& data, size_t& size);
  bool_t sendErrorResponse(BotProtocol::MessageType messageType, uint32_t requestId, const BotProtocol::Entity* entity, const String& errorMessage);
  bool_t sendEntity(const void_t* data, size_t size);
  bool_t removeEntity(uint32_t type, uint32_t id);
  template<class E> bool_t sendControlMarketResponse(uint32_t requestId, const BotProtocol::ControlMarketResponse& response, const List<E>& data)
  {
    BotProtocol::Header header;
    header.size = sizeof(header) + sizeof(response) + data.size() * sizeof(E);
    header.messageType = BotProtocol::controlEntityResponse;
    header.requestId = requestId;
    if(socket.send((const byte_t*)&header, sizeof(header)) != sizeof(header) ||
       socket.send((const byte_t*)&response, sizeof(response)) != sizeof(response))
    {
      error = Socket::getLastErrorString();
      return false;
    }
    for(List<E>::Iterator i = data.begin(), end = data.end(); i != end; ++i)
    {
      const E& e = *i;
      if(socket.send((const byte_t*)&e, sizeof(E)) != sizeof(E))
      {
        error = Socket::getLastErrorString();
        return false;
      }
    }
    return true;
  }

private:
  Socket socket;
  String error;
  Buffer recvBuffer;

  String userName;
  String key;
  String secret;
};
