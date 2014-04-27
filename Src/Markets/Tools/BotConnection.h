
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

  bool_t receiveMessage(BotProtocol::Header& header, byte_t*& data);

private:
  Socket socket;
  String error;
  Buffer recvBuffer;

private:
  template <class E> bool_t createEntity(BotProtocol::EntityType type, const void_t* data, size_t size, uint32_t& id);
  bool_t removeEntity(uint32_t type, uint32_t id);
  bool_t sendPing();
  bool_t requestEntities(BotProtocol::EntityType entityType);
};
