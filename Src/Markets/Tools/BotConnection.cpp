
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
    if(!sendMessage(BotProtocol::registerMarketRequest, 0, &registerMarketRequest, sizeof(registerMarketRequest)))
      return false;
  }

  // receive register market response
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    if(!receiveMessage(header, data, size))
      return false;
    if(header.messageType == BotProtocol::errorResponse && size >= sizeof(BotProtocol::ErrorResponse))
    {
      BotProtocol::ErrorResponse* errorResponse = (BotProtocol::ErrorResponse*)data;
      error = BotProtocol::getString(errorResponse->errorMessage);
      return false;
    }
    if(!(header.messageType == BotProtocol::registerMarketResponse && header.requestId == 0 && size >= sizeof(BotProtocol::RegisterBotResponse)))
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
  BotProtocol::EntityType entityType = (BotProtocol::EntityType)((BotProtocol::Entity*)data)->entityType;
  uint32_t entityId = ((BotProtocol::Entity*)data)->entityId;
  if(!sendMessage(BotProtocol::updateEntity, 0, data, size))
    return false;

  // receive update entity response
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    if(!receiveMessage(header, data, size))
      return false;
    if(header.messageType == BotProtocol::errorResponse && size >= sizeof(BotProtocol::ErrorResponse))
    {
      BotProtocol::ErrorResponse* errorResponse = (BotProtocol::ErrorResponse*)data;
      error = BotProtocol::getString(errorResponse->errorMessage);
      return false;
    }
    BotProtocol::Entity* entity = (BotProtocol::Entity*)data;
    if(!(header.messageType == BotProtocol::updateEntityResponse && header.requestId == 0 && size >= sizeof(BotProtocol::Entity) &&
         entity->entityType == entityType && entityId == entityId))
    {
      error = "Could not receive update entity response.";
      return false;
    }
  }

  return true;
}

bool_t BotConnection::removeEntity(uint32_t type, uint32_t id)
{
  // send remove entity message
  {
    BotProtocol::Entity entity;
    entity.entityType = type;
    entity.entityId = id;
    if(!sendMessage(BotProtocol::removeEntity, 0, &entity, sizeof(entity)))
      return false;
  }

  // receive remove entity response
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    if(!receiveMessage(header, data, size))
      return false;
    if(header.messageType == BotProtocol::errorResponse && size >= sizeof(BotProtocol::ErrorResponse))
    {
      BotProtocol::ErrorResponse* errorResponse = (BotProtocol::ErrorResponse*)data;
      error = BotProtocol::getString(errorResponse->errorMessage);
      return false;
    }
    BotProtocol::Entity* entity = (BotProtocol::Entity*)data;
    if(!(header.messageType == BotProtocol::removeEntityResponse && header.requestId == 0 && size >= sizeof(BotProtocol::removeEntity) &&
         entity->entityType == type && entity->entityId == id))
    {
      error = "Could not receive remove entity response.";
      return false;
    }
  }

  return true;
}

bool_t BotConnection::sendMessage(BotProtocol::MessageType type, uint32_t requestId, const void_t* data, size_t size)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = type;
  header.requestId = requestId;
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

bool_t BotConnection::sendErrorResponse(BotProtocol::MessageType messageType, uint32_t requestId, const BotProtocol::Entity* entity, const String& errorMessage)
{
  BotProtocol::ErrorResponse errorResponse;
  errorResponse.messageType = messageType;
  if(entity)
  {
    errorResponse.entityType = entity->entityType;
    errorResponse.entityId = entity->entityId;
  }
  else
  {
    errorResponse.entityType = BotProtocol::none;
    errorResponse.entityId = 0;
  }
  BotProtocol::setString(errorResponse.errorMessage, errorMessage);
  return sendMessage(BotProtocol::errorResponse, requestId, &errorResponse, sizeof(errorResponse));
}
