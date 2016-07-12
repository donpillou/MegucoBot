
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

  broker.registerProperty(String("Balance ") + broker.getCurrencyBase(), 0, meguco_user_session_property_none, broker.getCurrencyBase());
  broker.registerProperty(String("Balance ") + broker.getCurrencyComm(), 0, meguco_user_session_property_none, broker.getCurrencyComm());
  broker.registerProperty(String("Available Balance ") + broker.getCurrencyBase(), 0, meguco_user_session_property_read_only, broker.getCurrencyBase());
  broker.registerProperty(String("Available Balance ") + broker.getCurrencyComm(), 0, meguco_user_session_property_read_only, broker.getCurrencyComm());
  broker.registerProperty("Min Bet", DEFAULT_MIN_BET, meguco_user_session_property_none, broker.getCurrencyBase());

  balanceBase = broker.getProperty(String("Balance ") + broker.getCurrencyBase(), 0);
  balanceComm = broker.getProperty(String("Balance ") + broker.getCurrencyComm(), 0);

  updateAvailableBalance();
}

void_t BetBot::Session::updateAvailableBalance()
{
  availableBalanceBase = balanceBase;
  availableBalanceComm = balanceComm;
  const HashMap<uint64_t, meguco_user_session_asset_entity>& assets = broker.getAssets();
  for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const meguco_user_session_asset_entity& asset = *i;
    // todo: allow reinvesting assets!
    availableBalanceComm -= asset.balance_comm;
    availableBalanceBase -= asset.balance_base;
  }
  const HashMap<uint64_t, meguco_user_broker_order_entity>& orders = broker.getOrders();
  for(HashMap<uint64_t, meguco_user_broker_order_entity>::Iterator i = orders.begin(), end = orders.end(); i != end; ++i)
  {
    const meguco_user_broker_order_entity& order = *i;
    if(order.entity.id == buyInOrderId || order.entity.id == sellInOrderId)
    {
      if(order.type == meguco_user_broker_order_buy)
        availableBalanceBase -= order.total;
      else
        availableBalanceComm -= order.amount;
    }
  }
  broker.setProperty(String("Available Balance ") + broker.getCurrencyBase(), availableBalanceBase);
  broker.setProperty(String("Available Balance ") + broker.getCurrencyComm(), availableBalanceComm);
}

void_t BetBot::Session::applyBalanceUpdate(double base, double comm)
{
  balanceBase += base;
  balanceComm += comm;
  broker.setProperty(String("Balance ") + broker.getCurrencyBase(), balanceBase);
  broker.setProperty(String("Balance ") + broker.getCurrencyComm(), balanceComm);
  updateAvailableBalance();
}

double BetBot::Session::getBuyInBase(double currentPrice, const TradeHandler::Values& values) const
{
  int bottomness = 1;
  if(currentPrice <= values.regressions[TradeHandler::regression24h].min)
      ++bottomness;

  double botValueBase = balanceBase + balanceComm * currentPrice;
  double maxBase = botValueBase / 10.;
  double base = Math::min(availableBalanceBase / 2., maxBase) * 0.5 * bottomness;
  if(base < broker.getProperty(String("Min Bet"), DEFAULT_MIN_BET))
    return 0;
  return base;
}

double BetBot::Session::getSellInComm(double currentPrice, const TradeHandler::Values& values) const
{
  int topness = 1;
  if(currentPrice >= values.regressions[TradeHandler::regression24h].max)
      ++topness;

  double botValueComm = balanceComm + balanceBase / currentPrice;
  double maxComm = botValueComm / 10.;
  double comm = Math::min(availableBalanceComm / 2., maxComm) * 0.5 * topness;
  if(comm * currentPrice < broker.getProperty(String("Min Bet"), DEFAULT_MIN_BET))
    return 0;
  return comm;
}

void BetBot::Session::handleTrade(const meguco_trade_entity& trade, int64_t tradeAge)
{
  tradeHandler.add(trade, tradeAge);
  if(!tradeHandler.isComplete())
    return;

  checkAssetBuy(trade);
  checkAssetSell(trade);
  TradeHandler::Values& values = tradeHandler.getValues();
  checkBuyIn(trade, values);
  checkSellIn(trade, values);
}

