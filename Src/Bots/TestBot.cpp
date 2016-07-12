
#include "TestBot.h"

TestBot::Session::Session(Broker& broker) : broker(broker), updateCount(0)
{
  broker.registerProperty("prop1", 42., meguco_user_session_property_read_only, "leet");
  broker.registerProperty("prop2", "sda", meguco_user_session_property_read_only, "teel");
  broker.registerProperty("prop4ro", "edit me", meguco_user_session_property_none, "teel");
}

void TestBot::Session::handleTrade(const meguco_trade_entity& trade, int64_t tradeAge)
{
  if(updateCount++ == 0)
  {
    broker.warning("Executing bot test...");

    // test buy and sell
    if(!broker.buy(100, 0.02, 0., 30 * 1000))
      broker.warning("buy returned false.");
    if(!broker.sell(2000, 0.01, 0., 25 * 1000))
      broker.warning("sell returned false.");
    if(broker.getOpenBuyOrderCount() != 1)
      broker.warning("buy order count is not 1.");
    if(broker.getOpenSellOrderCount() != 1)
      broker.warning("sell order count is not 1.");

    // test item creating, updating and removing
    meguco_user_session_asset_entity asset;
    asset.type = meguco_user_session_asset_buy;
    asset.state = meguco_user_session_asset_wait_sell;
    asset.lastTransactionTime = 89;
    asset.price = 300;
    asset.invest_comm = 0.;
    asset.invest_base = 0.;
    asset.balance_comm = 0.;
    asset.balance_base = 12.;
    asset.price = 320;
    asset.profitable_price = 330;
    asset.flip_price = 340;
    if(!broker.createAsset(asset))
      broker.warning("createItem returned false.");
    {
      const HashMap<uint64_t, meguco_user_session_asset_entity>& assets = broker.getAssets();
      if(assets.size() == 0)
        broker.warning("items size is 0.");
      else
      {
        meguco_user_session_asset_entity asset = *assets.begin();
        double newBalanceBase = asset.balance_base / 2.;
        asset.balance_base = newBalanceBase;
        size_t assetCount = assets.size();
        broker.updateAsset(asset);
        const HashMap<uint64_t, meguco_user_session_asset_entity>& assets = broker.getAssets();
        if(assets.size() != assetCount)
          broker.warning("asset count changed after update.");
        meguco_user_session_asset_entity& asset2 = *assets.find(asset.entity.id);
        if(asset.balance_base != asset2.balance_base)
          broker.warning("asset update failed.");
        broker.removeAsset(asset2.entity.id);
        const HashMap<uint64_t, meguco_user_session_asset_entity>& assets2 = broker.getAssets();
        if(assets2.size() != assetCount - 1)
          broker.warning("asset remove failed.");
      }
    }

    // test properties
    broker.setProperty("prop1", 42.);
    broker.setProperty("prop2", "sda");
    broker.setProperty("prop4ro", "edit me");
    if(broker.getProperty("prop1", 23.) != 42.)
      broker.warning("property has incorrect value.");
    broker.setProperty("prop1", 43.);
    if(broker.getProperty("prop1", 23.) != 43.)
      broker.warning("prop1 has incorrect value.");
    if(broker.getProperty("prop2", "hallo") != "sda")
      broker.warning("prop2 has incorrect value.");
    if(broker.getProperty("prop3notexistent", 123.) != 123.)
      broker.warning("prop3notexistent has incorrect value.");

    broker.warning("finished test.");
  }
}

void TestBot::Session::handleBuy(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction)
{
}

void TestBot::Session::handleSell(uint64_t orderId, const meguco_user_broker_transaction_entity& transaction)
{
}
