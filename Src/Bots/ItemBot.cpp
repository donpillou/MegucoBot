
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

void ItemBot::Session::handleBuy(const BotProtocol::Transaction& transaction2)
{
  String message;
  message.printf("Bought %.08f @ %.02f", transaction2.amount, transaction2.price);
  broker.warning(message);

  double price = transaction2.price;
  double amount = transaction2.amount;

  // get sell transaction list
  List<BotProtocol::SessionItem> items;
  broker.getSellItems(items);

  // sort sell transaction list by price
  Map<double, BotProtocol::SessionItem> sortedItems;
  for(List<BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    sortedItems.insert(item.profitablePrice, item);
  }

  // iterate over sorted transaction list (ascending)
  double initialBuyFlipAmount = 0.;
  double initialSellFlipAmount = 0.;
  for(Map<double, BotProtocol::SessionItem>::Iterator i = sortedItems.begin(), end = sortedItems.end(); i != end; ++i)
  {
    BotProtocol::SessionItem& item = *i;
    if(item.profitablePrice >= price)
    {
      if(item.amount > amount)
      {
        if(item.type == BotProtocol::SessionItem::buy)
          initialBuyFlipAmount += amount;
        else
          initialSellFlipAmount += amount;

        item.amount -= amount;
        broker.updateItem(item);
        amount = 0.;
        break;
      }
      else
      {
        if(item.type == BotProtocol::SessionItem::buy)
          initialBuyFlipAmount += item.amount;
        else
          initialSellFlipAmount += item.amount;

        broker.removeItem(item.entityId);
        amount -= item.amount;
        if(amount == 0.)
          break;
      }
    }
  }

  if(initialBuyFlipAmount != 0.)
  {
    BotProtocol::SessionItem item;
    item.entityType = BotProtocol::sessionItem;
    item.type = BotProtocol::SessionItem::buy;
    item.state = BotProtocol::SessionItem::waitBuy;
    item.price = transaction2.price;
    item.amount = initialBuyFlipAmount;
    double fee = broker.getFee();
    item.profitablePrice = price * (1. + fee * 2.);
    item.flipPrice = price * (1. + fee * (1. + parameters.sellProfitGain) * 2.);;
  }
  if(initialSellFlipAmount != 0.)
  {
    BotProtocol::SessionItem item;
    item.entityType = BotProtocol::sessionItem;
    item.type = BotProtocol::SessionItem::sell;
    item.state = BotProtocol::SessionItem::waitBuy;
    item.price = transaction2.price;
    item.amount = initialSellFlipAmount;
    double fee = broker.getFee();
    item.profitablePrice = price * (1. + fee * 2.);
    item.flipPrice = price * (1. + fee * (1. + parameters.sellProfitGain) * 2.);;
  }
}

void ItemBot::Session::handleSell(const BotProtocol::Transaction& transaction2)
{
  String message;
  message.printf("Sold %.08f @ %.02f", transaction2.amount, transaction2.price);
  broker.warning(message);

  double price = transaction2.price;
  double amount = transaction2.amount;

  // get buy transaction list
  List<BotProtocol::SessionItem> items;
  broker.getBuyItems(items);

  // sort buy transaction list by price
  Map<double, BotProtocol::SessionItem> sortedItems;
  for(List<BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    sortedItems.insert(item.profitablePrice, item);
  }

  // iterate over sorted transaction list (descending)
  double initialBuyFlipAmount = 0.;
  double initialSellFlipAmount = 0.;
  if(!sortedItems.isEmpty())
  {
    for(Map<double, BotProtocol::SessionItem>::Iterator i = --sortedItems.end(), begin = sortedItems.begin(); ; --i)
    {
      BotProtocol::SessionItem& item = *i;
      if(item.profitablePrice <= price)
      {
        if(item.amount > amount)
        {
          if(item.type == BotProtocol::SessionItem::buy)
            initialBuyFlipAmount += amount;
          else
            initialSellFlipAmount += amount;

          item.amount -= amount;
          broker.updateItem(item);
          amount = 0.;
          break;
        }
        else
        {
          if(item.type == BotProtocol::SessionItem::buy)
            initialBuyFlipAmount += item.amount;
          else
            initialSellFlipAmount += item.amount;

          broker.removeItem(item.entityId);
          amount -= item.amount;
          if(amount == 0.)
            break;
        }
      }
      if(i == begin)
        break;
    }
  }

  if(initialBuyFlipAmount != 0.)
  {
    BotProtocol::SessionItem item;
    item.entityType = BotProtocol::sessionItem;
    item.type = BotProtocol::SessionItem::buy;
    item.state = BotProtocol::SessionItem::waitSell;
    item.price = transaction2.price;
    item.amount = initialBuyFlipAmount;
    double fee = broker.getFee();
    item.profitablePrice = price / (1. + fee * 2.);
    item.flipPrice = price / (1. + fee * (1. + parameters.buyProfitGain) * 2.);
  }
  if(initialSellFlipAmount != 0.)
  {
    BotProtocol::SessionItem item;
    item.entityType = BotProtocol::sessionItem;
    item.type = BotProtocol::SessionItem::sell;
    item.state = BotProtocol::SessionItem::waitSell;
    item.price = transaction2.price;
    item.amount = initialBuyFlipAmount;
    double fee = broker.getFee();
    item.profitablePrice = price / (1. + fee * 2.);
    item.flipPrice = price / (1. + fee * (1. + parameters.buyProfitGain) * 2.);
  }
}

void ItemBot::Session::checkBuy(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenBuyOrderCount() > 0)
    return; // there is already an open buy order
  if(broker.getTimeSinceLastBuy() < 60 * 60 * 1000)
    return; // do not buy too often

  double tradePrice = trade.price;
  double profitableAmount = 0.;
  List<BotProtocol::SessionItem> items;
  broker.getSellItems(items);
  for(List<BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.flipPrice < tradePrice)
      profitableAmount += item.amount;
  }
  if(profitableAmount >= 0.01)
  {
    if(broker.buy(trade.price, Math::max(Math::min(0.02, profitableAmount), profitableAmount * 0.5), 60 * 60 * 1000))
      return;
    return;
  }
}

void ItemBot::Session::checkSell(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenSellOrderCount() > 0)
    return; // there is already an open sell order
  if(broker.getTimeSinceLastSell() < 60 * 60 * 1000)
    return; // do not sell too often

  double tradePrice = trade.price;
  double profitableAmount = 0.;
  List<BotProtocol::SessionItem> items;
  broker.getBuyItems(items);
  for(List<BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(tradePrice > item.flipPrice)
      profitableAmount += item.amount;
  }
  if(profitableAmount >= 0.01)
  {
    if(broker.sell(trade.price, Math::max(Math::min(0.02, profitableAmount), profitableAmount * 0.5), 60 * 60 * 1000))
      return;
    return;
  }
}