void BetBot::Session::handleBuy(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction)
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

    meguco_user_session_asset_entity sessionAsset;
    sessionAsset.type = meguco_user_session_asset_buy;
    sessionAsset.state = meguco_user_session_asset_wait_sell;
    sessionAsset.lastTransactionTime = transaction.entity.time;
    sessionAsset.price = transaction.price;
    sessionAsset.invest_comm = 0.;
    sessionAsset.invest_base = transaction.total;
    sessionAsset.balance_comm = transaction.amount;
    sessionAsset.balance_base = 0.;
    double fee = 0.005;
    sessionAsset.profitable_price = transaction.price * (1. + fee * 2.);
    double sellProfitGain = broker.getProperty("Sell Profit Gain", DEFAULT_SELL_PROFIT_GAIN);
    sessionAsset.flip_price = transaction.price * (1. + fee * (1. + sellProfitGain) * 2.);
    sessionAsset.order_id = 0;
    broker.createAsset(sessionAsset);

    buyInOrderId = 0;
    resetBetOrders();
    lastBuyInTime = broker.getTime();
    updateAvailableBalance();
  }
  else
  {
    const HashMap<uint64_t, meguco_user_session_asset_entity>& assets = broker.getAssets();
    for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
    {
      const meguco_user_session_asset_entity& asset = *i;
      if(asset.state == meguco_user_session_asset_buying && asset.order_id == orderId)
      {
        double gainBase = asset.balance_base - transaction.total;
        double gainComm = transaction.amount - asset.invest_comm + asset.balance_comm;

        Map<double, const meguco_user_session_asset_entity*> sortedSellAssets;
        Map<double, const meguco_user_session_asset_entity*> sortedBuyAssets;
        if(gainBase > 0.)
          for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
          {
            const meguco_user_session_asset_entity& asset = *i;
            if(asset.state == meguco_user_session_asset_wait_sell && asset.profitable_price > transaction.price)
              sortedSellAssets.insert(asset.profitable_price, &asset);
          }
        if(gainComm > 0.)
          for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
          {
            const meguco_user_session_asset_entity& asset = *i;
            if(asset.state == meguco_user_session_asset_wait_buy && asset.profitable_price < transaction.price)
              sortedBuyAssets.insert(asset.profitable_price, &asset);
          }

        String message;
        message.printf("Sold asset: %.08f %s @ %.02f => %.02f %s @ %.02f + %.08f %s => %.08f %s + %.02f %s (Balance: %+.08f %s, %+.02f %s)",
          asset.invest_comm, (const char_t*)broker.getCurrencyComm(), asset.price,
          asset.balance_base, (const char_t*)broker.getCurrencyBase(), transaction.price, asset.balance_comm, (const char_t*)broker.getCurrencyComm(),
          transaction.amount + asset.balance_comm, (const char_t*)broker.getCurrencyComm(), gainBase, (const char_t*)broker.getCurrencyBase(),
          gainComm, (const char_t*)broker.getCurrencyComm(), gainBase, (const char_t*)broker.getCurrencyBase());
        broker.warning(message);

        if(!sortedSellAssets.isEmpty())
        {
          meguco_user_session_asset_entity lowestSellAsset = *sortedSellAssets.front();

          String message;
          message.printf("Updated asset:  %.02f %s @ %.02f => %.08f %s + %.02f %s => %.08f %s + %.02f %s (Balance: %+.02f %s)",
            lowestSellAsset.invest_base, (const char_t*)broker.getCurrencyBase(), lowestSellAsset.price,
            lowestSellAsset.balance_comm, (const char_t*)broker.getCurrencyComm(), lowestSellAsset.balance_base, (const char_t*)broker.getCurrencyBase(),
            lowestSellAsset.balance_comm, (const char_t*)broker.getCurrencyComm(), lowestSellAsset.balance_base + gainBase, (const char_t*)broker.getCurrencyBase(),
            -gainBase, (const char_t*)broker.getCurrencyBase());
          broker.warning(message);

          lowestSellAsset.balance_base += gainBase;

          double fee = 0.005;
          // lowestSellAsset.balanceBase + lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee);
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase;
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase) * (1. + fee);
          // approx:
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee);
          lowestSellAsset.profitable_price = (lowestSellAsset.balance_comm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balance_base * (1. + fee)) / lowestSellAsset.balance_comm;

          broker.updateAsset(lowestSellAsset);
        }
        if(!sortedBuyAssets.isEmpty())
        {
          meguco_user_session_asset_entity highestBuyAsset = *sortedBuyAssets.back();

          gainComm *= 0.5;

          String message;
          message.printf("Updated asset:  %.08f %s @ %.02f => %.02f %s + %.08f %s => %.02f %s + %.08f %s (Balance: %+.08f %s)",
            highestBuyAsset.invest_comm, (const char_t*)broker.getCurrencyComm(), highestBuyAsset.price,
            highestBuyAsset.balance_base, (const char_t*)broker.getCurrencyBase(), highestBuyAsset.balance_comm, (const char_t*)broker.getCurrencyComm(),
            highestBuyAsset.balance_base, (const char_t*)broker.getCurrencyBase(), highestBuyAsset.balance_comm + gainComm, (const char_t*)broker.getCurrencyComm(),
            -gainComm, (const char_t*)broker.getCurrencyComm());
          broker.warning(message);

          highestBuyAsset.balance_comm += gainComm;

          double fee = 0.005;
          // highestBuyAsset.balanceComm + highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee);
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm;
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm) * (1. + fee);
          // approx:
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee);
          // highestBuyAsset.balanceBase = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee)) * highestBuyAsset.profitablePrice;
          highestBuyAsset.profitable_price = highestBuyAsset.balance_base / (highestBuyAsset.balance_base / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balance_comm * (1. + fee));

          broker.updateAsset(highestBuyAsset);
        }

        broker.removeAsset(asset.entity.id);
        lastAssetBuyTime = broker.getTime();
        updateAvailableBalance();
        break;
      }
    }
  }
}

