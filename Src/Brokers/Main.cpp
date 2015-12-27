
#include <nstd/Console.h>
#include <nstd/Thread.h>
#include <nstd/Log.h>
#include <nstd/Map.h>
#include <megucoprotocol.h>

#include "Main.h"

#ifdef BROKER_BITSTAMPBTCUSD
#include "Brokers/BitstampBtcUsd.h"
typedef BitstampBtcUsd BrokerImpl;
#endif

int_t main(int_t argc, char_t* argv[])
{
  if(argc < 3)
  {
    Console::errorf("error: Missing broker attributes\n");
    return -1;
  }
  String userName(argv[1], String::length(argv[1]));
  uint64_t brokerId = String::toUInt64(argv[2]);

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
    if(!main.connect(userName, brokerId))
    {
      Log::errorf("Could not connect to zlimdb server: %s", (const char_t*)main.getErrorString());
      continue;
    }
    Log::infof("Connected to zlimdb server.");

    // wait for requests
    main.process();
    Log::errorf("Lost connection to zlimdb server: %s", (const char_t*)main.getErrorString());
  }
}

Main::~Main() {delete broker;}

bool_t Main::connect(const String& userName, uint64_t brokerId)
{
  // cleanup
  delete broker;
  broker = 0;
  orders2.clear();
  transactions2.clear();
  balance.entity.id = 0;

  // establish connection to ZlimDB server
  if(!connection.connect(*this))
    return false;

  // subscribe to user broker table
  String tablePrefix = String("users/") + userName + "/brokers/" + String::fromUInt64(brokerId);
  if(!connection.createTable(tablePrefix + "/broker", userBrokerTableId))
    return false;
  if(!connection.subscribe(userBrokerTableId, zlimdb_subscribe_flag_responder))
    return false;
  String brokerUserName, brokerKey, brokerSecret;
  byte_t buffer[ZLIMDB_MAX_MESSAGE_SIZE];
  while(connection.getResponse(buffer))
  {
    for(const meguco_user_broker_entity* userBrokerEntity = (const meguco_user_broker_entity*)zlimdb_get_first_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_entity));
        userBrokerEntity;
        userBrokerEntity = (const meguco_user_broker_entity*)zlimdb_get_next_entity((const zlimdb_header*)buffer, sizeof(meguco_user_broker_entity), &userBrokerEntity->entity))
    {
      if(userBrokerEntity->entity.id != 1)
        continue;
      size_t offset = sizeof(meguco_user_broker_entity);
      if(!ZlimdbConnection::getString(userBrokerEntity->entity, userBrokerEntity->user_name_size, brokerUserName, offset) ||
        !ZlimdbConnection::getString(userBrokerEntity->entity, userBrokerEntity->key_size, brokerKey, offset) ||
        !ZlimdbConnection::getString(userBrokerEntity->entity, userBrokerEntity->secret_size, brokerSecret, offset))
        return false;
    }
  }
  if(connection.getErrno() != 0)
    return false;
  broker = new BrokerImpl(brokerUserName, brokerKey, brokerSecret);

  // load orders table
  if(!connection.createTable(tablePrefix + "/orders", userBrokerOrdersTableId))
    return false;
  if(!connection.query(userBrokerOrdersTableId))
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

  // load transactions table
  if(!connection.createTable(tablePrefix + "/transactions", userBrokerTransactionsTableId))
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
  if(!connection.createTable(tablePrefix + "/balance", userBrokerBalanceTableId))
    return false;

  // get log table id
  if(!connection.createTable(tablePrefix + "/log", userBrokerLogTableId))
    return false;

  return true;
}

void_t Main::controlEntity(uint32_t tableId, uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size)
{
  if(tableId == userBrokerTableId)
    return controlUserBroker(requestId, entityId, controlCode, data, size);
  else
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
}

