
#include <nstd/Map.h>
#include <nstd/Math.h>

#include "BetBot2.h"

#define DEFAULT_BUYIN_COOLDOWN (60 * 60)
#define DEFAULT_BUYIN_TIMEOUT (6 * 60 * 60)
#define DEFAULT_BUYIN_PRICE_DROP 5.
#define DEFAULT_BUYIN_PRICE_RISE 5.
#define DEFAULT_BUYIN_PREDICT_TIME (60 * 60)
#define DEFAULT_BUYIN_MIN_AMOUNT 7.
#define DEFAULT_BUYIN_BALANCE_DIVIDER 5.
#define DEFAULT_SELL_COOLDOWN (60 * 60)
#define DEFAULT_SELL_TIMEOUT (60 * 60)
#define DEFAULT_SELL_PRICE_DROP 6.
#define DEFAULT_SELL_PRICE_RISE 6.

BetBot2::Session::Session(Broker& broker) : broker(broker), buyInOrderId(0), sellInOrderId(0), lastBuyInTime(0), lastSellInTime(0), lastAssetBuyTime(0), lastAssetSellTime(0)
{
  buyInState = idle;
  sellInState = idle;

  broker.registerProperty("BuyIn Cooldown", DEFAULT_BUYIN_COOLDOWN, BotProtocol::SessionProperty::none, "s");
  broker.registerProperty("BuyIn Timeout", DEFAULT_BUYIN_TIMEOUT, BotProtocol::SessionProperty::none, "s");
  broker.registerProperty("BuyIn Price Drop", DEFAULT_BUYIN_PRICE_DROP, BotProtocol::SessionProperty::none, "%");
  broker.registerProperty("BuyIn Price Rise", DEFAULT_BUYIN_PRICE_RISE, BotProtocol::SessionProperty::none, "%");
  broker.registerProperty("BuyIn Predict Time", DEFAULT_BUYIN_PREDICT_TIME, BotProtocol::SessionProperty::none, "s");
  broker.registerProperty("BuyIn Min Amount", DEFAULT_BUYIN_MIN_AMOUNT, BotProtocol::SessionProperty::none, broker.getCurrencyBase());
  broker.registerProperty("BuyIn Balance Divider", DEFAULT_BUYIN_BALANCE_DIVIDER, BotProtocol::SessionProperty::none);
  broker.registerProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN, BotProtocol::SessionProperty::none, "s");
  broker.registerProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT, BotProtocol::SessionProperty::none, "s");
  broker.registerProperty("Sell Price Drop", DEFAULT_SELL_PRICE_DROP, BotProtocol::SessionProperty::none, "%");
  broker.registerProperty("Sell Price Rise", DEFAULT_SELL_PRICE_RISE, BotProtocol::SessionProperty::none, "%");

  broker.registerProperty(String("Balance ") + broker.getCurrencyBase(), 0, BotProtocol::SessionProperty::none, broker.getCurrencyBase());
  broker.registerProperty(String("Balance ") + broker.getCurrencyComm(), 0, BotProtocol::SessionProperty::none, broker.getCurrencyComm());
  broker.registerProperty(String("Available Balance ") + broker.getCurrencyBase(), 0, BotProtocol::SessionProperty::readOnly, broker.getCurrencyBase());
  broker.registerProperty(String("Available Balance ") + broker.getCurrencyComm(), 0, BotProtocol::SessionProperty::readOnly, broker.getCurrencyComm());

  balanceBase = broker.getProperty(String("Balance ") + broker.getCurrencyBase(), 0);
  balanceComm = broker.getProperty(String("Balance ") + broker.getCurrencyComm(), 0);

  updateAvailableBalance();
}

void_t BetBot2::Session::updateAvailableBalance()
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

void_t BetBot2::Session::applyBalanceUpdate(double base, double comm)
{
  balanceBase += base;
  balanceComm += comm;
  broker.setProperty(String("Balance ") + broker.getCurrencyBase(), balanceBase, BotProtocol::SessionProperty::none, broker.getCurrencyBase());
  broker.setProperty(String("Balance ") + broker.getCurrencyComm(), balanceComm, BotProtocol::SessionProperty::none, broker.getCurrencyComm());
  updateAvailableBalance();
}

