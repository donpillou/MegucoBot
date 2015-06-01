
#include <nstd/Console.h>
#include <nstd/Thread.h>

#include "Main.h"

#ifdef BROKER_BITSTAMPBTCUSD
#include "Brokers/BitstampBtcUsd.h"
typedef BitstampBtcUsd MarketAdapter;
#endif

int_t main(int_t argc, char_t* argv[])
{
  if(argc < 2)
  {
    Console::errorf("error: Missing market table id\n");
    return -1;
  }
  uint32_t userMarketTableId = String(argv[1], String::length(argv[1])).toUInt();

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
      Console::errorf("error: Could not connect to zlimdb server: %s\n", (const char_t*)main.getErrorString());
      continue;
    }
    Console::printf("Connected to zlimdb server.\n");

    // wait for requests
    main.process();
    Console::errorf("error: Lost connection to zlimdb server: %s\n", (const char_t*)main.getErrorString());
  }

  return 0;
}

Main::~Main()
{
  delete broker;
}

bool_t Main::connect2(uint32_t userMarketTableId)
{
  delete broker;
  broker = 0;
  orders2.clear();
  transactions2.clear();
  balance.entity.id = 0;

  if(!connection.connect(*this))
    return false;

  // get user name and market name
  Buffer buffer;
  if(!connection.query(zlimdb_table_tables, userMarketTableId, buffer))
    return false;
  if(buffer.size() < sizeof(zlimdb_table_entity))
    return false;
  zlimdb_table_entity* tableEntity = (zlimdb_table_entity*)(byte_t*)buffer;
  String tableName; // e.g. users/user1/markets/market1/market
  if(!ZlimdbConnection::getString(tableEntity->entity, sizeof(*tableEntity), tableEntity->name_size, tableName))
    return false;
  if(!tableName.startsWith("users/"))
    return false;
  const char_t* userNameStart = (const tchar_t*)tableName + 6;
  const char_t* userNameEnd = String::find(userNameStart, '/');
  if(!userNameEnd)
    return false;
  if(String::compare(userNameEnd + 1, "markets/", 8) != 0)
    return false;
  const char_t* marketNameStart = userNameEnd + 9;
  const char_t* marketNameEnd = String::find(marketNameStart, '/');
  if(!marketNameEnd)
    return false;
  if(String::compare(marketNameEnd + 1, "market") != 0)
    return false;
  String userName = tableName.substr(userNameStart - tableName, userNameEnd - userNameStart);
  String marketName = tableName.substr(marketNameStart - tableName, marketNameEnd - marketNameStart);

  // get user market
  if(!connection.query(userMarketTableId, 1, buffer))
    return false;
  if(buffer.size() < sizeof(meguco_user_market_entity))
    return false;
  meguco_user_market_entity* userMarketEntity = (meguco_user_market_entity*)(byte_t*)buffer;
  {
    String userName, key, secret;
    if(!ZlimdbConnection::getString(userMarketEntity->entity, sizeof(*userMarketEntity), userMarketEntity->user_name_size, userName))
      return false;
    if(!ZlimdbConnection::getString(userMarketEntity->entity, sizeof(*userMarketEntity) + userName.length(), userMarketEntity->key_size, key))
      return false;
    if(!ZlimdbConnection::getString(userMarketEntity->entity, sizeof(*userMarketEntity) + userName.length() + key.length(), userMarketEntity->secret_size, secret))
      return false;
    broker = new MarketAdapter(userName, key, secret);
  }

  // subscribe to orders table
  if(!connection.createTable(String("users/") + userName + "/markets/" + marketName + "/orders", userMarketOrdersTableId))
    return false;
  if(!connection.subscribe(userMarketOrdersTableId))
    return false;
  while(connection.getResponse(buffer))
  {
    void* data = (byte_t*)buffer;
    uint32_t size = buffer.size();
    for(const meguco_user_market_order_entity* order; order = (const meguco_user_market_order_entity*)zlimdb_get_entity(sizeof(meguco_user_market_order_entity), &data, &size);)
      orders2.append(order->entity.id, *order);
  }
  if(connection.getErrno() != 0)
    return false;

  // get transaction table id
  if(!connection.createTable(String("users/") + userName + "/markets/" + marketName + "/transactions", userMarketTransactionsTableId))
    return false;
  if(!connection.query(userMarketTransactionsTableId))
    return false;
  while(connection.getResponse(buffer))
  {
    void* data = (byte_t*)buffer;
    uint32_t size = buffer.size();
    for(const meguco_user_market_transaction_entity* transaction; transaction = (const meguco_user_market_transaction_entity*)zlimdb_get_entity(sizeof(meguco_user_market_transaction_entity), &data, &size);)
      transactions2.append(transaction->entity.id, *transaction);
  }
  if(connection.getErrno() != 0)
    return false;

  // get balance table id
  if(!connection.createTable(String("users/") + userName + "/markets/" + marketName + "/balance", userMarketBalanceTableId))
    return false;

  // get log table id
  if(!connection.createTable(String("users/") + userName + "/markets/" + marketName + "/log", userMarketLogTableId))
    return false;

  return true;
}

