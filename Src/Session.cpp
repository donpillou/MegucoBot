
#include "Session.h"
#include "ServerHandler.h"
#include "ClientHandler.h"
#include "Engine.h"
#include "Market.h"

Session::Session(ServerHandler& serverHandler, uint32_t id, const String& name, Engine& engine, Market& market, double balanceBase, double balanceComm) :
  serverHandler(serverHandler),
  id(id), name(name), engine(&engine), market(&market), balanceBase(balanceBase), balanceComm(balanceComm),
  state(BotProtocol::Session::inactive), pid(0), botClient(0) {}

Session::Session(ServerHandler& serverHandler, const Variant& variant) : serverHandler(serverHandler),
  state(BotProtocol::Session::inactive), pid(0), botClient(0)
{
  const HashMap<String, Variant>& data = variant.toMap();
  id = data.find("id")->toUInt();
  name = data.find("name")->toString();
  engine = serverHandler.findEngine(data.find("engine")->toString());
  market = serverHandler.findMarket(data.find("market")->toString());
  balanceBase = data.find("balanceBase")->toDouble();
  balanceComm = data.find("balanceComm")->toDouble();
}

void_t Session::toVariant(Variant& variant)
{
  HashMap<String, Variant>& data = variant.toMap();
  data.append("id", id);
  data.append("name", name);
  data.append("engine", engine->getName());
  data.append("market", market->getName());
  data.append("balanceBase", balanceBase);
  data.append("balanceComm", balanceComm);
}

Session::~Session()
{
  if(pid != 0)
    serverHandler.unregisterSession(pid);
  process.kill();
  if(botClient)
    botClient->deselectSession();
  for(HashSet<ClientHandler*>::Iterator i = clients.begin(), end = clients.end(); i != end; ++i)
    (*i)->deselectSession();
}

bool_t Session::startSimulation()
{
  if(pid != 0)
    return false;
  pid = process.start(engine->getPath());
  if(!pid)
    return false;
  serverHandler.registerSession(pid, *this);
  state = BotProtocol::Session::simulating;
  return true;
}

bool_t Session::stop()
{
  if(pid == 0)
    return false;
  if(!process.kill())
    return false;
  pid = 0;
  state = BotProtocol::Session::inactive;
  return true;
}

bool_t Session::registerClient(ClientHandler& client, bool_t bot)
{
  if(bot)
  {
    if(botClient)
      return false;
    botClient = &client;
  }
  else
    clients.append(&client);
  return true;
}

void_t Session::unregisterClient(ClientHandler& client)
{
  if(&client == botClient)
    botClient = 0;
  else
    clients.remove(&client);
}

void_t Session::getInitialBalance(double& balanceBase, double& balanceComm) const
{
  balanceBase = this->balanceBase;
  balanceComm = this->balanceComm;
}