void BetBot::Session::handleSell(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction)
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

    meguco_user_session_asset_entity sessionAsset;
    sessionAsset.type = meguco_user_session_asset_sell;
    sessionAsset.state = meguco_user_session_asset_wait_buy;
    sessionAsset.lastTransactionTime = transaction.entity.time;
    sessionAsset.price = transaction.price;
    sessionAsset.invest_comm = transaction.amount;
    sessionAsset.invest_base = 0.;
    sessionAsset.balance_comm = 0.;
    sessionAsset.balance_base = transaction.total;
    double fee = 0.005;
    sessionAsset.profitable_price = transaction.price / (1. + fee * 2.);
    double buyProfitGain = broker.getProperty("Buy Profit Gain", DEFAULT_BUY_PROFIT_GAIN);
    sessionAsset.flip_price = transaction.price / (1. + fee * (1. + buyProfitGain) * 2.);
    sessionAsset.order_id = 0;
    broker.createAsset(sessionAsset);

    sellInOrderId = 0;
    resetBetOrders();
    lastSellInTime = broker.getTime();
    updateAvailableBalance();
  }
  else
  {
    const HashMap<uint64_t, meguco_user_session_asset_entity>& assets = broker.getAssets();
    for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
    {
      const meguco_user_session_asset_entity& asset = *i;
      if(asset.state == meguco_user_session_asset_selling && asset.order_id == orderId)
      {
        double gainBase = transaction.total - asset.invest_base + asset.balance_base;
        double gainComm = asset.balance_comm - transaction.amount;

        Map<double, const meguco_user_session_asset_entity*> sortedBuyAssets;
        Map<double, const meguco_user_session_asset_entity*> sortedSellAssets;
        if(gainComm > 0.)
          for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
          {
            const meguco_user_session_asset_entity& asset = *i;
            if(asset.state == meguco_user_session_asset_wait_buy && asset.profitable_price < transaction.price)
              sortedBuyAssets.insert(asset.profitable_price, &asset);
          }
        if(gainBase > 0.)
          for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
          {
            const meguco_user_session_asset_entity& asset = *i;
            if(asset.state == meguco_user_session_asset_wait_sell && asset.profitable_price > transaction.price)
              sortedSellAssets.insert(asset.profitable_price, &asset);
          }

        String message;
        message.printf("Sold asset: %.02f %s @ %.02f => %.08f %s @ %.02f + %.02f %s => %.02f %s + %.08f %s (Balance: %+.02f %s, %+.08f %s)",
          asset.invest_base, (const char_t*)broker.getCurrencyBase(), asset.price,
          asset.balance_comm, (const char_t*)broker.getCurrencyComm(), transaction.price, asset.balance_base, (const char_t*)broker.getCurrencyBase(),
          transaction.total + asset.balance_base, (const char_t*)broker.getCurrencyBase(), gainComm, (const char_t*)broker.getCurrencyComm(),
          gainBase, (const char_t*)broker.getCurrencyBase(), gainComm, (const char_t*)broker.getCurrencyComm());
        broker.warning(message);

        if(!sortedBuyAssets.isEmpty())
        {
          meguco_user_session_asset_entity highestBuyAsset = *sortedBuyAssets.back();

          String message;
          message.printf("Updated asset:  %.08f %s @ %.02f => %.02f %s + %.08f %s => %.02f %s + %.08f %s (Balance: %+.08f %s)",
            highestBuyAsset.invest_comm, (const char_t*)broker.getCurrencyComm(), highestBuyAsset.price,
            highestBuyAsset.balance_base, (const char_t*)broker.getCurrencyBase(), highestBuyAsset.balance_comm, (const char_t*)broker.getCurrencyComm(),
            highestBuyAsset.balance_base, (const char_t*)broker.getCurrencyBase(), highestBuyAsset.balance_comm + gainComm, (const char_t*)broker.getCurrencyComm(),
            -gainComm, (const char_t*)broker.getCurrencyComm());
          broker.warning(message);

          highestBuyAsset.balance_comm += gainComm;

          double fee = 0.005;
          // highestBuyAsset.balanceComm + highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee);
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice / (1. + fee) = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm;
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + fee) - highestBuyAsset.balanceComm) * (1. + fee);
          // approx:
          // highestBuyAsset.balanceBase / highestBuyAsset.profitablePrice = highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee);
          // highestBuyAsset.balanceBase = (highestBuyAsset.balanceBase / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balanceComm * (1. + fee)) * highestBuyAsset.profitablePrice;
          highestBuyAsset.profitable_price = highestBuyAsset.balance_base / (highestBuyAsset.balance_base / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balance_comm * (1. + fee));

          broker.updateAsset(highestBuyAsset);
        }
        if(!sortedSellAssets.isEmpty())
        {
          meguco_user_session_asset_entity lowestSellAsset = *sortedSellAssets.front();

          gainBase *= 0.5;

          String message;
          message.printf("Updated asset:  %.02f %s @ %.02f => %.08f %s + %.02f %s => %.08f %s + %.02f %s (Balance: %+.02f %s)",
            lowestSellAsset.invest_base, (const char_t*)broker.getCurrencyBase(), lowestSellAsset.price,
            lowestSellAsset.balance_comm, (const char_t*)broker.getCurrencyComm(), lowestSellAsset.balance_base, (const char_t*)broker.getCurrencyBase(),
            lowestSellAsset.balance_comm, (const char_t*)broker.getCurrencyComm(), lowestSellAsset.balance_base + gainBase, (const char_t*)broker.getCurrencyBase(),
            -gainBase, (const char_t*)broker.getCurrencyBase());
          broker.warning(message);

          lowestSellAsset.balance_base += gainBase;

          double fee = 0.005;
          // lowestSellAsset.balanceBase + lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee);
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice / (1. + fee) = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase;
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = (lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + fee) - lowestSellAsset.balanceBase) * (1. + fee);
          // approx:
          // lowestSellAsset.balanceComm * lowestSellAsset.profitablePrice = lowestSellAsset.balanceComm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balanceBase * (1. + fee);
          lowestSellAsset.profitable_price = (lowestSellAsset.balance_comm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balance_base * (1. + fee)) / lowestSellAsset.balance_comm;

          broker.updateAsset(lowestSellAsset);
        }

        broker.removeAsset(asset.entity.id);
        lastAssetSellTime = broker.getTime();
        updateAvailableBalance();
        break;
      }
    }
  }
}

