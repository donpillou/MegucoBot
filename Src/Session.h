

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>
#include <nstd/Variant.h>

#include "BotProtocol.h"

class ServerHandler;
class ClientHandler;
class Engine;
class Market;

class Session
{
public:
  Session(ServerHandler& serverHandler, uint32_t id, const String& name, Engine& engine, Market& market, double balanceBase, double balanceComm);
  Session(ServerHandler& serverHandler, const Variant& variant);
  ~Session();

  bool_t startSimulation();
  bool_t stop();

  bool_t setClient(ClientHandler* client);

  uint32_t getId() const {return id;}
  const String& getName() const {return name;}
  Engine* getEngine() const {return engine;}
  Market* getMarket() const {return market;}
  BotProtocol::Session::State getState() const {return state;}
  void_t getInitialBalance(double& balanceBase, double& balanceComm) const;

  void_t toVariant(Variant& variant);

private:
  ServerHandler& serverHandler;
  uint32_t id;
  String name;
  Engine* engine;
  Market* market;
  double balanceBase;
  double balanceComm;
  BotProtocol::Session::State state;
  Process process;
  uint32_t pid;
  ClientHandler* client;
};
