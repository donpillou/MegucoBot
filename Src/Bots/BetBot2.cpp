
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

  broker.registerProperty("BuyIn Cooldown", DEFAULT_BUYIN_COOLDOWN, meguco_user_session_property_none, "s");
  broker.registerProperty("BuyIn Timeout", DEFAULT_BUYIN_TIMEOUT, meguco_user_session_property_none, "s");
  broker.registerProperty("BuyIn Price Drop", DEFAULT_BUYIN_PRICE_DROP, meguco_user_session_property_none, "%");
  broker.registerProperty("BuyIn Price Rise", DEFAULT_BUYIN_PRICE_RISE, meguco_user_session_property_none, "%");
  broker.registerProperty("BuyIn Predict Time", DEFAULT_BUYIN_PREDICT_TIME, meguco_user_session_property_none, "s");
  broker.registerProperty("BuyIn Min Amount", DEFAULT_BUYIN_MIN_AMOUNT, meguco_user_session_property_none, broker.getCurrencyBase());
  broker.registerProperty("BuyIn Balance Divider", DEFAULT_BUYIN_BALANCE_DIVIDER, meguco_user_session_property_none);
  broker.registerProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN, meguco_user_session_property_none, "s");
  broker.registerProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT, meguco_user_session_property_none, "s");
  broker.registerProperty("Sell Price Drop", DEFAULT_SELL_PRICE_DROP, meguco_user_session_property_none, "%");
  broker.registerProperty("Sell Price Rise", DEFAULT_SELL_PRICE_RISE, meguco_user_session_property_none, "%");

  broker.registerProperty(String("Balance ") + broker.getCurrencyBase(), 0, meguco_user_session_property_none, broker.getCurrencyBase());
  broker.registerProperty(String("Balance ") + broker.getCurrencyComm(), 0, meguco_user_session_property_none, broker.getCurrencyComm());
  broker.registerProperty(String("Available Balance ") + broker.getCurrencyBase(), 0, meguco_user_session_property_read_only, broker.getCurrencyBase());
  broker.registerProperty(String("Available Balance ") + broker.getCurrencyComm(), 0, meguco_user_session_property_read_only, broker.getCurrencyComm());

  balanceBase = broker.getProperty(String("Balance ") + broker.getCurrencyBase(), 0);
  balanceComm = broker.getProperty(String("Balance ") + broker.getCurrencyComm(), 0);

  updateAvailableBalance();
}

void_t BetBot2::Session::updateAvailableBalance()
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

void_t BetBot2::Session::applyBalanceUpdate(double base, double comm)
{
  balanceBase += base;
  balanceComm += comm;
  broker.setProperty(String("Balance ") + broker.getCurrencyBase(), balanceBase);
  broker.setProperty(String("Balance ") + broker.getCurrencyComm(), balanceComm);
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

void BetBot2::Session::handleTrade(const meguco_trade_entity& trade, int64_t tradeAge)
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
  incline.time = trade.entity.time;
  incline.value = values.regressions[TradeHandler::regression12h].incline;
  while(trade.entity.time - inclines.front().time > 40 * 60 * 1000)
    inclines.removeFront();

  checkAssetBuy(trade);
  checkAssetSell(trade);
  checkBuyIn(trade, values);
  checkSellIn(trade, values);
}

