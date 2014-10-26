
#include <nstd/Thread.h>
#include <nstd/Time.h>

#include "Tools/ConnectionHandler.h"
#include "Tools/SimBroker.h"
#include "Tools/LiveBroker.h"

#ifdef BOT_BETBOT
#include "Bots/BetBot.h"
typedef BetBot BotFactory;
const char* botName = "BetBot";
#endif
#ifdef BOT_FLIPBOT
#include "Bots/FlipBot.h"
typedef FlipBot BotFactory;
const char* botName = "FlipBot";
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

  // load session data
  List<BotProtocol::Transaction> sessionTransactions;
  List<BotProtocol::SessionAsset> sessionAssets;
  List<BotProtocol::Order> sessionOrders;
  BotProtocol::Balance sessionBalance;
  List<BotProtocol::SessionProperty> sessionProperties;
  if(!botConnection.getSessionTransactions(sessionTransactions) ||
     !botConnection.getSessionAssets(sessionAssets) ||
     !botConnection.getSessionOrders(sessionOrders) ||
     !botConnection.getSessionProperties(sessionProperties))
  {
    error = botConnection.getErrorString();
    return false;
  }

  // create broker
  BotFactory botFactory;
  maxTradeAge = botFactory.getMaxTradeAge();
  if(handlerConnection.isSimulation())
  {
    // get market fee
    BotProtocol::Balance marketBalance;
    if(!botConnection.getMarketBalance(marketBalance))
    {
      error = botConnection.getErrorString();
      return false;
    }

    //
    broker = new SimBroker(botConnection, handlerConnection.getCurrencyBase(), handlerConnection.getCurrencyComm(), marketBalance.fee, sessionBalance, sessionTransactions, sessionAssets, sessionOrders, sessionProperties, maxTradeAge);
  }
  else
    broker = new LiveBroker(botConnection, handlerConnection.getCurrencyBase(), handlerConnection.getCurrencyComm(), sessionBalance, sessionTransactions, sessionAssets, sessionOrders, sessionProperties);

  // instantiate bot implementation
  botSession = botFactory.createSession(*broker);

  return true;
}

