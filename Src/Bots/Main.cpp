
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

#ifdef BOT_BUYBOT
#include "Bots/BuyBot.h"
typedef BuyBot MarketConnection;
const char* botName = "BuyBot";
#endif

class DataConnectionHandler : private DataConnection::Callback
{
public:
  DataConnectionHandler(BotConnection& botConnection) : botConnection(botConnection) {}

  bool_t connect() {return dataConnection.connect();}
  bool_t subscribe(const String& channel) {return dataConnection.subscribe(channel, 0);}

  bool_t process()
  {
    for(;;)
      if(!dataConnection.process(*this))
        return false;
  }

private:
  BotConnection& botConnection;
  DataConnection dataConnection;

private: // DataConnection::Callback
  virtual void receivedChannelInfo(const String& channelName) {};
  virtual void receivedSubscribeResponse(const String& channelName, uint64_t channelId) {};
  virtual void receivedUnsubscribeResponse(const String& channelName, uint64_t channelId) {};
  virtual void receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade) {};
  virtual void receivedTicker(uint64_t channelId, const DataProtocol::Ticker& ticker) {};
  virtual void receivedErrorResponse(const String& message) {};
};


int_t main(int_t argc, char_t* argv[])
{
  static const uint16_t port = 40124;
//  bool background = true;
//
//  // parse parameters
//  for(int i = 1; i < argc; ++i)
//    if(String::compare(argv[i], "-f") == 0)
//      background = false;
//    else
//    {
//      Console::errorf("Usage: %s [-f]\n"
//"  -f            run in foreground (not as daemon)\n", argv[0]);
//      return -1;
//    }

#ifndef _WIN32
  // daemonize process
//  if(background)
//  {
//    Console::printf("Starting as daemon...\n");
//
//    char logFileName[200];
//    strcpy(logFileName, botName);
//    strcat(logFileName, ".log");
//    int fd = open(logFileName, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
//    if(fd == -1)
//    {
//      Console::errorf("error: Could not open file %s: %s\n", logFileName, strerror(errno));
//      return -1;
//    }
//    if(dup2(fd, STDOUT_FILENO) == -1)
//    {
//      Console::errorf("error: Could not reopen stdout: %s\n", strerror(errno));
//      return 0;
//    }
//    if(dup2(fd, STDERR_FILENO) == -1)
//    {
//      Console::errorf("error: Could not reopen stdout: %s\n", strerror(errno));
//      return 0;
//    }
//    close(fd);
//
//    pid_t childPid = fork();
//    if(childPid == -1)
//      return -1;
//    if(childPid != 0)
//      return 0;
//  }
#endif

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

  List<BotProtocol::Transaction> transactions;
  if(!botConnection.getTransactions(transactions))
  {
    Console::errorf("error: Could not retrieve session transactions: %s\n", (const char_t*)botConnection.getErrorString());
    return -1;
  }

  List<BotProtocol::Order> orders;
  if(!botConnection.getOrders(orders))
  {
    Console::errorf("error: Could not retrieve session orders: %s\n", (const char_t*)botConnection.getErrorString());
    return -1;
  }

  String marketAdapterName = botConnection.getMarketAdapterName();
  for(;;)
  {
    DataConnectionHandler dataConnection(botConnection);
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

  for(int i = 0;; ++i)
  {
    String message;
    message.printf("bot test log message iteration %d", i);
    if(!botConnection.addLogMessage(message))
    {
      Console::errorf("error: Could not add test log message: %s\n", (const char_t*)botConnection.getErrorString());
      return -1;
    }

    BotProtocol::Transaction transaction;
    transaction.entityType = BotProtocol::sessionTransaction;
    transaction.amount = 1.;
    transaction.fee = 0.01;
    transaction.price = 1000.;
    transaction.type = BotProtocol::Transaction::buy;
    uint32_t entityId;
    if(!botConnection.createTransaction(transaction, entityId))
    {
      Console::errorf("error: Could not create test transaction: %s\n", (const char_t*)botConnection.getErrorString());
      return -1;
    }

    Thread::sleep(2500);
    if(!botConnection.removeTransaction(entityId))
    {
      Console::errorf("error: Could not remove test transaction: %s\n", (const char_t*)botConnection.getErrorString());
      return -1;
    }
    Thread::sleep(1000);

    BotProtocol::Order order;
    order.entityType = BotProtocol::sessionOrder;
    order.amount = 1.;
    order.fee = 0.01;
    order.price = 1000.;
    order.type = BotProtocol::Order::buy;
    if(!botConnection.createOrder(order, entityId))
    {
      Console::errorf("error: Could not create test order: %s\n", (const char_t*)botConnection.getErrorString());
      return -1;
    }
    Thread::sleep(2500);
    if(!botConnection.removeOrder(entityId))
    {
      Console::errorf("error: Could not remove test order: %s\n", (const char_t*)botConnection.getErrorString());
      return -1;
    }
  }

  /*
  RelayConnection relayConnection;
  MarketConnection marketConnection;
  String channelName = marketConnection.getChannelName();

  class Callback : public Market::Callback
  {
  public:
    virtual bool_t receivedTrade(const Market::Trade& trade)
    {
      if(!relayConnection->sendTrade(trade))
        return false;
      return true;
    }

    virtual bool_t receivedTime(uint64_t time)
    {
      if(!relayConnection->sendServerTime(time))
        return false;
      return true;
    }

    virtual bool_t receivedTicker(const Market::Ticker& ticker)
    {
      if(!relayConnection->sendTicker(ticker))
        return false;
      return true;
    }

    RelayConnection* relayConnection;
  } callback;
  callback.relayConnection = &relayConnection;

  for(;; Thread::sleep(10 * 1000))
  {
    if(!relayConnection.isOpen())
    {
      Console::printf("Connecting to relay server...\n");
      if(!relayConnection.connect(port, channelName))
      {
        Console::printf("Could not connect to relay server: %s\n", (const char_t*)relayConnection.getErrorString());
        continue;
      }
      else
        Console::printf("Connected to relay server.\n");
    }

    if(!marketConnection.isOpen())
    {
      Console::printf("Connecting to %s...\n", (const char_t*)channelName);
      if(!marketConnection.connect())
      {
        Console::printf("Could not connect to %s: %s\n", (const char_t*)channelName, (const char_t*)marketConnection.getErrorString());
        continue;
      }
      else
        Console::printf("Connected to %s.\n", (const char_t*)channelName);
    }

    for(;;)
      if(!marketConnection.process(callback))
        break;

    if(!relayConnection.isOpen())
      Console::printf("Lost connection to relay server: %s\n", (const char_t*)relayConnection.getErrorString());
    if(!marketConnection.isOpen())
      Console::printf("Lost connection to %s: %s\n", (const char_t*)channelName, (const char_t*)marketConnection.getErrorString());
    marketConnection.close(); // reconnect to reload the trade history
  }
  */
  return 0;
}
