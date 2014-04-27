
#include <nstd/File.h>

#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ClientHandler.h"
#include "ServerHandler.h"
#include "User.h"
#include "Session.h"
#include "Engine.h"
#include "MarketAdapter.h"
#include "Transaction.h"
#include "Order.h"
#include "Market.h"

ClientHandler::ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client) : id(id), clientAddr(clientAddr), serverHandler(serverHandler), client(client),
  state(newState), user(0), session(0), market(0) {}

ClientHandler::~ClientHandler()
{
  if(user)
    user->unregisterClient(*this);
  if(session)
  {
    session->unregisterClient(*this);
    if(state == botState)
      session->send();
  }
  if(market)
  {
    market->unregisterClient(*this);
    if(state == marketState)
      market->send();
  }
}

void_t ClientHandler::deselectSession()
{
  session = 0;
  if(state == botState)
    client.close();
}

void_t ClientHandler::deselectMarket()
{
  market = 0;
  if(state == marketState)
    client.close();
}

size_t ClientHandler::handle(byte_t* data, size_t size)
{
  byte_t* pos = data;
  while(size > 0)
  {
    if(size < sizeof(BotProtocol::Header))
      break;
    BotProtocol::Header* header = (BotProtocol::Header*)pos;
    if(header->size < sizeof(BotProtocol::Header) || header->size >= 5000)
    {
      client.close();
      return 0;
    }
    if(size < header->size)
      break;
    handleMessage(*header, pos + sizeof(BotProtocol::Header), header->size - sizeof(BotProtocol::Header));
    pos += header->size;
    size -= header->size;
  }
  if(size >= 5000)
  {
    client.close();
    return 0;
  }
  return pos - data;
}