double BetBot2::Session::getBuyInBase(double currentPrice, const TradeHandler::Values& values) const
{
  int bottomness = 1;
  if(currentPrice <= values.regressions[TradeHandler::regression24h].min)
      ++bottomness;

  double botValueBase = balanceBase + balanceComm * currentPrice;
  double maxBase = botValueBase / broker.getProperty("BuyIn Balance Divider", DEFAULT_BUYIN_BALANCE_DIVIDER);
  double base = Math::min(availableBalanceBase / 2., maxBase) * 0.5 * bottomness;
  double minBuyInAmount = broker.getProperty("BuyIn Min Amount", DEFAULT_BUYIN_MIN_AMOUNT);
  if(base < minBuyInAmount)
  {
    if(availableBalanceBase >= minBuyInAmount)
      return minBuyInAmount;
    return 0;
  }
  return base;
}

double BetBot2::Session::getSellInComm(double currentPrice, const TradeHandler::Values& values) const
{
  int topness = 1;
  if(currentPrice >= values.regressions[TradeHandler::regression24h].max)
      ++topness;

  double botValueComm = balanceComm + balanceBase / currentPrice;
  double maxComm = botValueComm / broker.getProperty("BuyIn Balance Divider", DEFAULT_BUYIN_BALANCE_DIVIDER);
  double comm = Math::min(availableBalanceComm / 2., maxComm) * 0.5 * topness;
  double minBuyInAmount = broker.getProperty("BuyIn Min Amount", DEFAULT_BUYIN_MIN_AMOUNT);
  if(comm * currentPrice < minBuyInAmount)
  {
    if(availableBalanceComm * currentPrice >= minBuyInAmount)
      return minBuyInAmount / currentPrice;
    return 0;
  }
  return comm;
}

void BetBot2::Session::handleTrade(const DataProtocol::Trade& trade, timestamp_t tradeAge)
{
  tradeHandler.add(trade, tradeAge);
  if(!tradeHandler.isComplete())
    return;

  TradeHandler::Values& values = tradeHandler.getValues();
  /*
  ValueSample& range = ranges.append(ValueSample());
  range.time = trade.time;
  range.value = values.regressions[TradeHandler::regression12h].max - values.regressions[TradeHandler::regression12h].min;
  //while(trade.time - ranges.front().time > 12 * 60 * 60 * 1000)
  while(trade.time - ranges.front().time > 20 * 60 * 1000)
    ranges.removeFront();
    */

  ValueSample& incline = inclines.append(ValueSample());
  incline.time = trade.time;
  incline.value = values.regressions[TradeHandler::regression12h].incline;
  while(trade.time - inclines.front().time > 40 * 60 * 1000)
    inclines.removeFront();

  checkAssetBuy(trade);
  checkAssetSell(trade);
  checkBuyIn(trade, values);
  checkSellIn(trade, values);
}

