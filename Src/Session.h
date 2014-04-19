

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>
#include <nstd/Variant.h>
#include <nstd/HashSet.h>

#include "BotProtocol.h"

class ServerHandler;
class ClientHandler;
class Engine;
class Market;
class Transaction;
class User;
class Order;

class Session
{
public:
  Session(ServerHandler& serverHandler, User& user, uint32_t id, const String& name, Engine& engine, Market& market, double balanceBase, double balanceComm);
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
  Engine* getEngine() const {return engine;}
  Market* getMarket() const {return market;}
  BotProtocol::Session::State getState() const {return state;}
  void_t getInitialBalance(double& balanceBase, double& balanceComm) const;

  Transaction* createTransaction(double price, double amount, double fee, BotProtocol::Transaction::Type type);
  const HashMap<uint32_t, Transaction*>& getTransactions() const {return transactions;}
  bool_t deleteTransaction(uint32_t id);

  Order* createOrder(double price, double amount, double fee, BotProtocol::Order::Type type);
  const HashMap<uint32_t, Order*>& getOrders() const {return orders;}
  bool_t deleteOrder(uint32_t id);

  void_t sendEntity(BotProtocol::EntityType type, uint32_t id, const void_t* data, size_t size);
  void_t removeEntity(BotProtocol::EntityType type, uint32_t id);

private:
  ServerHandler& serverHandler;
  User& user;
  uint32_t id;
  String name;
  Engine* engine;
  Market* market;
  double balanceBase;
  double balanceComm;
  BotProtocol::Session::State state;
  Process process;
  uint32_t pid;
  ClientHandler* botClient;
  HashSet<ClientHandler*> clients;
  HashMap<uint32_t, Transaction*> transactions;
  HashMap<uint32_t, Order*> orders;
  uint32_t nextEntityId;
};
