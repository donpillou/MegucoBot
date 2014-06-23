
#include <nstd/Map.h>
#include <nstd/Math.h>

#include "ItemBot.h"

ItemBot::Session::Session(Broker& broker) : broker(broker), minBuyInPrice(0.), maxSellInPrice(0.)
{
  Memory::fill(&parameters, 0, sizeof(Session::Parameters));

  //parameters.sellProfitGain = 0.8;
  //parameters.buyProfitGain = 0.6;
  
  //parameters.sellProfitGain = 0.2;
  parameters.sellProfitGain = 0.4;
  parameters.buyProfitGain = 0.2;

  //parameters.sellProfitGain = 0.;
  //parameters.buyProfitGain = 0.127171;

  updateBalance();
}

void ItemBot::Session::updateBalance()
{
  balanceBtc = broker.getBalanceComm();
  balanceUsd = broker.getBalanceBase();
  double fee = broker.getFee();

  maxSellInPrice = 0.;
  minBuyInPrice = 0.;

  List<BotProtocol::Transaction> transactions;
  broker.getSellTransactions(transactions);
  for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    balanceUsd -= transaction.amount * transaction.price * (1. + fee);
    if(maxSellInPrice == 0. || transaction.price > maxSellInPrice)
      maxSellInPrice = transaction.price;
  }

  transactions.clear();
  broker.getBuyTransactions(transactions);
  for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    balanceBtc -= transaction.amount;
    if(minBuyInPrice == 0. || transaction.price < minBuyInPrice)
      minBuyInPrice = transaction.price;
  }
}

void ItemBot::Session::setParameters(double* parameters)
{
  Memory::copy(&this->parameters, parameters, sizeof(Session::Parameters));
}

void ItemBot::Session::handle(const DataProtocol::Trade& trade, const Values& values)
{
  checkBuy(trade, values);
  checkSell(trade, values);
}

void ItemBot::Session::handleBuy(const BotProtocol::Transaction& transaction)
{
  double price = transaction.price;
  double amount = transaction.amount;

  // get sell transaction list
  List<BotProtocol::Transaction> transactions;
  broker.getSellTransactions(transactions);

  // sort sell transaction list by price
  Map<double, BotProtocol::Transaction> sortedTransactions;
  for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    sortedTransactions.insert(transaction.price, transaction);
  }

  // iterate over sorted transaction list (ascending)
  double fee = broker.getFee();
  double invest = 0.;
  for(Map<double, BotProtocol::Transaction>::Iterator i = sortedTransactions.begin(), end = sortedTransactions.end(); i != end; ++i)
  {
    BotProtocol::Transaction& transaction = *i;
    if(transaction.price >= price * (1. + fee * 2))
    {
      if(transaction.amount > amount)
      {
        invest += amount * transaction.price / (1. + fee);
        transaction.fee *= (transaction.amount - amount) / transaction.amount;
        transaction.amount -= amount;
        broker.updateTransaction(transaction);
        amount = 0.;
        break;
      }
      else
      {
        invest += transaction.amount * transaction.price / (1. + fee);
        broker.removeTransaction(transaction.entityId);
        amount -= transaction.amount;
        if(amount == 0.)
          break;
      }
    }
  }
  if(amount == 0.)
  {
    String message;
    message.printf("Earned? %.02f.", invest - price * transaction.amount * (1. + fee));
    broker.warning(message);
  }
  else
    broker.warning("Bought something without profit.");

  updateBalance();
}

void ItemBot::Session::handleSell(const BotProtocol::Transaction& transaction)
{
  double price = transaction.price;
  double amount = transaction.amount;

  // get buy transaction list
  List<BotProtocol::Transaction> transactions;
  broker.getBuyTransactions(transactions);

  // sort buy transaction list by price
  Map<double, BotProtocol::Transaction> sortedTransactions;
  for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
  {
    const BotProtocol::Transaction& transaction = *i;
    sortedTransactions.insert(transaction.price, transaction);
  }

  // iterate over sorted transaction list (descending)
  double fee = broker.getFee();
  double invest = 0.;
  if(!sortedTransactions.isEmpty())
  {
    for(Map<double, BotProtocol::Transaction>::Iterator i = --sortedTransactions.end(), begin = sortedTransactions.begin(); ; --i)
    {
      BotProtocol::Transaction& transaction = *i;
      if(price >= transaction.price * (1. + fee * 2))
      {
        if(transaction.amount > amount)
        {
          invest += amount * transaction.price * (1. + fee);
          transaction.fee *= (transaction.amount - amount) / transaction.amount;
          transaction.amount -= amount;
          broker.updateTransaction(transaction);
          amount = 0.;
          break;
        }
        else
        {
          invest += transaction.amount * transaction.price * (1. + fee);
          broker.removeTransaction(transaction.entityId);
          amount -= transaction.amount;
          if(amount == 0.)
            break;
        }
      }
      if(i == begin)
        break;
    }
  }
  if(amount == 0.)
  {
    String message;
    message.printf("sold something, Earned %.02f.", price * transaction.amount / (1. + fee) - invest);
    broker.warning(message);
  }
  else
    broker.warning("Sold something without profit.");

  updateBalance();
}

