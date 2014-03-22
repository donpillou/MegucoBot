
#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ClientHandler.h"
#include "ServerHandler.h"
#include "User.h"

ClientHandler::ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client) : id(id), clientAddr(clientAddr), serverHandler(serverHandler), client(client),
  state(newState), user(0) {}

ClientHandler::~ClientHandler()
{
  if(user && state == authedState)
    user->removeClient(*this);
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
    if((BotProtocol::MessageType)messageHeader.messageType == BotProtocol::loginRequest)
      if(size >= sizeof(BotProtocol::LoginRequest))
        handleLogin(messageHeader.source, *(BotProtocol::LoginRequest*)data);
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
    sendErrorResponse(BotProtocol::loginRequest, source, "Unknown user");
    return;
  }

  for(uint32_t* p = (uint32_t*)loginkey, * end = (uint32_t*)(loginkey + 64); p < end; ++p)
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
  byte_t signature[64];
  Sha256::hmac(loginkey, 64, user->pwhmac, 64, signature);
  if(Memory::compare(signature, authRequest.signature, 64) != 0)
  {
    sendErrorResponse(BotProtocol::authRequest, source, "Incorrect signature");
    return;
  }

  BotProtocol::Header header;
  header.size = sizeof(header);
  header.source = 0;
  header.destination = source;
  header.messageType = BotProtocol::authResponse;
  client.send((const byte_t*)&header, sizeof(header));
  state = authedState;
  user->addClient(*this);
}

void_t ClientHandler::handleCreateSimSession(uint64_t source, BotProtocol::CreateSimSessionRequest& createSimSessionRequest)
{
  createSimSessionRequest.name[sizeof(createSimSessionRequest.name) - 1] = '\0';
  createSimSessionRequest.engine[sizeof(createSimSessionRequest.engine) - 1] = '\0';
  String name;
  String engine;
  name.attach(createSimSessionRequest.name, String::length(createSimSessionRequest.name));
  engine.attach(createSimSessionRequest.engine, String::length(createSimSessionRequest.engine));
  uint64_t id = user->createSimSession(name, engine, createSimSessionRequest.balanceBase, createSimSessionRequest.balanceComm);
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
