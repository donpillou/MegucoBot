
#include "SimBroker.h"

SimBroker::SimBroker(BotConnection& botConnection, double balanceBase, double balanceComm, double fee) :
  botConnection(botConnection), balanceBase(balanceBase), balanceComm(balanceComm), fee(fee), 
  time(0), lastBuyTime(0), lastSellTime(0) {}

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
