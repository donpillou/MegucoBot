
#include <nstd/Thread.h>

#include "Tools/ConnectionHandler.h"
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

bool_t ConnectionHandler::connect(uint16_t botPort, uint32_t dataIp, uint16_t dataPort)
{
  // close current connections
  botConnection.close();
  handlerConnection.close();
  dataConnection.close();
  delete broker;
  broker = 0;
  delete botSession;
  botSession = 0;

  // store data server address
  this->dataIp = dataIp;
  this->dataPort = dataPort;

  // create session entity connection
  if(!botConnection.connect(botPort))
  {
    error = botConnection.getErrorString();
    return false;
  }

  // create session handler connection
  if(!handlerConnection.connect(botPort))
  {
    error = handlerConnection.getErrorString();
    return false;
  }
  sessionHandlerSocket = &handlerConnection.getSocket();

  // some testing 
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

  // load session data
  List<BotProtocol::Transaction> sessionTransactions;
  List<BotProtocol::SessionItem> sessionItems;
  List<BotProtocol::Order> sessionOrders;
  BotProtocol::Balance sessionBalance;
  if(!botConnection.getSessionTransactions(sessionTransactions) ||
     !botConnection.getSessionItems(sessionItems) ||
     !botConnection.getSessionOrders(sessionOrders) ||
     !botConnection.getSessionBalance(sessionBalance))
  {
    error = botConnection.getErrorString();
    return false;
  }

  // update session fee
  BotProtocol::Balance marketBalance;
  if(!botConnection.getMarketBalance(marketBalance))
  {
    error = botConnection.getErrorString();
    return false;
  }
  if(sessionBalance.fee != marketBalance.fee)
  {
    sessionBalance.fee = marketBalance.fee;
    if(!botConnection.updateSessionBalance(sessionBalance))
    {
      error = botConnection.getErrorString();
      return false;
    }
  }

  // create broker
  if(handlerConnection.isSimulation())
    broker = new SimBroker(botConnection, sessionBalance, sessionTransactions, sessionItems, sessionOrders);
  else
    broker = new LiveBroker(botConnection, sessionBalance, sessionTransactions, sessionItems, sessionOrders);

  // instantiate bot implementation
  BotFactory botFactory;
  botSession = botFactory.createSession(*broker);

  return true;
}

bool_t ConnectionHandler::process()
{
  for(;; Thread::sleep(10 * 1000))
  {
    if(!dataConnection.connect(dataIp, dataPort))
      continue;
    if(!dataConnection.subscribe(handlerConnection.getMarketAdapterName(), lastReceivedTradeId))
      continue;
    Socket::Selector selector;
    Socket* dataSocket = &dataConnection.getSocket();
    selector.set(*dataSocket, Socket::Selector::readEvent);
    selector.set(*sessionHandlerSocket, Socket::Selector::readEvent);
    Socket* selectedSocket;
    uint_t events;
    for(;;)
    {
      if(!selector.select(selectedSocket, events, 10 * 60 * 1000))
      {
        error = Socket::getLastErrorString();
        return false;
      }
      if(selectedSocket == dataSocket)
      {
        if(!dataConnection.process(*this))
          goto dataReconnect;
      }
      else if(selectedSocket == sessionHandlerSocket)
      {
        if(!handlerConnection.process(*this))
        {
          error = handlerConnection.getErrorString();
          return false;
        }
      }
    }
  dataReconnect: ;
  }
}

void ConnectionHandler::receivedTrade(uint64_t channelId, const DataProtocol::Trade& trade)
{
  lastReceivedTradeId = trade.id;
  broker->handleTrade(*botSession, trade);
}

void_t ConnectionHandler::receivedControlEntity(BotProtocol::Entity& entity, size_t size)
{
  // do something
}
