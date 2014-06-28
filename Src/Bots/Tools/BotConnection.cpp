
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
    if(!sendMessage(BotProtocol::registerBotRequest, 0, &registerBotRequest, sizeof(registerBotRequest)))
      return false;
  }

  // receive register bot response
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
    if(!(header.messageType == BotProtocol::registerBotResponse && header.requestId == 0 && size >= sizeof(BotProtocol::RegisterBotResponse)))
    {
      error = "Could not receive register bot response.";
      return false;
    }
    BotProtocol::RegisterBotResponse* response = (BotProtocol::RegisterBotResponse*)data;
    sessionId = response->sessionId;
    marketId = response->marketId;
    marketAdapterName = BotProtocol::getString(response->marketAdapterName);
    simulation = response->simulation != 0;
  }

  return true;
}

bool_t BotConnection::getMarketBalance(BotProtocol::Balance& balance)
{
  List<BotProtocol::Balance> result;
  if(!sendControlMarket(BotProtocol::ControlMarket::requestBalance, result))
    return false;
  if(result.isEmpty())
  {
    error = "Received response without market balance.";
    return false;
  }
  balance = result.front();
  return true;
}

bool_t BotConnection::getMarketOrders(List<BotProtocol::Order>& orders)
{
  if(!sendControlMarket(BotProtocol::ControlMarket::requestOrders, orders))
    return false;
  return true;
}

bool_t BotConnection::createMarketOrder(BotProtocol::Order& order)
{
  return createEntity(&order, sizeof(order));
}

bool_t BotConnection::removeMarketOrder(uint32_t id)
{
  return removeEntity(BotProtocol::marketOrder, id);
}

bool_t BotConnection::addLogMessage(const String& message)
{
  BotProtocol::SessionLogMessage logMessage;
  logMessage.entityType = BotProtocol::sessionLogMessage;
  logMessage.entityId = 0;
  logMessage.date = Time::time();
  BotProtocol::setString(logMessage.message, message);
  return createEntity(&logMessage, sizeof(logMessage));
}


bool_t BotConnection::getSessionTransactions(List<BotProtocol::Transaction>& transactions)
{
  if(!sendControlSession(BotProtocol::ControlSession::requestTransactions, transactions))
    return false;
  return true;
}

bool_t BotConnection::getSessionItems(List<BotProtocol::SessionItem>& items)
{
  if(!sendControlSession(BotProtocol::ControlSession::requestItems, items))
    return false;
  return true;
}

bool_t BotConnection::getSessionOrders(List<BotProtocol::Order>& orders)
{
  if(!sendControlSession(BotProtocol::ControlSession::requestOrders, orders))
    return false;
  return true;
}

bool_t BotConnection::createSessionTransaction(BotProtocol::Transaction& transaction)
{
  return createEntity(&transaction, sizeof(transaction));
}

bool_t BotConnection::updateSessionTransaction(const BotProtocol::Transaction& transaction)
{
  return updateEntity(&transaction, sizeof(transaction));
}

bool_t BotConnection::removeSessionTransaction(uint32_t id)
{
  return removeEntity(BotProtocol::sessionTransaction, id);
}

bool_t BotConnection::createSessionItem(BotProtocol::SessionItem& item)
{
  return createEntity(&item, sizeof(item));
}

bool_t BotConnection::updateSessionItem(const BotProtocol::SessionItem& item)
{
  return updateEntity(&item, sizeof(item));
}

bool_t BotConnection::removeSessionItem(uint32_t id)
{
  return removeEntity(BotProtocol::sessionItem, id);
}

bool_t BotConnection::createSessionOrder(BotProtocol::Order& order)
{
  return createEntity(&order, sizeof(order));
}

bool_t BotConnection::updateSessionOrder(BotProtocol::Order& order)
{
  return updateEntity(&order, sizeof(order));
}

bool_t BotConnection::removeSessionOrder(uint32_t id)
{
  return removeEntity(BotProtocol::sessionOrder, id);
}

bool_t BotConnection::createSessionMarker(BotProtocol::Marker& marker)
{
  return createEntity(&marker, sizeof(marker));
}

bool_t BotConnection::removeSessionMarker(uint32_t id)
{
  return removeEntity(BotProtocol::sessionMarker, id);
}

bool_t BotConnection::getSessionBalance(BotProtocol::Balance& balance)
{
  List<BotProtocol::Balance> result;
  if(!sendControlSession(BotProtocol::ControlSession::requestBalance, result))
    return false;
  if(result.isEmpty())
  {
    error = "Received response without session balance.";
    return false;
  }
  balance = result.front();
  return true;
}

bool_t BotConnection::updateSessionBalance(BotProtocol::Balance& balance)
{
  return updateEntity(&balance, sizeof(balance));
}