void_t Main::addedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == userMarketOrdersTableId)
  {
    if(entity.size >= sizeof(meguco_user_market_order_entity))
      return addedUserMarketOrder(*(meguco_user_market_order_entity*)&entity);
  }
}

void_t Main::updatedEntity(uint32_t tableId, const zlimdb_entity& entity)
{
  if(tableId == userMarketOrdersTableId)
  {
    if(entity.size >= sizeof(meguco_user_market_order_entity))
      return updatedUserMarketOrder(*(meguco_user_market_order_entity*)&entity);
  }
}

void_t Main::removedEntity(uint32_t tableId, uint64_t entityId)
{
  if(tableId == userMarketOrdersTableId)
    return removedUserMarketOrder(entityId);
}

void_t Main::controlEntity(uint32_t tableId, uint64_t entityId, uint32_t controlCode, const Buffer& buffer)
{
  if(tableId == userMarketOrdersTableId)
    return controlUserMarketOrder(entityId, controlCode);
}

void_t Main::addedUserMarketOrder(const meguco_user_market_order_entity& createOrderArgs)
{
  meguco_user_market_order_entity order;
  if(createOrderArgs.state != meguco_user_market_order_opening || !broker->createOrder(createOrderArgs.entity.id, (meguco_user_market_order_type)createOrderArgs.type, createOrderArgs.price, createOrderArgs.amount, createOrderArgs.total, order))
  {
    if(createOrderArgs.state == meguco_user_market_order_opening)
      addLogMessage(meguco_log_error, broker->getLastError());
    order = createOrderArgs;
    order.state = meguco_user_market_order_error;
  }
  else
  {
    order.state = meguco_user_market_order_open;
    order.timeout = createOrderArgs.timeout;
  }
  orders2.append(order.entity.id, order);
  connection.update(userMarketOrdersTableId, order.entity);
}

void_t Main::updatedUserMarketOrder(const meguco_user_market_order_entity& updateOrderArgs)
{
  HashMap<uint64_t, meguco_user_market_order_entity>::Iterator it = orders2.find(updateOrderArgs.entity.id);
  if(it == orders2.end())
    return;
  meguco_user_market_order_entity& order = *it;
  if(Memory::compare(&order, &updateOrderArgs, sizeof(order)) == 0)
    return;

  if(updateOrderArgs.state == meguco_user_market_order_open)
  {
    // step #1 cancel current order
    if(!broker->cancelOrder(updateOrderArgs.entity.id))
      return addLogMessage(meguco_log_error, broker->getLastError());
    
    // step #2 create new order with same id
    meguco_user_market_order_entity order;
    if(!broker->createOrder(updateOrderArgs.entity.id, (meguco_user_market_order_type)updateOrderArgs.type, updateOrderArgs.price, updateOrderArgs.amount, updateOrderArgs.total, order))
    {
      addLogMessage(meguco_log_error, broker->getLastError());
      order = updateOrderArgs;
      order.state = meguco_user_market_order_error;
    }
    else
    {
      order.state = meguco_user_market_order_open;
      order.timeout = updateOrderArgs.timeout;
    }
  }
  else
    order.state = meguco_user_market_order_error;
  connection.update(userMarketOrdersTableId, order.entity);
}

void_t Main::removedUserMarketOrder(uint64_t entityId)
{
  broker->cancelOrder(entityId);
}

