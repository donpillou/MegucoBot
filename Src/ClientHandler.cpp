
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

ClientHandler::ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client) :
  __id(id), clientAddr(clientAddr), serverHandler(serverHandler), client(client),
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
    case BotProtocol::updateEntity:
      if(size >= sizeof(BotProtocol::Entity))
        handleUpdateEntity(*(BotProtocol::Entity*)data, size);
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
    case BotProtocol::removeEntity:
      if(size >= sizeof(BotProtocol::Entity))
        handleRemoveEntity(*(const BotProtocol::Entity*)data);
      break;
    case BotProtocol::createEntityResponse:
      if(size >= sizeof(BotProtocol::CreateEntityResponse))
        handleCreateEntityResponse(*(BotProtocol::CreateEntityResponse*)data);
      break;
    case BotProtocol::errorResponse:
      if(size >= sizeof(BotProtocol::ErrorResponse))
        handleErrorResponse(*(BotProtocol::ErrorResponse*)data);
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
    sendErrorResponse(BotProtocol::loginRequest, loginRequest, "Unknown user.");
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
    sendErrorResponse(BotProtocol::authRequest, authRequest, "Incorrect signature.");
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
    sendErrorResponse(BotProtocol::registerBotRequest, registerBotRequest, "Unknown session.");
    return;
  }
  if(!session->registerClient(*this, true))
  {
    sendErrorResponse(BotProtocol::registerBotRequest, registerBotRequest, "Invalid session.");
    return;
  } 

  BotProtocol::RegisterBotResponse response;
  response.sessionId = session->getId();
  BotProtocol::setString(response.marketAdapterName, session->getMarket()->getMarketAdapter()->getName());
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
    sendErrorResponse(BotProtocol::registerMarketRequest, registerMarketRequest, "Unknown market.");
    return;
  }
  if(!market->registerClient(*this, true))
  {
    sendErrorResponse(BotProtocol::registerMarketRequest, registerMarketRequest, "Invalid market.");
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

  // request balance
  BotProtocol::ControlMarket controlMarket;
  controlMarket.entityType = BotProtocol::market;
  controlMarket.entityId = market->getId();
  controlMarket.cmd = BotProtocol::ControlMarket::refreshBalance;
  sendMessage(BotProtocol::controlEntity, &controlMarket, sizeof(controlMarket));
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
      if(size >= sizeof(BotProtocol::Session))
        handleUserCreateSession(*(BotProtocol::Session*)&entity);
      break;
    case BotProtocol::market:
      if(size >= sizeof(BotProtocol::Market))
        handleUserCreateMarket(*(BotProtocol::Market*)&entity);
      break;
    case BotProtocol::marketOrder:
      if(size >= sizeof(BotProtocol::Order))
        handleUserCreateMarketOrder(*(BotProtocol::Order*)&entity);
      break;
    default:
      break;
    }
    break;
  case botState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::sessionTransaction:
      if(size >= sizeof(BotProtocol::Transaction))
        handleBotCreateSessionTransaction(*(BotProtocol::Transaction*)&entity);
      break;
    case BotProtocol::sessionOrder:
      if(size >= sizeof(BotProtocol::Order))
        handleBotCreateSessionOrder(*(BotProtocol::Order*)&entity);
      break;
    case BotProtocol::sessionLogMessage:
      if(size >= sizeof(BotProtocol::Order))
        handleBotCreateSessionLogMessage(*(BotProtocol::SessionLogMessage*)&entity);
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
      handleUserRemoveSession(entity);
      break;
    case BotProtocol::market:
      handleUserRemoveMarket(entity);
      break;
    case BotProtocol::marketOrder:
      handleUserRemoveMarketOrder(entity);
      break;
    default:
      break;
    }
    break;
  case botState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::sessionTransaction:
      handleBotRemoveSessionTransaction(entity);
      break;
    case BotProtocol::sessionOrder:
      handleBotRemoveSessionOrder(entity);
      break;
    default:
      break;
    }
    break;
  case marketState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketTransaction:
      handleMarketRemoveMarketTransaction(entity.entityId);
      break;
    case BotProtocol::marketOrder:
      handleMarketRemoveMarketOrder(entity.entityId);
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
      if(size >= sizeof(BotProtocol::ControlSession))
        handleUserControlSession(*(BotProtocol::ControlSession*)&entity);
      break;
    case BotProtocol::market:
      if(size >= sizeof(BotProtocol::ControlMarket))
        handleUserControlMarket(*(BotProtocol::ControlMarket*)&entity);
      break;
    default:
      break;
    }
  case botState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::session:
      if(size >= sizeof(BotProtocol::ControlSession))
        handleBotControlSession(*(BotProtocol::ControlSession*)&entity);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleUpdateEntity(BotProtocol::Entity& entity, size_t size)
{
  switch(state)
  {
  case userState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketOrder:
      if(size >= sizeof(BotProtocol::Order))
        handleUserUpdateMarketOrder(*(BotProtocol::Order*)&entity);
      break;
    default:
      break;
    }
    break;
  case marketState:
    switch((BotProtocol::EntityType)entity.entityType)
    {
    case BotProtocol::marketTransaction:
      if(size >= sizeof(BotProtocol::Transaction))
        handleMarketUpdateMarketTransaction(*(BotProtocol::Transaction*)&entity);
      break;
    case BotProtocol::marketOrder:
      if(size >= sizeof(BotProtocol::Order))
        handleMarketUpdateMarketOrder(*(BotProtocol::Order*)&entity);
      break;
    case BotProtocol::marketBalance:
      if(size >= sizeof(BotProtocol::MarketBalance))
        handleMarketUpdateMarketBalance(*(BotProtocol::MarketBalance*)&entity);
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleCreateEntityResponse(BotProtocol::CreateEntityResponse& response)
{
  switch(state)
  {
  case marketState:
    {
      uint32_t userRequestId;
      ClientHandler* client;
      if(!market->removeRequestId((BotProtocol::EntityType)response.entityType, response.entityId, userRequestId, client))
        return;
      response.entityId = userRequestId;
      client->sendMessage(BotProtocol::createEntityResponse, &response, sizeof(response));
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleErrorResponse(BotProtocol::ErrorResponse& errorResponse)
{
  switch(state)
  {
  case marketState:
    {
      uint32_t userRequestId;
      ClientHandler* client;
      if(!market->removeRequestId((BotProtocol::EntityType)errorResponse.entityType, errorResponse.entityId, userRequestId, client))
        return;
      errorResponse.entityId = userRequestId;
      client->sendMessage(BotProtocol::errorResponse, &errorResponse, sizeof(errorResponse));
    }
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleUserCreateMarket(BotProtocol::Market& createMarketArgs)
{
  MarketAdapter* marketAdapter = serverHandler.findMarketAdapter(createMarketArgs.marketAdapterId);
  if(!marketAdapter)
  {
    sendErrorResponse(BotProtocol::createEntity, createMarketArgs, "Unknown market adapter.");
    return;
  }
  
  String username = BotProtocol::getString(createMarketArgs.userName);
  String key = BotProtocol::getString(createMarketArgs.key);
  String secret = BotProtocol::getString(createMarketArgs.secret);
  Market* market = user->createMarket(*marketAdapter, username, key, secret);
  if(!market)
  {
    sendErrorResponse(BotProtocol::createEntity, createMarketArgs, "Could not create market.");
    return;
  }

  BotProtocol::CreateEntityResponse response;
  response.entityType = BotProtocol::market;
  response.entityId = createMarketArgs.entityId;
  response.id = market->getId();
  sendMessage(BotProtocol::createEntityResponse, &response, sizeof(response));

  market->send();
  user->saveData();

  market->start();
  market->send();
}

void_t ClientHandler::handleUserRemoveMarket(const BotProtocol::Entity& entity)
{
  // todo: do not remove markts that are in use by a bot session

  if(!user->deleteMarket(entity.entityId))
  {
    sendErrorResponse(BotProtocol::removeEntity, entity, "Unknown market.");
    return;
  }

  user->removeEntity(BotProtocol::market, entity.entityId);
  user->saveData();
}

void_t ClientHandler::handleUserControlMarket(BotProtocol::ControlMarket& controlMarket)
{
  BotProtocol::ControlMarketResponse response;
  response.entityType = BotProtocol::market;
  response.entityId = controlMarket.entityId;
  response.cmd = controlMarket.cmd;

  Market* market = user->findMarket(controlMarket.entityId);
  if(!market)
  {
    sendErrorResponse(BotProtocol::controlEntity, controlMarket, "Unknown market.");
    return;
  }

  switch((BotProtocol::ControlMarket::Command)controlMarket.cmd)
  {
  case BotProtocol::ControlMarket::select:
    if(this->market)
      this->market->unregisterClient(*this);
    market->registerClient(*this, false);
    this->market = market;
    sendMessage(BotProtocol::controlEntityResponse, &response, sizeof(response));
    {
      const BotProtocol::MarketBalance& balance = market->getBalance();
      if(balance.entityType == BotProtocol::marketBalance)
        sendEntity(&balance, sizeof(balance));
      const HashMap<uint32_t, BotProtocol::Transaction>& transactions = market->getTransactions();
      for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Transaction));
      const HashMap<uint32_t, BotProtocol::Order>& orders = market->getOrders();
      for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Order));
    }
    break;
  case BotProtocol::ControlMarket::refreshTransactions:
  case BotProtocol::ControlMarket::refreshOrders:
  case BotProtocol::ControlMarket::refreshBalance:
    {
      ClientHandler* adapterClient = market->getAdapaterClient();
      if(!adapterClient)
      {
        sendErrorResponse(BotProtocol::controlEntity, controlMarket, "Invalid market state.");
        return;
      }
      adapterClient->sendMessage(BotProtocol::controlEntity, &controlMarket, sizeof(controlMarket));
      sendMessage(BotProtocol::controlEntityResponse, &response, sizeof(response));
      // todo: do not send response here.. create request id here and wait for a response from adapter client
    }
    break;
  }
}

void_t ClientHandler::handleUserCreateSession(BotProtocol::Session& createSessionArgs)
{
  String name = BotProtocol::getString(createSessionArgs.name);
  BotEngine* botEngine = serverHandler.findBotEngine(createSessionArgs.botEngineId);
  if(!botEngine)
  {
    sendErrorResponse(BotProtocol::createEntity, createSessionArgs, "Unknown bot engine.");
    return;
  }

  Market* market = user->findMarket(createSessionArgs.marketId);
  if(!market)
  {
    sendErrorResponse(BotProtocol::createEntity, createSessionArgs, "Unknown market.");
    return;
  }

  Session* session = user->createSession(name, *botEngine, *market, createSessionArgs.balanceBase, createSessionArgs.balanceComm);
  if(!session)
  {
    sendErrorResponse(BotProtocol::createEntity, createSessionArgs, "Could not create session.");
    return;
  }

  BotProtocol::CreateEntityResponse response;
  response.entityType = BotProtocol::session;
  response.entityId = createSessionArgs.entityId;
  response.id = session->getId();
  sendMessage(BotProtocol::createEntityResponse, &response, sizeof(response));

  session->send();
  user->saveData();
}

void_t ClientHandler::handleUserRemoveSession(const BotProtocol::Entity& entity)
{
  if(!user->deleteSession(entity.entityId))
  {
    sendErrorResponse(BotProtocol::removeEntity, entity, "Unknown session.");
    return;
  }

  user->removeEntity(BotProtocol::session, entity.entityId);
  user->saveData();
}

void_t ClientHandler::handleUserControlSession(BotProtocol::ControlSession& controlSession)
{
  BotProtocol::ControlSessionResponse response;
  response.entityType = BotProtocol::session;
  response.entityId = controlSession.entityId;
  response.cmd = controlSession.cmd;

  Session* session = user->findSession(controlSession.entityId);
  if(!session)
  {
    sendErrorResponse(BotProtocol::controlEntity, controlSession, "Unknown session.");
    return;
  }

  switch((BotProtocol::ControlSession::Command)controlSession.cmd)
  {
  case BotProtocol::ControlSession::startSimulation:
    if(!session->startSimulation())
    {
      sendErrorResponse(BotProtocol::controlEntity, controlSession, "Could not start simulation session.");
      return;
    }
    sendMessage(BotProtocol::controlEntityResponse, &response, sizeof(response));
    session->send();
    break;
  case BotProtocol::ControlSession::stop:
    if(!session->stop())
    {
      sendErrorResponse(BotProtocol::controlEntity, controlSession, "Could not stop session.");
      return;
    }
    sendMessage(BotProtocol::controlEntityResponse, &response, sizeof(response));
    session->send();
    break;
  case BotProtocol::ControlSession::select:
    if(this->session)
      this->session->unregisterClient(*this);
    session->registerClient(*this, false);
    this->session = session;
    sendMessage(BotProtocol::controlEntityResponse, &response, sizeof(response));
    {
      const HashMap<uint32_t, BotProtocol::Transaction>& transactions = session->getTransactions();
      for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Transaction));
      const HashMap<uint32_t, BotProtocol::Order>& orders = session->getOrders();
      for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Order));
      const List<BotProtocol::SessionLogMessage>& logMessages = session->getLogMessages();
      for(List<BotProtocol::SessionLogMessage>::Iterator i = logMessages.begin(), end = logMessages.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::SessionLogMessage));
    }
    break;
  }
}

