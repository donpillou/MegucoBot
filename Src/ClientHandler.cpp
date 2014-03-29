
#include <nstd/File.h>

#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ClientHandler.h"
#include "ServerHandler.h"
#include "User.h"
#include "Session.h"

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
    switch((BotProtocol::EntityType)messageHeader.entityType)
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
    if((BotProtocol::EntityType)messageHeader.entityType == BotProtocol::authRequest)
      if(size >= sizeof(BotProtocol::AuthRequest))
        handleAuth(*(BotProtocol::AuthRequest*)data);
    break;
  case authedState:
    switch((BotProtocol::EntityType)messageHeader.entityType)
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
    sendError(BotProtocol::loginRequest, "Unknown user.");
    return;
  }

  for(uint32_t* p = (uint32_t*)loginkey, * end = (uint32_t*)(loginkey + 32); p < end; ++p)
    *p = Math::random();

  BotProtocol::LoginResponse loginResponse;
  Memory::copy(loginResponse.userkey, user->key, sizeof(loginResponse.userkey));
  Memory::copy(loginResponse.loginkey, loginkey, sizeof(loginResponse.loginkey));
  sendEntity(BotProtocol::loginResponse, &loginResponse, sizeof(loginResponse));
  state = loginState;
}

void ClientHandler::handleAuth(BotProtocol::AuthRequest& authRequest)
{
  byte_t signature[32];
  Sha256::hmac(loginkey, 32, user->pwhmac, 32, signature);
  if(Memory::compare(signature, authRequest.signature, 32) != 0)
  {
    sendError(BotProtocol::authRequest, "Incorrect signature.");
    return;
  }

  sendEntity(BotProtocol::authResponse, 0, 0);
  state = authedState;
  user->registerClient(*this);

  // send engine list
  {
    BotProtocol::Engine engine;
    const List<String>& engines = serverHandler.getEngines();
    for(List<String>::Iterator i = engines.begin(), end = engines.end(); i != end; ++i)
    {
      const String engineName = File::basename(*i, ".exe");
      setString(engine.name, engineName);
      sendEntity(BotProtocol::engine, &engine, sizeof(engine));
    }
  }

  // send session list
  {
    BotProtocol::Session session;
    const HashMap<uint32_t, Session*>& sessions = user->getSessions();
    for(HashMap<uint32_t, Session*>::Iterator i = sessions.begin(), end = sessions.end(); i != end; ++i)
    {
      const Session* sessionHandler = *i;
      const String& name = sessionHandler->getName();
      const String& engine = sessionHandler->getEngine();
      session.id = id;
      setString(session.name, name);
      setString(session.engine, engine);
      sendEntity(BotProtocol::session, &session, sizeof(session));
    }
  }
}

void_t ClientHandler::handleCreateSession(BotProtocol::CreateSessionRequest& createSessionRequest)
{
  String name = getString(createSessionRequest.name);
  String engine = getString(createSessionRequest.engine);
  uint32_t id = user->createSession(name, engine, createSessionRequest.balanceBase, createSessionRequest.balanceComm);
  if(id == 0)
  {
    sendError(BotProtocol::createSessionRequest, "Could not create sim session.");
    return;
  }

  BotProtocol::CreateSessionResponse createSessionResponse;
  createSessionResponse.id = id;
  sendEntity(BotProtocol::createSessionResponse, &createSessionResponse, sizeof(createSessionResponse));
  
  BotProtocol::Session session;
  session.id = id;
  setString(session.name, createSessionRequest.name);
  setString(session.engine, createSessionRequest.engine);
  user->sendEntity(BotProtocol::session, &session, sizeof(session));
}

void_t ClientHandler::handleRegisterBot(BotProtocol::RegisterBotRequest& registerBotRequest)
{
  Session* session = serverHandler.findSession(registerBotRequest.pid);
  if(!session)
  {
    sendError(BotProtocol::registerBotRequest, "Unknown session.");
    return;
  }
  if(!session->setClient(this))
  {
    sendError(BotProtocol::registerBotRequest, "Invalid session.");
    return;
  }

  BotProtocol::RegisterBotResponse response;
  response.isSimulation = session->isSimulation();
  session->getInitialBalance(response.balanceBase, response.balanceComm);
  sendEntity(BotProtocol::registerBotResponse, &response, sizeof(response));
  this->session = session;
  state = botState;
}

void_t ClientHandler::sendEntity(BotProtocol::EntityType type, const void_t* data, size_t size)
{
  BotProtocol::Header header;
  header.size = sizeof(header) + size;
  header.entityType = type;
  client.reserve(header.size);
  client.send((const byte_t*)&header, sizeof(header));
  if(size > 0)
    client.send((const byte_t*)data, size);
}

void_t ClientHandler::sendError(BotProtocol::EntityType entityType, const String& errorMessage)
{
  BotProtocol::Error error;
  error.entityType = entityType;
  setString(error.errorMessage, errorMessage);
  sendEntity(BotProtocol::error, &error, sizeof(error));
}
