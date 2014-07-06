
#include <nstd/Map.h>
#include <nstd/Math.h>

#include "ItemBot.h"

ItemBot::Session::Session(Broker& broker) : broker(broker)//, minBuyInPrice(0.), maxSellInPrice(0.)
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

void ItemBot::Session::handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction2)
{
  String message;
  message.printf("Bought %.08f @ %.02f", transaction2.amount, transaction2.price);
  broker.warning(message);

  const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.state == BotProtocol::SessionItem::buying && item.orderId == orderId)
    {
      BotProtocol::SessionItem updatedItem = item;
      updatedItem.state = BotProtocol::SessionItem::waitSell;
      updatedItem.orderId = 0;
      updatedItem.price = transaction2.price;
      updatedItem.amount = transaction2.amount;
      double fee = broker.getFee();
      updatedItem.profitablePrice = transaction2.price * (1. + fee * 2.);
      updatedItem.flipPrice = transaction2.price * (1. + fee * (1. + parameters.sellProfitGain) * 2.);

      broker.updateItem(updatedItem);
      break;
    }
  }
}

void ItemBot::Session::handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction2)
{
  String message;
  message.printf("Sold %.08f @ %.02f", transaction2.amount, transaction2.price);
  broker.warning(message);

  const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.state == BotProtocol::SessionItem::selling && item.orderId == orderId)
    {
      BotProtocol::SessionItem updatedItem = item;
      updatedItem.state = BotProtocol::SessionItem::waitBuy;
      updatedItem.orderId = 0;
      updatedItem.price = transaction2.price;
      updatedItem.amount = transaction2.amount;
      double fee = broker.getFee();
      updatedItem.profitablePrice = transaction2.price / (1. + fee * 2.);
      updatedItem.flipPrice = transaction2.price / (1. + fee * (1. + parameters.buyProfitGain) * 2.);

      broker.updateItem(updatedItem);
      break;
    }
  }
}

void_t ItemBot::Session::handleBuyTimeout(uint32_t orderId)
{
  const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.state == BotProtocol::SessionItem::buying && item.orderId == orderId)
    {
      BotProtocol::SessionItem updatedItem = item;
      updatedItem.state = BotProtocol::SessionItem::waitBuy;
      updatedItem.orderId = 0;
      broker.updateItem(updatedItem);
      break;
    }
  }
}

void_t ItemBot::Session::handleSellTimeout(uint32_t orderId)
{
  const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.state == BotProtocol::SessionItem::selling && item.orderId == orderId)
    {
      BotProtocol::SessionItem updatedItem = item;
      updatedItem.state = BotProtocol::SessionItem::waitSell;
      updatedItem.orderId = 0;
      broker.updateItem(updatedItem);
      break;
    }
  }
}

void ItemBot::Session::checkBuy(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenBuyOrderCount() > 0)
    return; // there is already an open buy order
  if(broker.getTimeSinceLastBuy() < 60 * 60 * 1000)
    return; // do not buy too often

  double tradePrice = trade.price;
  const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.state == BotProtocol::SessionItem::waitBuy && tradePrice <= item.flipPrice)
    {
      BotProtocol::SessionItem updatedItem = item;
      updatedItem.state = BotProtocol::SessionItem::buying;
      broker.updateItem(updatedItem);

      double amount = item.amount;
      if(item.price != 0)
      {
        double newPrice = tradePrice;
        double oldPrice = item.price;
        double oldPayedFee = Math::ceil(oldPrice * item.amount * broker.getFee() * 100.) / 100.;
        double newFeeToPay = oldPayedFee;
      reccomputeAmount:
        double oldTotal = oldPrice * item.amount - oldPayedFee - newFeeToPay;
        amount = oldTotal / tradePrice;
        double newNewFeeToPay = Math::ceil(tradePrice * amount * broker.getFee() * 100.) / 100.;
        if(newNewFeeToPay < newFeeToPay)
        {
          newFeeToPay = newNewFeeToPay;
          goto reccomputeAmount;
        }
        else
        {
          newFeeToPay = newNewFeeToPay;
          oldTotal = oldPrice * item.amount - oldPayedFee - newFeeToPay;
          amount = oldTotal / tradePrice;
        }
      }

      if(broker.buy(tradePrice, amount, 60 * 60 * 1000, &updatedItem.orderId))
        broker.updateItem(updatedItem);
      else
      {
        updatedItem.state = BotProtocol::SessionItem::waitBuy;
        broker.updateItem(updatedItem);
      }
      break;
    }
  }
}

void ItemBot::Session::checkSell(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenSellOrderCount() > 0)
    return; // there is already an open sell order
  if(broker.getTimeSinceLastSell() < 60 * 60 * 1000)
    return; // do not sell too often

  double tradePrice = trade.price;
  const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
  {
    const BotProtocol::SessionItem& item = *i;
    if(item.state == BotProtocol::SessionItem::waitSell && tradePrice >= item.flipPrice)
    {
      BotProtocol::SessionItem updatedItem = item;
      updatedItem.state = BotProtocol::SessionItem::selling;
      broker.updateItem(updatedItem);
      if(broker.sell(tradePrice, item.amount, 60 * 60 * 1000, &updatedItem.orderId))
        broker.updateItem(updatedItem);
      else
      {
        updatedItem.state = BotProtocol::SessionItem::waitSell;
        broker.updateItem(updatedItem);
      }
      break;
    }
  }
}