void_t ClientHandler::handleBotControlSession(BotProtocol::ControlSession& controlSession)
{
  BotProtocol::ControlSessionResponse response;
  response.entityType = BotProtocol::session;
  response.entityId = controlSession.entityId;
  response.cmd = controlSession.cmd;

  if(controlSession.entityId != session->getId())
  {
    sendErrorResponse(BotProtocol::controlEntity, controlSession, "Unknown session.");
    return;
  }

  switch((BotProtocol::ControlSession::Command)controlSession.cmd)
  {
  case BotProtocol::ControlSession::requestTransactions:
    {
      sendMessage(BotProtocol::controlEntityResponse, &response, sizeof(response));
      const HashMap<uint32_t, BotProtocol::Transaction>& transactions = session->getTransactions();
      for(HashMap<uint32_t, BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Transaction));
    }
    break;
  case BotProtocol::ControlSession::requestOrders:
    {
      sendMessage(BotProtocol::controlEntityResponse, &response, sizeof(response));
      const HashMap<uint32_t, BotProtocol::Order>& orders = session->getOrders();
      for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
        sendEntity(&*i, sizeof(BotProtocol::Order));
    }
    break;
  }
}

void_t ClientHandler::handleBotCreateSessionTransaction(BotProtocol::Transaction& createTransactionArgs)
{
  BotProtocol::Transaction* transaction = session->createTransaction(createTransactionArgs.price, createTransactionArgs.amount, createTransactionArgs.fee, (BotProtocol::Transaction::Type)createTransactionArgs.type);
  if(!transaction)
  {
    sendErrorResponse(BotProtocol::createEntity, createTransactionArgs, "Could not create transaction.");
    return;
  }

  BotProtocol::CreateEntityResponse response;
  response.entityType = BotProtocol::sessionTransaction;
  response.entityId = createTransactionArgs.entityId;
  response.id = transaction->entityId;
  sendMessage(BotProtocol::createEntityResponse, &response, sizeof(response));

  session->sendEntity(transaction, sizeof(BotProtocol::Transaction));
  session->saveData();
}