bool_t ConnectionHandler::process()
{
  for(;; Thread::sleep(10 * 1000))
  {
    if(!dataConnection.connect(dataIp, dataPort))
      continue;
    if(!dataConnection.subscribe(handlerConnection.getMarketAdapterName(), lastReceivedTradeId,
      handlerConnection.isSimulation() ? (6ULL * 31ULL * 24ULL * 60ULL * 60ULL * 1000ULL) : (maxTradeAge + 10 * 60 * 1000)))
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

void_t ConnectionHandler::handleMessage(const BotProtocol::Header& header, byte_t* data, size_t size)
{
  switch((BotProtocol::MessageType)header.messageType)
  {
  case BotProtocol::createEntity:
    if(size >= sizeof(BotProtocol::Entity))
      handleCreateEntity(header.requestId, *(BotProtocol::Entity*)data, size);
    break;
  case BotProtocol::updateEntity:
    if(size >= sizeof(BotProtocol::Entity))
      handleUpdateEntity(header.requestId, *(BotProtocol::Entity*)data, size);
    break;
  case BotProtocol::removeEntity:
    if(size >= sizeof(BotProtocol::Entity))
      handleRemoveEntity(header.requestId, *(const BotProtocol::Entity*)data);
    break;
  case BotProtocol::controlEntity:
    if(size >= sizeof(BotProtocol::Entity)) 
      handleControlEntity(header.requestId, *(BotProtocol::Entity*)data, size);
    break;
  default:
    break;
  }
}

void_t ConnectionHandler::handleCreateEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size)
{
  switch((BotProtocol::EntityType)entity.entityType)
  {
  case BotProtocol::sessionAsset:
    if(size >= sizeof(BotProtocol::SessionAsset))
      handleCreateSessionAsset(requestId, *(BotProtocol::SessionAsset*)&entity);
    break;
  default:
    break;
  }
}

void_t ConnectionHandler::handleUpdateEntity(uint32_t requestId, const BotProtocol::Entity& entity, size_t size)
{
  switch((BotProtocol::EntityType)entity.entityType)
  {
  case BotProtocol::sessionAsset:
    if(size >= sizeof(BotProtocol::SessionAsset))
      handleUpdateSessionAsset(requestId, *(BotProtocol::SessionAsset*)&entity);
    break;
  case BotProtocol::sessionProperty:
    if(size >= sizeof(BotProtocol::SessionProperty))
      handleUpdateSessionProperty(requestId, *(BotProtocol::SessionProperty*)&entity);
    break;
  default:
    break;
  }
}

void_t ConnectionHandler::handleRemoveEntity(uint32_t requestId, const BotProtocol::Entity& entity)
{
  switch((BotProtocol::EntityType)entity.entityType)
  {
  case BotProtocol::sessionAsset:
    handleRemoveSessionAsset(requestId, entity);
    break;
  default:
    break;
  }
}

void_t ConnectionHandler::handleControlEntity(uint32_t requestId, BotProtocol::Entity& entity, size_t size)
{
  //switch((BotProtocol::EntityType)entity.entityType)
  //{
  //  // do something
  //default:
  //  break;
  //}
}

void_t ConnectionHandler::handleCreateSessionAsset(uint32_t requestId, BotProtocol::SessionAsset& sessionAsset)
{
  if(!broker->createAsset(sessionAsset))
    handlerConnection.sendErrorResponse(BotProtocol::createEntity, requestId, &sessionAsset, broker->getLastError());
  else
    handlerConnection.sendMessage(BotProtocol::createEntityResponse, requestId, &sessionAsset, sizeof(sessionAsset));
}

void_t ConnectionHandler::handleUpdateSessionAsset(uint32_t requestId, const BotProtocol::SessionAsset& sessionAsset)
{
  const BotProtocol::SessionAsset* asset = broker->getAsset(sessionAsset.entityId);
  if(!asset)
    handlerConnection.sendErrorResponse(BotProtocol::updateEntity, requestId, &sessionAsset, "Could not find session item.");
  else
  {
    BotProtocol::SessionAsset updatedAsset = *asset;
    updatedAsset.flipPrice = sessionAsset.flipPrice;
    broker->updateAsset(updatedAsset);
    handlerConnection.sendMessage(BotProtocol::updateEntityResponse, requestId, &sessionAsset, sizeof(BotProtocol::Entity));

    botSession->handleAssetUpdate(updatedAsset);
  }
}

void_t ConnectionHandler::handleRemoveSessionAsset(uint32_t requestId, const BotProtocol::Entity& entity)
{
  const BotProtocol::SessionAsset* asset = broker->getAsset(entity.entityId);
  if(!asset)
    handlerConnection.sendErrorResponse(BotProtocol::removeEntity, requestId, &entity, "Could not find session item.");
  else
  {
    BotProtocol::SessionAsset removedAsset = *asset;

    broker->removeAsset(entity.entityId);
    handlerConnection.sendMessage(BotProtocol::removeEntityResponse, requestId, &entity, sizeof(entity));

    botSession->handleAssetRemoval(removedAsset);
  }
}

void_t ConnectionHandler::handleUpdateSessionProperty(uint32_t requestId, BotProtocol::SessionProperty& sessionProperty)
{
  const BotProtocol::SessionProperty* property = broker->getProperty(sessionProperty.entityId);
  if(!property)
    handlerConnection.sendErrorResponse(BotProtocol::updateEntity, requestId, &sessionProperty, "Could not find session property.");
  else
  {
    if(property->flags & BotProtocol::SessionProperty::readOnly)
    {
      handlerConnection.sendErrorResponse(BotProtocol::updateEntity, requestId, &sessionProperty, "Property is not editable.");
      return;
    }

    BotProtocol::SessionProperty updatedProperty = *property;
    BotProtocol::setString(updatedProperty.value, BotProtocol::getString(sessionProperty.value));
    broker->updateProperty(updatedProperty);
    handlerConnection.sendMessage(BotProtocol::updateEntityResponse, requestId, &updatedProperty, sizeof(BotProtocol::Entity));

    botSession->handlePropertyUpdate(updatedProperty);
  }
}
