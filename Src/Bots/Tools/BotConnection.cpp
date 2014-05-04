
#include <nstd/Process.h>
#include <nstd/Debug.h>
#include <nstd/Time.h>

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
    BotProtocol::RegisterBotRequest registerBotRequest;
    registerBotRequest.pid = Process::getCurrentProcessId();
    if(!sendMessage(BotProtocol::registerBotRequest, &registerBotRequest, sizeof(registerBotRequest)))
      return false;
  }

  // receive register bot response
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    if(!receiveMessage(header, data, size))
      return false;
    if(header.messageType != BotProtocol::registerBotResponse || size < sizeof(BotProtocol::RegisterBotResponse))
    {
      error = "Could not receive register bot response.";
      return false;
    }
    BotProtocol::RegisterBotResponse* response = (BotProtocol::RegisterBotResponse*)data;
    sessionId = response->sessionId;
    marketAdapterName = BotProtocol::getString(response->marketAdapterName);
  }

  return true;
}

bool_t BotConnection::addLogMessage(const String& message)
{
  BotProtocol::SessionLogMessage logMessage;
  logMessage.entityType = BotProtocol::sessionLogMessage;
  logMessage.entityId = 0;
  logMessage.date = Time::time();
  BotProtocol::setString(logMessage.message, message);
  uint32_t id;
  return createEntity<BotProtocol::SessionLogMessage>(&logMessage, id);
}

bool_t BotConnection::getTransactions(List<BotProtocol::Transaction>& transactions)
{
  if(!sendControlSession(BotProtocol::ControlSession::requestTransactions))
    return false;
  if(!sendPing())
    return false;

  BotProtocol::Header header;
  byte_t* data;
  size_t size;
  for(;;)
  {
    if(!receiveMessage(header, data, size))
      return false;
    switch((BotProtocol::MessageType)header.messageType)
    {
    case BotProtocol::updateEntity:
      if(size >= sizeof(BotProtocol::Entity))
      {
        BotProtocol::Entity* entity = (BotProtocol::Entity*)data;
        if(entity->entityType == BotProtocol::sessionTransaction && size >= sizeof(BotProtocol::Transaction))
          transactions.append(*(BotProtocol::Transaction*)data);
      }
      break;
    case BotProtocol::pingResponse:
      return true;
    default:
      break;
    }
  }
}

bool_t BotConnection::getOrders(List<BotProtocol::Order>& orders)
{
  if(!sendControlSession(BotProtocol::ControlSession::requestOrders))
    return false;
  if(!sendPing())
    return false;

  BotProtocol::Header header;
  byte_t* data;
  size_t size;
  for(;;)
  {
    if(!receiveMessage(header, data, size))
      return false;
    switch((BotProtocol::MessageType)header.messageType)
    {
    case BotProtocol::updateEntity:
      if(size >= sizeof(BotProtocol::Entity))
      {
        BotProtocol::Entity* entity = (BotProtocol::Entity*)data;
        if(entity->entityType == BotProtocol::sessionOrder && size >= sizeof(BotProtocol::Order))
          orders.append(*(BotProtocol::Order*)data);
      }
      break;
    case BotProtocol::pingResponse:
      return true;
    default:
      break;
    }
  }
}

bool_t BotConnection::createTransaction(const BotProtocol::Transaction& transaction, uint32_t& id)
{
  return createEntity<BotProtocol::Transaction>(&transaction, id);
}

bool_t BotConnection::removeTransaction(uint32_t id)
{
  return removeEntity(BotProtocol::sessionTransaction, id);
}

bool_t BotConnection::createOrder(const BotProtocol::Order& order, uint32_t& id)
{
  return createEntity<BotProtocol::Order>(&order, id);
}

bool_t BotConnection::removeOrder(uint32_t id)
{
  return removeEntity(BotProtocol::sessionOrder, id);
}

template <class E> bool_t BotConnection::createEntity(const void_t* data, uint32_t& id)
{
  BotProtocol::EntityType entityType = (BotProtocol::EntityType)((BotProtocol::Entity*)data)->entityType;

  // send create request
  {
    if(!sendMessage(BotProtocol::createEntity, data, sizeof(E)))
      return false;
  }

  // receive create response
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    if(!receiveMessage(header, data, size))
      return false;
    BotProtocol::CreateEntityResponse* response = (BotProtocol::CreateEntityResponse*)data;
    if(header.messageType != BotProtocol::createEntityResponse || size < sizeof(BotProtocol::CreateEntityResponse) ||
      response->entityType != entityType)
    {
      error = "Received invalid create entity response.";
      return false;
    }
    if(!response->success)
    {
      error = "Could not create entity.";
      return false;
    }
    id = response->id;
  }
  
  // receive entity
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    if(!receiveMessage(header, data, size))
      return false;
    BotProtocol::Entity* entity = (BotProtocol::Entity*)data;
    if(header.messageType != BotProtocol::updateEntity || size < sizeof(E) ||
       entity->entityType != entityType ||
       entity->entityId != id)
    {
      error = "Could not receive created entity.";
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
    if(!sendMessage(BotProtocol::removeEntity, &entity, sizeof(entity)))
      return false;
  }

  // receive entity remove message
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    if(!receiveMessage(header, data, size))
      return false;
    BotProtocol::Entity* entity = (BotProtocol::Entity*)data;
    if(header.messageType != BotProtocol::removeEntity || size < sizeof(BotProtocol::Entity) ||
       entity->entityType != type)
    {
      error = "Received invalid remove entity response.";
      return false;
    }
  }

  return true;
}

bool_t BotConnection::sendPing()
{
  return sendMessage(BotProtocol::pingRequest, 0, 0);
}

bool_t BotConnection::sendControlSession(BotProtocol::ControlSession::Command cmd)
{
  // send control session request
  {
    BotProtocol::ControlSession controlSession;
    controlSession.entityType = BotProtocol::session;
    controlSession.entityId = sessionId;
    controlSession.cmd = cmd;
    if(!sendMessage(BotProtocol::controlEntity, &controlSession, sizeof(controlSession)))
      return false;
    return true;
  }

  // receive control session response
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    if(!receiveMessage(header, data, size))
      return false;
    if(header.messageType != BotProtocol::controlEntityResponse || size < sizeof(BotProtocol::ControlSessionResponse))
    {
      error = "Received invalid control session response.";
      return false;
    }
    BotProtocol::ControlSessionResponse* response = (BotProtocol::ControlSessionResponse*)data;
    if(!response->success)
    {
      error = "Could not control session.";
      return false;
    }
  }
  return true;
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
