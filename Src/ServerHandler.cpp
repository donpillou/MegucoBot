
#include "Tools/Math.h"
#include "Tools/Sha256.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "User.h"

ServerHandler::~ServerHandler()
{
  for(HashMap<uint64_t, ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    delete *i;
  for(HashMap<String, User*>::Iterator i = users.begin(), end = users.end(); i != end; ++i)
    delete *i;
}

void_t ServerHandler::acceptedClient(Server::Client& client, uint32_t addr, uint16_t port)
{
  uint64_t clientId = nextClientId++;
  ClientHandler* clientHandler = new ClientHandler(clientId, addr, *this, client);
  client.setListener(clientHandler);
  clients.append(clientId, clientHandler);
}

void_t ServerHandler::closedClient(Server::Client& client)
{
  ClientHandler* clientHandler = (ClientHandler*)client.getListener();
  clients.remove(clientHandler->getId());
  delete clientHandler;
}

bool_t ServerHandler::addUser(const String& userName, const String& password)
{
  if(findUser(userName))
    return false;
  User* user = new User(*this);
  user->userName = userName;
  for(uint32_t* p = (uint32_t*)user->key, * end = (uint32_t*)(user->key + 64); p < end; ++p)
    *p = Math::random();
  Sha256::hmac(user->key, 64, (const byte_t*)(const char_t*)password, password.length(), user->pwhmac);
  users.append(userName, user);
  return true;
}

User* ServerHandler::findUser(const String& userName)
{
  HashMap<String, User*>::Iterator it = users.find(userName);
  if(it == users.end())
    return 0;
  return *it;
}

Session* ServerHandler::findSession(uint32_t pid)
{
  HashMap<uint32_t, Session*>::Iterator it = sessions.find(pid);
  if(it == sessions.end())
    return 0;
  return *it;
}

void_t ServerHandler::registerSession(uint32_t pid, Session& session)
{
  sessions.append(pid, &session);
}

void_t ServerHandler::unregisterSession(uint32_t pid)
{
  sessions.remove(pid);
}
