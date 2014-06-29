
#include <nstd/Console.h>
//#include <nstd/String.h>
#include <nstd/Thread.h> // sleep
//#include <nstd/Error.h>

#include "Tools/BotConnection.h"
#include "Tools/HandlerConnection.h"
#include "Tools/DataConnectionHandler.h"
#include "Tools/SimBroker.h"
#include "Tools/LiveBroker.h"

#ifdef BOT_BUYBOT
#include "Bots/BuyBot.h"
typedef BuyBot BotFactory;
const char* botName = "BuyBot";
#endif
#ifdef BOT_ITEMBOT
#include "Bots/ItemBot.h"
typedef ItemBot BotFactory;
const char* botName = "ItemBot";
#endif
#ifdef BOT_TESTBOT
#include "Bots/TestBot.h"
typedef TestBot BotFactory;
const char* botName = "TestBot";
#endif

int_t main(int_t argc, char_t* argv[])
{
  static const uint16_t botPort = 40124;
  static const uint32_t dataIp = Socket::inetAddr("192.168.0.49");
  static const uint16_t dataPort = 40123;

  //for(;;)
  //{
  //  bool stop = true;
  //  if(!stop)
  //    break;
  //}

  // create entity connection with bot server
  BotConnection botConnection;
  if(!botConnection.connect(botPort))
  {
    Console::errorf("error: Could not connect to bot server: %s\n", (const char_t*)botConnection.getErrorString());
    return -1;
  }

  // create handler connection with bot server
  HandlerConnection handlerConnection;
  if(!handlerConnection.connect(botPort))
  {
    Console::errorf("error: Could not connect to bot server: %s\n", (const char_t*)handlerConnection.getErrorString());
    return -1;
  }

  // load some data
  // todo: move this to broker implementation?
  BotProtocol::Balance marketBalance;
  if(!botConnection.getMarketBalance(marketBalance))
  {
    Console::errorf("error: Could not retrieve market balance: %s\n", (const char_t*)botConnection.getErrorString());
    return -1;
  }
#ifdef BOT_TESTBOT
  BotProtocol::Transaction transaction;
  transaction.entityType = BotProtocol::sessionTransaction;
  transaction.type = BotProtocol::Transaction::buy;
  transaction.date = 89;
  transaction.price = 300;
  transaction.amount = 0.02;
  transaction.fee = 0.05;
  botConnection.createSessionTransaction(transaction);
#endif
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
  BotProtocol::Balance sessionBalance;
  if(!botConnection.getSessionBalance(sessionBalance))
  {
    Console::errorf("error: Could not retrieve session balance: %s\n", (const char_t*)botConnection.getErrorString());
    return -1;
  }
  sessionBalance.fee = marketBalance.fee;

  // create broker
  Broker* broker = handlerConnection.isSimulation() ? (Broker*)new SimBroker(botConnection, sessionBalance) :
    (Broker*)new LiveBroker(botConnection, sessionBalance);
  for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
    broker->loadTransaction(*i);
  for(List<BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
    broker->loadOrder(*i);

  // create bot session
  BotFactory botFactory;
  Bot::Session* session = botFactory.createSession(*broker);
  if(!session)
  {
    Console::errorf("error: Could not create bot session.");
    return -1;
  }
  broker->setBotSession(*session);

  // receive and handle trade data
  DataConnectionHandler dataConnection(botConnection, *broker, *session, handlerConnection.isSimulation());
  String marketAdapterName = handlerConnection.getMarketAdapterName();
  for(;; Thread::sleep(10 * 1000))
  {
    if(!dataConnection.connect(dataIp, dataPort))
      continue;
    if(!dataConnection.subscribe(marketAdapterName))
      continue;
    for(;;)
    {
      if(!dataConnection.process())
        break;
    }
  }

  return 0;
}