void BetBot2::Session::handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
  applyBalanceUpdate(-transaction.total, transaction.amount);

  if(orderId == buyInOrderId)
  {
    String message;
    message.printf("Bought asset: %.02f %s @ %.02f => %.08f %s (Balance: %+.02f %s)",
      transaction.total, (const char_t*)broker.getCurrencyBase(), transaction.price,
      transaction.amount, (const char_t*)broker.getCurrencyComm(),
      -transaction.total, (const char_t*)broker.getCurrencyBase());
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
    double sellPriceRise = broker.getProperty("Sell Price Rise", DEFAULT_SELL_PRICE_RISE) * 0.01;
    sessionAsset.flipPrice = sessionAsset.profitablePrice * (1. + sellPriceRise);
    sessionAsset.orderId = 0;
    broker.createAsset(sessionAsset);

    buyInOrderId = 0;
    buyInState = idle;
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
        message.printf("Sold asset: %.08f %s @ %.02f => %.02f %s @ %.02f + %.08f %s => %.08f %s + %.02f %s (Balance: %+.08f %s, %+.02f %s)",
          asset.investComm, (const char_t*)broker.getCurrencyComm(), asset.price,
          asset.balanceBase, (const char_t*)broker.getCurrencyBase(), transaction.price, asset.balanceComm, (const char_t*)broker.getCurrencyComm(),
          transaction.amount + asset.balanceComm, (const char_t*)broker.getCurrencyComm(), gainBase, (const char_t*)broker.getCurrencyBase(),
          gainComm, (const char_t*)broker.getCurrencyComm(), gainBase, (const char_t*)broker.getCurrencyBase());
        broker.warning(message);

        if(!sortedSellAssets.isEmpty())
        {
          BotProtocol::SessionAsset lowestSellAsset = *sortedSellAssets.back();

          gainBase *= 0.5;

          String message;
          message.printf("Updated asset:  %.02f %s @ %.02f => %.08f %s + %.02f %s => %.08f %s + %.02f %s (Balance: %+.02f %s)",
            lowestSellAsset.investBase, (const char_t*)broker.getCurrencyBase(), lowestSellAsset.price,
            lowestSellAsset.balanceComm, (const char_t*)broker.getCurrencyComm(), lowestSellAsset.balanceBase, (const char_t*)broker.getCurrencyBase(),
            lowestSellAsset.balanceComm, (const char_t*)broker.getCurrencyComm(), lowestSellAsset.balanceBase + gainBase, (const char_t*)broker.getCurrencyBase(),
            -gainBase, (const char_t*)broker.getCurrencyBase());
          broker.warning(message);

          lowestSellAsset.balanceBase += gainBase;

          double fee = 0.005;
          // lowestSellAsset.balanceBase + lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee);
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase;
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase) * (1. + fee);
          // approx:
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee);
          double newProfitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee)) / lowestSellAsset.balanceComm;
          lowestSellAsset.flipPrice += (newProfitablePrice - lowestSellAsset.profitablePrice);
          lowestSellAsset.profitablePrice = newProfitablePrice;

          broker.updateAsset(lowestSellAsset);
        }
        if(!sortedBuyAssets.isEmpty())
        {
          BotProtocol::SessionAsset highestBuyAsset = *sortedBuyAssets.front();

          gainComm *= 0.5;

          String message;
          message.printf("Updated asset:  %.08f %s @ %.02f => %.02f %s + %.08f %s => %.02f %s + %.08f %s (Balance: %+.08f %s)",
            highestBuyAsset.investComm, (const char_t*)broker.getCurrencyComm(), highestBuyAsset.price,
            highestBuyAsset.balanceBase, (const char_t*)broker.getCurrencyBase(), highestBuyAsset.balanceComm, (const char_t*)broker.getCurrencyComm(),
            highestBuyAsset.balanceBase, (const char_t*)broker.getCurrencyBase(), highestBuyAsset.balanceComm + gainComm, (const char_t*)broker.getCurrencyComm(),
            -gainComm, (const char_t*)broker.getCurrencyComm());
          broker.warning(message);

          highestBuyAsset.balanceComm += gainComm;

          double fee = 0.005;
          // highestBuyAsset.balanceComm + highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee);
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm;
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm) * (1. + fee);
          // approx:
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee);
          // highestBuyAsset.balanceBase = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee)) * highestBuyAsset.profitablePrice;
          double newProfitablePrice = highestBuyAsset.balanceBase / (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee));
          highestBuyAsset.flipPrice += (newProfitablePrice - highestBuyAsset.profitablePrice);
          highestBuyAsset.profitablePrice = newProfitablePrice;

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

