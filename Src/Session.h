

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>
#include <nstd/Variant.h>

#include "BotProtocol.h"

class ServerHandler;
class ClientHandler;

class Session
{
public:
  Session(ServerHandler& serverHandler, uint32_t id, const String& name, const String& engine, double balanceBase, double balanceComm);
  ~Session();

  bool_t startSimulation();
  bool_t stop();

  bool_t setClient(ClientHandler* client);

  uint32_t getId() const {return id;}
  bool_t isSimulation() const {return simulation;}
  const String& getName() const {return name;}
  const String& getEngine() const {return engine;}
  BotProtocol::Session::State getState() const {return state;}
  void_t getInitialBalance(double& balanceBase, double& balanceComm) const;

  void_t toVariant(Variant& variant);

private:
  ServerHandler& serverHandler;
  uint32_t id;
  String name;
  String engine;
  double balanceBase;
  double balanceComm;
  BotProtocol::Session::State state;
  bool_t simulation;
  Process process;
  uint32_t pid;
  ClientHandler* client;
};
