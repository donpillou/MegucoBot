
#include <nstd/Process.h>
#include <nstd/Debug.h>

#include "HandlerConnection.h"

bool_t HandlerConnection::connect(uint16_t port)
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

  // send register bot handler request
  {
    BotProtocol::RegisterBotHandlerRequest registerBotHandlerRequest;
    registerBotHandlerRequest.pid = Process::getCurrentProcessId();
    if(!sendMessage(BotProtocol::registerBotHandlerRequest, 0, &registerBotHandlerRequest, sizeof(registerBotHandlerRequest)))
      return false;
  }

  // receive register bot handler response
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
    if(!(header.messageType == BotProtocol::registerBotHandlerResponse && header.requestId == 0 && size >= sizeof(BotProtocol::RegisterBotHandlerResponse)))
    {
      error = "Could not receive register bot response.";
      return false;
    }
    BotProtocol::RegisterBotHandlerResponse* registerBotHandlerResponse = (BotProtocol::RegisterBotHandlerResponse*)data;
    marketAdapterName = BotProtocol::getString(registerBotHandlerResponse->marketAdapterName);
    simulation = registerBotHandlerResponse->simulation != 0;
  }

  return true;
}

bool_t HandlerConnection::process(Callback& callback)
{
  BotProtocol::Header header;
  byte_t* data;
  size_t size;
  if(!receiveMessage(header, data, size))
    return false;
  switch((BotProtocol::MessageType)header.messageType)
  {
  case BotProtocol::controlEntity:
    if(size >= sizeof(BotProtocol::Entity)) 
      callback.receivedControlEntity(*(BotProtocol::Entity*)data, size);
    break;
  default:
    break;
  }
  return true;
}

bool_t HandlerConnection::sendMessage(BotProtocol::MessageType type, uint32_t requestId, const void_t* data, size_t size)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = type;
  header.requestId = requestId;
  if(socket.send((const byte_t*)&header, sizeof(header)) != sizeof(header) ||
     (size > 0 && socket.send((const byte_t*)data, size) != (ssize_t)size))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t HandlerConnection::receiveMessage(BotProtocol::Header& header, byte_t*& data, size_t& size)
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

bool_t HandlerConnection::sendMessageHeader(BotProtocol::MessageType type, uint32_t requestId, size_t dataSize)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + dataSize;
  header.messageType = type;
  header.requestId = requestId;
  if(socket.send((const byte_t*)&header, sizeof(header)) != sizeof(header))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t HandlerConnection::sendMessageData(const void_t* data, size_t size)
{
  if(socket.send((const byte_t*)data, size) != (ssize_t)size)
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t HandlerConnection::sendErrorResponse(BotProtocol::MessageType messageType, uint32_t requestId, const BotProtocol::Entity* entity, const String& errorMessage)
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