void BetBot2::Session::handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
  applyBalanceUpdate(transaction.total, -transaction.amount);

  if(orderId == sellInOrderId)
  {
    String message;
    message.printf("Bought asset: %.08f %s @ %.02f => %.02f %s (Balance: %+.08f %s)",
      transaction.amount, (const char_t*)broker.getCurrencyComm(), transaction.price,
      transaction.total, (const char_t*)broker.getCurrencyBase(),
      -transaction.amount, (const char_t*)broker.getCurrencyComm());
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
    double sellPriceDrop = broker.getProperty("Sell Price Drop", DEFAULT_SELL_PRICE_DROP) * 0.01;
    sessionAsset.flipPrice = sessionAsset.profitablePrice / (1. + sellPriceDrop);
    sessionAsset.orderId = 0;
    broker.createAsset(sessionAsset);

    sellInOrderId = 0;
    sellInState = idle;
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
        message.printf("Sold asset: %.02f %s @ %.02f => %.08f %s @ %.02f + %.02f %s => %.02f %s + %.08f %s (Balance: %+.02f %s, %+.08f %s)",
          asset.investBase, (const char_t*)broker.getCurrencyBase(), asset.price,
          asset.balanceComm, (const char_t*)broker.getCurrencyComm(), transaction.price, asset.balanceBase, (const char_t*)broker.getCurrencyBase(),
          transaction.total + asset.balanceBase, (const char_t*)broker.getCurrencyBase(), gainComm, (const char_t*)broker.getCurrencyComm(),
          gainBase, (const char_t*)broker.getCurrencyBase(), gainComm, (const char_t*)broker.getCurrencyComm());
        broker.warning(message);

        if(!sortedBuyAssets.isEmpty())
        {
          BotProtocol::SessionAsset highestBuyAsset = *sortedBuyAssets.front();

          gainComm *= 0.5;

          String message;
          message.printf("Updated asset:  %.08f %s @ %.02f => %.02f %s + %.08f %s => %.02f %s + %.08f %s (Balance: %+.08f %s)",
            highestBuyAsset.investComm, (const char_t*)broker.getCurrencyComm(), highestBuyAsset.price,
            highestBuyAsset.balanceBase, (const char_t*)broker.getCurrencyBase(), highestBuyAsset.balanceComm, (const char_t*)broker.getCurrencyComm(),
            highestBuyAsset.balanceBase, (const char_t*)broker.getCurrencyBase(), highestBuyAsset.balanceComm + gainComm, (const char_t*)broker.getCurrencyComm(),
            -gainComm, (const char_t*)broker.getCurrencyComm());
          broker.warning(message);

          highestBuyAsset.balanceComm += gainComm;

          double fee = 0.005;
          // highestBuyAsset.balanceComm + highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee);
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm;
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm) * (1. + fee);
          // approx:
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee);
          // highestBuyAsset.balanceBase = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee)) * highestBuyAsset.profitablePrice;
          double newProfitablePrice = highestBuyAsset.balanceBase / (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee));
          highestBuyAsset.flipPrice += (newProfitablePrice - highestBuyAsset.profitablePrice);
          highestBuyAsset.profitablePrice = newProfitablePrice;

          broker.updateAsset(highestBuyAsset);
        }
        if(!sortedSellAssets.isEmpty())
        {
          BotProtocol::SessionAsset lowestSellAsset = *sortedSellAssets.back();

          gainBase *= 0.5;

          String message;
          message.printf("Updated asset:  %.02f %s @ %.02f => %.08f %s + %.02f %s => %.08f %s + %.02f %s (Balance: %+.02f %s)",
            lowestSellAsset.investBase, (const char_t*)broker.getCurrencyBase(), lowestSellAsset.price,
            lowestSellAsset.balanceComm, (const char_t*)broker.getCurrencyComm(), lowestSellAsset.balanceBase, (const char_t*)broker.getCurrencyBase(),
            lowestSellAsset.balanceComm, (const char_t*)broker.getCurrencyComm(), lowestSellAsset.balanceBase + gainBase, (const char_t*)broker.getCurrencyBase(),
            -gainBase, (const char_t*)broker.getCurrencyBase());
          broker.warning(message);

          lowestSellAsset.balanceBase += gainBase;

          double fee = 0.005;
          // lowestSellAsset.balanceBase + lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee);
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase;
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase) * (1. + fee);
          // approx:
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee);
          double newProfitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee)) / lowestSellAsset.balanceComm;
          lowestSellAsset.flipPrice += (newProfitablePrice - lowestSellAsset.profitablePrice);
          lowestSellAsset.profitablePrice = newProfitablePrice;

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

