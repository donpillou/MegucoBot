
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
    byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::RegisterMarketRequest)];
    BotProtocol::Header* header = (BotProtocol::Header*)message;
    BotProtocol::RegisterMarketRequest* registerMarketRequest = (BotProtocol::RegisterMarketRequest*)(header + 1);
    header->size = sizeof(message);
    header->messageType = BotProtocol::registerMarketRequest;
    registerMarketRequest->pid = Process::getCurrentProcessId();
    if(socket.send(message, sizeof(message)) != sizeof(message))
    {
      error = Socket::getLastErrorString();
      return false;
    }
  }

  // receive register market response
  {
    byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::RegisterMarketResponse)];
    if(socket.recv(message, sizeof(message), sizeof(message)) != sizeof(message))
    {
      error = Socket::getLastErrorString();
      return false;
    }
    BotProtocol::Header* header = (BotProtocol::Header*)message;
    BotProtocol::RegisterMarketResponse* registerMarketResponse = (BotProtocol::RegisterMarketResponse*)(header + 1);
    if(header->messageType != BotProtocol::registerMarketResponse || header->size != sizeof(message))
    {
      error = "Could not receive register market response.";
      return false;
    }
    registerMarketResponse->userName[sizeof(registerMarketResponse->userName) - 1] = '\0';
    registerMarketResponse->key[sizeof(registerMarketResponse->key) - 1] = '\0';
    registerMarketResponse->secret[sizeof(registerMarketResponse->secret) - 1] = '\0';
    userName = String(registerMarketResponse->userName, String::length(registerMarketResponse->userName));
    key = String(registerMarketResponse->key, String::length(registerMarketResponse->key));
    secret = String(registerMarketResponse->secret, String::length(registerMarketResponse->secret));
  }

  return true;
}

bool_t BotConnection::sendEntity(const void_t* data, size_t size)
{
  ASSERT(size >= sizeof(BotProtocol::Entity));
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = BotProtocol::updateEntity;
  if(!socket.send((const byte_t*)&header, sizeof(header)) ||
     !socket.send((const byte_t*)data, size))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t BotConnection::removeEntity(uint32_t type, uint32_t id)
{
  byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::Entity)];
  BotProtocol::Header* header = (BotProtocol::Header*)message;
  BotProtocol::Entity* entity = (BotProtocol::Entity*)(header + 1);
  header->size = sizeof(message);
  header->messageType = BotProtocol::removeEntity;
  entity->entityType = type;
  entity->entityId = id;
  if(socket.send(message, sizeof(message)) != sizeof(message))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t BotConnection::receiveMessage(BotProtocol::Header& header, byte_t*& data)
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
  size_t dataSize = header.size - sizeof(header);
  Buffer recvBuffer;
  recvBuffer.resize(dataSize);
  data = recvBuffer;
  if(socket.recv(data, dataSize, dataSize) != (ssize_t)dataSize)
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t BotConnection::sendError(const String& errorMessage)
{
  BotProtocol::Error error;
  error.entityType = BotProtocol::error;
  error.entityId = 0;
  BotProtocol::setString(error.errorMessage, errorMessage);
  return sendEntity(&error, sizeof(error));
}
