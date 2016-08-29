
#pragma once

#include <nstd/HashSet.h>

#include "Tools/ZlimdbConnection.h"
#include "Tools/Bot.h"

class Broker;

class Main : public ZlimdbConnection::Callback
{
public:
  Main() : broker(0), botSession(0), lastReceivedTradeId(0) {}
  ~Main();

  bool_t connect(const String& userName, uint64_t sessionId);
  bool_t process() {return connection.process();}
  String getErrorString() {return connection.getErrorString();}

  bool_t getBrokerBalance(meguco_user_broker_balance_entity& balance);
  bool_t getBrokerOrders(List<meguco_user_broker_order_entity>& orders);
  bool_t createBrokerOrder2(Bot::Order& order);
  bool_t removeBrokerOrder(uint64_t id);

  bool_t createSessionTransaction(Bot::Transaction& transaction);
  bool_t createSessionOrder(Bot::Order& order);
  bool_t removeSessionOrder(uint64_t id);
  bool_t createSessionAsset(Bot::Asset& asset);
  bool_t updateSessionAsset(const Bot::Asset& asset);
  bool_t removeSessionAsset(uint64_t id);
  bool_t createSessionMarker(Bot::Marker& marker);
  bool_t createSessionProperty(Bot::Property& property);
  bool_t updateSessionProperty(const Bot::Property& property);
  bool_t removeSessionProperty(uint64_t id);

  bool_t addLogMessage(int64_t time, const String& message);

private:
  ZlimdbConnection connection;
  int64_t maxTradeAge;
  bool_t simulation;
  Broker* broker;
  Bot::Session* botSession;
  uint64_t lastReceivedTradeId;

  uint32_t sessionTableId;
  uint32_t transactionsTableId;
  uint32_t assetsTableId;
  uint32_t ordersTableId;
  uint32_t logTableId;
  uint32_t propertiesTableId;
  uint32_t markersTableId;
  uint32_t brokerTableId;

  uint32_t livePropertiesTableId;
  HashSet<String> liveProperties;

private:
  void_t controlUserSession(uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size);

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity) {}
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity) {}
  virtual void_t removedEntity(uint32_t tableId, uint64_t entityId) {}
  virtual void_t controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size);
};
