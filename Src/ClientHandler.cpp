
#include <nstd/File.h>

#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ClientHandler.h"
#include "ServerHandler.h"
#include "User.h"
#include "Session.h"
#include "Engine.h"

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
    case BotProtocol::createSessionRequest:
      if(size >= sizeof(BotProtocol::CreateSessionRequest))
        handleCreateSession(*(BotProtocol::CreateSessionRequest*)data);
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
  Memory::copy(loginResponse.userkey, user->key, sizeof(loginResponse.userkey));
  Memory::copy(loginResponse.loginkey, loginkey, sizeof(loginResponse.loginkey));
  sendMessage(BotProtocol::loginResponse, &loginResponse, sizeof(loginResponse));
  state = loginState;
}

void ClientHandler::handleAuth(BotProtocol::AuthRequest& authRequest)
{
  byte_t signature[32];
  Sha256::hmac(loginkey, 32, user->pwhmac, 32, signature);
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

  // send session list
  {
    BotProtocol::Session sessionData;
    const HashMap<uint32_t, Session*>& sessions = user->getSessions();
    for(HashMap<uint32_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    {
      const Session* session = *i;
      setString(sessionData.name, session->getName());
      setString(sessionData.engine, session->getEngine());
      sendEntity(BotProtocol::session, session->getId(), &sessionData, sizeof(sessionData));
    }
  }
}

void_t ClientHandler::handleCreateSession(BotProtocol::CreateSessionRequest& createSessionRequest)
{
  String name = getString(createSessionRequest.name);
  String engine = getString(createSessionRequest.engine);
  Session* session = user->createSession(name, engine, createSessionRequest.balanceBase, createSessionRequest.balanceComm);
  if(!session)
  {
    sendError("Could not create session.");
    return;
  }

  BotProtocol::CreateSessionResponse createSessionResponse;
  createSessionResponse.id = id;
  sendMessage(BotProtocol::createSessionResponse, &createSessionResponse, sizeof(createSessionResponse));
  
  BotProtocol::Session sessionData;
  setString(sessionData.name, session->getName());
  setString(sessionData.engine, session->getEngine());
  user->sendEntity(BotProtocol::session, session->getId(), &sessionData, sizeof(sessionData));
}

void_t ClientHandler::handleRegisterBot(BotProtocol::RegisterBotRequest& registerBotRequest)
{
  Session* session = serverHandler.findSession(registerBotRequest.pid);
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
  response.isSimulation = session->isSimulation();
  session->getInitialBalance(response.balanceBase, response.balanceComm);
  sendMessage(BotProtocol::registerBotResponse, &response, sizeof(response));
  this->session = session;
  state = botState;
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