void_t Main::controlUserMarketOrder(uint64_t entityId, uint32_t controlCode)
{
  if(entityId != 0)
    return;

  switch(controlCode)
  {
  case meguco_user_market_order_control_refresh:
    {
      List<meguco_user_market_order_entity> orders;
      if(!broker->loadOrders(orders))
        return addLogMessage(meguco_log_error, broker->getLastError());
      HashMap<uint64_t, meguco_user_market_order_entity> ordersMapByRaw;
      for(HashMap<uint64_t, meguco_user_market_order_entity>::Iterator i = orders2.begin(), end = orders2.end(); i != end; ++i)
      {
        meguco_user_market_order_entity& order = *i;
        ordersMapByRaw.append(order.raw_id, order);
      }
      orders2.clear();
      for(List<meguco_user_market_order_entity>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
      {
        meguco_user_market_order_entity& order = *i;
        HashMap<uint64_t, meguco_user_market_order_entity>::Iterator it = ordersMapByRaw.find(order.raw_id);
        if(it == ordersMapByRaw.end() || it->entity.id == 0)
        { // add
          uint64_t id;
          if(connection.add(userMarketOrdersTableId, order.entity, id))
            order.entity.id = id;
        }
        else
        { // update
          order.entity.id = it->entity.id;
          if(Memory::compare(&*it, &order, sizeof(order)) != 0)
            connection.update(userMarketOrdersTableId, order.entity);
          ordersMapByRaw.remove(it);
        }
        orders2.append(order.entity.id, order);
      }
      for(HashMap<uint64_t, meguco_user_market_order_entity>::Iterator i = ordersMapByRaw.end(), end = ordersMapByRaw.end(); i != end; ++i)
      { // remove
        meguco_user_market_order_entity& order = *i;
        connection.remove(userMarketOrdersTableId, order.entity.id);
      }
    }
    break;
  }
}

void_t Main::controlUserMarketTransaction(uint64_t entityId, uint32_t controlCode)
{
  if(entityId != 0)
    return;

  switch(controlCode)
  {
  case meguco_user_market_transaction_control_refresh:
    {
      List<meguco_user_market_transaction_entity> transactions;
      if(!broker->loadTransactions(transactions))
        return addLogMessage(meguco_log_error, broker->getLastError());
      HashMap<uint64_t, meguco_user_market_transaction_entity> transactionsapByRaw;
      for(HashMap<uint64_t, meguco_user_market_transaction_entity>::Iterator i = transactions2.begin(), end = transactions2.end(); i != end; ++i)
      {
        meguco_user_market_transaction_entity& transaction = *i;
        transactionsapByRaw.append(transaction.raw_id, transaction);
      }
      transactions2.clear();
      for(List<meguco_user_market_transaction_entity>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
      {
        meguco_user_market_transaction_entity& transaction = *i;
        HashMap<uint64_t, meguco_user_market_transaction_entity>::Iterator it = transactionsapByRaw.find(transaction.raw_id);
        if(it == transactionsapByRaw.end() || it->entity.id == 0)
        { // add
          uint64_t id;
          if(connection.add(userMarketTransactionsTableId, transaction.entity, id))
            transaction.entity.id = id;
        }
        else
        { // update
          transaction.entity.id = it->entity.id;
          if(Memory::compare(&*it, &transaction, sizeof(transaction)) != 0)
            connection.update(userMarketTransactionsTableId, transaction.entity);
          transactionsapByRaw.remove(it);
        }
        transactions2.append(transaction.raw_id, transaction);
      }
      for(HashMap<uint64_t, meguco_user_market_transaction_entity>::Iterator i = transactionsapByRaw.end(), end = transactionsapByRaw.end(); i != end; ++i)
      { // remove
        meguco_user_market_transaction_entity& transaction = *i;
        connection.remove(userMarketTransactionsTableId, transaction.entity.id);
      }
    }
    break;
  }
}

void_t Main::controlUserMarketBalance(uint64_t entityId, uint32_t controlCode)
{
  if(entityId != 0)
    return;

  switch(controlCode)
  {
  case meguco_user_market_balance_control_refresh:
    {
      meguco_user_market_balance_entity balance;
      if(!broker->loadBalance(balance))
        return addLogMessage(meguco_log_error, broker->getLastError());
      if(this->balance.entity.id == 0)
      {
        uint64_t id;
        if(connection.add(userMarketBalanceTableId, balance.entity, id))
          balance.entity.id = id;
      }
      else
      {
        balance.entity.id = this->balance.entity.id;
        connection.update(userMarketBalanceTableId, balance.entity);
      }
      this->balance = balance;
    }
    break;
  }
}

void_t Main::addLogMessage(meguco_log_type type, const String& message)
{
  meguco_log_entity logEntity;
  ZlimdbConnection::setEntityHeader(logEntity.entity, 0, 0, sizeof(logEntity));
  logEntity.type = type;
  ZlimdbConnection::setString(logEntity.entity, logEntity.message_size, sizeof(logEntity), message);
  uint64_t id;
  connection.add(userMarketLogTableId, logEntity.entity, id);
}
