
#include "TestBot.h"

void TestBot::Session::handleTrade(const DataProtocol::Trade& trade, const Values& values)
{
  if(updateCount++ == 0)
  {
    broker.warning("Executing bot test...");

    // test buy and sell
    if(!broker.buy(300, 0.02, 0., 30 * 1000))
      broker.warning("buy returned false.");
    if(!broker.sell(1000, 0.01, 0., 25 * 1000))
      broker.warning("sell returned false.");
    if(broker.getOpenBuyOrderCount() != 1)
      broker.warning("buy order count is not 1.");
    if(broker.getOpenSellOrderCount() != 1)
      broker.warning("sell order count is not 1.");

    // test item creating, updating and removing
    BotProtocol::SessionAsset asset;
    asset.entityType = BotProtocol::sessionAsset;
    asset.type = BotProtocol::SessionAsset::buy;
    asset.state = BotProtocol::SessionAsset::waitSell;
    asset.date = 89;
    asset.price = 300;
    asset.balanceComm = 0.;
    asset.balanceBase = 12.;
    asset.price = 320;
    asset.profitablePrice = 330;
    asset.flipPrice = 340;
    if(!broker.createAsset(asset))
      broker.warning("createItem returned false.");
    {
      const HashMap<uint32_t, BotProtocol::SessionAsset>& assets = broker.getAssets();
      if(assets.size() == 0)
        broker.warning("items size is 0.");
      else
      {
        BotProtocol::SessionAsset asset = *assets.begin();
        double newBalanceBase = asset.balanceBase / 2.;
        asset.balanceBase = newBalanceBase;
        broker.updateAsset(asset);
        size_t assetCount = assets.size();
        const HashMap<uint32_t, BotProtocol::SessionAsset>& assets = broker.getAssets();
        if(assets.size() != assetCount)
          broker.warning("asset count changed after update.");
        BotProtocol::SessionAsset& asset2 = *assets.find(asset.entityId);
        if(asset.balanceBase != asset2.balanceBase)
          broker.warning("asset update failed.");
        broker.removeAsset(asset2.entityId);
        const HashMap<uint32_t, BotProtocol::SessionAsset>& assets2 = broker.getAssets();
        if(assets2.size() != assetCount - 1)
          broker.warning("asset remove failed.");
      }
    }

    // test properties
    broker.removeProperty("prop1");
    broker.removeProperty("prop2");
    broker.removeProperty("prop3");
    broker.removeProperty("prop4ro");
    size_t propCount = broker.getProperties().size();
    broker.setProperty("prop1", 42., BotProtocol::SessionProperty::readOnly, "leet");
    broker.setProperty("prop2", "sda", BotProtocol::SessionProperty::readOnly, "teel");
    broker.setProperty("prop4ro", "edit me", BotProtocol::SessionProperty::none, "teel");
    if(broker.getProperties().size() != propCount + 3)
      broker.warning("property creating did not increase property count.");
    if(broker.getProperty("prop1", 23.) != 42.)
      broker.warning("property has incorrect value.");
    broker.setProperty("prop1", 43., BotProtocol::SessionProperty::readOnly, "leet2");
    if(broker.getProperty("prop1", 23.) != 43.)
      broker.warning("property has incorrect value.");
    if(broker.getProperty("prop2", "hallo") != "sda")
      broker.warning("property has incorrect value.");
    if(broker.getProperty("prop3", 123.) != 123.)
      broker.warning("property has incorrect value.");
    propCount = broker.getProperties().size();
    broker.removeProperty("prop2");
    if(broker.getProperties().size() != propCount - 1)
      broker.warning("property remove failed.");

    broker.warning("finished test.");
  }
}

void TestBot::Session::handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
}

void TestBot::Session::handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
}
