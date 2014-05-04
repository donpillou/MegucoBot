
#include <nstd/Process.h>
#include <nstd/Debug.h>

#include "BotConnection.h"

bool_t BotConnection::connect(uint16_t port)
{
  close();

  // connect to server
  if(!socket.open() ||
    !socket.connect(Socket::loopbackAddr, port) ||
    !socket.setNoDelay())
  {
    error = Socket::getLastErrorString();
    return false;
  }

  // send register market request
  {
    BotProtocol::RegisterMarketRequest registerMarketRequest;
    registerMarketRequest.pid = Process::getCurrentProcessId();
    if(!sendMessage(BotProtocol::registerMarketRequest, &registerMarketRequest, sizeof(registerMarketRequest)))
      return false;
  }

  // receive register market response
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    if(!receiveMessage(header, data, size))
      return false;
    if(header.messageType != BotProtocol::registerMarketResponse || size < sizeof(BotProtocol::RegisterMarketResponse))
    {
      error = "Could not receive register market response.";
      return false;
    }
    BotProtocol::RegisterMarketResponse* registerMarketResponse = (BotProtocol::RegisterMarketResponse*)data;
    userName = BotProtocol::getString(registerMarketResponse->userName);
    key = BotProtocol::getString(registerMarketResponse->key);
    secret = BotProtocol::getString(registerMarketResponse->secret);
  }

  return true;
}

bool_t BotConnection::sendEntity(const void_t* data, size_t size)
{
  ASSERT(size >= sizeof(BotProtocol::Entity));
  return sendMessage(BotProtocol::updateEntity, data, size);
}

bool_t BotConnection::removeEntity(uint32_t type, uint32_t id)
{
  BotProtocol::Entity entity;
  entity.entityType = type;
  entity.entityId = id;
  return sendMessage(BotProtocol::removeEntity, &entity, sizeof(entity));
}

bool_t BotConnection::sendMessage(BotProtocol::MessageType type, const void_t* data, size_t size)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = type;
  if(socket.send((const byte_t*)&header, sizeof(header)) != sizeof(header) ||
     (size > 0 && socket.send((const byte_t*)data, size) != size))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t BotConnection::receiveMessage(BotProtocol::Header& header, byte_t*& data, size_t& size)
{
  if(socket.recv((byte_t*)&header, sizeof(header), sizeof(header)) != sizeof(header))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  if(header.size < sizeof(BotProtocol::Header))
  {
    error = "Received invalid data.";
    return false;
  }
  size = header.size - sizeof(header);
  if(size > 0)
  {
    recvBuffer.resize(size);
    data = recvBuffer;
    if(socket.recv(data, size, size) != (ssize_t)size)
    {
      error = Socket::getLastErrorString();
      return false;
    }
  }
  return true;
}

bool_t BotConnection::sendError(const String& errorMessage)
{
  BotProtocol::Error error;
  error.entityType = BotProtocol::error;
  error.entityId = 0;
  BotProtocol::setString(error.errorMessage, errorMessage);
  return sendMessage(BotProtocol::updateEntity, &error, sizeof(error));
}
