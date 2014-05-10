
#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#endif

#include <nstd/Console.h>
#include <nstd/String.h>
#include <nstd/Thread.h> // sleep
#include <nstd/HashSet.h>
//#include <nstd/Error.h>

#include "Tools/BotConnection.h"

#ifdef MARKET_BITSTAMPUSD
#include "Markets/BitstampUsd.h"
typedef BitstampMarket MarketAdapter;
#endif

class BotConnectionHandler
{
public:

  BotConnectionHandler() : market(0) {}
  ~BotConnectionHandler()
  {
    delete market;
  }

  bool_t connect(uint16_t port)
  {
    if(market)
      return false;

    if(!connection.connect(port))
      return false;

    market = new MarketAdapter(connection.getUserName(), connection.getKey(), connection.getSecret());
    return true;
  }

  bool_t process()
  {
    BotProtocol::Header header;
    byte_t* data;
    size_t size;
    for(;;)
    {
      if(!connection.receiveMessage(header, data, size))
        return false;
      if(!handleMessage(header, data, size))
        return false;
    }
  }

  const String& getErrorString() const {return connection.getErrorString();}

private:
  BotConnection connection;
  Market* market;

  HashSet<uint32_t> orders;
  HashSet<uint32_t> transactions;

private:
  bool_t handleMessage(const BotProtocol::Header& header, byte_t* data, size_t size)
  {
    switch((BotProtocol::MessageType)header.messageType)
    {
    case BotProtocol::createEntity:
      if(size >= sizeof(BotProtocol::Entity))
        return handleCreateEntity(header.requestId, *(BotProtocol::Entity*)data, size);
      break;
    case BotProtocol::updateEntity:
      if(size >= sizeof(BotProtocol::Entity))
        return handleUpdateEntity(header.requestId, *(BotProtocol::Entity*)data, size);
      break;
    case BotProtocol::removeEntity:
      if(size >= sizeof(BotProtocol::Entity))
        return handleRemoveEntity(header.requestId, *(BotProtocol::Entity*)data);
      break;
    case BotProtocol::controlEntity:
      if(size >= sizeof(BotProtocol::Entity))
        return handleControlEntity(header.requestId, *(BotProtocol::Entity*)data, size);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleCreateEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size)
  {
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketOrder:
      if(size >= sizeof(BotProtocol::Order))
        return handleCreateOrder(requestId, *(BotProtocol::Order*)&entity);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleUpdateEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size)
  {
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketOrder:
      if(size >= sizeof(BotProtocol::Order))
        return handleUpdateOrder(requestId, *(BotProtocol::Order*)&entity);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleRemoveEntity(uint32_t requestId, const BotProtocol::Entity& entity)
  {
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketOrder:
      return handleRemoveOrder(requestId, entity);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleControlEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size)
  {
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::market:
      if(size >= sizeof(BotProtocol::ControlMarket))
        return handleControlMarket(requestId, *(BotProtocol::ControlMarket*)&entity);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleCreateOrder(uint32_t requestId, BotProtocol::Order& createOrderArgs)
  {
    BotProtocol::Order order;
    if(!market->createOrder(0, (BotProtocol::Order::Type)createOrderArgs.type, createOrderArgs.price, createOrderArgs.amount, order))
      return connection.sendErrorResponse(BotProtocol::createEntity, requestId, createOrderArgs, market->getLastError());

    BotProtocol::Entity response;
    response.entityType = BotProtocol::marketOrder;
    response.entityId = order.entityId;
    if(!connection.sendMessage(BotProtocol::createEntityResponse, requestId, &response, sizeof(response)))
      return false;
    return connection.sendEntity(&order, sizeof(order));
  }

  bool_t handleUpdateOrder(uint32_t requestId, BotProtocol::Order& updateOrderArgs)
  {
    // step #1 cancel current order
    if(!market->cancelOrder(updateOrderArgs.entityId))
      return connection.sendErrorResponse(BotProtocol::updateEntity, requestId, updateOrderArgs, market->getLastError());

    // step #2 create new order with same id
    BotProtocol::Order order;
    if(!market->createOrder(updateOrderArgs.entityId, (BotProtocol::Order::Type)updateOrderArgs.type, updateOrderArgs.price, updateOrderArgs.amount, order))
    {
      if(!connection.sendErrorResponse(BotProtocol::updateEntity, requestId, updateOrderArgs, market->getLastError()))
        return false;
      if(!connection.removeEntity(BotProtocol::marketOrder, updateOrderArgs.entityId))
        return false;
      return true;
    }
    if(!connection.sendMessage(BotProtocol::updateEntityResponse, requestId, &updateOrderArgs, sizeof(BotProtocol::Entity)))
      return false;
    return connection.sendEntity(&order, sizeof(order));
  }

  bool_t handleRemoveOrder(uint32_t requestId, const BotProtocol::Entity& entity)
  {
    if(!market->cancelOrder(entity.entityId))
    {
      if(!connection.sendErrorResponse(BotProtocol::removeEntity, requestId, entity, market->getLastError()))
        return false;
      BotProtocol::Order order;
      if(market->getOrder(entity.entityId, order))
        if(!connection.sendEntity(&order, sizeof(order)))
          return false;
      return true;
    }
    if(!connection.sendMessage(BotProtocol::removeEntityResponse, requestId, &entity, sizeof(entity)))
      return false;
    return connection.removeEntity(BotProtocol::marketOrder, entity.entityId);
  }

  bool_t handleControlMarket(uint32_t requestId, BotProtocol::ControlMarket& controlMarket)
  {
    BotProtocol::ControlMarketResponse response;
    response.entityType = BotProtocol::market;
    response.entityId = controlMarket.entityId;
    response.cmd = controlMarket.cmd;

    switch((BotProtocol::ControlMarket::Command)controlMarket.cmd)
    {
    case BotProtocol::ControlMarket::refreshOrders:
      {
        List<BotProtocol::Order> orders;
        if(!market->loadOrders(orders))
          return connection.sendErrorResponse(BotProtocol::controlEntity, requestId, controlMarket, market->getLastError());
        if(!connection.sendMessage(BotProtocol::controlEntityResponse, requestId, &response, sizeof(response)))
            return false;
        HashSet<uint32_t> ordersToRemove;
        ordersToRemove.swap(this->orders);
        for(List<BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
        {
          this->orders.append(i->entityId);
          if(!connection.sendEntity(&*i, sizeof(BotProtocol::Order)))
            return false;
          ordersToRemove.remove(i->entityId);
        }
        for(HashSet<uint32_t>::Iterator i = ordersToRemove.begin(), end = ordersToRemove.end(); i != end; ++i)
          if(!connection.removeEntity(BotProtocol::marketOrder, *i))
            return false;
      }
      break;
    case BotProtocol::ControlMarket::refreshTransactions:
      {
        List<BotProtocol::Transaction> transactions;
        if(!market->loadTransactions(transactions))
          return connection.sendErrorResponse(BotProtocol::controlEntity, requestId, controlMarket, market->getLastError());
        if(!connection.sendMessage(BotProtocol::controlEntityResponse, requestId, &response, sizeof(response)))
            return false;
        HashSet<uint32_t> transactionsToRemove;
        transactionsToRemove.swap(this->transactions);
        for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
        {
          this->transactions.append(i->entityId);
          if(!connection.sendEntity(&*i, sizeof(BotProtocol::Transaction)))
            return false;
          transactionsToRemove.remove(i->entityId);
        }
        for(HashSet<uint32_t>::Iterator i = transactionsToRemove.begin(), end = transactionsToRemove.end(); i != end; ++i)
          if(!connection.removeEntity(BotProtocol::marketTransaction, *i))
            return false;
      }
      break;
    case BotProtocol::ControlMarket::refreshBalance:
      {
        BotProtocol::MarketBalance balance;
        if(!market->loadBalance(balance))
          return connection.sendErrorResponse(BotProtocol::controlEntity, requestId, controlMarket, market->getLastError());
        if(!connection.sendMessage(BotProtocol::controlEntityResponse, requestId, &response, sizeof(response)))
            return false;
        if(!connection.sendEntity(&balance, sizeof(balance)))
          return false;
      }
      break;
    default:
      break;
    }
    return true;
  }
};

int_t main(int_t argc, char_t* argv[])
{
  static const uint16_t port = 40124;

  //for(;;)
  //{
  //  bool stop = true;
  //  if(!stop)
  //    break;
  //}

  // create connection to bot server
  BotConnectionHandler connection;
  if(!connection.connect(port))
  {
    Console::errorf("error: Could not connect to bot server: %s\n", (const char_t*)connection.getErrorString());
    return -1;
  }

  // wait for requests
  if(!connection.process())
  {
    Console::errorf("error: Lost connection to bot server: %s\n", (const char_t*)connection.getErrorString());
    return -1;
  }

  return 0;
}
