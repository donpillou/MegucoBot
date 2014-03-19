
#include "Tools/Math.h"
#include "ClientHandler.h"
#include "ServerHandler.h"
#include "User.h"

ClientHandler::ClientHandler(uint64_t id, uint32_t clientAddr, ServerHandler& serverHandler, Server::Client& client) : id(id), clientAddr(clientAddr), serverHandler(serverHandler), client(client)
{
  //byte_t message[sizeof(BotProtocol::Header) + sizeof(BotProtocol::WelcomeMessage)];
  //BotProtocol::Header* header = (BotProtocol::Header*)message;
  //BotProtocol::WelcomeMessage* welcomeMessage = (BotProtocol::WelcomeMessage*)(header + 1);
  //header->size = sizeof(message);
  //header->source = 0;
  //header->destination = 0;
  //header->messageType = BotProtocol::welcomeMessage;
  //uint32_t* salt = (uint32_t*)welcomeMessage->salt;
  //salt[0] = Math::random();
  //salt[1] = Math::random();
  //salt[2] = Math::random();
  //salt[3] = Math::random();
  //Memory::copy(this->salt, salt, sizeof(uint32_t) * 4);
  //client.send(message, sizeof(message));
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
  switch((BotProtocol::MessageType)messageHeader.messageType)
  {
  case BotProtocol::loginRequest:
    if(size >= sizeof(BotProtocol::LoginRequest))
      handleLogin(messageHeader.source, *(BotProtocol::LoginRequest*)data);
    break;
  }
}

void_t ClientHandler::handleLogin(uint64_t source, BotProtocol::LoginRequest& loginRequest)
{
  loginRequest.username[sizeof(loginRequest.username) - 1] = '\0';
  String username;
  username.attach(loginRequest.username, String::length(loginRequest.username));
  User* user = serverHandler.findUser(username);
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
  header->destination = 0;
  header->messageType = BotProtocol::loginRequest;
  Memory::copy(loginResponse->userkey, user->key, sizeof(loginResponse->userkey));
  Memory::copy(loginResponse->loginkey, loginkey, sizeof(loginResponse->loginkey));
  client.send(message, sizeof(message));
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
