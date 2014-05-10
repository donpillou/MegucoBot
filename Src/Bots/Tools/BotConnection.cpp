
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
    marketAdapterName = BotProtocol::getString(response->marketAdapterName);
    balanceBase = response->balanceBase;
    balanceComm = response->balanceComm;
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
  return createEntity(&logMessage, sizeof(logMessage), id);
}

bool_t BotConnection::getBalance(BotProtocol::MarketBalance& balance)
{
  //if(!sendControlSession(BotProtocol::ControlSession::requestBalance))
  //  return false;
  //BotProtocol::Header header;
  //byte_t* data;
  //size_t size;
  //for(;;)
  //{
  //  if(!receiveMessage(header, data, size))
  //    return false;
  //  switch((BotProtocol::MessageType)header.messageType)
  //  {
  //  case BotProtocol::updateEntity:
  //    if(size >= sizeof(BotProtocol::Entity))
  //    {
  //      BotProtocol::Entity* entity = (BotProtocol::Entity*)data;
  //      if(entity->entityType == BotProtocol::marketBalance && size >= sizeof(BotProtocol::MarketBalance))
  //      {
  //        balance = *(BotProtocol::MarketBalance*)data;
  //        return true;
  //      }
  //    }
  //    break;
  //  default:
  //    break;
  //  }
  //}
  return false;
}

bool_t BotConnection::getTransactions(List<BotProtocol::Transaction>& transactions)
{
//  if(!sendControlSession(BotProtocol::ControlSession::requestTransactions))
//    return false;
//  if(!sendPing())
//    return false;
//
//  BotProtocol::Header header;
//  byte_t* data;
//  size_t size;
//  for(;;)
//  {
//    if(!receiveMessage(header, data, size))
//      return false;
//    switch((BotProtocol::MessageType)header.messageType)
//    {
//    case BotProtocol::updateEntity:
//      if(size >= sizeof(BotProtocol::Entity))
//      {
//        BotProtocol::Entity* entity = (BotProtocol::Entity*)data;
//        if(entity->entityType == BotProtocol::sessionTransaction && size >= sizeof(BotProtocol::Transaction))
//          transactions.append(*(BotProtocol::Transaction*)data);
//      }
//      break;
//    case BotProtocol::pingResponse:
//      return true;
//    default:
//      break;
//    }
//  }
  return false;
}

bool_t BotConnection::getOrders(List<BotProtocol::Order>& orders)
{
//  if(!sendControlSession(BotProtocol::ControlSession::requestOrders))
//    return false;
//  if(!sendPing())
//    return false;
//
//  BotProtocol::Header header;
//  byte_t* data;
//  size_t size;
//  for(;;)
//  {
//    if(!receiveMessage(header, data, size))
//      return false;
//    switch((BotProtocol::MessageType)header.messageType)
//    {
//    case BotProtocol::updateEntity:
//      if(size >= sizeof(BotProtocol::Entity))
//      {
//        BotProtocol::Entity* entity = (BotProtocol::Entity*)data;
//        if(entity->entityType == BotProtocol::sessionOrder && size >= sizeof(BotProtocol::Order))
//          orders.append(*(BotProtocol::Order*)data);
//      }
//      break;
//    case BotProtocol::pingResponse:
//      return true;
//    default:
//      break;
//    }
//  }
  return false;
}

bool_t BotConnection::createTransaction(const BotProtocol::Transaction& transaction, uint32_t& id)
{
  return createEntity(&transaction, sizeof(transaction), id);
}

bool_t BotConnection::removeTransaction(uint32_t id)
{
  return removeEntity(BotProtocol::sessionTransaction, id);
}

bool_t BotConnection::createOrder(const BotProtocol::Order& order, uint32_t& id)
{
  return createEntity(&order, sizeof(order), id);
}

bool_t BotConnection::removeOrder(uint32_t id)
{
  return removeEntity(BotProtocol::sessionOrder, id);
}

bool_t BotConnection::createEntity(const void_t* data, size_t size, uint32_t& id)
{
  ASSERT(size >= sizeof(BotProtocol::Entity));
  BotProtocol::EntityType entityType = (BotProtocol::EntityType)((BotProtocol::Entity*)data)->entityType;
  if(!sendMessage(BotProtocol::createEntity, 0, data, size))
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
    if(!(header.messageType == BotProtocol::updateEntityResponse && header.requestId == 0 && size >= sizeof(BotProtocol::Entity) &&
         entity->entityType == entityType))
    {
      error = "Could not receive update entity response.";
      return false;
    }
    id = entity->entityId;
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
    return true;
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
    if(!(header.messageType != BotProtocol::controlEntityResponse && header.requestId != 0 && size >= sizeof(BotProtocol::ControlSessionResponse) &&
         controlSessionResponse->entityType == BotProtocol::session && controlSessionResponse->entityId == sessionId && 
         controlSessionResponse->cmd == cmd))
    {
      error = "Could not receive control session response.";
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