void_t ClientHandler::handleBotRemoveSessionTransaction(const BotProtocol::Entity& entity)
{
  if(!session->deleteTransaction(entity.entityId))
  {
    sendErrorResponse(BotProtocol::removeEntity, entity, "Unknown transaction.");
    return;
  }

  session->removeEntity(BotProtocol::sessionTransaction, entity.entityId);
  session->saveData();
}

void_t ClientHandler::handleBotCreateSessionOrder(BotProtocol::Order& createOrderArgs)
{
  BotProtocol::Order* order = session->createOrder(createOrderArgs.price, createOrderArgs.amount, createOrderArgs.fee, (BotProtocol::Order::Type)createOrderArgs.type);
  if(!order)
  {
    sendErrorResponse(BotProtocol::createEntity, createOrderArgs, "Could not create order.");
    return;
  }

  BotProtocol::CreateEntityResponse response;
  response.entityType = BotProtocol::sessionOrder;
  response.entityId = createOrderArgs.entityId;
  response.id = order->entityId;
  sendMessage(BotProtocol::createEntityResponse, &response, sizeof(response));

  session->sendEntity(order, sizeof(BotProtocol::Order));
  session->saveData();
}

void_t ClientHandler::handleBotRemoveSessionOrder(const BotProtocol::Entity& entity)
{
  if(!session->deleteOrder(entity.entityId))
  {
    sendErrorResponse(BotProtocol::removeEntity, entity, "Unknown order.");
    return;
  }

  session->removeEntity(BotProtocol::sessionOrder, entity.entityId);
  session->saveData();
}