void_t ClientHandler::handleMessage(const BotProtocol::Header& messageHeader, byte_t* data, size_t size)
{
  switch(state)
  {
  case newState:
    switch((BotProtocol::MessageType)messageHeader.messageType)
    {
    case BotProtocol::loginRequest:
      if(size >= sizeof(BotProtocol::LoginRequest))
        handleLogin(*(BotProtocol::LoginRequest*)data);
      break;
    case BotProtocol::registerBotRequest:
      if(clientAddr == Socket::loopbackAddr && size >= sizeof(BotProtocol::RegisterBotRequest))
        handleRegisterBot(*(BotProtocol::RegisterBotRequest*)data);
      break;
    case BotProtocol::registerMarketRequest:
      if(clientAddr == Socket::loopbackAddr && size >= sizeof(BotProtocol::RegisterMarketRequest))
        handleRegisterMarket(*(BotProtocol::RegisterMarketRequest*)data);
      break;
    default:
      break;
    }
    break;
  case loginState:
    if((BotProtocol::MessageType)messageHeader.messageType == BotProtocol::authRequest)
      if(size >= sizeof(BotProtocol::AuthRequest))
        handleAuth(*(BotProtocol::AuthRequest*)data);
    break;
  case userState:
  case botState:
    switch((BotProtocol::MessageType)messageHeader.messageType)
    {
    case BotProtocol::pingRequest:
      handlePing(data, size);
      break;
    case BotProtocol::createEntity:
      handleCreateEntity((BotProtocol::EntityType)messageHeader.entityType, data, size);
      break;
    case BotProtocol::controlEntity:
      handleControlEntity((BotProtocol::EntityType)messageHeader.entityType, messageHeader.entityId, data, size);
      break;
    case BotProtocol::removeEntity:
      handleRemoveEntity((BotProtocol::EntityType)messageHeader.entityType, messageHeader.entityId);
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleLogin(BotProtocol::LoginRequest& loginRequest)
{
  String username = BotProtocol::getString(loginRequest.username);
  user = serverHandler.findUser(username);
  if(!user)
  {
    sendError("Unknown user.");
    return;
  }

  for(uint32_t* p = (uint32_t*)loginkey, * end = (uint32_t*)(loginkey + 32); p < end; ++p)
    *p = Math::random();

  BotProtocol::LoginResponse loginResponse;
  Memory::copy(loginResponse.userkey, user->getKey(), sizeof(loginResponse.userkey));
  Memory::copy(loginResponse.loginkey, loginkey, sizeof(loginResponse.loginkey));
  sendMessage(BotProtocol::loginResponse, &loginResponse, sizeof(loginResponse));
  state = loginState;
}

void ClientHandler::handleAuth(BotProtocol::AuthRequest& authRequest)
{
  byte_t signature[32];
  Sha256::hmac(loginkey, 32, user->getPwHmac(), 32, signature);
  if(Memory::compare(signature, authRequest.signature, 32) != 0)
  {
    sendError("Incorrect signature.");
    return;
  }

  sendMessage(BotProtocol::authResponse, 0, 0);
  state = userState;
  user->registerClient(*this);

  // send engine list
  {
    BotProtocol::Engine engineData;
    const HashMap<uint32_t, Engine*>& engines = serverHandler.getEngines();
    for(HashMap<uint32_t, Engine*>::Iterator i = engines.begin(), end = engines.end(); i != end; ++i)
    {
      const Engine* engine = *i;
      BotProtocol::setString(engineData.name, engine->getName());
      sendEntity(BotProtocol::engine, engine->getId(), &engineData, sizeof(engineData));
    }
  }

  // send market adapter list
  {
    BotProtocol::MarketAdapter marketAdapterData;
    const HashMap<uint32_t, MarketAdapter*>& marketAdapters = serverHandler.getMarketAdapters();
    for(HashMap<uint32_t, MarketAdapter*>::Iterator i = marketAdapters.begin(), end = marketAdapters.end(); i != end; ++i)
    {
      const MarketAdapter* marketAdapter = *i;
      BotProtocol::setString(marketAdapterData.name, marketAdapter->getName());
      BotProtocol::setString(marketAdapterData.currencyBase, marketAdapter->getCurrencyBase());
      BotProtocol::setString(marketAdapterData.currencyComm, marketAdapter->getCurrencyComm());
      sendEntity(BotProtocol::marketAdapter, marketAdapter->getId(), &marketAdapterData, sizeof(marketAdapterData));
    }
  }

  // send market list
  {
    const HashMap<uint32_t, Market*>& markets = user->getMarkets();
    for(HashMap<uint32_t, Market*>::Iterator i = markets.begin(), end = markets.end(); i != end; ++i)
      (*i)->send(this);
  }

  // send session list
  {
    const HashMap<uint32_t, Session*>& sessions = user->getSessions();
    for(HashMap<uint32_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
      (*i)->send(this);
  }
}

void_t ClientHandler::handleRegisterBot(BotProtocol::RegisterBotRequest& registerBotRequest)
{
  Session* session = serverHandler.findSessionByPid(registerBotRequest.pid);
  if(!session)
  {
    sendError("Unknown session.");
    return;
  }
  if(!session->registerClient(*this, true))
  {
    sendError("Invalid session.");
    return;
  } 

  BotProtocol::RegisterBotResponse response;
  response.isSimulation = session->isSimulation();
  session->getInitialBalance(response.balanceBase, response.balanceComm);
  sendMessage(BotProtocol::registerBotResponse, &response, sizeof(response));
  this->session = session;
  state = botState;

  session->send();
}

void_t ClientHandler::handleRegisterMarket(BotProtocol::RegisterMarketRequest& registerMarketRequest)
{
  Market* market = serverHandler.findMarketByPid(registerMarketRequest.pid);
  if(!market)
  {
    sendError("Unknown market.");
    return;
  }
  if(!market->registerClient(*this, true))
  {
    sendError("Invalid market.");
    return;
  } 

  sendMessage(BotProtocol::registerMarketResponse, 0, 0);
  this->market = market;
  state = marketState;

  market->send();
}

void_t ClientHandler::handlePing(const byte_t* data, size_t size)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = BotProtocol::pingResponse;
  header.entityType = 0;
  header.entityId = 0;
  client.reserve(header.size);
  client.send((const byte_t*)&header, sizeof(header));
  if(size > 0)
    client.send(data, size);
}

void_t ClientHandler::handleCreateEntity(BotProtocol::EntityType type, byte_t* data, size_t size)
{
  switch(state)
  {
  case userState:
    switch(type)
    {
    case BotProtocol::session:
      if(size >= sizeof(BotProtocol::CreateSessionArgs))
        handleCreateSession(*(BotProtocol::CreateSessionArgs*)data);
      break;
    case BotProtocol::market:
      if(size >= sizeof(BotProtocol::CreateMarketArgs))
        handleCreateMarket(*(BotProtocol::CreateMarketArgs*)data);
    default:
      break;
    }
    break;
  case botState:
    switch(type)
    {
    case BotProtocol::transaction:
      if(size >= sizeof(BotProtocol::CreateTransactionArgs))
        handleCreateTransaction(*(BotProtocol::CreateTransactionArgs*)data);
      break;
    case BotProtocol::order:
      if(size >= sizeof(BotProtocol::CreateOrderArgs))
        handleCreateOrder(*(BotProtocol::CreateOrderArgs*)data);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleRemoveEntity(BotProtocol::EntityType type, uint32_t id)
{
  switch(state)
  {
  case userState:
    switch(type)
    {
    case BotProtocol::session:
      handleRemoveSession(id);
      break;
    case BotProtocol::market:
      handleRemoveMarket(id);
    default:
      break;
    }
    break;
  case botState:
    switch(type)
    {
    case BotProtocol::transaction:
      handleRemoveTransaction(id);
      break;
    case BotProtocol::order:
      handleRemoveOrder(id);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleControlEntity(BotProtocol::EntityType type, uint32_t id, byte_t* data, size_t size)
{
  switch(state)
  {
  case userState:
    switch(type)
    {
    case BotProtocol::session:
      if(size >= sizeof(BotProtocol::ControlSessionArgs))
        handleControlSession(id, *(BotProtocol::ControlSessionArgs*)data);
      break;
    default:
      break;
    }
  case botState:
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleCreateMarket(BotProtocol::CreateMarketArgs& createMarketArgs)
{
  MarketAdapter* marketAdapter = serverHandler.findMarketAdapter(createMarketArgs.marketAdapterId);
  if(!marketAdapter)
  {
    sendError("Unknown market adapter.");
    return;
  }
  
  String username = BotProtocol::getString(createMarketArgs.username);
  String key = BotProtocol::getString(createMarketArgs.key);
  String secret = BotProtocol::getString(createMarketArgs.secret);
  Market* market = user->createMarket(*marketAdapter, username, key, secret);
  if(!market)
  {
    sendError("Could not create market.");
    return;
  }
  market->send();
  user->saveData();

  market->start();
  market->send();
}

void_t ClientHandler::handleRemoveMarket(uint32_t id)
{
  // todo: do not remove markts that are in use by a bot session

  if(!user->deleteMarket(id))
  {
    sendError("Unknown market.");
    return;
  }

  user->removeEntity(BotProtocol::market, id);
  user->saveData();
}

void_t ClientHandler::handleCreateSession(BotProtocol::CreateSessionArgs& createSessionArgs)
{
  String name = BotProtocol::getString(createSessionArgs.name);
  Engine* engine = serverHandler.findEngine(createSessionArgs.engineId);
  if(!engine)
  {
    sendError("Unknown engine.");
    return;
  }
  MarketAdapter* marketAdapter = serverHandler.findMarketAdapter(createSessionArgs.marketId);
  if(!marketAdapter)
  {
    sendError("Unknown market.");
    return;
  }

  Session* session = user->createSession(name, *engine, *marketAdapter, createSessionArgs.balanceBase, createSessionArgs.balanceComm);
  if(!session)
  {
    sendError("Could not create session.");
    return;
  }

  session->send();
  user->saveData();
}

void_t ClientHandler::handleRemoveSession(uint32_t id)
{
  if(!user->deleteSession(id))
  {
    sendError("Unknown session.");
    return;
  }

  user->removeEntity(BotProtocol::session, id);
  user->saveData();
}

void_t ClientHandler::handleControlSession(uint32_t id, BotProtocol::ControlSessionArgs& controlSessionArgs)
{
  Session* session = user->findSession(id);
  if(!session)
  {
    sendError("Unknown session.");
    return;
  }

  switch((BotProtocol::ControlSessionArgs::Command)controlSessionArgs.cmd)
  {
  case BotProtocol::ControlSessionArgs::startSimulation:
    session->startSimulation();
    session->send();
    break;
  case BotProtocol::ControlSessionArgs::stop:
    session->stop();
    session->send();
    break;
  case BotProtocol::ControlSessionArgs::select:
    if(this->session)
      this->session->unregisterClient(*this);
    session->registerClient(*this, false);
    this->session = session;
    {
      const HashMap<uint32_t, Transaction*>& transactions = session->getTransactions();
      for(HashMap<uint32_t, Transaction*>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
        (*i)->send(this);
      const HashMap<uint32_t, Order*>& orders = session->getOrders();
      for(HashMap<uint32_t, Order*>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
        (*i)->send(this);
    }
    break;
  }
}

void_t ClientHandler::handleCreateTransaction(BotProtocol::CreateTransactionArgs& createTransactionArgs)
{
  Transaction* transaction = session->createTransaction(createTransactionArgs.price, createTransactionArgs.amount, createTransactionArgs.fee, (BotProtocol::Transaction::Type)createTransactionArgs.type);
  if(!transaction)
  {
    sendError("Could not create transaction.");
    return;
  }

  transaction->send();
  session->saveData();
}

void_t ClientHandler::handleRemoveTransaction(uint32_t id)
{
  if(!session->deleteTransaction(id))
  {
    sendError("Unknown transaction.");
    return;
  }

  session->removeEntity(BotProtocol::transaction, id);
  session->saveData();
}

void_t ClientHandler::handleCreateOrder(BotProtocol::CreateOrderArgs& createOrderArgs)
{
  Order* order = session->createOrder(createOrderArgs.price, createOrderArgs.amount, createOrderArgs.fee, (BotProtocol::Order::Type)createOrderArgs.type);
  if(!order)
  {
    sendError("Could not create order.");
    return;
  }

  order->send();
  session->saveData();
}

void_t ClientHandler::handleRemoveOrder(uint32_t id)
{
  if(!session->deleteOrder(id))
  {
    sendError("Unknown order.");
    return;
  }

  session->removeEntity(BotProtocol::order, id);
  session->saveData();
}

void_t ClientHandler::sendMessage(BotProtocol::MessageType type, const void_t* data, size_t size)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = type;
  header.entityType = 0;
  header.entityId = 0;
  client.reserve(header.size);
  client.send((const byte_t*)&header, sizeof(header));
  if(size > 0)
    client.send((const byte_t*)data, size);
}

void_t ClientHandler::sendEntity(BotProtocol::EntityType type, uint32_t id, const void_t* data, size_t size)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = BotProtocol::updateEntity;
  header.entityType = type;
  header.entityId = id;
  client.reserve(header.size);
  client.send((const byte_t*)&header, sizeof(header));
  if(size > 0)
    client.send((const byte_t*)data, size);
}

void_t ClientHandler::removeEntity(BotProtocol::EntityType type, uint32_t id)
{
  BotProtocol::Header header;
  header.size = sizeof(header);
  header.messageType = BotProtocol::removeEntity;
  header.entityType = type;
  header.entityId = id;
  client.send((const byte_t*)&header, sizeof(header));
}

void_t ClientHandler::sendError(const String& errorMessage)
{
  BotProtocol::Error error;
  BotProtocol::setString(error.errorMessage, errorMessage);
  sendEntity(BotProtocol::error, 0, &error, sizeof(error));
}
