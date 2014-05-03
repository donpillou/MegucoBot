
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
    for(;;)
    {
      if(!connection.receiveMessage(header, data))
        return false;
      if(!handleMessage(header, data, header.size - sizeof(header)))
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
        return handleCreateEntity(*(BotProtocol::Entity*)data, size);
      break;
    case BotProtocol::updateEntity:
      if(size >= sizeof(BotProtocol::Entity))
        return handleUpdateEntity(*(BotProtocol::Entity*)data, size);
      break;
    case BotProtocol::removeEntity:
      if(size >= sizeof(BotProtocol::Entity))
        return handleRemoveEntity(*(BotProtocol::Entity*)data);
      break;
    case BotProtocol::controlEntity:
      if(size >= sizeof(BotProtocol::Entity))
        return handleControlEntity(*(BotProtocol::Entity*)data, size);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleCreateEntity(BotProtocol::Entity& entity, size_t size)
  {
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketOrder:
      if(size >= sizeof(BotProtocol::Order))
        return handleCreateOrder(*(BotProtocol::Order*)&entity);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleUpdateEntity(BotProtocol::Entity& entity, size_t size)
  {
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketOrder:
      if(size >= sizeof(BotProtocol::Order))
        return handleUpdateOrder(*(BotProtocol::Order*)&entity);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleRemoveEntity(const BotProtocol::Entity& entity)
  {
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketOrder:
      return handleRemoveOrder(entity.entityId);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleControlEntity(BotProtocol::Entity& entity, size_t size)
  {
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::market:
      if(size >= sizeof(BotProtocol::ControlMarket))
        return handleControlMarket(*(BotProtocol::ControlMarket*)&entity);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleCreateOrder(BotProtocol::Order& createOrderArgs)
  {
    BotProtocol::CreateEntityResponse response;
    response.entityType = BotProtocol::marketOrder;
    response.entityId = createOrderArgs.entityId;
    response.id = 0;
    response.success = 0;

    BotProtocol::Order order;
    if(!market->createOrder(0, (BotProtocol::Order::Type)createOrderArgs.type, createOrderArgs.price, createOrderArgs.amount, order))
    {
      if(!connection.sendMessage(BotProtocol::createEntityResponse, &response, sizeof(response)))
        return false;
      return connection.sendError(market->getLastError());
    }

    response.id = order.entityId;
    response.success = 1;
    if(!connection.sendMessage(BotProtocol::createEntityResponse, &response, sizeof(response)))
      return false;
    return connection.sendEntity(&order, sizeof(order));
  }

  bool_t handleUpdateOrder(BotProtocol::Order& updateOrderArgs)
  {
    // step #1 cancel current order
    if(!market->cancelOrder(updateOrderArgs.entityId))
      return connection.sendError(market->getLastError());

    // step #2 create new order with same id
    BotProtocol::Order order;
    if(!market->createOrder(updateOrderArgs.entityId, (BotProtocol::Order::Type)updateOrderArgs.type, updateOrderArgs.price, updateOrderArgs.amount, order))
    {
      if(!connection.sendError(market->getLastError()))
        return false;
      if(!connection.removeEntity(BotProtocol::marketOrder, updateOrderArgs.entityId))
        return false;
      return true;
    }
    return connection.sendEntity(&order, sizeof(order));
  }

  bool_t handleRemoveOrder(uint32_t id)
  {
    if(!market->cancelOrder(id))
      return connection.sendError(market->getLastError());
    return connection.removeEntity(BotProtocol::marketOrder, id);
  }

  bool_t handleControlMarket(BotProtocol::ControlMarket& controlMarket)
  {
    switch((BotProtocol::ControlMarket::Command)controlMarket.cmd)
    {
    case BotProtocol::ControlMarket::refreshOrders:
      {
        List<BotProtocol::Order> orders;
        if(!market->loadOrders(orders))
          return connection.sendError(market->getLastError());
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
          return connection.sendError(market->getLastError());
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
          return connection.sendError(market->getLastError());
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
