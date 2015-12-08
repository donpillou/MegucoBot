
#include <nstd/Console.h>
#include <nstd/Thread.h>
#include <nstd/Log.h>
#include <nstd/Map.h>

#include "Main.h"

#ifdef BROKER_BITSTAMPBTCUSD
#include "Brokers/BitstampBtcUsd.h"
typedef BitstampBtcUsd BrokerImpl;
#endif

int_t main(int_t argc, char_t* argv[])
{
  if(argc < 2)
  {
    Console::errorf("error: Missing market table id\n");
    return -1;
  }
  uint32_t userMarketTableId = String(argv[1], String::length(argv[1])).toUInt();

  Log::setFormat("%P> %m");

  //for(;;)
  //{
  //  bool stop = true;
  //  if(!stop)
  //    break;
  //}

  // create connection to bot server
  Main main;
  for(;; Thread::sleep(10 * 1000))
  {
    if(!main.connect2(userMarketTableId))
    {
      Log::errorf("Could not connect to zlimdb server: %s", (const char_t*)main.getErrorString());
      continue;
    }
    Log::infof("Connected to zlimdb server.");

    // wait for requests
    main.process();
    Log::errorf("Lost connection to zlimdb server: %s", (const char_t*)main.getErrorString());
  }

  return 0;
}

Main::~Main()
{
  delete broker;
}

