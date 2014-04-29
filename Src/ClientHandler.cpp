
#include <nstd/File.h>
#include <nstd/Debug.h>

#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ClientHandler.h"
#include "ServerHandler.h"
#include "User.h"
#include "Session.h"
#include "BotEngine.h"
#include "MarketAdapter.h"
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
      if(size >= sizeof(BotProtocol::Entity))
        handleCreateEntity(*(BotProtocol::Entity*)data, size);
      break;
    case BotProtocol::controlEntity:
      if(size >= sizeof(BotProtocol::Entity))
        handleControlEntity(*(BotProtocol::Entity*)data, size);
      break;
    case BotProtocol::removeEntity:
      if(size >= sizeof(BotProtocol::Entity))
        handleRemoveEntity(*(const BotProtocol::Entity*)data);
      break;
    default:
      break;
    }
    break;
  case marketState:
    switch((BotProtocol::MessageType)messageHeader.messageType)
    {
    case BotProtocol::pingRequest:
      handlePing(data, size);
      break;
    case BotProtocol::updateEntity:
      if(size >= sizeof(BotProtocol::Entity))
        handleUpdateEntity(*(BotProtocol::Entity*)data, size);
      break;
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
  String userName = BotProtocol::getString(loginRequest.userName);
  user = serverHandler.findUser(userName);
  if(!user)
  {
    sendError("Unknown user.");
    return;
  }

  for(uint32_t* p = (uint32_t*)loginkey, * end = (uint32_t*)(loginkey + 32); p < end; ++p)
    *p = Math::random();

  BotProtocol::LoginResponse loginResponse;
  Memory::copy(loginResponse.userKey, user->getKey(), sizeof(loginResponse.userKey));
  Memory::copy(loginResponse.loginKey, loginkey, sizeof(loginResponse.loginKey));
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
    const HashMap<uint32_t, BotEngine*>& botEngines = serverHandler.getBotEngines();
    for(HashMap<uint32_t, BotEngine*>::Iterator i = botEngines.begin(), end = botEngines.end(); i != end; ++i)
      (*i)->send(*this);
  }

  // send market adapter list
  {
    const HashMap<uint32_t, MarketAdapter*>& marketAdapters = serverHandler.getMarketAdapters();
    for(HashMap<uint32_t, MarketAdapter*>::Iterator i = marketAdapters.begin(), end = marketAdapters.end(); i != end; ++i)
      (*i)->send(*this);
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
  response.simulation = session->isSimulation();
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

  BotProtocol::RegisterMarketResponse response;
  BotProtocol::setString(response.userName, market->getUserName());
  BotProtocol::setString(response.key, market->getKey());
  BotProtocol::setString(response.secret, market->getSecret());
  sendMessage(BotProtocol::registerMarketResponse, &response, sizeof(response));

  this->market = market;
  state = marketState;

  market->send();
}

void_t ClientHandler::handlePing(const byte_t* data, size_t size)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = BotProtocol::pingResponse;
  client.reserve(header.size);
  client.send((const byte_t*)&header, sizeof(header));
  if(size > 0)
    client.send(data, size);
}