void_t BetBot::Session::handleBuyTimeout(uint64_t orderId)
{
  if(orderId == buyInOrderId)
    buyInOrderId = 0;
  else
  {
    const HashMap<uint64_t, meguco_user_session_asset_entity>& assets = broker.getAssets();
    for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
    {
      const meguco_user_session_asset_entity& asset = *i;
      if(asset.state == meguco_user_session_asset_buying && asset.order_id == orderId)
      {
        meguco_user_session_asset_entity updatedAsset = asset;
        updatedAsset.state = meguco_user_session_asset_wait_buy;
        updatedAsset.order_id = 0;
        broker.updateAsset(updatedAsset);
        break;
      }
    }
  }
  updateAvailableBalance();
}

void_t BetBot::Session::handleSellTimeout(uint64_t orderId)
{
  if(orderId == sellInOrderId)
    sellInOrderId = 0;
  else
  {
    const HashMap<uint64_t, meguco_user_session_asset_entity>& assets = broker.getAssets();
    for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
    {
      const meguco_user_session_asset_entity& asset = *i;
      if(asset.state == meguco_user_session_asset_selling && asset.order_id == orderId)
      {
        meguco_user_session_asset_entity updatedAsset = asset;
        updatedAsset.state = meguco_user_session_asset_wait_sell;
        updatedAsset.order_id = 0;
        broker.updateAsset(updatedAsset);
        break;
      }
    }
  }
  updateAvailableBalance();
}

