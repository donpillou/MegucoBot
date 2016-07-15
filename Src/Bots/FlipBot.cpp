
#include <nstd/Map.h>
#include <nstd/Math.h>
#include <megucoprotocol.h>
#include <zlimdbprotocol.h>

#include "FlipBot.h"

#define DEFAULT_BUY_PROFIT_GAIN 0.4
#define DEFAULT_SELL_PROFIT_GAIN 0.4
#define DEFAULT_BUY_COOLDOWN (60 * 60)
#define DEFAULT_BUY_TIMEOUT (60 * 60)
#define DEFAULT_SELL_COOLDOWN (60 * 60)
#define DEFAULT_SELL_TIMEOUT (60 * 60)

FlipBot::Session::Session(Broker& broker) : broker(broker)//, minBuyInPrice(0.), maxSellInPrice(0.)
{
  broker.registerProperty("Sell Profit Gain", DEFAULT_SELL_PROFIT_GAIN);
  broker.registerProperty("Buy Profit Gain", DEFAULT_BUY_PROFIT_GAIN);
  broker.registerProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  broker.registerProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
  broker.registerProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  broker.registerProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);

  broker.registerProperty(String("Balance ") + broker.getCurrencyBase(), 0, meguco_user_session_property_read_only, broker.getCurrencyBase());
  broker.registerProperty(String("Balance ") + broker.getCurrencyComm(), 0, meguco_user_session_property_read_only, broker.getCurrencyComm());

  updateBalance();
}

void_t FlipBot::Session::updateBalance()
{
  double balanceBase = 0.;
  double balanceComm = 0.;
  const HashMap<uint64_t, Bot::Asset>& assets = broker.getAssets();
  for(HashMap<uint64_t, Bot::Asset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const Bot::Asset& asset = *i;
    balanceComm += asset.balance_comm;
    balanceBase += asset.balance_base;
  }
  broker.setProperty(String("Balance ") + broker.getCurrencyBase(), balanceBase);
  broker.setProperty(String("Balance ") + broker.getCurrencyComm(), balanceComm);
}

void FlipBot::Session::handleTrade2(const Bot::Trade& trade, int64_t tradeAge)
{
  checkBuy(trade);
  checkSell(trade);
}

void FlipBot::Session::handleBuy2(uint64_t orderId, const Bot::Transaction& transaction2)
{
  String message;
  message.printf("Bought %.08f @ %.02f", transaction2.amount, transaction2.price);
  broker.warning(message);

  const HashMap<uint64_t, Bot::Asset>& assets = broker.getAssets();
  for(HashMap<uint64_t, Bot::Asset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const Bot::Asset& asset = *i;
    if(asset.state == meguco_user_session_asset_buying && asset.order_id == orderId)
    {
      Bot::Asset updatedAsset = asset;
      updatedAsset.state = meguco_user_session_asset_wait_sell;
      updatedAsset.order_id = 0;
      updatedAsset.price = transaction2.price;
      updatedAsset.balance_comm += transaction2.amount;
      updatedAsset.balance_base -= transaction2.total;
      //double fee = broker.getFee();
      double fee = 0.005;
      updatedAsset.profitable_price = transaction2.price * (1. + fee * 2.);
      double sellProfitGain = broker.getProperty("Sell Profit Gain", DEFAULT_SELL_PROFIT_GAIN);
      updatedAsset.flip_price = transaction2.price * (1. + fee * (1. + sellProfitGain) * 2.);
      updatedAsset.last_transaction_time = transaction2.time;

      broker.updateAsset2(updatedAsset);
      updateBalance();
      break;
    }
  }
}

void FlipBot::Session::handleSell2(uint64_t orderId, const Bot::Transaction& transaction2)
{
  String message;
  message.printf("Sold %.08f @ %.02f", transaction2.amount, transaction2.price);
  broker.warning(message);

  const HashMap<uint64_t, Bot::Asset>& assets = broker.getAssets();
  for(HashMap<uint64_t, Bot::Asset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const Bot::Asset& asset = *i;
    if(asset.state == meguco_user_session_asset_selling && asset.order_id == orderId)
    {
      Bot::Asset updatedAsset = asset;
      updatedAsset.state = meguco_user_session_asset_wait_buy;
      updatedAsset.order_id = 0;
      updatedAsset.price = transaction2.price;
      updatedAsset.balance_comm -= transaction2.amount;
      updatedAsset.balance_base += transaction2.total;
      //double fee = broker.getFee();
      double fee = 0.005;
      updatedAsset.profitable_price = transaction2.price / (1. + fee * 2.);
      double buyProfitGain = broker.getProperty("Buy Profit Gain", DEFAULT_BUY_PROFIT_GAIN);
      updatedAsset.flip_price = transaction2.price / (1. + fee * (1. + buyProfitGain) * 2.);
      updatedAsset.last_transaction_time = transaction2.time;

      broker.updateAsset2(updatedAsset);
      updateBalance();
      break;
    }
  }
}