bool ItemBot::Session::isGoodBuy(const Values& values)
{
  for(int i = 0; i < (int)bellRegression2h; ++i)
    if(values.bellRegressions[i].incline > 0.)
      return false; // price is not falling enough
  return true;
}

bool ItemBot::Session::isVeryGoodBuy(const Values& values)
{
  for(int i = 0; i < (int)regression24h; ++i)
    if(values.regressions[i].incline > 0.)
      return false; // price is not falling enough
  return true;
}

bool ItemBot::Session::isGoodSell(const Values& values)
{
  for(int i = 0; i < (int)bellRegression2h; ++i)
    if(values.bellRegressions[i].incline < 0.)
      return false; // price is not rising enough
  return true;
}

bool ItemBot::Session::isVeryGoodSell(const Values& values)
{
  for(int i = 0; i < (int)regression24h; ++i)
    if(values.regressions[i].incline < 0.)
      return false; // price is not rising enough
  return true;
}


void ItemBot::Session::checkBuy(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenBuyOrderCount() > 0)
    return; // there is already an open buy order
  if(broker.getTimeSinceLastBuy() < 60 * 60 * 1000)
    return; // do not buy too often

  double fee = broker.getFee();

  if(isVeryGoodBuy(values) && balanceUsd >= trade.price * 0.02 * (1. + fee) && (minBuyInPrice == 0. || trade.price * (1. + broker.getFee() * 2.) < minBuyInPrice))
  {
    if(broker.buy(trade.price, 0.02, 60 * 60 * 1000))
      return;
    return;
  }

  if(isGoodBuy(values))
  {
    double price = trade.price;
    double fee = broker.getFee() * (1. + parameters.buyProfitGain);

    List<BotProtocol::Transaction> transactions;
    broker.getSellTransactions(transactions);
    double profitableAmount = 0.;
    for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
    {
      const BotProtocol::Transaction& transaction = *i;
      if(price * (1. + fee * 2.) < transaction.price)
        profitableAmount += transaction.amount;
    }
    if(profitableAmount >= 0.01)
    {
      if(broker.buy(trade.price, Math::max(Math::min(0.02, profitableAmount), profitableAmount * 0.5), 60 * 60 * 1000))
        return;
      return;
    }
  }
}

void ItemBot::Session::checkSell(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenSellOrderCount() > 0)
    return; // there is already an open sell order
  if(broker.getTimeSinceLastSell() < 60 * 60 * 1000)
    return; // do not sell too often

  if(isVeryGoodSell(values) && balanceBtc >= 0.02 && (maxSellInPrice == 0. || trade.price > maxSellInPrice * (1. + broker.getFee() *.2)))
  {
    if(broker.sell(trade.price, 0.02, 60 * 60 * 1000))
      return;
    return;
  }

  if(isGoodSell(values))
  {
    double price = trade.price;
    double fee = broker.getFee() * (1. + parameters.sellProfitGain);

    List<BotProtocol::Transaction> transactions;
    broker.getBuyTransactions(transactions);
    double profitableAmount = 0.;
    for(List<BotProtocol::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
    {
      const BotProtocol::Transaction& transaction = *i;
      if(price > transaction.price * (1. + fee * 2.))
        profitableAmount += transaction.amount;
    }
    if(profitableAmount >= 0.01)
    {
      if(broker.sell(trade.price, Math::max(Math::min(0.02, profitableAmount), profitableAmount * 0.5), 60 * 60 * 1000))
        return;
      return;
    }
  }
}