#include <cmath>
void BetBot::Session::checkBuyIn(const meguco_trade_entity& trade, const TradeHandler::Values& values)
{
  int64_t buyCooldown = (int64_t)broker.getProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  if((int64_t)trade.entity.time - lastBuyInTime < buyCooldown * 1000)
    return; // do not buy too often

  if(buyInOrderId == 0)
  {
    //timestamp_t buyCooldown = (timestamp_t)broker.getProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
    //if(broker.getTimeSinceLastBuy() < buyCooldown * 1000)
    //  return; // do not buy too often

    if(trade.price > values.regressions[TradeHandler::regression12h].min || trade.price > values.regressions[TradeHandler::regression12h].max * (1. - broker.getProperty("Buy Min Price Shift", DEFAULT_BUY_MIN_PRICE_SHIFT)))
      return;
    buyInIncline = values.regressions[TradeHandler::regression6h].incline;
    buyInStartPrice = trade.price;
    maxBuyInPrice = buyInStartPrice + buyInIncline * (int64_t)broker.getProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
    if(!std::isnormal(maxBuyInPrice))
      return;
    buyInBase = getBuyInBase(trade.price, values);
    if(buyInBase == 0.)
      return;
    int64_t buyTimeout = (int64_t)broker.getProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
    if(!broker.buy(maxBuyInPrice, 0., buyInBase, buyTimeout * 1000, &buyInOrderId, 0))
      return;
    updateAvailableBalance();
  }
  else
  {
    double newBuyIncline = values.regressions[TradeHandler::regression6h].incline;
    if(newBuyIncline < buyInIncline)
    {
      double newMaxBuyPrice = buyInStartPrice + newBuyIncline * (int64_t)broker.getProperty("Buy Predict Time", DEFAULT_BUY_PREDICT_TIME);
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
        int64_t buyTimeout = (int64_t)broker.getProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
        if(!broker.buy(maxBuyInPrice, 0., newBuyInBase, buyTimeout * 1000, &buyInOrderId, 0))
          return;
        updateAvailableBalance();
      }
    }
  }

  if(trade.price > maxBuyInPrice)
    return;

  broker.addMarker(meguco_user_session_marker_good_buy);
}

