
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

private:
  bool_t handleMessage(const BotProtocol::Header& header, byte_t* data, size_t size)
  {
    switch((BotProtocol::MessageType)header.messageType)
    {
    case BotProtocol::controlEntity:
      if(size >= sizeof(BotProtocol::Entity))
        return handleControlEntity(*(BotProtocol::Entity*)data, size);
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

  bool_t handleControlMarket(BotProtocol::ControlMarketArgs& controlMarketArgs)
  {
    switch((BotProtocol::ControlMarketArgs::Command)controlMarketArgs.cmd)
    {
    case BotProtocol::ControlMarketArgs::refreshOrders:
      {
        List<BotProtocol::Order> orders;
        if(market->loadOrders(orders))
        {
          // todo: send orders
        }
        else
        {
          // todo:
          //return connection.sendError(market->getErrorString());
        }
      }
      break;
    case BotProtocol::ControlMarketArgs::refreshTransactions:
      // todo
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
