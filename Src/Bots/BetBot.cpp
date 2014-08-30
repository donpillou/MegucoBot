
#include <nstd/Map.h>
#include <nstd/Math.h>

#include "BetBot.h"

#define DEFAULT_BUY_PROFIT_GAIN 0.6
#define DEFAULT_SELL_PROFIT_GAIN 0.6
#define DEFAULT_BUY_COOLDOWN (60 * 60)
#define DEFAULT_BUY_TIMEOUT (6 * 60 * 60)
#define DEFAULT_SELL_COOLDOWN (60 * 60)
#define DEFAULT_SELL_TIMEOUT (6 * 60 * 60)
#define DEFAULT_MIN_BET 7.

#define DEFAULT_BUY_PREDICT_TIME (2 * 60 * 60)
#define DEFAULT_SELL_PREDICT_TIME (2 * 60 * 60)


BetBot::Session::Session(Broker& broker) : broker(broker), buyInOrderId(0), sellInOrderId(0), lastBuyInTime(0), lastSellInTime(0)
{
  broker.registerProperty("Sell Profit Gain", DEFAULT_SELL_PROFIT_GAIN);
  broker.registerProperty("Buy Profit Gain", DEFAULT_BUY_PROFIT_GAIN);
  broker.registerProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  broker.registerProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
  broker.registerProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  broker.registerProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
  broker.registerProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
  broker.registerProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
  broker.registerProperty(String("Balance ") + broker.getCurrencyBase(), 0, BotProtocol::SessionProperty::none, broker.getCurrencyBase());
  broker.registerProperty(String("Balance ") + broker.getCurrencyComm(), 0, BotProtocol::SessionProperty::none, broker.getCurrencyComm());
  broker.registerProperty(String("Available Balance ") + broker.getCurrencyBase(), 0, BotProtocol::SessionProperty::readOnly, broker.getCurrencyBase());
  broker.registerProperty(String("Available Balance ") + broker.getCurrencyComm(), 0, BotProtocol::SessionProperty::readOnly, broker.getCurrencyComm());
  broker.registerProperty("Min Bet", DEFAULT_MIN_BET, BotProtocol::SessionProperty::none, broker.getCurrencyBase());

  balanceBase = broker.getProperty(String("Balance ") + broker.getCurrencyBase(), 0);
  balanceComm = broker.getProperty(String("Balance ") + broker.getCurrencyComm(), 0);

  updateAvailableBalance();
}

void_t BetBot::Session::updateAvailableBalance()
{
  availableBalanceBase = balanceBase;
  availableBalanceComm = balanceComm;
  const HashMap<uint32_t, BotProtocol::SessionAsset>& assets = broker.getAssets();
  for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const BotProtocol::SessionAsset& asset = *i;
    // todo: allow reinvesting assets!
    availableBalanceComm -= asset.balanceComm;
    availableBalanceBase -= asset.balanceBase;
  }
  const HashMap<uint32_t, BotProtocol::Order>& orders = broker.getOrders();
  for(HashMap<uint32_t, BotProtocol::Order>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
  {
    const BotProtocol::Order& order = *i;
    if(order.entityId == buyInOrderId || order.entityId == sellInOrderId)
    {
      if(order.type == BotProtocol::Order::buy)
        availableBalanceBase -= order.total;
      else
        availableBalanceComm -= order.amount;
    }
  }
  broker.setProperty(String("Available Balance ") + broker.getCurrencyBase(), availableBalanceBase, BotProtocol::SessionProperty::readOnly, broker.getCurrencyBase());
  broker.setProperty(String("Available Balance ") + broker.getCurrencyComm(), availableBalanceComm, BotProtocol::SessionProperty::readOnly, broker.getCurrencyComm());
}

void_t BetBot::Session::applyBalanceUpdate(double base, double comm)
{
  balanceBase += base;
  balanceComm += comm;
  broker.setProperty(String("Balance ") + broker.getCurrencyBase(), balanceBase, BotProtocol::SessionProperty::none, broker.getCurrencyBase());
  broker.setProperty(String("Balance ") + broker.getCurrencyComm(), balanceComm, BotProtocol::SessionProperty::none, broker.getCurrencyComm());
  updateAvailableBalance();
}

double BetBot::Session::getBuyInBase(double currentPrice, const Values& values) const
{
  int bottomness = 1;
  if(currentPrice <= values.regressions[regression24h].min)
      ++bottomness;

  double botValueBase = balanceBase + balanceComm * currentPrice;
  double maxBase = botValueBase / 10.;
  double base = Math::min(availableBalanceBase / 2., maxBase) * 0.5 * bottomness;
  if(base < broker.getProperty(String("Min Bet"), DEFAULT_MIN_BET))
    return 0;
  return base;
}