void BetBot::Session::checkSellIn(const meguco_trade_entity& trade, const TradeHandler::Values& values)
{
  int64_t sellCooldown = (int64_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  if((int64_t)trade.entity.time - lastSellInTime < sellCooldown * 1000)
    return; // do not sell too often

  if(sellInOrderId == 0)
  {
    //timestamp_t sellCooldown = (timestamp_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
    //if(broker.getTimeSinceLastSell() < sellCooldown * 1000)
    //  return; // do not sell too often

    if(trade.price < values.regressions[TradeHandler::regression12h].max || trade.price < values.regressions[TradeHandler::regression12h].min * (1. + broker.getProperty("Sell Min Price Shift", DEFAULT_SELL_MIN_PRICE_SHIFT)))
      return;
    sellInIncline = values.regressions[TradeHandler::regression6h].incline;
    sellInStartPrice = trade.price;
    minSellInPrice = sellInStartPrice + sellInIncline * (int64_t)broker.getProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
    if(!std::isnormal(minSellInPrice))
      return;
    sellInComm = getSellInComm(trade.price, values);
    if(sellInComm == 0.)
      return;
    int64_t sellTimeout = (int64_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
    if(!broker.sell(minSellInPrice, sellInComm, 0., sellTimeout * 1000, &sellInOrderId, 0))
      return;
    updateAvailableBalance();
  }
  else
  {
    double newSellIncline = values.regressions[TradeHandler::regression6h].incline;
    if(newSellIncline > sellInIncline)
    {
      double newMinSellPrice = sellInStartPrice + newSellIncline * (int64_t)broker.getProperty("Sell Predict Time", DEFAULT_SELL_PREDICT_TIME);
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
        sellInOrderId = 0;
        updateAvailableBalance();
        minSellInPrice = newMinSellPrice;
        sellInIncline = newSellIncline;
        sellInComm = newSellInComm;
        int64_t sellTimeout = (int64_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
        if(!broker.sell(minSellInPrice, newSellInComm, 0., sellTimeout * 1000, &sellInOrderId, 0))
          return;
        updateAvailableBalance();
      }
    }
  }

  if(trade.price < minSellInPrice)
    return;

  broker.addMarker(meguco_user_session_marker_good_sell);
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

void BetBot::Session::checkAssetBuy(const meguco_trade_entity& trade)
{
  int64_t buyCooldown = (int64_t)broker.getProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  if((int64_t)trade.entity.time - lastAssetBuyTime < buyCooldown * 1000)
    return; // do not buy too often

  double tradePrice = trade.price;
  const HashMap<uint64_t, meguco_user_session_asset_entity>& assets = broker.getAssets();
  for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const meguco_user_session_asset_entity& asset = *i;
    if(asset.state == meguco_user_session_asset_wait_buy && tradePrice <= asset.flip_price)
    {
      meguco_user_session_asset_entity updatedAsset = asset;
      updatedAsset.state = meguco_user_session_asset_buying;
      broker.updateAsset(updatedAsset);

      bool waitingForSell = false;
      for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
      {
        const meguco_user_session_asset_entity& asset = *i;
        if(asset.state == meguco_user_session_asset_wait_sell)
        {
          waitingForSell = true;
          break;
        }
      }
      double buyAmountComm = 0.;
      double buyAmountBase = asset.balance_base;
      if(waitingForSell && asset.invest_comm > 0.)
      {
        buyAmountComm = asset.invest_comm - asset.balance_comm;
        buyAmountBase = 0.;
      }

      int64_t buyTimeout = (int64_t)broker.getProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
      if(broker.buy(tradePrice, buyAmountComm, buyAmountBase, buyTimeout * 1000, &updatedAsset.order_id, 0))
        broker.updateAsset(updatedAsset);
      else
      {
        updatedAsset.state = meguco_user_session_asset_wait_buy;
        broker.updateAsset(updatedAsset);
      }
      break;
    }
  }
}

void BetBot::Session::checkAssetSell(const meguco_trade_entity& trade)
{
  int64_t sellCooldown = (int64_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  if((int64_t)trade.entity.time - lastAssetSellTime < sellCooldown * 1000)
    return; // do not sell too often

  double tradePrice = trade.price;
  const HashMap<uint64_t, meguco_user_session_asset_entity>& assets = broker.getAssets();
  for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const meguco_user_session_asset_entity& asset = *i;
    if(asset.state == meguco_user_session_asset_wait_sell && tradePrice >= asset.flip_price)
    {
      meguco_user_session_asset_entity updatedAsset = asset;
      updatedAsset.state = meguco_user_session_asset_selling;
      broker.updateAsset(updatedAsset);

      bool waitingForBuy = false;
      for(HashMap<uint64_t, meguco_user_session_asset_entity>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
      {
        const meguco_user_session_asset_entity& asset = *i;
        if(asset.state == meguco_user_session_asset_wait_buy)
        {
          waitingForBuy = true;
          break;
        }
      }
      double buyAmountComm = asset.balance_comm;
      double buyAmountBase = 0.;
      if(waitingForBuy && asset.invest_base > 0.)
      {
        buyAmountComm = 0.;
        buyAmountBase = asset.invest_base - asset.balance_base;
      }

      int64_t sellTimeout = (int64_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
      if(broker.sell(tradePrice, buyAmountComm, buyAmountBase, sellTimeout * 1000, &updatedAsset.order_id, 0))
        broker.updateAsset(updatedAsset);
      else
      {
        updatedAsset.state = meguco_user_session_asset_wait_sell;
        broker.updateAsset(updatedAsset);
      }
      break;
    }
  }
}

void_t BetBot::Session::handlePropertyUpdate(const meguco_user_session_property_entity& property)
{
  balanceBase = broker.getProperty(String("Balance ") + broker.getCurrencyBase(), 0);
  balanceComm = broker.getProperty(String("Balance ") + broker.getCurrencyComm(), 0);

  updateAvailableBalance();
}

void_t BetBot::Session::handleAssetUpdate(const meguco_user_session_asset_entity& asset)
{
  updateAvailableBalance();
}

void_t BetBot::Session::handleAssetRemoval(const meguco_user_session_asset_entity& asset)
{
  updateAvailableBalance();
}
