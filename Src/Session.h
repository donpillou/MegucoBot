

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>
#include <nstd/Variant.h>
#include <nstd/HashSet.h>

#include "BotProtocol.h"

class ServerHandler;
class ClientHandler;
class BotEngine;
class Market;
class Transaction;
class User;
class Order;

class Session
{
public:
  Session(ServerHandler& serverHandler, User& user, uint32_t id, const String& name, BotEngine& engine, Market& market, double balanceBase, double balanceComm);
  Session(ServerHandler& serverHandler, User& user, const Variant& variant);
  ~Session();
  void_t toVariant(Variant& variant);

  bool_t saveData();

  bool_t startSimulation();
  bool_t stop();

  bool_t registerClient(ClientHandler& client, bool_t bot);
  void_t unregisterClient(ClientHandler& client);

  uint32_t getId() const {return id;}
  const String& getName() const {return name;}
  BotEngine* getEngine() const {return engine;}
  Market* getMarket() const {return market;}
  BotProtocol::Session::State getState() const {return state;}
  bool isSimulation() const {return simulation;}
  void_t getInitialBalance(double& balanceBase, double& balanceComm) const;

  BotProtocol::Transaction* createTransaction(double price, double amount, double fee, BotProtocol::Transaction::Type type);
  const HashMap<uint32_t, BotProtocol::Transaction>& getTransactions() const {return transactions;}
  bool_t deleteTransaction(uint32_t id);

  BotProtocol::Order* createOrder(double price, double amount, double fee, BotProtocol::Order::Type type);
  const HashMap<uint32_t, BotProtocol::Order>& getOrders() const {return orders;}
  bool_t deleteOrder(uint32_t id);

  void_t send(ClientHandler* client = 0);
  void_t sendEntity(const void_t* data, size_t size);
  void_t removeEntity(BotProtocol::EntityType type, uint32_t id);

private:
  ServerHandler& serverHandler;
  User& user;
  uint32_t id;
  String name;
  BotEngine* engine;
  Market* market;
  bool simulation;
  double balanceBase;
  double balanceComm;
  BotProtocol::Session::State state;
  Process process;
  uint32_t pid;
  ClientHandler* botClient;
  HashSet<ClientHandler*> clients;
  HashMap<uint32_t, BotProtocol::Transaction> transactions;
  HashMap<uint32_t, BotProtocol::Order> orders;
  uint32_t nextEntityId;
};
