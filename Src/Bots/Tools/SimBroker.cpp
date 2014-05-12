
#include "SimBroker.h"

SimBroker::SimBroker(BotConnection& botConnection, double balanceBase, double balanceComm, double fee) :
  botConnection(botConnection), balanceBase(balanceBase), balanceComm(balanceComm), fee(fee), 
  time(0), lastBuyTime(0), lastSellTime(0) {}

void_t SimBroker::loadTransaction(const BotProtocol::Transaction& sessionTransaction)
{
  Bot::Broker::Transaction transaction;
  transaction.id = sessionTransaction.entityId;
  transaction.date = sessionTransaction.date;
  transaction.price = sessionTransaction.price;
  transaction.amount = sessionTransaction.amount;
  transaction.fee = sessionTransaction.fee;
  transaction.type = sessionTransaction.type == BotProtocol::Transaction::buy ? Bot::Broker::Transaction::Type::buy : Bot::Broker::Transaction::Type::sell;
  transactions.append(transaction.id, transaction);
}

void_t SimBroker::loadOrder(const BotProtocol::Order& order)
{
}

bool_t SimBroker::handleTrade(const DataProtocol::Trade& trade)
{
  return false;
}


bool_t SimBroker::buy(double price, double amount, timestamp_t timeout)
{
  return false;
}

bool_t SimBroker::sell(double price, double amount, timestamp_t timeout)
{
  return false;
}

uint_t SimBroker::getOpenBuyOrderCount() const
{
  return 0;
}

uint_t SimBroker::getOpenSellOrderCount() const
{
  return 0;
}

void_t SimBroker::getTransactions(List<Transaction>& transactions) const
{
}

void_t SimBroker::getBuyTransactions(List<Transaction>& transactions) const
{
}

void_t SimBroker::getSellTransactions(List<Transaction>& transactions) const
{
}

void_t SimBroker::removeTransaction(uint32_t id)
{
}

void_t SimBroker::updateTransaction(uint32_t id, const Transaction& transaction)
{
}

void_t SimBroker::warning(const String& message)
{
}
