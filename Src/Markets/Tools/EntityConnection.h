
#pragma once

#include <nstd/String.h>
#include <nstd/List.h>
#include <nstd/Buffer.h>

#include "Tools/Socket.h"
#include "BotProtocol.h"

class EntityConnection
{
public:
  EntityConnection() {}

  bool_t connect(uint16_t port);
  void_t close() {socket.close();}
  bool_t isOpen() const {return socket.isOpen();}
  const String& getErrorString() const {return error;}

  bool_t sendEntity(const void_t* data, size_t size);
  bool_t removeEntity(uint32_t type, uint32_t id);

private:
  Socket socket;
  String error;
  Buffer recvBuffer;

private:
  bool_t receiveMessage(BotProtocol::Header& header, byte_t*& data, size_t& size);
  bool_t sendMessage(BotProtocol::MessageType type, uint32_t requestId, const void_t* data, size_t size);
};
