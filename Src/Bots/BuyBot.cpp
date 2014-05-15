
#include "BuyBot.h"
#if 0
BuyBot::Session::Session(Broker& broker) : broker(broker), minBuyInPrice(0.), maxSellInPrice(0.)
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

void BuyBot::Session::updateBalance()
{
  balanceBtc = broker.getBalanceComm();
  balanceUsd = broker.getBalanceBase();
  double fee = broker.getFee();

  maxSellInPrice = 0.;
  minBuyInPrice = 0.;

  List<Broker::Transaction> transactions;
  broker.getSellTransactions(transactions);
  for(List<Broker::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
  {
    const Broker::Transaction& transaction = *i;
    balanceUsd -= transaction.amount * transaction.price * (1. + fee);
    if(maxSellInPrice == 0. || transaction.price > maxSellInPrice)
      maxSellInPrice = transaction.price;
  }

  transactions.clear();
  broker.getBuyTransactions(transactions);
  for(List<Broker::Transaction>::Iterator i = transactions.begin(), end = transactions.end(); i != end; ++i)
  {
    const Broker::Transaction& transaction = *i;
    balanceBtc -= transaction.amount;
    if(minBuyInPrice == 0. || transaction.price < minBuyInPrice)
      minBuyInPrice = transaction.price;
  }
}

void BuyBot::Session::setParameters(double* parameters)
{
  Memory::copy(&this->parameters, parameters, sizeof(Session::Parameters));
}

void BuyBot::Session::handle(const DataProtocol::Trade& trade, const Values& values)
{
  checkBuy(trade, values);
  checkSell(trade, values);
}

void BuyBot::Session::handleBuy(const Broker::Transaction& transaction)
{
  double price = transaction.price;
  double amount = transaction.amount;

  // get sell transaction list
  List<Broker::Transaction> transactions;
  broker.getSellTransactions(transactions);

  // sort sell transaction list by price
  QMap<double, Broker::Transaction> sortedTransactions;
  foreach(const Broker::Transaction& transaction, transactions)
    sortedTransactions.insert(transaction.price, transaction);

  // iterate over sorted transaction list (ascending)
  double fee = broker.getFee();
  double invest = 0.;
  for(QMap<double, Broker::Transaction>::Iterator i = sortedTransactions.begin(), end = sortedTransactions.end(); i != end; ++i)
  {
    Broker::Transaction& transaction = i.value();
    if(transaction.price >= price * (1. + fee * 2))
    {
      if(transaction.amount > amount)
      {
        invest += amount * transaction.price / (1. + fee);
        transaction.fee *= (transaction.amount - amount) / transaction.amount;
        transaction.amount -= amount;
        broker.updateTransaction(transaction.id, transaction);
        amount = 0.;
        break;
      }
      else
      {
        invest += transaction.amount * transaction.price / (1. + fee);
        broker.removeTransaction(transaction.id);
        amount -= transaction.amount;
        if(amount == 0.)
          break;
      }
    }
  }
  if(amount == 0.)
    broker.warning(QString("Earned? %1.").arg(DataModel::formatPrice(invest - price * transaction.amount * (1. + fee))));
  else
    broker.warning("Bought something without profit.");

  updateBalance();
}

void BuyBot::Session::handleSell(const Broker::Transaction& transaction)
{
  double price = transaction.price;
  double amount = transaction.amount;

  // get buy transaction list
  QList<Broker::Transaction> transactions;
  broker.getBuyTransactions(transactions);

  // sort buy transaction list by price
  QMap<double, Broker::Transaction> sortedTransactions;
  foreach(const Broker::Transaction& transaction, transactions)
    sortedTransactions.insert(transaction.price, transaction);

  // iterate over sorted transaction list (descending)
  double fee = broker.getFee();
  double invest = 0.;
  if(!sortedTransactions.isEmpty())
  {
    for(QMap<double, Broker::Transaction>::Iterator i = --sortedTransactions.end(), begin = sortedTransactions.begin(); ; --i)
    {
      Broker::Transaction& transaction = i.value();
      if(price >= transaction.price * (1. + fee * 2))
      {
        if(transaction.amount > amount)
        {
          invest += amount * transaction.price * (1. + fee);
          transaction.fee *= (transaction.amount - amount) / transaction.amount;
          transaction.amount -= amount;
          broker.updateTransaction(transaction.id, transaction);
          amount = 0.;
          break;
        }
        else
        {
          invest += transaction.amount * transaction.price * (1. + fee);
          broker.removeTransaction(transaction.id);
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
    broker.warning(QString("Earned %1.").arg(DataModel::formatPrice(price * transaction.amount / (1. + fee) - invest)));
  else
    broker.warning("Sold something without profit.");

  updateBalance();
}

bool BuyBot::Session::isGoodBuy(const Values& values)
{
  for(int i = 0; i < (int)BellRegressions::bellRegression2h; ++i)
    if(values.bellRegressions[i].incline > 0.)
      return false; // price is not falling enough
  return true;
}

bool BuyBot::Session::isVeryGoodBuy(const Values& values)
{
  for(int i = 0; i < (int)Regressions::regression24h; ++i)
    if(values.regressions[i].incline > 0.)
      return false; // price is not falling enough
  return true;
}

bool BuyBot::Session::isGoodSell(const Values& values)
{
  for(int i = 0; i < (int)BellRegressions::bellRegression2h; ++i)
    if(values.bellRegressions[i].incline < 0.)
      return false; // price is not rising enough
  return true;
}

bool BuyBot::Session::isVeryGoodSell(const Values& values)
{
  for(int i = 0; i < (int)Regressions::regression24h; ++i)
    if(values.regressions[i].incline < 0.)
      return false; // price is not rising enough
  return true;
}


void BuyBot::Session::checkBuy(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenBuyOrderCount() > 0)
    return; // there is already an open buy order
  if(broker.getTimeSinceLastBuy() < 60 * 60)
    return; // do not buy too often

  double fee = broker.getFee();

  if(isVeryGoodBuy(values) && balanceUsd >= trade.price * 0.02 * (1. + fee) && (minBuyInPrice == 0. || trade.price * (1. + broker.getFee() * 2.) < minBuyInPrice))
  {
    if(broker.buy(trade.price, 0.02, 60 * 60))
      return;
    return;
  }

  if(isGoodBuy(values))
  {
    double price = trade.price;
    double fee = broker.getFee() * (1. + parameters.buyProfitGain);

    QList<Broker::Transaction> transactions;
    broker.getSellTransactions(transactions);
    double profitableAmount = 0.;
    foreach(const Broker::Transaction& transaction, transactions)
    {
      if(price * (1. + fee * 2.) < transaction.price)
        profitableAmount += transaction.amount;
    }
    if(profitableAmount >= 0.01)
    {
      if(broker.buy(trade.price, qMax(qMin(0.02, profitableAmount), profitableAmount * 0.5), 60 * 60))
        return;
      return;
    }
  }
}

void BuyBot::Session::checkSell(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenSellOrderCount() > 0)
    return; // there is already an open sell order
  if(broker.getTimeSinceLastSell() < 60 * 60)
    return; // do not sell too often

  if(isVeryGoodSell(values) && balanceBtc >= 0.02 && (maxSellInPrice == 0. || trade.price > maxSellInPrice * (1. + broker.getFee() *.2)))
  {
    if(broker.sell(trade.price, 0.02, 60 * 60))
      return;
    return;
  }

  if(isGoodSell(values))
  {
    double price = trade.price;
    double fee = broker.getFee() * (1. + parameters.sellProfitGain);

    QList<Broker::Transaction> transactions;
    broker.getBuyTransactions(transactions);
    double profitableAmount = 0.;
    foreach(const Broker::Transaction& transaction, transactions)
    {
      if(price > transaction.price * (1. + fee * 2.))
        profitableAmount += transaction.amount;
    }
    if(profitableAmount >= 0.01)
    {
      if(broker.sell(trade.price, qMax(qMin(0.02, profitableAmount), profitableAmount * 0.5), 60 * 60))
        return;
      return;
    }
  }
}
#endif
