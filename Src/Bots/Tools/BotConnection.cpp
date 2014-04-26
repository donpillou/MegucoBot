
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

  // send register bot request
  {
    byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::RegisterBotRequest)];
    BotProtocol::Header* header = (BotProtocol::Header*)message;
    BotProtocol::RegisterBotRequest* registerBotRequest = (BotProtocol::RegisterBotRequest*)(header + 1);
    header->size = sizeof(message);
    header->messageType = BotProtocol::registerBotRequest;
    header->entityId = header->entityType = 0;
    registerBotRequest->pid = Process::getCurrentProcessId();
    if(!socket.send(message, sizeof(message)))
    {
      error = Socket::getLastErrorString();
      return false;
    }
  }

  // receive register bot response
  {
    BotProtocol::Header header;
    BotProtocol::RegisterBotResponse response;
    if(!socket.recv((byte_t*)&header, sizeof(header)))
    {
      error = Socket::getLastErrorString();
      return false;
    }
    if(header.messageType != BotProtocol::registerBotResponse || header.size != sizeof(header) + sizeof(response))
    {
      error = "Could not receive register bot response.";
      return false;
    }
    if(!socket.recv((byte_t*)&response, sizeof(response)))
    {
      error = Socket::getLastErrorString();
      return false;
    }
  }

  return true;
}

bool_t BotConnection::getTransactions(List<BotProtocol::Transaction>& transactions)
{
  if(!requestEntities(BotProtocol::transaction))
    return false;
  if(!sendPing())
    return false;

  BotProtocol::Header header;
  byte_t* data;
  for(;;)
  {
    if(!receiveMessage(header, data))
      return false;
    switch((BotProtocol::MessageType)header.messageType)
    {
    case BotProtocol::updateEntity:
      if(header.size >= sizeof(BotProtocol::Transaction))
        transactions.append(*(BotProtocol::Transaction*)data);
      break;
    case BotProtocol::pingResponse:
      return true;
    default:
      break;
    }
  }
}

bool_t BotConnection::createTransaction(const BotProtocol::CreateTransactionArgs& transaction, uint32_t& id)
{
  return createEntity<BotProtocol::Transaction>(BotProtocol::transaction, &transaction, sizeof(BotProtocol::CreateTransactionArgs), id);
}

bool_t BotConnection::removeTransaction(uint32_t id)
{
  return removeEntity(BotProtocol::transaction, id);
}

bool_t BotConnection::createOrder(const BotProtocol::CreateOrderArgs& order, uint32_t& id)
{
  return createEntity<BotProtocol::Order>(BotProtocol::order, &order, sizeof(BotProtocol::CreateOrderArgs), id);
}

bool_t BotConnection::removeOrder(uint32_t id)
{
  return removeEntity(BotProtocol::order, id);
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
  if(!socket.send((const byte_t*)&header, sizeof(header)))
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
  if(!socket.send((const byte_t*)&header, sizeof(header)))
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
  if(!socket.send((const byte_t*)&header, sizeof(header)))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

bool_t BotConnection::receiveMessage(BotProtocol::Header& header, byte_t*& data)
{
  if(!socket.recv((byte_t*)&header, sizeof(header)))
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
  if(!socket.recv(data, header.size))
  {
    error = Socket::getLastErrorString();
    return false;
  }
  return true;
}

