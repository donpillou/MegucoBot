
#include <nstd/Map.h>
#include <nstd/Math.h>

#include "BetBot.h"

//#define DEFAULT_BUY_PROFIT_GAIN 0.4
//#define DEFAULT_SELL_PROFIT_GAIN 0.4
#define DEFAULT_BUY_COOLDOWN (60 * 60)
//#define DEFAULT_BUY_TIMEOUT (60 * 60)
#define DEFAULT_SELL_COOLDOWN (60 * 60)
//#define DEFAULT_SELL_TIMEOUT (60 * 60)

#define DEFAULT_BUY_PREDICT_TIME (10 * 60)
#define DEFAULT_SELL_PREDICT_TIME (10 * 60)


BetBot::Session::Session(Broker& broker) : broker(broker), buyState(0), sellState(0)
{
  //updateBalance();
  //
  //broker.registerProperty("Sell Profit Gain", DEFAULT_SELL_PROFIT_GAIN);
  //broker.registerProperty("Buy Profit Gain", DEFAULT_BUY_PROFIT_GAIN);
  broker.registerProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  //broker.registerProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
  broker.registerProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  //broker.registerProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
  broker.registerProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
  broker.registerProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
}

//void_t BetBot::Session::updateBalance()
//{
//  double balanceBase = 0.;
//  double balanceComm = 0.;
//  const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
//  for(HashMap<uint32_t, BotProtocol::SessionItem>::Iterator i = items.begin(), end = items.end(); i != end; ++i)
//  {
//    const BotProtocol::SessionItem& item = *i;
//    balanceComm += item.balanceComm;
//    balanceBase += item.balanceBase;
//  }
//  broker.setProperty(String("Balance ") + broker.getCurrencyBase(), balanceBase, BotProtocol::SessionProperty::readOnly, broker.getCurrencyBase());
//  broker.setProperty(String("Balance ") + broker.getCurrencyComm(), balanceComm, BotProtocol::SessionProperty::readOnly, broker.getCurrencyComm());
//}

void BetBot::Session::handle(const DataProtocol::Trade& trade, const Values& values)
{
  checkBuy(trade, values);
  checkSell(trade, values);
}

void BetBot::Session::handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
  String message;
  message.printf("Bought %.08f @ %.02f", transaction.amount, transaction.price);
  broker.warning(message);
}

void BetBot::Session::handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
  String message;
  message.printf("Sold %.08f @ %.02f", transaction.amount, transaction.price);
  broker.warning(message);
}

void_t BetBot::Session::handleBuyTimeout(uint32_t orderId)
{
}

void_t BetBot::Session::handleSellTimeout(uint32_t orderId)
{
}

void BetBot::Session::checkBuy(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenBuyOrderCount() > 0)
    return; // there is already an open buy order
  timestamp_t buyCooldown = (timestamp_t)broker.getProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  if(broker.getTimeSinceLastBuy() < buyCooldown * 1000)
    return; // do not buy too often

  if(buyState == 0)
  {
    if(trade.price > values.regressions[regression12h].min)
      return;
    buyState = 1;
    buyIncline = values.bellRegressions[bellRegression30m].incline;
    maxBuyPrice = trade.price + buyIncline * (timestamp_t)broker.getProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
  }
  else if(buyState == 1)
  {
    double buyIncline = values.bellRegressions[bellRegression30m].incline;
    if(buyIncline < this->buyIncline)
    {
      double newMaxBuyPrice = trade.price + buyIncline * (timestamp_t)broker.getProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
      if(newMaxBuyPrice < maxBuyPrice)
      {
        maxBuyPrice = newMaxBuyPrice;
        this->buyIncline = buyIncline;
      }
    }
  }

  if(trade.price > maxBuyPrice)
    return;

  broker.addMarker(Broker::goodBuy);
  buyState = 0;
  sellState = 0;
}

void BetBot::Session::checkSell(const DataProtocol::Trade& trade, const Values& values)
{
  if(broker.getOpenSellOrderCount() > 0)
    return; // there is already an open sell order
  timestamp_t sellCooldown = (timestamp_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  if(broker.getTimeSinceLastSell() < sellCooldown * 1000)
    return; // do not sell too often

  if(sellState == 0)
  {
    if(trade.price < values.regressions[regression12h].max)
      return;
    sellState = 1;
    sellIncline = values.bellRegressions[bellRegression30m].incline;
    minSellPrice = trade.price + sellIncline * (timestamp_t)broker.getProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
    
  }
  else if(sellState == 1)
  {
    double sellIncline = values.bellRegressions[bellRegression30m].incline;
    if(sellIncline > this->sellIncline)
    {
      double newMinSellPrice = trade.price + sellIncline * (timestamp_t)broker.getProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
      if(newMinSellPrice > minSellPrice)
      {
        minSellPrice = newMinSellPrice;
        this->sellIncline = sellIncline;
      }
    }
  }

  if(trade.price < minSellPrice)
    return;

  broker.addMarker(Broker::goodSell);
  sellState = 0;
  buyState = 0;
}