void_t Main::controlUserBroker(uint32_t requestId, uint64_t entityId, uint32_t controlCode, const byte_t* data, size_t size)
{
  switch(controlCode)
  {
  case meguco_user_broker_control_refresh_orders:
    {
      List<meguco_user_broker_order_entity> newOrders;
      if(!broker->loadOrders(newOrders))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        return (void_t)connection.sendControlResponse(requestId, 0);
      }
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
    return (void_t)connection.sendControlResponse(requestId, 0, 0);
  case meguco_user_broker_control_refresh_transactions:
    {
      List<meguco_user_broker_transaction_entity> newTransactions;
      if(!broker->loadTransactions(newTransactions))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        return (void_t)connection.sendControlResponse(requestId, 0);
      }
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
    return (void_t)connection.sendControlResponse(requestId, 0, 0);
  case meguco_user_broker_control_refresh_balance:
    {
      meguco_user_broker_balance_entity newBalance;
      if(!broker->loadBalance(newBalance))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        return (void_t)connection.sendControlResponse(requestId, 0);
      }
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
    return (void_t)connection.sendControlResponse(requestId, (const byte_t*)&balance, sizeof(meguco_user_broker_balance_entity));
  case meguco_user_broker_control_create_order:
    if(size < sizeof(meguco_user_broker_order_entity))
      return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
    {
      const meguco_user_broker_order_entity& args = *(const meguco_user_broker_order_entity*)data;
      meguco_user_broker_order_entity newOrder;
      if(!broker->createOrder(0, (meguco_user_broker_order_type)args.type, args.price, args.amount, args.total, newOrder))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        return (void_t)connection.sendControlResponse(requestId, 0);
      }
      newOrder.state = meguco_user_broker_order_open;
      newOrder.timeout = args.timeout;

      // add entity to user brokers table
      uint64_t id;
      if(!connection.add(userBrokerOrdersTableId, newOrder.entity, id))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      newOrder.entity.id = id;
      orders2.append(id, newOrder);

      // return entity id
      return (void_t)connection.sendControlResponse(requestId, (const byte_t*)&id, sizeof(uint64_t));
    }
  case meguco_user_broker_control_cancel_order:
  case meguco_user_broker_control_remove_order:
    {
      HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = orders2.find(entityId);
      if(it == orders2.end())
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_entity_not_found);
      meguco_user_broker_order_entity& order = *it;

      // cancel order
      if(order.state == meguco_user_broker_order_open)
      {
        if(!broker->cancelOrder(order.entity.id))
        {
          addLogMessage(meguco_log_error, broker->getLastError());
          return (void_t)connection.sendControlResponse(requestId, 0);
        }
      }

      // update order state
      if(controlCode == meguco_user_broker_control_cancel_order)
      {
        order.state = meguco_user_broker_order_canceled;
        if(!connection.update(userBrokerOrdersTableId, order.entity))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
      }
      else
      {
        if(!connection.remove(userBrokerOrdersTableId, order.entity.id))
          return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());
        orders2.remove(it);
      }

      // send answer
      return (void_t)connection.sendControlResponse(requestId, 0, 0);
    }
  case meguco_user_broker_control_update_order:
    if(size < sizeof(meguco_user_broker_order_control_update_params))
      return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
    {
      HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator it = orders2.find(entityId);
      if(it == orders2.end())
        return (void_t)connection.sendControlResponse(requestId, zlimdb_error_entity_not_found);
      meguco_user_broker_order_entity& order = *it;

      const meguco_user_broker_order_control_update_params& params = *(const meguco_user_broker_order_control_update_params*)data;

      // cancel order
      if(!broker->cancelOrder(order.entity.id))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        return (void_t)connection.sendControlResponse(requestId, 0);
      }

      // create new order with same id
      meguco_user_broker_order_entity newOrder;
      if(!broker->createOrder(order.entity.id, (meguco_user_broker_order_type)order.type, params.price, params.amount, params.total, newOrder))
      {
        addLogMessage(meguco_log_error, broker->getLastError());
        order.state = meguco_user_broker_order_error;
      }
      else
      {
        newOrder.timeout = order.timeout;
        order = newOrder;
        order.state = meguco_user_broker_order_open;
      }

      // update order state in table
      if(!connection.update(userBrokerOrdersTableId, order.entity))
        return (void_t)connection.sendControlResponse(requestId, (uint16_t)connection.getErrno());

      // send answer
      if(order.state == meguco_user_broker_order_error)
        return (void_t)connection.sendControlResponse(requestId, 0);
      else
        return (void_t)connection.sendControlResponse(requestId, 0, 0);
    }
  default:
    return (void_t)connection.sendControlResponse(requestId, zlimdb_error_invalid_request);
  }
}

void_t Main::addLogMessage(meguco_log_type type, const String& message)
{
  byte_t buffer[ZLIMDB_MAX_ENTITY_SIZE];
  meguco_log_entity* logEntity = (meguco_log_entity*)buffer;
  ZlimdbConnection::setEntityHeader(logEntity->entity, 0, 0, sizeof(meguco_log_entity));
  logEntity->type = type;
  if(!ZlimdbConnection::copyString(message, logEntity->entity, logEntity->message_size, ZLIMDB_MAX_ENTITY_SIZE))
    return;
  uint64_t id;
  connection.add(userBrokerLogTableId, logEntity->entity, id);
}
