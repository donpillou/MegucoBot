
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
    switch((BotProtocol::MessageType)messageHeader.messageType)
    {
    case BotProtocol::loginRequest:
      if(size >= sizeof(BotProtocol::LoginRequest))
        handleLogin(messageHeader.source, *(BotProtocol::LoginRequest*)data);
      break;
    case BotProtocol::registerBotRequest:
      if(clientAddr == Socket::loopbackAddr && size >= sizeof(BotProtocol::RegisterBotRequest))
        handleRegisterBot(messageHeader.source, *(BotProtocol::RegisterBotRequest*)data);
      break;
    }
    break;
  case loginState:
    if((BotProtocol::MessageType)messageHeader.messageType == BotProtocol::authRequest)
      if(size >= sizeof(BotProtocol::AuthRequest))
        handleAuth(messageHeader.source, *(BotProtocol::AuthRequest*)data);
    break;
  case authedState:
    switch((BotProtocol::MessageType)messageHeader.messageType)
    {
    case BotProtocol::createSimSessionRequest:
      if(size >= sizeof(BotProtocol::CreateSimSessionRequest))
        handleCreateSimSession(messageHeader.source, *(BotProtocol::CreateSimSessionRequest*)data);
      break;
    case BotProtocol::createSessionRequest:
      if(size >= sizeof(BotProtocol::CreateSessionRequest))
        handleCreateSession(messageHeader.source, *(BotProtocol::CreateSessionRequest*)data);
      break;
    }
    break;
  }

}

void_t ClientHandler::handleLogin(uint64_t source, BotProtocol::LoginRequest& loginRequest)
{
  loginRequest.username[sizeof(loginRequest.username) - 1] = '\0';
  String username;
  username.attach(loginRequest.username, String::length(loginRequest.username));
  user = serverHandler.findUser(username);
  if(!user)
  {
    sendErrorResponse(BotProtocol::loginRequest, source, "Unknown user.");
    return;
  }

  for(uint32_t* p = (uint32_t*)loginkey, * end = (uint32_t*)(loginkey + 32); p < end; ++p)
    *p = Math::random();

  byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::LoginResponse)];
  BotProtocol::Header* header = (BotProtocol::Header*)message;
  BotProtocol::LoginResponse* loginResponse = (BotProtocol::LoginResponse*)(header + 1);
  header->size = sizeof(message);
  header->source = 0;
  header->destination = source;
  header->messageType = BotProtocol::loginResponse;
  Memory::copy(loginResponse->userkey, user->key, sizeof(loginResponse->userkey));
  Memory::copy(loginResponse->loginkey, loginkey, sizeof(loginResponse->loginkey));
  client.send(message, sizeof(message));
  state = loginState;
}

void ClientHandler::handleAuth(uint64_t source, BotProtocol::AuthRequest& authRequest)
{
  byte_t signature[32];
  Sha256::hmac(loginkey, 32, user->pwhmac, 32, signature);
  if(Memory::compare(signature, authRequest.signature, 32) != 0)
  {
    sendErrorResponse(BotProtocol::authRequest, source, "Incorrect signature.");
    return;
  }

  BotProtocol::Header header;
  header.size = sizeof(header);
  header.source = 0;
  header.destination = source;
  header.messageType = BotProtocol::authResponse;
  client.send((const byte_t*)&header, sizeof(header));
  state = authedState;
  user->registerClient(*this);

  // send engine list
  {
    const List<String>& engines = serverHandler.getEngines();
    byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::EngineMessage)];
    BotProtocol::Header* header = (BotProtocol::Header*)message;
    BotProtocol::EngineMessage* engineMessage = (BotProtocol::EngineMessage*)(header + 1);
    header->size = sizeof(message);
    header->source = 0;
    header->destination = source;
    header->messageType = BotProtocol::engineMessage;
    for(List<String>::Iterator i = engines.begin(), end = engines.end(); i != end; ++i)
    {
      const String engineName = File::basename(*i, ".exe");
      Memory::copy(engineMessage->name, (const char_t*)engineName, Math::min(engineName.length() + 1, sizeof(engineMessage->name) -1));
      engineMessage->name[sizeof(engineMessage->name) -1] = '\0';
      client.send(message, sizeof(message));
    }
  }

  // send session list
  // todo: 
}

void_t ClientHandler::handleCreateSimSession(uint64_t source, BotProtocol::CreateSimSessionRequest& createSimSessionRequest)
{
  createSimSessionRequest.name[sizeof(createSimSessionRequest.name) - 1] = '\0';
  createSimSessionRequest.engine[sizeof(createSimSessionRequest.engine) - 1] = '\0';
  String name;
  String engine;
  name.attach(createSimSessionRequest.name, String::length(createSimSessionRequest.name));
  engine.attach(createSimSessionRequest.engine, String::length(createSimSessionRequest.engine));
  uint32_t id = user->createSimSession(name, engine, createSimSessionRequest.balanceBase, createSimSessionRequest.balanceComm);
  if(id == 0)
  {
    sendErrorResponse(BotProtocol::createSimSessionRequest, source, "Could not create sim session.");
    return;
  }

  byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::CreateSimSessionResponse)];
  BotProtocol::Header* header = (BotProtocol::Header*)message;
  BotProtocol::CreateSimSessionResponse* createSimSessionResponse = (BotProtocol::CreateSimSessionResponse*)(header + 1);
  header->size = sizeof(message);
  header->source = 0;
  header->destination = source;
  header->messageType = BotProtocol::createSimSessionResponse;
  createSimSessionResponse->id = id;
  client.send(message, sizeof(message));
}

void_t ClientHandler::handleCreateSession(uint64_t source, BotProtocol::CreateSessionRequest& createSessionRequest)
{
}

void_t ClientHandler::handleRegisterBot(uint64_t source, BotProtocol::RegisterBotRequest& registerBotRequest)
{
  Session* session = serverHandler.findSession(registerBotRequest.pid);
  if(!session)
  {
    sendErrorResponse(BotProtocol::registerBotRequest, source, "Unknown session.");
    return;
  }
  if(!session->setClient(this))
  {
    sendErrorResponse(BotProtocol::registerBotRequest, source, "Invalid session.");
    return;
  }

  byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::RegisterBotResponse)];
  BotProtocol::Header* header = (BotProtocol::Header*)message;
  BotProtocol::RegisterBotResponse* response = (BotProtocol::RegisterBotResponse*)(header + 1);
  header->size = sizeof(message);
  header->source = 0;
  header->destination = source;
  header->messageType = BotProtocol::registerBotResponse;
  response->isSimulation = session->isSimulation();
  session->getInitialBalance(response->balanceBase, response->balanceComm);
  client.send(message, sizeof(message));
  this->session = session;
  state = botState;
}

void_t ClientHandler::sendErrorResponse(BotProtocol::MessageType messageType, uint64_t destination, const String& errorMessage)
{
  byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::ErrorResponse)];
  BotProtocol::Header* header = (BotProtocol::Header*)message;
  BotProtocol::ErrorResponse* errorResponse = (BotProtocol::ErrorResponse*)(header + 1);
  header->size = sizeof(message);
  header->destination = destination;
  header->source = 0;
  header->messageType = BotProtocol::errorResponse;
  errorResponse->messageType = messageType;
  Memory::copy(errorResponse->errorMessage, (const char_t*)errorMessage, Math::min(errorMessage.length() + 1, sizeof(errorResponse->errorMessage) - 1));
  errorResponse->errorMessage[sizeof(errorResponse->errorMessage) - 1] = '\0';
  client.send(message, sizeof(message));
}