bool_t Main::connect2(uint32_t userBrokerTableId)
{
  delete broker;
  broker = 0;
  orders2.clear();
  transactions2.clear();
  balance.entity.id = 0;

  if(!connection.connect(*this))
    return false;

  // get user name and broker id
  byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];
  if(!connection.queryEntity(zlimdb_table_tables, userBrokerTableId, *(zlimdb_entity*)buffer, sizeof(zlimdb_table_entity), ZLIMDB_MAX_MESSAGE_SIZE))
    return false;
  zlimdb_table_entity* tableEntity = (zlimdb_table_entity*)buffer;
  String tableName; // e.g. users/user1/brokers/<id>/broker
  if(!ZlimdbConnection::getString(tableEntity->entity, sizeof(*tableEntity), tableEntity->name_size, tableName))
    return false;
  if(!tableName.startsWith("users/"))
    return false;
  const char_t* userNameStart = (const tchar_t*)tableName + 6;
  const char_t* userNameEnd = String::find(userNameStart, '/');
  if(!userNameEnd)
    return false;
  if(String::compare(userNameEnd + 1, "brokers/", 8) != 0)
    return false;
  const char_t* brokerIdStart = userNameEnd + 9;
  const char_t* brokerIdEnd = String::find(brokerIdStart, '/');
  if(!brokerIdEnd)
    return false;
  if(String::compare(brokerIdEnd + 1, "broker") != 0)
    return false;
  String userName = tableName.substr(userNameStart - tableName, userNameEnd - userNameStart);
  String brokerId = tableName.substr(brokerIdStart - tableName, brokerIdEnd - brokerIdStart);

  // get and subscribe to user broker table
  if(!connection.subscribe(userBrokerTableId, 0))
    return false;
  while(connection.getResponse(buffer))
  {
    for(const meguco_user_broker_entity* userBrokerEntity = (const meguco_user_broker_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_entity));
        userBrokerEntity;
        userBrokerEntity = (const meguco_user_broker_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_entity), &userBrokerEntity->entity))
    {
      if(userBrokerEntity->entity.id != 1)
        continue;
      String userName, key, secret;
      if(!ZlimdbConnection::getString(userBrokerEntity->entity, sizeof(*userBrokerEntity), userBrokerEntity->user_name_size, userName))
        return false;
      if(!ZlimdbConnection::getString(userBrokerEntity->entity, sizeof(*userBrokerEntity) + userName.length() + 1, userBrokerEntity->key_size, key))
        return false;
      if(!ZlimdbConnection::getString(userBrokerEntity->entity, sizeof(*userBrokerEntity) + userName.length() + 1 + key.length() + 1, userBrokerEntity->secret_size, secret))
        return false;
      broker = new BrokerImpl(userName, key, secret);
    }
  }
  if(connection.getErrno() != 0)
    return false;

  // subscribe to orders table
  if(!connection.createTable(String("users/") + userName + "/brokers/" + brokerId + "/orders", userBrokerOrdersTableId))
    return false;
  if(!connection.subscribe(userBrokerOrdersTableId, 0))
    return false;
  while(connection.getResponse(buffer))
  {
    for(const meguco_user_broker_order_entity* order = (const meguco_user_broker_order_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_order_entity)); 
        order;
        order = (const meguco_user_broker_order_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_order_entity), &order->entity))
      orders2.append(order->entity.id, *order);
  }
  if(connection.getErrno() != 0)
    return false;

  // get transaction table
  if(!connection.createTable(String("users/") + userName + "/brokers/" + brokerId + "/transactions", userBrokerTransactionsTableId))
    return false;
  if(!connection.query(userBrokerTransactionsTableId))
    return false;
  while(connection.getResponse(buffer))
  {
    for(const meguco_user_broker_transaction_entity* transaction = (const meguco_user_broker_transaction_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_transaction_entity));
        transaction;
        transaction = (const meguco_user_broker_transaction_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_transaction_entity), &transaction->entity))
      transactions2.append(transaction->entity.id, *transaction);
  }
  if(connection.getErrno() != 0)
    return false;

  // get balance table id
  if(!connection.createTable(String("users/") + userName + "/brokers/" + brokerId + "/balance", userBrokerBalanceTableId))
    return false;

  // get log table id
  if(!connection.createTable(String("users/") + userName + "/brokers/" + brokerId + "/log", userBrokerLogTableId))
    return false;

  this->userBrokerTableId = userBrokerTableId;
  return true;
}
/*
void_t Main::addedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == userBrokerOrdersTableId)
  {
    if(entity.size >= sizeof(meguco_user_broker_order_entity))
      return addedUserBrokerOrder(*(meguco_user_broker_order_entity*)&entity);
  }
}
*/
/*
void_t Main::updatedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == userBrokerOrdersTableId)
  {
    if(entity.size >= sizeof(meguco_user_broker_order_entity))
      return updatedUserBrokerOrder(*(meguco_user_broker_order_entity*)&entity);
  }
}
*/
/*
void_t Main::removedEntity(uint32_t tableId, uint64_t entityId)
{
  if(tableId == userBrokerOrdersTableId)
    return removedUserBrokerOrder(entityId);
}
*/
void_t Main::controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size)
{
  if(tableId == userBrokerTableId)
    return controlUserBroker(entityId, controlCode);
}
/*
void_t Main::addedUserBrokerOrder(const meguco_user_broker_order_entity& newOrder)
{
  HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = orders2.find(newOrder.entity.id);
  if(it != orders2.end())
  {
    meguco_user_broker_order_entity& order = *it;
    if(Memory::compare(&order, &newOrder, sizeof(order)) == 0)
      return;
    connection.update(userBrokerOrdersTableId, order.entity);
    return;
  }

  if(newOrder.state == meguco_user_broker_order_submitting)
  {
    meguco_user_broker_order_entity order;
    if(!broker->createOrder(newOrder.entity.id, (meguco_user_broker_order_type)newOrder.type, newOrder.price, newOrder.amount, newOrder.total, order))
    {
      addLogMessage(meguco_log_error, broker->getLastError());
      order = newOrder;
      order.state = meguco_user_broker_order_error;
    }
    else
      order.state = meguco_user_broker_order_open;
    order.timeout = newOrder.timeout;
    connection.update(userBrokerOrdersTableId, order.entity);
    orders2.append(order.entity.id, order);
  }
  else
    orders2.append(newOrder.entity.id, newOrder);
}
*/
/*
void_t Main::updatedUserBrokerOrder(const meguco_user_broker_order_entity& updatedOrder)
{
  HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = orders2.find(updatedOrder.entity.id);
  if(it == orders2.end())
    return;
  meguco_user_broker_order_entity& order = *it;
  if(Memory::compare(&order, &updatedOrder, sizeof(order)) == 0)
    return;

  if(updatedOrder.state == meguco_user_broker_order_open)
  {
    // step #1 cancel current order
    if(!broker->cancelOrder(updatedOrder.entity.id))
      return addLogMessage(meguco_log_error, broker->getLastError());
    
    // step #2 create new order with same id
    meguco_user_broker_order_entity order;
    if(!broker->createOrder(updatedOrder.entity.id, (meguco_user_broker_order_type)updatedOrder.type, updatedOrder.price, updatedOrder.amount, updatedOrder.total, order))
    {
      addLogMessage(meguco_log_error, broker->getLastError());
      order = updatedOrder;
      order.state = meguco_user_broker_order_error;
    }
    else
      order.state = meguco_user_broker_order_open;
    order.timeout = updatedOrder.timeout;
    connection.update(userBrokerOrdersTableId, order.entity);
    orders2.append(updatedOrder.entity.id, updatedOrder);
  }
  else
    orders2.append(updatedOrder.entity.id, updatedOrder);
}
*/
/*
void_t Main::removedUserBrokerOrder(uint64_t entityId)
{
  HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = orders2.find(entityId);
  if(it == orders2.end())
    return;
  orders2.remove(entityId);
  if(!broker->cancelOrder(entityId))
  {
    meguco_user_broker_order_entity order = *it;
    order.entity.id = 0;
    order.entity.time = 0;
    uint64_t newId;
    connection.add(userBrokerOrdersTableId, order.entity, newId);
  }
}
*/
void_t Main::controlUserBroker(uint64_t entityId, uint32_t controlCode)
{
  if(entityId != 1)
    return;

  switch(controlCode)
  {
  case meguco_user_broker_control_refresh_orders:
    {
      List<meguco_user_broker_order_entity> newOrders;
      if(!broker->loadOrders(newOrders))
        return addLogMessage(meguco_log_error, broker->getLastError());
      Map<int64_t, meguco_user_broker_order_entity*> sortedNewOrders;
      for(List<meguco_user_broker_order_entity>::Iterator i = newOrders.begin(), end = newOrders.end(); i != end; ++i)
        sortedNewOrders.insert((*i).entity.time, &*i);
      HashMap<uint64_t, meguco_user_broker_order_entity> ordersMapByRaw;
      for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = orders2.begin(), end = orders2.end(); i != end; ++i)
      {
        meguco_user_broker_order_entity& order = *i;
        ordersMapByRaw.append(order.raw_id, order);
      }
      orders2.clear();
      for(Map<int64_t, meguco_user_broker_order_entity*>::Iterator i = sortedNewOrders.begin(), end = sortedNewOrders.end(); i != end; ++i)
      {
        meguco_user_broker_order_entity& order = **i;
        HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = ordersMapByRaw.find(order.raw_id);
        if(it == ordersMapByRaw.end() || it->entity.id == 0)
        { // add
          uint64_t id;
          if(connection.add(userBrokerOrdersTableId, order.entity, id))
          {
            order.entity.id = id;
            orders2.append(order.entity.id, order);
          }
        }
        else
        { // update
          order.entity.id = it->entity.id;
          if(Memory::compare(&*it, &order, sizeof(order)) != 0 &&
             connection.update(userBrokerOrdersTableId, order.entity))
            orders2.append(order.entity.id, order);
          else
            orders2.append(order.entity.id, *it);
          ordersMapByRaw.remove(it);
        }
      }
      for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = ordersMapByRaw.begin(), end = ordersMapByRaw.end(); i != end; ++i)
      { // remove
        meguco_user_broker_order_entity& order = *i;
        if(!connection.remove(userBrokerOrdersTableId, order.entity.id))
          orders2.append(order.entity.id, order);
      }
    }
    break;
  case meguco_user_broker_control_refresh_transactions:
    {
      List<meguco_user_broker_transaction_entity> newTransactions;
      if(!broker->loadTransactions(newTransactions))
        return addLogMessage(meguco_log_error, broker->getLastError());
      Map<int64_t, meguco_user_broker_transaction_entity*> sortedNewTransactions;
      for(List<meguco_user_broker_transaction_entity>::Iterator i = newTransactions.begin(), end = newTransactions.end(); i != end; ++i)
        sortedNewTransactions.insert((*i).entity.time, &*i);
      HashMap<uint64_t, meguco_user_broker_transaction_entity> transactionsapByRaw;
      for(HashMap<uint64_t, meguco_user_broker_transaction_entity>::Iterator i = transactions2.begin(), end = transactions2.end(); i != end; ++i)
      {
        meguco_user_broker_transaction_entity& transaction = *i;
        transactionsapByRaw.append(transaction.raw_id, transaction);
      }
      transactions2.clear();
      for(Map<int64_t, meguco_user_broker_transaction_entity*>::Iterator i = sortedNewTransactions.begin(), end = sortedNewTransactions.end(); i != end; ++i)
      {
        meguco_user_broker_transaction_entity& transaction = **i;
        HashMap<uint64_t, meguco_user_broker_transaction_entity>::Iterator it = transactionsapByRaw.find(transaction.raw_id);
        if(it == transactionsapByRaw.end() || it->entity.id == 0)
        { // add
          uint64_t id;
          if(connection.add(userBrokerTransactionsTableId, transaction.entity, id))
          {
            transaction.entity.id = id;
            transactions2.append(transaction.entity.id, transaction);
          }
        }
        else
        { // update
          transaction.entity.id = it->entity.id;
          if(Memory::compare(&*it, &transaction, sizeof(transaction)) != 0 &&
             connection.update(userBrokerTransactionsTableId, transaction.entity))
            transactions2.append(transaction.entity.id, transaction);
          else
            transactions2.append(transaction.entity.id, *it);
          transactionsapByRaw.remove(it);
        }
      }
      for(HashMap<uint64_t, meguco_user_broker_transaction_entity>::Iterator i = transactionsapByRaw.begin(), end = transactionsapByRaw.end(); i != end; ++i)
      { // remove
        meguco_user_broker_transaction_entity& transaction = *i;
        if(!connection.remove(userBrokerTransactionsTableId, transaction.entity.id))
          transactions2.append(transaction.entity.id, transaction);
      }
    }
    break;
  case meguco_user_broker_control_refresh_balance:
    {
      meguco_user_broker_balance_entity newBalance;
      if(!broker->loadBalance(newBalance))
        return addLogMessage(meguco_log_error, broker->getLastError());
      if(this->balance.entity.id == 0)
      {
        uint64_t id;
        if(connection.add(userBrokerBalanceTableId, newBalance.entity, id))
          newBalance.entity.id = id;
      }
      else
      {
        newBalance.entity.id = this->balance.entity.id;
        connection.update(userBrokerBalanceTableId, newBalance.entity);
      }
      this->balance = newBalance;
    }
    break;
  }
}

void_t Main::addLogMessage(meguco_log_type type, const String& message)
{
  byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];
  meguco_log_entity* logEntity = (meguco_log_entity*)buffer;
  ZlimdbConnection::setEntityHeader(logEntity->entity, 0, 0, sizeof(meguco_log_entity));
  logEntity->type = type;
  if(!ZlimdbConnection::copyString(logEntity->entity, logEntity->message_size, message, ZLIMDB_MAX_MESSAGE_SIZE))
    return;
  uint64_t id;
  connection.add(userBrokerLogTableId, logEntity->entity, id);
}