void_t ClientHandler::handleBotCreateSessionLogMessage(BotProtocol::SessionLogMessage& logMessageArgs)
{
  String message = BotProtocol::getString(logMessageArgs.message);
  BotProtocol::SessionLogMessage* logMessage = session->addLogMessage(logMessageArgs.date, message);
  if(!logMessage)
  {
    sendErrorResponse(BotProtocol::createEntity, logMessageArgs, "Could not add log message.");
    return;
  }

  BotProtocol::CreateEntityResponse response;
  response.entityType = BotProtocol::sessionLogMessage;
  response.entityId = logMessageArgs.entityId;
  response.id = logMessage->entityId;
  sendMessage(BotProtocol::createEntityResponse, &response, sizeof(response));

  session->sendEntity(logMessage, sizeof(BotProtocol::SessionLogMessage));
  session->saveData();
}

void_t ClientHandler::handleMarketUpdateMarketTransaction(BotProtocol::Transaction& transaction)
{
  market->updateTransaction(transaction);
  market->sendEntity(&transaction, sizeof(transaction));
}

void_t ClientHandler::handleMarketRemoveMarketTransaction(uint32_t id)
{
  market->deleteTransaction(id);
  market->removeEntity(BotProtocol::marketTransaction, id);
}

void_t ClientHandler::handleMarketUpdateMarketOrder(BotProtocol::Order& order)
{
  market->updateOrder(order);
  market->sendEntity(&order, sizeof(order));
}

