
#include <nstd/Map.h>
#include <nstd/Math.h>

#include "BetBot.h"

#define DEFAULT_BUY_PROFIT_GAIN 0.6
#define DEFAULT_SELL_PROFIT_GAIN 0.6
#define DEFAULT_BUY_COOLDOWN (60 * 60)
#define DEFAULT_SELL_COOLDOWN (60 * 60)
#define DEFAULT_BUY_TIMEOUT (6 * 60 * 60)
#define DEFAULT_SELL_TIMEOUT (6 * 60 * 60)
#define DEFAULT_BUY_PREDICT_TIME (2 * 60 * 60)
#define DEFAULT_SELL_PREDICT_TIME (2 * 60 * 60)
#define DEFAULT_BUY_MIN_PRICE_SHIFT 0.01
#define DEFAULT_SELL_MIN_PRICE_SHIFT 0.01
#define DEFAULT_MIN_BET 7.

BetBot::Session::Session(Broker& broker) : broker(broker), buyInOrderId(0), sellInOrderId(0), lastBuyInTime(0), lastSellInTime(0), lastAssetBuyTime(0), lastAssetSellTime(0)
{
  broker.registerProperty("Buy Profit Gain", DEFAULT_BUY_PROFIT_GAIN);
  broker.registerProperty("Sell Profit Gain", DEFAULT_SELL_PROFIT_GAIN);
  broker.registerProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  broker.registerProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  broker.registerProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
  broker.registerProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
  broker.registerProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
  broker.registerProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
  broker.registerProperty("Buy Min Price Shift", DEFAULT_BUY_MIN_PRICE_SHIFT);
  broker.registerProperty("Sell Min Price Shift", DEFAULT_SELL_MIN_PRICE_SHIFT);

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
  applyBalanceUpdate(-transaction.total, transaction.amount);

  if(orderId == buyInOrderId)
  {
    String message;
    message.printf("BuyIn: @ %.02f, %.02f %s => %.08f %s", transaction.price,
      transaction.total, (const char_t*)broker.getCurrencyBase(), transaction.amount, (const char_t*)broker.getCurrencyComm());
    broker.warning(message);

    BotProtocol::SessionAsset sessionAsset;
    sessionAsset.entityId = 0;
    sessionAsset.entityType = BotProtocol::sessionAsset;
    sessionAsset.type = BotProtocol::SessionAsset::buy;
    sessionAsset.state = BotProtocol::SessionAsset::waitSell;
    sessionAsset.date = transaction.date;
    sessionAsset.price = transaction.price;
    sessionAsset.investComm = 0.;
    sessionAsset.investBase = transaction.total;
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
        double gainBase = asset.balanceBase - transaction.total;
        double gainComm = transaction.amount - asset.investComm + asset.balanceComm;

        Map<double, const BotProtocol::SessionAsset*> sortedSellAssets;
        Map<double, const BotProtocol::SessionAsset*> sortedBuyAssets;
        if(gainBase > 0.)
          for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
          {
            const BotProtocol::SessionAsset& asset = *i;
            if(asset.state == BotProtocol::SessionAsset::waitSell && asset.profitablePrice > transaction.price)
              sortedSellAssets.insert(asset.profitablePrice, &asset);
          }
        if(gainComm > 0.)
          for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
          {
            const BotProtocol::SessionAsset& asset = *i;
            if(asset.state == BotProtocol::SessionAsset::waitBuy && asset.profitablePrice < transaction.price)
              sortedBuyAssets.insert(asset.profitablePrice, &asset);
          }

        String message;
        message.printf("Buy: asset %.02f @ %.02f, %.02f %s => %.08f %s, Made %.08f %s, %.02f %s", asset.price, transaction.price,
          transaction.total, (const char_t*)broker.getCurrencyBase(), transaction.amount, (const char_t*)broker.getCurrencyComm(),
          gainComm, (const char_t*)broker.getCurrencyComm(), gainBase, (const char_t*)broker.getCurrencyBase());
        broker.warning(message);

        if(!sortedSellAssets.isEmpty())
        {
          BotProtocol::SessionAsset lowestSellAsset = *sortedSellAssets.front();

          String message;
          message.printf("Gave %.02f %s to asset %.02f", gainBase, (const char_t*)broker.getCurrencyBase(), lowestSellAsset.price);
          broker.warning(message);

          lowestSellAsset.balanceBase += gainBase;

          double fee = 0.005;
          // lowestSellAsset.balanceBase + lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee);
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase;
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase) * (1. + fee);
          // approx:
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee);
          lowestSellAsset.profitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee)) / lowestSellAsset.balanceComm;

          broker.updateAsset(lowestSellAsset);
        }
        if(!sortedBuyAssets.isEmpty())
        {
          BotProtocol::SessionAsset highestBuyAsset = *sortedBuyAssets.back();

          String message;
          message.printf("Gave %.08f %s to asset %.02f", gainComm, (const char_t*)broker.getCurrencyComm(), highestBuyAsset.price);
          broker.warning(message);

          highestBuyAsset.balanceComm += gainComm;

          double fee = 0.005;
          // highestBuyAsset.balanceComm + highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee);
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm;
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm) * (1. + fee);
          // approx:
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee);
          // highestBuyAsset.balanceBase = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee)) * highestBuyAsset.profitablePrice;
          highestBuyAsset.profitablePrice = highestBuyAsset.balanceBase / (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee));

          broker.updateAsset(highestBuyAsset);
        }

        broker.removeAsset(asset.entityId);
        lastAssetBuyTime = transaction.date;
        updateAvailableBalance();
        break;
      }
    }
  }
}