void_t ClientHandler::handleCreateEntity(BotProtocol::Entity& entity, size_t size)
{
  switch(state)
  {
  case userState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::session:
      if(size >= sizeof(BotProtocol::CreateSessionArgs))
        handleCreateSession(*(BotProtocol::CreateSessionArgs*)&entity);
      break;
    case BotProtocol::market:
      if(size >= sizeof(BotProtocol::CreateMarketArgs))
        handleCreateMarket(*(BotProtocol::CreateMarketArgs*)&entity);
    default:
      break;
    }
    break;
  case botState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::sessionTransaction:
      if(size >= sizeof(BotProtocol::CreateTransactionArgs))
        handleCreateSessionTransaction(*(BotProtocol::CreateTransactionArgs*)&entity);
      break;
    case BotProtocol::sessionOrder:
      if(size >= sizeof(BotProtocol::CreateOrderArgs))
        handleCreateSessionOrder(*(BotProtocol::CreateOrderArgs*)&entity);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleRemoveEntity(const BotProtocol::Entity& entity)
{
  switch(state)
  {
  case userState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::session:
      handleRemoveSession(entity.entityId);
      break;
    case BotProtocol::market:
      handleRemoveMarket(entity.entityId);
    default:
      break;
    }
    break;
  case botState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::sessionTransaction:
      handleRemoveSessionTransaction(entity.entityId);
      break;
    case BotProtocol::sessionOrder:
      handleRemoveSessionOrder(entity.entityId);
      break;
    default:
      break;
    }
    break;
  case marketState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketTransaction:
      handleRemoveMarketTransaction(entity.entityId);
      break;
    case BotProtocol::marketOrder:
      handleRemoveMarketOrder(entity.entityId);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleControlEntity(BotProtocol::Entity& entity, size_t size)
{
  switch(state)
  {
  case userState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::session:
      if(size >= sizeof(BotProtocol::ControlSessionArgs))
        handleControlSession(*(BotProtocol::ControlSessionArgs*)&entity);
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

void_t ClientHandler::handleUpdateEntity(BotProtocol::Entity& entity, size_t size)
{
  switch(state)
  {
  case marketState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketTransaction:
      if(size >= sizeof(BotProtocol::Transaction))
        handleUpdateMarketTransaction(*(BotProtocol::Transaction*)&entity);
      break;
    case BotProtocol::marketOrder:
      if(size >= sizeof(BotProtocol::Order))
        handleUpdateMarketOrder(*(BotProtocol::Order*)&entity);
      break;
    default:
      break;
    }
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
  
  String username = BotProtocol::getString(createMarketArgs.userName);
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

void_t ClientHandler::handleControlMarket(BotProtocol::ControlMarketArgs& controlMarketArgs)
{
  Market* market = user->findMarket(controlMarketArgs.entityId);
  if(!market)
  {
    sendError("Unknown market.");
    return;
  }

  switch((BotProtocol::ControlMarketArgs::Command)controlMarketArgs.cmd)
  {
  case BotProtocol::ControlMarketArgs::select:
    if(this->market)
      this->market->unregisterClient(*this);
    market->registerClient(*this, false);
    this->market = market;
    {
      const HashMap<uint32_t, BotProtocol::Transaction>& transactions = market->getTransactions();
      for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Transaction));
      const HashMap<uint32_t, BotProtocol::Order>& orders = market->getOrders();
      for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Order));
    }
    break;
  case BotProtocol::ControlMarketArgs::refreshTransactions:
  case BotProtocol::ControlMarketArgs::refreshOrders:
    {
      ClientHandler* adapterClient = market->getAdapaterClient();
      if(adapterClient)
        adapterClient->sendMessage(BotProtocol::controlEntity, &controlMarketArgs, sizeof(controlMarketArgs));
    }
  }
}

void_t ClientHandler::handleCreateSession(BotProtocol::CreateSessionArgs& createSessionArgs)
{
  String name = BotProtocol::getString(createSessionArgs.name);
  BotEngine* botEngine = serverHandler.findBotEngine(createSessionArgs.botEngineId);
  if(!botEngine)
  {
    sendError("Unknown bot engine.");
    return;
  }

  // todo: use market here!
  MarketAdapter* marketAdapter = serverHandler.findMarketAdapter(createSessionArgs.marketId);
  if(!marketAdapter)
  {
    sendError("Unknown market.");
    return;
  }

  Session* session = user->createSession(name, *botEngine, *marketAdapter, createSessionArgs.balanceBase, createSessionArgs.balanceComm);
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

void_t ClientHandler::handleControlSession(BotProtocol::ControlSessionArgs& controlSessionArgs)
{
  Session* session = user->findSession(controlSessionArgs.entityId);
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
      const HashMap<uint32_t, BotProtocol::Transaction>& transactions = session->getTransactions();
      for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Transaction));
      const HashMap<uint32_t, BotProtocol::Order>& orders = session->getOrders();
      for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Order));
    }
    break;
  }
}