void_t FlipBot::Session::handleBuyTimeout(uint64_t orderId)
{
  const HashMap<uint64_t, Bot::Asset>& assets = broker.getAssets();
  for(HashMap<uint64_t, Bot::Asset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const Bot::Asset& asset = *i;
    if(asset.state == meguco_user_session_asset_buying && asset.order_id == orderId)
    {
      Bot::Asset updatedAsset = asset;
      updatedAsset.state = meguco_user_session_asset_wait_buy;
      updatedAsset.order_id = 0;
      broker.updateAsset2(updatedAsset);
      break;
    }
  }
}

void_t FlipBot::Session::handleSellTimeout(uint64_t orderId)
{
  const HashMap<uint64_t, Bot::Asset>& assets = broker.getAssets();
  for(HashMap<uint64_t, Bot::Asset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const Bot::Asset& asset = *i;
    if(asset.state == meguco_user_session_asset_selling && asset.order_id == orderId)
    {
      Bot::Asset updatedAsset = asset;
      updatedAsset.state = meguco_user_session_asset_wait_sell;
      updatedAsset.order_id = 0;
      broker.updateAsset2(updatedAsset);
      break;
    }
  }
}

void_t FlipBot::Session::handleAssetUpdate2(const Bot::Asset& asset)
{
  updateBalance();
}

void_t FlipBot::Session::handleAssetRemoval2(const Bot::Asset& asset)
{
  updateBalance();
}

void FlipBot::Session::checkBuy(const Bot::Trade& trade)
{
  if(broker.getOpenBuyOrderCount() > 0)
    return; // there is already an open buy order
  int64_t buyCooldown = (int64_t)broker.getProperty("Buy Cooldown", DEFAULT_BUY_COOLDOWN);
  if(broker.getTimeSinceLastBuy() < buyCooldown * 1000)
    return; // do not buy too often

  double tradePrice = trade.price;
  const HashMap<uint64_t, Bot::Asset>& assets = broker.getAssets();
  for(HashMap<uint64_t, Bot::Asset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const Bot::Asset& asset = *i;
    if(asset.state == meguco_user_session_asset_wait_buy && tradePrice <= asset.flip_price)
    {
      Bot::Asset updatedAsset = asset;
      updatedAsset.state = meguco_user_session_asset_buying;
      broker.updateAsset2(updatedAsset);

      int64_t buyTimeout = (int64_t)broker.getProperty("Buy Timeout", DEFAULT_BUY_TIMEOUT);
      if(broker.buy(tradePrice, 0., asset.balance_base, buyTimeout * 1000, &updatedAsset.order_id, 0))
        broker.updateAsset2(updatedAsset);
      else
      {
        updatedAsset.state = meguco_user_session_asset_wait_buy;
        broker.updateAsset2(updatedAsset);
      }
      break;
    }
  }
}

void FlipBot::Session::checkSell(const Bot::Trade& trade)
{
  if(broker.getOpenSellOrderCount() > 0)
    return; // there is already an open sell order
  int64_t sellCooldown = (int64_t)broker.getProperty("Sell Cooldown", DEFAULT_SELL_COOLDOWN);
  if(broker.getTimeSinceLastSell() < sellCooldown * 1000)
    return; // do not sell too often

  double tradePrice = trade.price;
  const HashMap<uint64_t, Bot::Asset>& assets = broker.getAssets();
  for(HashMap<uint64_t, Bot::Asset>::Iterator i = assets.begin(), end = assets.end(); i != end; ++i)
  {
    const Bot::Asset& asset = *i;
    if(asset.state == meguco_user_session_asset_wait_sell && tradePrice >= asset.flip_price)
    {
      Bot::Asset updatedAsset = asset;
      updatedAsset.state = meguco_user_session_asset_selling;
      broker.updateAsset2(updatedAsset);

      int64_t sellTimeout = (int64_t)broker.getProperty("Sell Timeout", DEFAULT_SELL_TIMEOUT);
      if(broker.sell(tradePrice, asset.balance_comm, 0., sellTimeout * 1000, &updatedAsset.order_id, 0))
        broker.updateAsset2(updatedAsset);
      else
      {
        updatedAsset.state = meguco_user_session_asset_wait_sell;
        broker.updateAsset2(updatedAsset);
      }
      break;
    }
  }
}