void_t ClientHandler::handleMarketRemoveMarketOrder(uint32_t id)
{
  market->deleteOrder(id);
  market->removeEntity(BotProtocol::marketOrder, id);
}

void_t ClientHandler::handleMarketUpdateMarketBalance(BotProtocol::MarketBalance& balance)
{
  market->updateBalance(balance);
  market->sendEntity(&balance, sizeof(balance));
}

void_t ClientHandler::handleUserCreateMarketOrder(BotProtocol::Order& createOrderArgs)
{
  if(!market)
  {
    sendErrorResponse(BotProtocol::createEntity, createOrderArgs, "Unknown market.");
    return;
  }
  ClientHandler* marketAdapter = market->getAdapaterClient();
  if(!marketAdapter)
  {
    sendErrorResponse(BotProtocol::createEntity, createOrderArgs, "Invalid market state.");
    return;
  }

  createOrderArgs.entityId = market->createRequestId(BotProtocol::marketOrder, createOrderArgs.entityId, *this);
  marketAdapter->sendMessage(BotProtocol::createEntity, &createOrderArgs, sizeof(createOrderArgs));
}

void_t ClientHandler::handleUserUpdateMarketOrder(BotProtocol::Order& order)
{
  if(!market)
  {
    sendErrorResponse(BotProtocol::updateEntity, order, "Unknown market.");
    return;
  }
  ClientHandler* marketAdapter = market->getAdapaterClient();
  if(!marketAdapter)
  {
    sendErrorResponse(BotProtocol::updateEntity, order, "Invalid market state.");
    return;
  }

  marketAdapter->sendMessage(BotProtocol::updateEntity, &order, sizeof(order));
}

void_t ClientHandler::handleUserRemoveMarketOrder(const BotProtocol::Entity& entity)
{
  if(!market)
  {
    sendErrorResponse(BotProtocol::removeEntity, entity, "Unknown market.");
    return;
  }
  ClientHandler* marketAdapter = market->getAdapaterClient();
  if(!marketAdapter)
  {
    sendErrorResponse(BotProtocol::removeEntity, entity, "Invalid market state.");
    return;
  }
  marketAdapter->removeEntity(BotProtocol::marketOrder, entity.entityId);
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

void_t ClientHandler::sendErrorResponse(BotProtocol::MessageType messageType, const BotProtocol::Entity& entity, const String& errorMessage)
{
  BotProtocol::ErrorResponse errorResponse;
  errorResponse.entityType = entity.entityType;
  errorResponse.entityId = entity.entityId;
  errorResponse.messageType = messageType;
  BotProtocol::setString(errorResponse.errorMessage, errorMessage);
  sendMessage(BotProtocol::errorResponse, &errorResponse, sizeof(errorResponse));
}