void BetBot::Session::handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
  applyBalanceUpdate(transaction.total, -transaction.amount);

  if(orderId == sellInOrderId)
  {
    String message;
    message.printf("SellIn: @ %.02f, %.08f %s => %.02f %s", transaction.price, 
      transaction.amount, (const char_t*)broker.getCurrencyComm(), transaction.total, (const char_t*)broker.getCurrencyBase());
    broker.warning(message);

    BotProtocol::SessionAsset sessionAsset;
    sessionAsset.entityId = 0;
    sessionAsset.entityType = BotProtocol::sessionAsset;
    sessionAsset.type = BotProtocol::SessionAsset::sell;
    sessionAsset.state = BotProtocol::SessionAsset::waitBuy;
    sessionAsset.date = transaction.date;
    sessionAsset.price = transaction.price;
    sessionAsset.investComm = transaction.amount;
    sessionAsset.investBase = 0.;
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
        double gainBase = transaction.total - asset.investBase + asset.balanceBase;
        double gainComm = asset.balanceComm - transaction.amount;

        Map<double, const BotProtocol::SessionAsset*> sortedBuyAssets;
        Map<double, const BotProtocol::SessionAsset*> sortedSellAssets;
        if(gainComm > 0.)
          for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
          {
            const BotProtocol::SessionAsset& asset = *i;
            if(asset.state == BotProtocol::SessionAsset::waitBuy && asset.profitablePrice < transaction.price)
              sortedBuyAssets.insert(asset.profitablePrice, &asset);
          }
        if(gainBase > 0.)
          for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
          {
            const BotProtocol::SessionAsset& asset = *i;
            if(asset.state == BotProtocol::SessionAsset::waitSell && asset.profitablePrice > transaction.price)
              sortedSellAssets.insert(asset.profitablePrice, &asset);
          }

        String message;
        message.printf("Sell: asset %.02f @ %.02f, %.08f %s => %.02f %s, Made %.02f %s, %.08f %s", asset.price, transaction.price,
          transaction.amount, (const char_t*)broker.getCurrencyComm(), transaction.total, (const char_t*)broker.getCurrencyBase(),
          gainBase, (const char_t*)broker.getCurrencyBase(), gainComm, (const char_t*)broker.getCurrencyComm());
        broker.warning(message);

        if(!sortedBuyAssets.isEmpty())
        {
          BotProtocol::SessionAsset highestBuyAsset = *sortedBuyAssets.back();

          String message;
          message.printf("Gave %.08f %s to asset %.02f", gainComm, (const char_t*)broker.getCurrencyComm(), highestBuyAsset.price);
          broker.warning(message);

          highestBuyAsset.balanceComm += gainComm;

          double fee = 0.005;
          // highestBuyAsset.balanceComm + highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee);
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm;
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm) * (1. + fee);
          // approx:
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee);
          // highestBuyAsset.balanceBase = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee)) * highestBuyAsset.profitablePrice;
          highestBuyAsset.profitablePrice = highestBuyAsset.balanceBase / (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee));

          broker.updateAsset(highestBuyAsset);
        }
        if(!sortedSellAssets.isEmpty())
        {
          BotProtocol::SessionAsset lowestSellAsset = *sortedSellAssets.front();

          String message;
          message.printf("Gave %.02f %s to asset %.02f", gainBase, (const char_t*)broker.getCurrencyBase(), lowestSellAsset.price);
          broker.warning(message);

          lowestSellAsset.balanceBase += gainBase;

          double fee = 0.005;
          // lowestSellAsset.balanceBase + lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee);
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase;
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase) * (1. + fee);
          // approx:
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee);
          lowestSellAsset.profitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee)) / lowestSellAsset.balanceComm;

          broker.updateAsset(lowestSellAsset);
        }

        broker.removeAsset(asset.entityId);
        lastAssetSellTime = transaction.date;
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

    if(trade.price > values.regressions[regression12h].min || trade.price > values.regressions[regression12h].max * (1. - broker.getProperty("Buy Min Price Shift", DEFAULT_BUY_MIN_PRICE_SHIFT)))
      return;
    buyInIncline = values.regressions[regression6h].incline;
    buyInStartPrice = trade.price;
    maxBuyInPrice = buyInStartPrice + buyInIncline * (timestamp_t)broker.getProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
    if(!std::isnormal(maxBuyInPrice))
      return;
    buyInBase = getBuyInBase(trade.price, values);
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
        double newBuyInBase = getBuyInBase(trade.price, values);
        if(newBuyInBase == 0.)
          return;
        if(newBuyInBase < buyInBase)
          newBuyInBase = buyInBase;
        if(!broker.cancelOder(buyInOrderId))
          return;
        buyInOrderId = 0;
        updateAvailableBalance();
        maxBuyInPrice = newMaxBuyPrice;
        buyInIncline = newBuyIncline;
        buyInBase = newBuyInBase;
        timestamp_t buyTimeout = (timestamp_t)broker.getProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
        if(!broker.buy(maxBuyInPrice, 0., newBuyInBase, buyTimeout * 1000, &buyInOrderId, 0))
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

    if(trade.price < values.regressions[regression12h].max || trade.price < values.regressions[regression12h].min * (1. + broker.getProperty("Sell Min Price Shift", DEFAULT_SELL_MIN_PRICE_SHIFT)))
      return;
    sellInIncline = values.regressions[regression6h].incline;
    sellInStartPrice = trade.price;
    minSellInPrice = sellInStartPrice + sellInIncline * (timestamp_t)broker.getProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
    if(!std::isnormal(minSellInPrice))
      return;
    sellInComm = getSellInComm(trade.price, values);
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
        double newSellInComm = getSellInComm(trade.price, values);
        if(newSellInComm == 0.)
          return;
        if(newSellInComm < sellInComm)
          newSellInComm = sellInComm;
        if(!broker.cancelOder(sellInOrderId))
          return;
        buyInOrderId = 0;
        updateAvailableBalance();
        minSellInPrice = newMinSellPrice;
        sellInIncline = newSellIncline;
        sellInComm = newSellInComm;
        timestamp_t sellTimeout = (timestamp_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
        if(!broker.sell(minSellInPrice, newSellInComm, 0., sellTimeout * 1000, &sellInOrderId, 0))
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
  if((timestamp_t)trade.time - lastAssetBuyTime < buyCooldown * 1000)
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

      bool waitingForSell = false;
      for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
      {
        const BotProtocol::SessionAsset& asset = *i;
        if(asset.state == BotProtocol::SessionAsset::waitSell)
        {
          waitingForSell = true;
          break;
        }
      }
      double buyAmountComm = 0.;
      double buyAmountBase = asset.balanceBase;
      if(waitingForSell)
      {
        buyAmountComm = asset.investComm;
        buyAmountBase = 0.;
      }

      timestamp_t buyTimeout = (timestamp_t)broker.getProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
      if(broker.buy(tradePrice, buyAmountComm, buyAmountBase, buyTimeout * 1000, &updatedAsset.orderId, 0))
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
  if((timestamp_t)trade.time - lastAssetSellTime < sellCooldown * 1000)
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

      bool waitingForBuy = false;
      for(HashMap<uint32_t, BotProtocol::SessionAsset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
      {
        const BotProtocol::SessionAsset& asset = *i;
        if(asset.state == BotProtocol::SessionAsset::waitBuy)
        {
          waitingForBuy = true;
          break;
        }
      }
      double buyAmountComm = asset.balanceComm;
      double buyAmountBase = 0.;
      if(waitingForBuy)
      {
        buyAmountComm = 0.;
        buyAmountBase = asset.investBase;
      }

      timestamp_t sellTimeout = (timestamp_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
      if(broker.sell(tradePrice, buyAmountComm, buyAmountBase, sellTimeout * 1000, &updatedAsset.orderId, 0))
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
