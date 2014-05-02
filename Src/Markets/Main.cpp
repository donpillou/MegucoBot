
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
    case BotProtocol::controlEntity:
      if(size >= sizeof(BotProtocol::Entity))
        return handleControlEntity(*(BotProtocol::Entity*)data, size);
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

  bool_t handleControlEntity(BotProtocol::Entity& entity, size_t size)
  {
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::market:
      if(size >= sizeof(BotProtocol::ControlMarketArgs))
        return handleControlMarket(*(BotProtocol::ControlMarketArgs*)&entity);
      break;
    default:
      break;
    }
    return true;
  }

  bool_t handleCreateOrder(BotProtocol::Order& createOrderArgs)
  {
    BotProtocol::Order order;
    if(market->createOrder((BotProtocol::Order::Type)createOrderArgs.type, createOrderArgs.price, createOrderArgs.amount, order))
    {
      if(!connection.sendEntity(&order, sizeof(order)))
        return false;
    }
    else
      return connection.sendError(market->getLastError());
    return true;
  }

  bool_t handleControlMarket(BotProtocol::ControlMarketArgs& controlMarketArgs)
  {
    switch((BotProtocol::ControlMarketArgs::Command)controlMarketArgs.cmd)
    {
    case BotProtocol::ControlMarketArgs::refreshOrders:
      {
        List<BotProtocol::Order> orders;
        if(market->loadOrders(orders))
        {
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
        else
          return connection.sendError(market->getLastError());
      }
      break;
    case BotProtocol::ControlMarketArgs::refreshTransactions:
      {
        List<BotProtocol::Transaction> transactions;
        if(market->loadTransactions(transactions))
        {
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
        else
          return connection.sendError(market->getLastError());
      }
      break;
    case BotProtocol::ControlMarketArgs::refreshBalance:
      {
        BotProtocol::MarketBalance balance;
        if(market->loadBalance(balance))
        {
          if(!connection.sendEntity(&balance, sizeof(balance)))
            return false;
        }
        else
          return connection.sendError(market->getLastError());
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
