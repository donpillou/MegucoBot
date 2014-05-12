
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
#include "Tools/DataConnection.h"
#include "Tools/SimBroker.h"
#include "Tools/TradeHandler.h"

#ifdef BOT_BUYBOT
#include "Bots/BuyBot.h"
typedef BuyBot MarketConnection;
const char* botName = "BuyBot";
#endif

class DataConnectionHandler : private DataConnection::Callback
{
public:
  DataConnectionHandler(BotConnection& botConnection, Broker& broker) : botConnection(botConnection), broker(broker), lastReceivedTradeId(0) {}

  bool_t connect() {return dataConnection.connect();}
  bool_t subscribe(const String& channel) {return dataConnection.subscribe(channel, lastReceivedTradeId);}

  bool_t process()
  {
    for(;;)
      if(!dataConnection.process(*this))
        return false;
  }

private:
  BotConnection& botConnection;
  Broker& broker;
  DataConnection dataConnection;
  TradeHandler tradeHandler;
  uint64_t lastReceivedTradeId;

private: // DataConnection::Callback
  virtual void receivedChannelInfo(const String& channelName) {};
  virtual void receivedSubscribeResponse(const String& channelName, uint64_t channelId) {};
  virtual void receivedUnsubscribeResponse(const String& channelName, uint64_t channelId) {};

  virtual void receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade)
  {
    lastReceivedTradeId = trade.id;
    tradeHandler.add(trade, 0LL);
    if(!broker.handleTrade(trade))
      return;

    //if(simulation)
    //{
    //  if(startTime == 0)
    //    startTime = trade.time;
    //  if(trade.time - startTime <= 45 * 60 * 1000)
    //    return;
    //}
    //else if(trade.flags & DataProtocol::replayedFlag)
    //  return;
    //broker->update(trade, *session);
    //session->handle(trade, tradeHandler.values);
  }

  virtual void receivedTicker(uint64_t channelId, const DataProtocol::Ticker& ticker) {};
  virtual void receivedErrorResponse(const String& message) {};
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

  BotConnection botConnection;
  if(!botConnection.connect(port))
  {
    Console::errorf("error: Could not connect to bot server: %s\n", (const char_t*)botConnection.getErrorString());
    return -1;
  }

  BotProtocol::MarketBalance marketBalance;
  if(!botConnection.getMarketBalance(marketBalance))
  {
    Console::errorf("error: Could not retrieve market balance: %s\n", (const char_t*)botConnection.getErrorString());
    return -1;
  }

  List<BotProtocol::Transaction> transactions;
  if(!botConnection.getSessionTransactions(transactions))
  {
    Console::errorf("error: Could not retrieve session transactions: %s\n", (const char_t*)botConnection.getErrorString());
    return -1;
  }
  
  List<BotProtocol::Order> orders;
  if(!botConnection.getSessionOrders(orders))
  {
    Console::errorf("error: Could not retrieve session orders: %s\n", (const char_t*)botConnection.getErrorString());
    return -1;
  }

  SimBroker simBroker(botConnection, botConnection.getBalanceBase(), botConnection.getBalanceComm(), marketBalance.fee);
  for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
    simBroker.loadTransaction(*i);
  for(List<BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
    simBroker.loadOrder(*i);
  
  DataConnectionHandler dataConnection(botConnection, simBroker);
  String marketAdapterName = botConnection.getMarketAdapterName();
  for(;;)
  {
    
    if(!dataConnection.connect())
      continue;
    if(!dataConnection.subscribe(marketAdapterName))
      continue;
    for(;;)
    {
      if(!dataConnection.process())
        break;
    }
  }

  //for(int i = 0;; ++i)
  //{
  //  String message;
  //  message.printf("bot test log message iteration %d", i);
  //  if(!botConnection.addLogMessage(message))
  //  {
  //    Console::errorf("error: Could not add test log message: %s\n", (const char_t*)botConnection.getErrorString());
  //    return -1;
  //  }
  //
  //  BotProtocol::Transaction transaction;
  //  transaction.entityType = BotProtocol::sessionTransaction;
  //  transaction.amount = 1.;
  //  transaction.fee = 0.01;
  //  transaction.price = 1000.;
  //  transaction.type = BotProtocol::Transaction::buy;
  //  uint32_t entityId;
  //  if(!botConnection.createSessionTransaction(transaction, entityId))
  //  {
  //    Console::errorf("error: Could not create test transaction: %s\n", (const char_t*)botConnection.getErrorString());
  //    return -1;
  //  }
  //
  //  Thread::sleep(2500);
  //  if(!botConnection.removeSessionTransaction(entityId))
  //  {
  //    Console::errorf("error: Could not remove test transaction: %s\n", (const char_t*)botConnection.getErrorString());
  //    return -1;
  //  }
  //  Thread::sleep(1000);
  //
  //  BotProtocol::Order order;
  //  order.entityType = BotProtocol::sessionOrder;
  //  order.amount = 1.;
  //  order.fee = 0.01;
  //  order.price = 1000.;
  //  order.type = BotProtocol::Order::buy;
  //  if(!botConnection.createSessionOrder(order, entityId))
  //  {
  //    Console::errorf("error: Could not create test order: %s\n", (const char_t*)botConnection.getErrorString());
  //    return -1;
  //  }
  //  Thread::sleep(2500);
  //  if(!botConnection.removeSessionOrder(entityId))
  //  {
  //    Console::errorf("error: Could not remove test order: %s\n", (const char_t*)botConnection.getErrorString());
  //    return -1;
  //  }
  //}
  return 0;
}