double BetBot::Session::getSellInComm(double currentPrice, const Values& values) const
{
  int topness = 1;
  if(currentPrice >= values.regressions[regression24h].max)
      ++topness;

  double botValueComm = balanceComm + balanceBase / currentPrice;
  double maxComm = botValueComm / 10.;
  double comm = Math::min(availableBalanceComm / 2., maxComm) * 0.5 * topness;
  if(comm * currentPrice < broker.getProperty(String("Min Bet"), DEFAULT_MIN_BET))
    return 0;
  return comm;
}

void BetBot::Session::handleTrade(const DataProtocol::Trade& trade, const Values& values)
{
  checkAssetBuy(trade);
  checkAssetSell(trade);
  checkBuyIn(trade, values);
  checkSellIn(trade, values);
}

void BetBot::Session::handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
  String message;
  message.printf("Bought %.08f @ %.02f", transaction.amount, transaction.price);
  broker.warning(message);

  applyBalanceUpdate(-transaction.total, transaction.amount);

  if(orderId == buyInOrderId)
  {
    BotProtocol::SessionAsset sessionAsset;
    sessionAsset.entityId = 0;
    sessionAsset.entityType = BotProtocol::sessionAsset;
    sessionAsset.type = BotProtocol::SessionAsset::buy;
    sessionAsset.state = BotProtocol::SessionAsset::waitSell;
    sessionAsset.date = transaction.date;
    sessionAsset.price = transaction.price;
    sessionAsset.balanceComm = transaction.amount;
    sessionAsset.balanceBase = 0.;
    double fee = 0.005;
    sessionAsset.profitablePrice = transaction.price * (1. + fee * 2.);
    double sellProfitGain = broker.getProperty("Sell Profit Gain", DEFAULT_SELL_PROFIT_GAIN);
    sessionAsset.flipPrice = transaction.price * (1. + fee * (1. + sellProfitGain) * 2.);
    sessionAsset.orderId = 0;
    broker.createAsset(sessionAsset);

    buyInOrderId = 0;
    resetBetOrders();
    lastBuyInTime = transaction.date;
    updateAvailableBalance();
  }
  else
  {
    const HashMap<uint32_t, BotProtocol::SessionAsset>& assets = broker.getAssets();
    for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
    {
      const BotProtocol::SessionAsset& asset = *i;
      if(asset.state == BotProtocol::SessionAsset::buying && asset.orderId == orderId)
      {
        broker.removeAsset(asset.entityId);
        updateAvailableBalance();
        break;
      }
    }
  }
}

void BetBot::Session::handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
  String message;
  message.printf("Sold %.08f @ %.02f", transaction.amount, transaction.price);
  broker.warning(message);

  applyBalanceUpdate(transaction.total, -transaction.amount);

  if(orderId == sellInOrderId)
  {
    BotProtocol::SessionAsset sessionAsset;
    sessionAsset.entityId = 0;
    sessionAsset.entityType = BotProtocol::sessionAsset;
    sessionAsset.type = BotProtocol::SessionAsset::sell;
    sessionAsset.state = BotProtocol::SessionAsset::waitBuy;
    sessionAsset.date = transaction.date;
    sessionAsset.price = transaction.price;
    sessionAsset.balanceComm = 0.;
    sessionAsset.balanceBase = transaction.total;
    double fee = 0.005;
    sessionAsset.profitablePrice = transaction.price / (1. + fee * 2.);
    double buyProfitGain = broker.getProperty("Buy Profit Gain", DEFAULT_BUY_PROFIT_GAIN);
    sessionAsset.flipPrice = transaction.price / (1. + fee * (1. + buyProfitGain) * 2.);
    sessionAsset.orderId = 0;
    broker.createAsset(sessionAsset);

    sellInOrderId = 0;
    resetBetOrders();
    lastSellInTime = transaction.date;
    updateAvailableBalance();
  }
  else
  {
    const HashMap<uint32_t, BotProtocol::SessionAsset>& assets = broker.getAssets();
    for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
    {
      const BotProtocol::SessionAsset& asset = *i;
      if(asset.state == BotProtocol::SessionAsset::selling && asset.orderId == orderId)
      {
        broker.removeAsset(asset.entityId);
        updateAvailableBalance();
        break;
      }
    }
  }
}

