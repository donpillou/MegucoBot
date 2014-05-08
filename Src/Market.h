

#pragma once

#include <nstd/String.h>
#include <nstd/Process.h>
#include <nstd/Variant.h>
#include <nstd/HashSet.h>

#include "BotProtocol.h"

class ServerHandler;
class MarketAdapter;
class ClientHandler;
class User;

class Market
{
public:
  Market(ServerHandler& serverHandler, User& user, uint32_t id, MarketAdapter& marketAdapter, const String& userName, const String& key, const String& secret);
  Market(ServerHandler& serverHandler, User& user, const Variant& variant);
  ~Market();
  void_t toVariant(Variant& variant);

  bool_t start();
  bool_t stop();

  bool_t registerClient(ClientHandler& client, bool_t adapter);
  void_t unregisterClient(ClientHandler& client);

  uint32_t getId() const {return __id;}
  MarketAdapter* getMarketAdapter() const {return marketAdapter;}
  const String& getUserName() const {return userName;}
  const String& getKey() const {return key;}
  const String& getSecret() const {return secret;}
  BotProtocol::Market::State getState() const {return state;}
  ClientHandler* getAdapaterClient() const {return adapterClient;}

  uint32_t createRequestId(BotProtocol::EntityType entityType, uint32_t userRequestId, ClientHandler& client);
  bool_t removeRequestId(BotProtocol::EntityType entityType, uint32_t requestId, uint32_t& userRequestId, ClientHandler*& client);

  void_t updateTransaction(const BotProtocol::Transaction& transaction) {transactions.append(transaction.entityId, transaction);}
  const HashMap<uint32_t, BotProtocol::Transaction>& getTransactions() const {return transactions;}
  void_t deleteTransaction(uint32_t id) {transactions.remove(id);}

  void_t updateOrder(const BotProtocol::Order& order) {orders.append(order.entityId, order);}
  const HashMap<uint32_t, BotProtocol::Order>& getOrders() const {return orders;}
  void_t deleteOrder(uint32_t id) {orders.remove(id);}

  void_t updateBalance(const BotProtocol::MarketBalance& balance) {this->balance = balance;}
  const BotProtocol::MarketBalance& getBalance() const {return balance;}

  void_t send(ClientHandler* client = 0);
  void_t sendEntity(const void_t* data, size_t size);
  void_t removeEntity(BotProtocol::EntityType type, uint32_t id);

private:
  class ClientInfo
  {
  public:
    HashSet<uint32_t> responseIds;
  };

  class RequestId
  {
  public:
    ClientHandler* client;
    BotProtocol::EntityType entityType;
    uint32_t userRequestId;
  };

private:
  ServerHandler& serverHandler;
  User& user;
  uint32_t __id;
  MarketAdapter* marketAdapter;
  String userName;
  String key;
  String secret;
  BotProtocol::Market::State state;
  Process process;
  uint32_t pid;
  ClientHandler* adapterClient;
  HashMap<ClientHandler*, ClientInfo> clients;
  HashMap<uint32_t, BotProtocol::Transaction> transactions;
  HashMap<uint32_t, BotProtocol::Order> orders;
  BotProtocol::MarketBalance balance;
  HashMap<uint32_t, RequestId> requestIds;
  uint32_t nextRequestId;
};
