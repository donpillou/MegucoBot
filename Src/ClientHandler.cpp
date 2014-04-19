
#include <nstd/File.h>

#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ClientHandler.h"
#include "ServerHandler.h"
#include "User.h"
#include "Session.h"
#include "Engine.h"
#include "Market.h"
#include "Transaction.h"
#include "Order.h"

ClientHandler::ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client) : id(id), clientAddr(clientAddr), serverHandler(serverHandler), client(client),
  state(newState), user(0), session(0) {}

ClientHandler::~ClientHandler()
{
  if(user)
    user->unregisterClient(*this);
  if(session)
    session->unregisterClient(*this);
}

void_t ClientHandler::deselectSession()
{
  session = 0;
  if(state == botState)
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
  String username = getString(loginRequest.username);
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
      setString(engineData.name, engine->getName());
      sendEntity(BotProtocol::engine, engine->getId(), &engineData, sizeof(engineData));
    }
  }

  // send market list
  {
    BotProtocol::Market marketData;
    const HashMap<uint32_t, Market*>& markets = serverHandler.getMarkets();
    for(HashMap<uint32_t, Market*>::Iterator i = markets.begin(), end = markets.end(); i != end; ++i)
    {
      const Market* market = *i;
      setString(marketData.name, market->getName());
      setString(marketData.currencyBase, market->getCurrencyBase());
      setString(marketData.currencyComm, market->getCurrencyComm());
      sendEntity(BotProtocol::market, market->getId(), &marketData, sizeof(marketData));
    }
  }

  // send session list
  {
    BotProtocol::Session sessionData;
    const HashMap<uint32_t, Session*>& sessions = user->getSessions();
    for(HashMap<uint32_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    {
      const Session* session = *i;
      setString(sessionData.name, session->getName());
      sessionData.engineId = session->getEngine()->getId();
      sessionData.marketId = session->getMarket()->getId();
      sessionData.state = session->getState();
      sendEntity(BotProtocol::session, session->getId(), &sessionData, sizeof(sessionData));
    }
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
  response.isSimulation = session->getState() != BotProtocol::Session::active;
  session->getInitialBalance(response.balanceBase, response.balanceComm);
  sendMessage(BotProtocol::registerBotResponse, &response, sizeof(response));
  this->session = session;
  state = botState;
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
      handelRemoveSession(id);
      break;
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

void_t ClientHandler::handleCreateSession(BotProtocol::CreateSessionArgs& createSessionArgs)
{
  String name = getString(createSessionArgs.name);
  Engine* engine = serverHandler.findEngine(createSessionArgs.engineId);
  if(!engine)
  {
    sendError("Unknown engine.");
    return;
  }
  Market* market = serverHandler.findMarket(createSessionArgs.marketId);
  if(!market)
  {
    sendError("Unknown market.");
    return;
  }

  Session* session = user->createSession(name, *engine, *market, createSessionArgs.balanceBase, createSessionArgs.balanceComm);
  if(!session)
  {
    sendError("Could not create session.");
    return;
  }

  BotProtocol::Session sessionData;
  setString(sessionData.name, session->getName());
  sessionData.engineId = session->getEngine()->getId();
  sessionData.marketId = session->getMarket()->getId();
  sessionData.state = session->getState();
  user->sendEntity(BotProtocol::session, session->getId(), &sessionData, sizeof(sessionData));
  user->saveData();
}

void_t ClientHandler::handelRemoveSession(uint32_t id)
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
    break;
  case BotProtocol::ControlSessionArgs::stop:
    session->stop();
    break;
  case BotProtocol::ControlSessionArgs::select:
    if(this->session)
      this->session->unregisterClient(*this);
    session->registerClient(*this, false);
    this->session = session;
    {
      BotProtocol::Transaction transactionData;
      const HashMap<uint32_t, Transaction*>& transactions = session->getTransactions();
      for(HashMap<uint32_t, Transaction*>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
      {
        const Transaction* transaction = *i;
        transactionData.price = transaction->getPrice();
        transactionData.amount = transaction->getAmount();
        transactionData.fee = transaction->getFee();
        transactionData.type = transaction->getType();
        transactionData.date = transaction->getDate();
        sendEntity(BotProtocol::transaction, transaction->getId(), &transactionData, sizeof(transactionData));
      }
      BotProtocol::Order orderData;
      const HashMap<uint32_t, Order*>& orders = session->getOrders();
      for(HashMap<uint32_t, Order*>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
      {
        const Order* order = *i;
        orderData.price = order->getPrice();
        orderData.amount = order->getAmount();
        orderData.fee = order->getFee();
        orderData.type = order->getType();
        orderData.date = order->getDate();
        sendEntity(BotProtocol::order, order->getId(), &orderData, sizeof(orderData));
      }
    }
    return;
  }

  BotProtocol::Session sessionData;
  setString(sessionData.name, session->getName());
  sessionData.engineId = session->getEngine()->getId();
  sessionData.marketId = session->getMarket()->getId();
  sessionData.state = session->getState();
  user->sendEntity(BotProtocol::session, session->getId(), &sessionData, sizeof(sessionData));
}

void_t ClientHandler::handleCreateTransaction(BotProtocol::CreateTransactionArgs& createTransactionArgs)
{
  Transaction* transaction = session->createTransaction(createTransactionArgs.price, createTransactionArgs.amount, createTransactionArgs.fee, (BotProtocol::Transaction::Type)createTransactionArgs.type);
  if(!transaction)
  {
    sendError("Could not create transaction.");
    return;
  }

  BotProtocol::Transaction transactionData;
  transactionData.price = transaction->getPrice();
  transactionData.amount = transaction->getAmount();
  transactionData.fee = transaction->getFee();
  transactionData.type = transaction->getType();
  transactionData.date = transaction->getDate();
  session->sendEntity(BotProtocol::transaction, transaction->getId(), &transactionData, sizeof(transactionData));
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

  BotProtocol::Order orderData;
  orderData.price = order->getPrice();
  orderData.amount = order->getAmount();
  orderData.fee = order->getFee();
  orderData.type = order->getType();
  orderData.date = order->getDate();
  session->sendEntity(BotProtocol::order, order->getId(), &orderData, sizeof(orderData));
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
  setString(error.errorMessage, errorMessage);
  sendEntity(BotProtocol::error, 0, &error, sizeof(error));
}