void_t BetBot::Session::handleBuyTimeout(uint32_t orderId)
{
  if(orderId == buyInOrderId)
    buyInOrderId = 0;
  else
  {
    const HashMap<uint32_t, BotProtocol::SessionAsset>& assets = broker.getAssets();
    for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
    {
      const BotProtocol::SessionAsset& asset = *i;
      if(asset.state == BotProtocol::SessionAsset::buying && asset.orderId == orderId)
      {
        BotProtocol::SessionAsset updatedAsset = asset;
        updatedAsset.state = BotProtocol::SessionAsset::waitBuy;
        updatedAsset.orderId = 0;
        broker.updateAsset(updatedAsset);
        break;
      }
    }
  }
  updateAvailableBalance();
}

void_t BetBot::Session::handleSellTimeout(uint32_t orderId)
{
  if(orderId == sellInOrderId)
    sellInOrderId = 0;
  else
  {
    const HashMap<uint32_t, BotProtocol::SessionAsset>& assets = broker.getAssets();
    for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
    {
      const BotProtocol::SessionAsset& asset = *i;
      if(asset.state == BotProtocol::SessionAsset::selling && asset.orderId == orderId)
      {
        BotProtocol::SessionAsset updatedAsset = asset;
        updatedAsset.state = BotProtocol::SessionAsset::waitSell;
        updatedAsset.orderId = 0;
        broker.updateAsset(updatedAsset);
        break;
      }
    }
  }
  updateAvailableBalance();
}

#include <cmath>
void BetBot::Session::checkBuyIn(const DataProtocol::Trade& trade, const Values& values)
{
  timestamp_t buyCooldown = (timestamp_t)broker.getProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  if((timestamp_t)trade.time - lastBuyInTime < buyCooldown * 1000)
    return; // do not buy too often

  if(buyInOrderId == 0)
  {
    //timestamp_t buyCooldown = (timestamp_t)broker.getProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
    //if(broker.getTimeSinceLastBuy() < buyCooldown * 1000)
    //  return; // do not buy too often

    if(trade.price > values.regressions[regression12h].min)
      return;
    buyInIncline = values.regressions[regression6h].incline;
    buyInStartPrice = trade.price;
    maxBuyInPrice = buyInStartPrice + buyInIncline * (timestamp_t)broker.getProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
    if(!std::isnormal(maxBuyInPrice))
      return;
    double buyInBase = getBuyInBase(trade.price, values);
    if(buyInBase == 0.)
      return;
    timestamp_t buyTimeout = (timestamp_t)broker.getProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
    if(!broker.buy(maxBuyInPrice, 0., buyInBase, buyTimeout * 1000, &buyInOrderId, 0))
      return;
    updateAvailableBalance();
  }
  else
  {
    double newBuyIncline = values.regressions[regression6h].incline;
    if(newBuyIncline < buyInIncline)
    {
      double newMaxBuyPrice = buyInStartPrice + newBuyIncline * (timestamp_t)broker.getProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
      if(!std::isnormal(newMaxBuyPrice))
        return;
      if(newMaxBuyPrice < maxBuyInPrice * (1. - 0.00002))
      {
        double buyInBase = getBuyInBase(trade.price, values);
        if(buyInBase == 0.)
          return;
        if(!broker.cancelOder(buyInOrderId))
          return;
        buyInOrderId = 0;
        updateAvailableBalance();
        maxBuyInPrice = newMaxBuyPrice;
        buyInIncline = newBuyIncline;
        timestamp_t buyTimeout = (timestamp_t)broker.getProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
        if(!broker.buy(maxBuyInPrice, 0., buyInBase, buyTimeout * 1000, &buyInOrderId, 0))
          return;
        updateAvailableBalance();
      }
    }
  }

  if(trade.price > maxBuyInPrice)
    return;

  broker.addMarker(BotProtocol::Marker::goodBuy);
}

