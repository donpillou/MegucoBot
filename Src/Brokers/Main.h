
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

  bool_t connect(const String& userName, uint64_t brokerId);
  bool_t process() {return connection.process();}
  String getErrorString() const {return connection.getErrorString();}

private:
  ZlimdbConnection connection;
  Broker* broker;

  uint32_t userBrokerTableId;
  uint32_t userBrokerOrdersTableId;
  uint32_t userBrokerTransactionsTableId;
  uint32_t userBrokerBalanceTableId;
  uint32_t userBrokerLogTableId;

  HashMap<uint64_t, meguco_user_broker_order_entity> orders2;
  HashMap<uint64_t, meguco_user_broker_transaction_entity> transactions2;
  meguco_user_broker_balance_entity balance;

private: // ZlimdbConnection::Callback
  virtual void_t addedEntity(uint32_t tableId, const zlimdb_entity& entity) {}
  virtual void_t updatedEntity(uint32_t tableId, const zlimdb_entity& entity) {}
  virtual void_t removedEntity(uint32_t tableId, uint64_t entityId) {}
  virtual void_t controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size);

private:
  void_t controlUserBroker(uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size);
  void_t addLogMessage(meguco_log_type type, const String& message);
};