void_t BetBot2::Session::handleBuyTimeout(uint32_t orderId)
{
  if(orderId == buyInOrderId)
  {
    buyInOrderId = 0;
    buyInState = idle;
  }
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

void_t BetBot2::Session::handleSellTimeout(uint32_t orderId)
{
  if(orderId == sellInOrderId)
  {
    sellInOrderId = 0;
    sellInState = idle;
  }
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

void BetBot2::Session::checkBuyIn(const DataProtocol::Trade& trade, const TradeHandler::Values& values)
{
  double buyInPriceDrop = broker.getProperty("BuyIn Price Drop", DEFAULT_BUYIN_PRICE_DROP) * 0.01;
  //timestamp_t buyInPriceAge =  (timestamp_t)broker.getProperty("BuyIn Pirce Age", DEFAULT_BUYIN_PRICE_AGE) * 1000;
  //double range = (values.regressions[TradeHandler::regression12h].max - values.regressions[TradeHandler::regression12h].min);
  double incline = values.regressions[TradeHandler::regression12h].incline;
  timestamp_t buyInPredictTime = (timestamp_t)broker.getProperty("BuyIn Predict Time", DEFAULT_BUYIN_PREDICT_TIME);
  double newBuyInPrice = trade.price + incline * buyInPredictTime;

  timestamp_t buyInCooldown = (timestamp_t)broker.getProperty("BuyIn Cooldown", DEFAULT_BUYIN_COOLDOWN) * 1000;
  if((timestamp_t)trade.time - lastBuyInTime < buyInCooldown)
    return; // do not buy too often

  switch(buyInState)
  {
  case idle:
    if(trade.price < values.regressions[TradeHandler::regression12h].max / (1. + buyInPriceDrop) && 
       trade.price == values.regressions[TradeHandler::regression12h].min && 
       /*range > ranges.front().value */
       incline < inclines.front().value)
    {
      buyInState = waitForDecrease;
      buyInPrice = 0.;
    }
    break;
  case waitForDecrease:
    {
      if(buyInPrice == 0. || newBuyInPrice < buyInPrice)
        buyInPrice = newBuyInPrice;
      if(/*range < ranges.front().price*/
         incline > inclines.front().value)
      {
        double buyInBase = getBuyInBase(trade.price, values);
        if(buyInBase == 0.)
          return;
        timestamp_t buyInTimeout = (timestamp_t)broker.getProperty("BuyIn Timeout", DEFAULT_BUYIN_TIMEOUT) * 1000;
        if(broker.buy(buyInPrice, 0., buyInBase, buyInTimeout, &buyInOrderId, 0))
        {
          updateAvailableBalance();
          buyInState = waitForTrade;
        }
      }
    }
    break;
  case waitForTrade:
    if(/*range > ranges.front().price*/
      incline < inclines.front().value)
    {
      if(broker.cancelOder(buyInOrderId))
      {
        updateAvailableBalance();
        buyInOrderId = 0;
        buyInState = waitForDecrease;
      }
    }
    break;
  }
}

void BetBot2::Session::checkSellIn(const DataProtocol::Trade& trade, const TradeHandler::Values& values)
{
  double buyInPriceRise = broker.getProperty("BuyIn Price Rise", DEFAULT_BUYIN_PRICE_RISE) * 0.01;
  //timestamp_t buyInPriceAge =  (timestamp_t)broker.getProperty("BuyIn Pirce Age", DEFAULT_BUYIN_PRICE_AGE) * 1000;
  //double range = (values.regressions[TradeHandler::regression12h].max - values.regressions[TradeHandler::regression12h].min);
  double incline = values.regressions[TradeHandler::regression12h].incline;
  timestamp_t buyInPredictTime = (timestamp_t)broker.getProperty("BuyIn Predict Time", DEFAULT_BUYIN_PREDICT_TIME);
  double newSellInPrice = trade.price + incline * buyInPredictTime;

  timestamp_t buyInCooldown = (timestamp_t)broker.getProperty("BuyIn Cooldown", DEFAULT_BUYIN_COOLDOWN) * 1000;
  if((timestamp_t)trade.time - lastSellInTime < buyInCooldown)
    return; // do not sell too often

  switch(sellInState)
  {
  case idle:
    if(trade.price > values.regressions[TradeHandler::regression12h].min * (1. + buyInPriceRise) && 
      trade.price == values.regressions[TradeHandler::regression12h].max &&
      /*range > ranges.front().price*/
      incline > inclines.front().value)
    {
      sellInState = waitForDecrease;
      sellInPrice = 0.;
    }
    break;
  case waitForDecrease:
    {
      if(sellInPrice == 0. || newSellInPrice > sellInPrice)
        sellInPrice = newSellInPrice;
      if(/*range < ranges.front().price*/
        incline < inclines.front().value)
      {
        double sellInComm = getSellInComm(trade.price, values);
        if(sellInComm == 0.)
          return;
        timestamp_t buyInTimeout = (timestamp_t)broker.getProperty("BuyIn Timeout", DEFAULT_BUYIN_TIMEOUT) * 1000;
        if(broker.sell(sellInPrice, sellInComm, 0., buyInTimeout, &sellInOrderId, 0))
        {
          updateAvailableBalance();
          sellInState = waitForTrade;
        }
      }
    }
    break;
  case waitForTrade:
    if(/*range > ranges.front().price*/
      incline > inclines.front().value)
    {
      if(broker.cancelOder(sellInOrderId))
      {
        updateAvailableBalance();
        sellInOrderId = 0;
        sellInState = waitForDecrease;
      }
    }
    break;
  }
}

void_t BetBot2::Session::resetBetOrders()
{
  if(buyInOrderId != 0)
    if(broker.cancelOder(buyInOrderId))
    {
      buyInOrderId = 0;
      buyInState = idle;
    }
  if(sellInOrderId != 0)
    if(broker.cancelOder(sellInOrderId))
    {
      sellInOrderId = 0;
      sellInState = idle;
    }
}

void BetBot2::Session::checkAssetBuy(const DataProtocol::Trade& trade)
{
  timestamp_t sellCooldown = (timestamp_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN) * 1000;
  if((timestamp_t)trade.time - lastAssetBuyTime < sellCooldown)
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
      if(waitingForSell && asset.investComm > 0.)
      {
        buyAmountComm = asset.investComm - asset.balanceComm;
        buyAmountBase = 0.;
      }

      timestamp_t buyTimeout = (timestamp_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT) * 1000;
      if(broker.buy(tradePrice, buyAmountComm, buyAmountBase, buyTimeout, &updatedAsset.orderId, 0))
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

void BetBot2::Session::checkAssetSell(const DataProtocol::Trade& trade)
{
   timestamp_t sellCooldown = (timestamp_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN) * 1000;
  if((timestamp_t)trade.time - lastAssetSellTime < sellCooldown)
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
      if(waitingForBuy && asset.investBase > 0.)
      {
        buyAmountComm = 0.;
        buyAmountBase = asset.investBase - asset.balanceBase;
      }

      timestamp_t sellTimeout = (timestamp_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT) * 1000;
      if(broker.sell(tradePrice, buyAmountComm, buyAmountBase, sellTimeout, &updatedAsset.orderId, 0))
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

void_t BetBot2::Session::handlePropertyUpdate(const BotProtocol::SessionProperty& property)
{
  balanceBase = broker.getProperty(String("Balance ") + broker.getCurrencyBase(), 0);
  balanceComm = broker.getProperty(String("Balance ") + broker.getCurrencyComm(), 0);

  updateAvailableBalance();
}

void_t BetBot2::Session::handleAssetUpdate(const BotProtocol::SessionAsset& asset)
{
  updateAvailableBalance();
}

void_t BetBot2::Session::handleAssetRemoval(const BotProtocol::SessionAsset& asset)
{
  updateAvailableBalance();
}