bool_t BotConnection::createEntity(void_t* entityData, size_t entitySize)
{
  ASSERT(entitySize >= sizeof(BotProtocol::Entity));
  BotProtocol::EntityType entityType = (BotProtocol::EntityType)((BotProtocol::Entity*)entityData)->entityType;
  if(!sendMessage(BotProtocol::createEntity, 0, entityData, entitySize))
    return false;

  // receive create response
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
    if(!(header.messageType == BotProtocol::createEntityResponse && header.requestId == 0 && size >= entitySize &&
         entity->entityType == entityType))
    {
      error = "Could not receive create entity response.";
      return false;
    }
    Memory::copy(entityData, data, entitySize);
  }

  return true;
}

bool_t BotConnection::updateEntity(const void_t* data, size_t size)
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
         entity->entityType == entityType && entity->entityId == entityId))
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
    if(!(header.messageType == BotProtocol::removeEntityResponse && header.requestId == 0 && size >= sizeof(BotProtocol::Entity) &&
         entity->entityType == type && entity->entityId == id))
    {
      error = "Could not receive remove entity response.";
      return false;
    }
  }

  return true;
}

bool_t BotConnection::sendControlSession(BotProtocol::ControlSession::Command cmd)
{
  // send control session request
  {
    BotProtocol::ControlSession controlSession;
    controlSession.entityType = BotProtocol::session;
    controlSession.entityId = sessionId;
    controlSession.cmd = cmd;
    if(!sendMessage(BotProtocol::controlEntity, 0, &controlSession, sizeof(controlSession)))
      return false;
  }

  // receive control session response
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
    BotProtocol::ControlSessionResponse* controlSessionResponse = (BotProtocol::ControlSessionResponse*)data;
    if(!(header.messageType == BotProtocol::controlEntityResponse && header.requestId == 0 && size >= sizeof(BotProtocol::ControlSessionResponse) &&
         controlSessionResponse->entityType == BotProtocol::session && controlSessionResponse->entityId == sessionId && 
         controlSessionResponse->cmd == cmd))
    {
      error = "Could not receive control session response.";
      return false;
    }
  }
  return true;
}

template<class E> bool_t BotConnection::sendControlMarket(BotProtocol::ControlMarket::Command cmd, List<E>& result)
{
  // send control market request
  {
    BotProtocol::ControlMarket controlMarket;
    controlMarket.entityType = BotProtocol::market;
    controlMarket.entityId = marketId;
    controlMarket.cmd = cmd;
    if(!sendMessage(BotProtocol::controlEntity, 0, &controlMarket, sizeof(controlMarket)))
      return false;
  }

  // receive control session response
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
    BotProtocol::ControlMarketResponse* controlMarketResponse = (BotProtocol::ControlMarketResponse*)data;
    if(!(header.messageType == BotProtocol::controlEntityResponse && header.requestId == 0 && size >= sizeof(BotProtocol::ControlMarketResponse) &&
         controlMarketResponse->entityType == BotProtocol::market && controlMarketResponse->entityId == marketId && 
         controlMarketResponse->cmd == cmd))
    {
      error = "Could not receive control market response.";
      return false;
    }
    size_t itemCount = (size - sizeof(controlMarketResponse)) / sizeof(E);
    for(E* entity = (E*)(controlMarketResponse + 1), * end = entity + itemCount; entity < end; ++entity)
      result.append(*entity);
  }
  return true;
}

template<class E> bool_t BotConnection::sendControlSession(BotProtocol::ControlSession::Command cmd, List<E>& result)
{
  // send control session request
  {
    BotProtocol::ControlSession controlSession;
    controlSession.entityType = BotProtocol::session;
    controlSession.entityId = sessionId;
    controlSession.cmd = cmd;
    if(!sendMessage(BotProtocol::controlEntity, 0, &controlSession, sizeof(controlSession)))
      return false;
  }

  // receive control session response
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
    BotProtocol::ControlSessionResponse* controlSessionResponse = (BotProtocol::ControlSessionResponse*)data;
    if(!(header.messageType == BotProtocol::controlEntityResponse && header.requestId == 0 && size >= sizeof(BotProtocol::ControlSessionResponse) &&
         controlSessionResponse->entityType == BotProtocol::session && controlSessionResponse->entityId == sessionId && 
         controlSessionResponse->cmd == cmd))
    {
      error = "Could not receive control session response.";
      return false;
    }
    size_t itemCount = (size - sizeof(controlSessionResponse)) / sizeof(E);
    for(E* entity = (E*)(controlSessionResponse + 1), * end = entity + itemCount; entity < end; ++entity)
      result.append(*entity);
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
     (size > 0 && socket.send((const byte_t*)data, size) != (ssize_t)size))
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
