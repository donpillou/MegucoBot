
#include <nstd/File.h>

#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ClientHandler.h"
#include "ServerHandler.h"
#include "User.h"
#include "Session.h"
#include "Engine.h"
#include "Market.h"

ClientHandler::ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client) : id(id), clientAddr(clientAddr), serverHandler(serverHandler), client(client),
  state(newState), user(0), session(0) {}

ClientHandler::~ClientHandler()
{
  if(user)
    user->unregisterClient(*this);
  if(session)
    session->setClient(0);
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
  case authedState:
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
  state = authedState;
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
  if(!session->setClient(this))
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
  switch(type)
  {
  case BotProtocol::session:
    if(size >= sizeof(BotProtocol::CreateSessionArgs))
      handleCreateSession(*(BotProtocol::CreateSessionArgs*)data);
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleRemoveEntity(BotProtocol::EntityType type, uint32_t id)
{
  switch(type)
  {
  case BotProtocol::session:
    handelRemoveSession(id);
    break;
  default:
    break;
  }
}

void_t ClientHandler::handleControlEntity(BotProtocol::EntityType type, uint32_t id, byte_t* data, size_t size)
{
  switch(type)
  {
  case BotProtocol::session:
    if(size >= sizeof(BotProtocol::ControlSessionArgs))
      handleControlSession(id, *(BotProtocol::ControlSessionArgs*)data);
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
  }

  BotProtocol::Session sessionData;
  setString(sessionData.name, session->getName());
  sessionData.engineId = session->getEngine()->getId();
  sessionData.marketId = session->getMarket()->getId();
  sessionData.state = session->getState();
  user->sendEntity(BotProtocol::session, session->getId(), &sessionData, sizeof(sessionData));
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
