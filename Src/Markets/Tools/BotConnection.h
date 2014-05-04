
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

  bool_t sendMessage(BotProtocol::MessageType type, const void_t* data, size_t size);
  bool_t receiveMessage(BotProtocol::Header& header, byte_t*& data, size_t& size);
  bool_t sendError(const String& errorMessage);
  bool_t sendEntity(const void_t* data, size_t size);
  bool_t removeEntity(uint32_t type, uint32_t id);

private:
  Socket socket;
  String error;
  Buffer recvBuffer;

  String userName;
  String key;
  String secret;
};
