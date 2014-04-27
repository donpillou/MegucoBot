
#include <nstd/Process.h>

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
    header->entityId = header->entityType = 0;
    registerMarketRequest->pid = Process::getCurrentProcessId();
    if(socket.send2(message, sizeof(message)) != sizeof(message))
    {
      error = Socket::getLastErrorString();
      return false;
    }
  }

  // receive register market response
  {
    BotProtocol::Header header;
    if(socket.recv2((byte_t*)&header, sizeof(header), sizeof(header)) != sizeof(header))
    {
      error = Socket::getLastErrorString();
      return false;
    }
    if(header.messageType != BotProtocol::registerMarketResponse || header.size != sizeof(header))
    {
      error = "Could not receive register market response.";
      return false;
    }
  }

  return true;
}

template <class E> bool_t BotConnection::createEntity(BotProtocol::EntityType type, const void_t* data, size_t size, uint32_t& id)
{
  // send create request
  {
    BotProtocol::Header header;
    header.size = sizeof(header) + size;
    header.messageType = BotProtocol::createEntity;
    header.entityType = type;
    header.entityId = 0;
    if(!socket.send((const byte_t*)&header, sizeof(header)) ||
       (size > 0 && !socket.send((const byte_t*)data, size)))
    {
      error = Socket::getLastErrorString();
      return false;
    }
  }
  
  // receive response
  {
    byte_t message[sizeof(BotProtocol::Header) + sizeof(E)];
    if(!socket.recv(message, sizeof(message)))
    {
      error = Socket::getLastErrorString();
      return false;
    }
    BotProtocol::Header* header = (BotProtocol::Header*)message;
    if(header->messageType != BotProtocol::updateEntity)
    {
      error = "Received invalid response.";
      return false;
    }
    if(header->entityType != type)
    {
      error = "Received invalid response.";
      return false;
    }
    id = header->entityId;
  }
  return true;
}

bool_t BotConnection::removeEntity(uint32_t type, uint32_t id)
{
  BotProtocol::Header header;
  header.size = sizeof(header);
  header.messageType = BotProtocol::removeEntity;
  header.entityType = type;
  header.entityId = id;
  if(socket.send2((const byte_t*)&header, sizeof(header)) != sizeof(header))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t BotConnection::sendPing()
{
  BotProtocol::Header header;
  header.size = 0;
  header.messageType = BotProtocol::pingRequest;
  header.entityType = 0;
  header.entityId = 0;
  if(socket.send2((const byte_t*)&header, sizeof(header)) != sizeof(header))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t BotConnection::requestEntities(BotProtocol::EntityType entityType)
{
  BotProtocol::Header header;
  header.size = 0;
  header.messageType = BotProtocol::requestEntities;
  header.entityType = entityType;
  header.entityId = 0;
  if(socket.send2((const byte_t*)&header, sizeof(header)) != sizeof(header))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t BotConnection::receiveMessage(BotProtocol::Header& header, byte_t*& data)
{
  if(socket.recv2((byte_t*)&header, sizeof(header), sizeof(header)) != sizeof(header))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  if(header.size < sizeof(BotProtocol::Header))
  {
    error = "Received invalid data.";
    return false;
  }
  Buffer recvBuffer;
  recvBuffer.resize(header.size);
  data = recvBuffer;
  if(socket.recv2(data, header.size, header.size) != header.size)
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