void_t ClientHandler::handleCreateSessionTransaction(BotProtocol::CreateTransactionArgs& createTransactionArgs)
{
  BotProtocol::Transaction* transaction = session->createTransaction(createTransactionArgs.price, createTransactionArgs.amount, createTransactionArgs.fee, (BotProtocol::Transaction::Type)createTransactionArgs.type);
  if(!transaction)
  {
    sendError("Could not create transaction.");
    return;
  }

  session->sendEntity(transaction, sizeof(BotProtocol::Transaction));
  session->saveData();
}

void_t ClientHandler::handleRemoveSessionTransaction(uint32_t id)
{
  if(!session->deleteTransaction(id))
  {
    sendError("Unknown transaction.");
    return;
  }

  session->removeEntity(BotProtocol::sessionTransaction, id);
  session->saveData();
}

void_t ClientHandler::handleCreateSessionOrder(BotProtocol::CreateOrderArgs& createOrderArgs)
{
  BotProtocol::Order* order = session->createOrder(createOrderArgs.price, createOrderArgs.amount, createOrderArgs.fee, (BotProtocol::Order::Type)createOrderArgs.type);
  if(!order)
  {
    sendError("Could not create order.");
    return;
  }

  session->sendEntity(order, sizeof(BotProtocol::Order));
  session->saveData();
}

void_t ClientHandler::handleRemoveSessionOrder(uint32_t id)
{
  if(!session->deleteOrder(id))
  {
    sendError("Unknown order.");
    return;
  }

  session->removeEntity(BotProtocol::sessionOrder, id);
  session->saveData();
}

void_t ClientHandler::handleUpdateMarketTransaction(BotProtocol::Transaction& transaction)
{
  market->updateTransaction(transaction);
  market->sendEntity(&transaction, sizeof(BotProtocol::Transaction));
}

void_t ClientHandler::handleRemoveMarketTransaction(uint32_t id)
{
  market->deleteTransaction(id);
  market->removeEntity(BotProtocol::marketTransaction, id);
}

void_t ClientHandler::handleUpdateMarketOrder(BotProtocol::Order& order)
{
  market->updateOrder(order);
  market->sendEntity(&order, sizeof(BotProtocol::Order));
}

void_t ClientHandler::handleRemoveMarketOrder(uint32_t id)
{
  market->deleteOrder(id);
  market->removeEntity(BotProtocol::marketOrder, id);
}

void_t ClientHandler::sendMessage(BotProtocol::MessageType type, const void_t* data, size_t size)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = type;
  client.reserve(header.size);
  client.send((const byte_t*)&header, sizeof(header));
  if(size > 0)
    client.send((const byte_t*)data, size);
}

void_t ClientHandler::sendEntity(const void_t* data, size_t size)
{
  ASSERT(size >= sizeof(BotProtocol::Entity));
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.messageType = BotProtocol::updateEntity;
  client.reserve(header.size);
  client.send((const byte_t*)&header, sizeof(header));
  client.send((const byte_t*)data, size);
}

void_t ClientHandler::removeEntity(BotProtocol::EntityType type, uint32_t id)
{
  byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::Entity)];
  BotProtocol::Header* header = (BotProtocol::Header*)message;
  BotProtocol::Entity* entity = (BotProtocol::Entity*)(header + 1);
  header->size = sizeof(message);
  header->messageType = BotProtocol::removeEntity;
  entity->entityType = type;
  entity->entityId = id;
  client.send(message, sizeof(message));
}

void_t ClientHandler::sendError(const String& errorMessage)
{
  BotProtocol::Error error;
  error.entityType = BotProtocol::error;
  error.entityId = 0;
  BotProtocol::setString(error.errorMessage, errorMessage);
  sendEntity(&error, sizeof(error));
}