void BetBot::Session::checkSellIn(const DataProtocol::Trade& trade, const Values& values)
{
  timestamp_t sellCooldown = (timestamp_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  if((timestamp_t)trade.time - lastSellInTime < sellCooldown * 1000)
    return; // do not sell too often

  if(sellInOrderId == 0)
  {
    //timestamp_t sellCooldown = (timestamp_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
    //if(broker.getTimeSinceLastSell() < sellCooldown * 1000)
    //  return; // do not sell too often

    if(trade.price < values.regressions[regression12h].max)
      return;
    sellInIncline = values.regressions[regression6h].incline;
    sellInStartPrice = trade.price;
    minSellInPrice = sellInStartPrice + sellInIncline * (timestamp_t)broker.getProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
    if(!std::isnormal(minSellInPrice))
      return;
    double sellInComm = getSellInComm(trade.price, values);
    if(sellInComm == 0.)
      return;
    timestamp_t sellTimeout = (timestamp_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
    if(!broker.sell(minSellInPrice, sellInComm, 0., sellTimeout * 1000, &sellInOrderId, 0))
      return;
    updateAvailableBalance();
  }
  else
  {
    double newSellIncline = values.regressions[regression6h].incline;
    if(newSellIncline > sellInIncline)
    {
      double newMinSellPrice = sellInStartPrice + newSellIncline * (timestamp_t)broker.getProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
      if(!std::isnormal(newMinSellPrice))
        return;
      if(newMinSellPrice > minSellInPrice * (1. + 0.00002))
      {
        double sellInComm = getSellInComm(trade.price, values);
        if(sellInComm == 0.)
          return;
        if(!broker.cancelOder(sellInOrderId))
          return;
        buyInOrderId = 0;
        updateAvailableBalance();
        minSellInPrice = newMinSellPrice;
        sellInIncline = newSellIncline;
        timestamp_t sellTimeout = (timestamp_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
        if(!broker.sell(minSellInPrice, sellInComm, 0., sellTimeout * 1000, &sellInOrderId, 0))
          return;
        updateAvailableBalance();
      }
    }
  }

  if(trade.price < minSellInPrice)
    return;

  broker.addMarker(BotProtocol::Marker::goodSell);
}

void_t BetBot::Session::resetBetOrders()
{
  if(buyInOrderId != 0)
    if(broker.cancelOder(buyInOrderId))
      buyInOrderId = 0;
  if(sellInOrderId != 0)
    if(broker.cancelOder(sellInOrderId))
      sellInOrderId = 0;
}

void BetBot::Session::checkAssetBuy(const DataProtocol::Trade& trade)
{
  timestamp_t buyCooldown = (timestamp_t)broker.getProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  if(broker.getTimeSinceLastBuy() < buyCooldown * 1000)
    return; // do not buy too often

  double tradePrice = trade.price;
  const HashMap<uint32_t, BotProtocol::SessionAsset>& assets = broker.getAssets();
  for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const BotProtocol::SessionAsset& asset = *i;
    if(asset.state == BotProtocol::SessionAsset::waitBuy && tradePrice <= asset.flipPrice)
    {
      BotProtocol::SessionAsset updatedAsset = asset;
      updatedAsset.state = BotProtocol::SessionAsset::buying;
      broker.updateAsset(updatedAsset);

      timestamp_t buyTimeout = (timestamp_t)broker.getProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
      if(broker.buy(tradePrice, 0., asset.balanceBase, buyTimeout * 1000, &updatedAsset.orderId, 0))
        broker.updateAsset(updatedAsset);
      else
      {
        updatedAsset.state = BotProtocol::SessionAsset::waitBuy;
        broker.updateAsset(updatedAsset);
      }
      break;
    }
  }
}

void BetBot::Session::checkAssetSell(const DataProtocol::Trade& trade)
{
  timestamp_t sellCooldown = (timestamp_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  if(broker.getTimeSinceLastSell() < sellCooldown * 1000)
    return; // do not sell too often

  double tradePrice = trade.price;
  const HashMap<uint32_t, BotProtocol::SessionAsset>& assets = broker.getAssets();
  for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const BotProtocol::SessionAsset& asset = *i;
    if(asset.state == BotProtocol::SessionAsset::waitSell && tradePrice >= asset.flipPrice)
    {
      BotProtocol::SessionAsset updatedAsset = asset;
      updatedAsset.state = BotProtocol::SessionAsset::selling;
      broker.updateAsset(updatedAsset);

      timestamp_t sellTimeout = (timestamp_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
      if(broker.sell(tradePrice, asset.balanceComm, 0., sellTimeout * 1000, &updatedAsset.orderId, 0))
        broker.updateAsset(updatedAsset);
      else
      {
        updatedAsset.state = BotProtocol::SessionAsset::waitSell;
        broker.updateAsset(updatedAsset);
      }
      break;
    }
  }
}

void_t BetBot::Session::handlePropertyUpdate(const BotProtocol::SessionProperty& property)
{
  balanceBase = broker.getProperty(String("Balance ") + broker.getCurrencyBase(), 0);
  balanceComm = broker.getProperty(String("Balance ") + broker.getCurrencyComm(), 0);

  updateAvailableBalance();
}

void_t BetBot::Session::handleAssetUpdate(const BotProtocol::SessionAsset& asset)
{
  updateAvailableBalance();
}

void_t BetBot::Session::handleAssetRemoval(const BotProtocol::SessionAsset& asset)
{
  updateAvailableBalance();
}
