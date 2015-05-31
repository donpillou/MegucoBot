
#pragma once

#include <nstd/HashMap.h>

#include <megucoprotocol.h>

#include "Tools/ZlimdbConnection.h"

class Broker;

class Main : public ZlimdbConnection::Callback
{
public:
  Main() : broker(0) {}
  ~Main();

  bool_t connect2(uint32_t userMarketTableId);
  bool_t process() {return connection.process();}
  String getErrorString() const {return connection.getErrorString();}

private:
  ZlimdbConnection connection;
  Broker* broker;

  uint32_t userMarketOrdersTableId;
  uint32_t userMarketTransactionsTableId;
  uint32_t userMarketBalanceTableId;
  uint32_t userMarketLogTableId;

  HashMap<uint64_t, meguco_user_market_order_entity> orders2;
  HashMap<uint64_t, meguco_user_market_transaction_entity> transactions2;
  meguco_user_market_balance_entity balance;

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity);
  virtual void_t removedEntity(uint32_t tableId, uint64_t entityId);
  virtual void_t controlEntity(uint32_t tableId, uint64_t entityId, uint32_t controlCode, const Buffer& buffer);

private:
  void_t addedUserMarketOrder(const meguco_user_market_order_entity& createOrderArgs);
  void_t updatedUserMarketOrder(const meguco_user_market_order_entity& updateOrderArgs);
  void_t removedUserMarketOrder(uint64_t entityId);
  void_t controlUserMarketOrder(uint64_t entityId, uint32_t controlCode);
  void_t controlUserMarketTransaction(uint64_t entityId, uint32_t controlCode);
  void_t controlUserMarketBalance(uint64_t entityId, uint32_t controlCode);
  void_t addLogMessage(meguco_log_type type, const String& message);
};