void BetBot2::Session::handleBuy(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction)
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
    sessionAsset.state = meguco_user_session_asset_sell;
    sessionAsset.time = transaction.entity.time;
    sessionAsset.price = transaction.price;
    sessionAsset.invest_comm = 0.;
    sessionAsset.invest_base = transaction.total;
    sessionAsset.balance_comm = transaction.amount;
    sessionAsset.balance_base = 0.;
    double fee = 0.005;
    sessionAsset.profitable_price = transaction.price * (1. + fee * 2.);
    double sellPriceRise = broker.getProperty("Sell Price Rise", DEFAULT_SELL_PRICE_RISE) * 0.01;
    sessionAsset.flip_price = sessionAsset.profitable_price * (1. + sellPriceRise);
    sessionAsset.order_id = 0;
    broker.createAsset(sessionAsset);

    buyInOrderId = 0;
    buyInState = idle;
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
          meguco_user_session_asset_entity lowestSellAsset = *sortedSellAssets.back();

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
          double newProfitablePrice = (lowestSellAsset.balance_comm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balance_base * (1. + fee)) / lowestSellAsset.balance_comm;
          lowestSellAsset.flip_price += (newProfitablePrice - lowestSellAsset.profitable_price);
          lowestSellAsset.profitable_price = newProfitablePrice;

          broker.updateAsset(lowestSellAsset);
        }
        if(!sortedBuyAssets.isEmpty())
        {
          meguco_user_session_asset_entity highestBuyAsset = *sortedBuyAssets.front();

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
          double newProfitablePrice = highestBuyAsset.balance_base / (highestBuyAsset.balance_base / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balance_comm * (1. + fee));
          highestBuyAsset.flip_price += (newProfitablePrice - highestBuyAsset.profitable_price);
          highestBuyAsset.profitable_price = newProfitablePrice;

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

void BetBot2::Session::handleSell(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction)
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
    sessionAsset.time = transaction.entity.time;
    sessionAsset.price = transaction.price;
    sessionAsset.invest_comm = transaction.amount;
    sessionAsset.invest_base = 0.;
    sessionAsset.balance_comm = 0.;
    sessionAsset.balance_base = transaction.total;
    double fee = 0.005;
    sessionAsset.profitable_price = transaction.price / (1. + fee * 2.);
    double sellPriceDrop = broker.getProperty("Sell Price Drop", DEFAULT_SELL_PRICE_DROP) * 0.01;
    sessionAsset.flip_price = sessionAsset.profitable_price / (1. + sellPriceDrop);
    sessionAsset.order_id = 0;
    broker.createAsset(sessionAsset);

    sellInOrderId = 0;
    sellInState = idle;
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
          meguco_user_session_asset_entity highestBuyAsset = *sortedBuyAssets.front();

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
          double newProfitablePrice = highestBuyAsset.balance_base / (highestBuyAsset.balance_base / highestBuyAsset.price * (1. + 2. * fee) - highestBuyAsset.balance_comm * (1. + fee));
          highestBuyAsset.flip_price += (newProfitablePrice - highestBuyAsset.profitable_price);
          highestBuyAsset.profitable_price = newProfitablePrice;

          broker.updateAsset(highestBuyAsset);
        }
        if(!sortedSellAssets.isEmpty())
        {
          meguco_user_session_asset_entity lowestSellAsset = *sortedSellAssets.back();

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
          double newProfitablePrice = (lowestSellAsset.balance_comm * lowestSellAsset.price * (1. + 2. * fee) - lowestSellAsset.balance_base * (1. + fee)) / lowestSellAsset.balance_comm;
          lowestSellAsset.flip_price += (newProfitablePrice - lowestSellAsset.profitable_price);
          lowestSellAsset.profitable_price = newProfitablePrice;

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

void_t BetBot2::Session::handleBuyTimeout(uint64_t orderId)
{
  if(orderId == buyInOrderId)
  {
    buyInOrderId = 0;
    buyInState = idle;
  }
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

void_t BetBot2::Session::handleSellTimeout(uint64_t orderId)
{
  if(orderId == sellInOrderId)
  {
    sellInOrderId = 0;
    sellInState = idle;
  }
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

void BetBot2::Session::checkBuyIn(const meguco_trade_entity& trade, const TradeHandler::Values& values)
{
  double buyInPriceDrop = broker.getProperty("BuyIn Price Drop", DEFAULT_BUYIN_PRICE_DROP) * 0.01;
  //int64_t buyInPriceAge =  (int64_t)broker.getProperty("BuyIn Pirce Age", DEFAULT_BUYIN_PRICE_AGE) * 1000;
  //double range = (values.regressions[TradeHandler::regression12h].max - values.regressions[TradeHandler::regression12h].min);
  double incline = values.regressions[TradeHandler::regression12h].incline;
  int64_t buyInPredictTime = (int64_t)broker.getProperty("BuyIn Predict Time", DEFAULT_BUYIN_PREDICT_TIME);
  double newBuyInPrice = trade.price + incline * buyInPredictTime;

  int64_t buyInCooldown = (int64_t)broker.getProperty("BuyIn Cooldown", DEFAULT_BUYIN_COOLDOWN) * 1000;
  if((int64_t)trade.entity.time - lastBuyInTime < buyInCooldown)
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
        int64_t buyInTimeout = (int64_t)broker.getProperty("BuyIn Timeout", DEFAULT_BUYIN_TIMEOUT) * 1000;
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

void BetBot2::Session::checkSellIn(const meguco_trade_entity& trade, const TradeHandler::Values& values)
{
  double buyInPriceRise = broker.getProperty("BuyIn Price Rise", DEFAULT_BUYIN_PRICE_RISE) * 0.01;
  //int64_t buyInPriceAge =  (int64_t)broker.getProperty("BuyIn Pirce Age", DEFAULT_BUYIN_PRICE_AGE) * 1000;
  //double range = (values.regressions[TradeHandler::regression12h].max - values.regressions[TradeHandler::regression12h].min);
  double incline = values.regressions[TradeHandler::regression12h].incline;
  int64_t buyInPredictTime = (int64_t)broker.getProperty("BuyIn Predict Time", DEFAULT_BUYIN_PREDICT_TIME);
  double newSellInPrice = trade.price + incline * buyInPredictTime;

  int64_t buyInCooldown = (int64_t)broker.getProperty("BuyIn Cooldown", DEFAULT_BUYIN_COOLDOWN) * 1000;
  if((int64_t)trade.entity.time - lastSellInTime < buyInCooldown)
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
        int64_t buyInTimeout = (int64_t)broker.getProperty("BuyIn Timeout", DEFAULT_BUYIN_TIMEOUT) * 1000;
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

void BetBot2::Session::checkAssetBuy(const meguco_trade_entity& trade)
{
  int64_t sellCooldown = (int64_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN) * 1000;
  if((int64_t)trade.entity.time - lastAssetBuyTime < sellCooldown)
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

      int64_t buyTimeout = (int64_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT) * 1000;
      if(broker.buy(tradePrice, buyAmountComm, buyAmountBase, buyTimeout, &updatedAsset.order_id, 0))
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

void BetBot2::Session::checkAssetSell(const meguco_trade_entity& trade)
{
   int64_t sellCooldown = (int64_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN) * 1000;
  if((int64_t)trade.entity.time - lastAssetSellTime < sellCooldown)
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

      int64_t sellTimeout = (int64_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT) * 1000;
      if(broker.sell(tradePrice, buyAmountComm, buyAmountBase, sellTimeout, &updatedAsset.order_id, 0))
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

void_t BetBot2::Session::handlePropertyUpdate(const meguco_user_session_property_entity& property)
{
  balanceBase = broker.getProperty(String("Balance ") + broker.getCurrencyBase(), 0);
  balanceComm = broker.getProperty(String("Balance ") + broker.getCurrencyComm(), 0);

  updateAvailableBalance();
}

void_t BetBot2::Session::handleAssetUpdate(const meguco_user_session_asset_entity& asset)
{
  updateAvailableBalance();
}

void_t BetBot2::Session::handleAssetRemoval(const meguco_user_session_asset_entity& asset)
{
  updateAvailableBalance();
}
