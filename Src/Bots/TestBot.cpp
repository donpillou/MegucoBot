
#include "TestBot.h"

void TestBot::Session::setParameters(double* parameters)
{
}

void TestBot::Session::handle(const DataProtocol::Trade& trade, const Values& values)
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
    BotProtocol::SessionItem item;
    item.entityType = BotProtocol::sessionItem;
    item.type = BotProtocol::SessionItem::buy;
    item.state = BotProtocol::SessionItem::waitSell;
    item.date = 89;
    item.price = 300;
    item.balanceComm = 0.;
    item.balanceBase = 12.;
    item.price = 320;
    item.profitablePrice = 330;
    item.flipPrice = 340;
    if(!broker.createItem(item))
      broker.warning("createItem returned false.");
    {
      const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
      if(items.size() == 0)
        broker.warning("items size is 0.");
      else
      {
        BotProtocol::SessionItem item = *items.begin();
        double newBalanceBase = item.balanceBase / 2.;
        item.balanceBase = newBalanceBase;
        broker.updateItem(item);
        size_t itemCount = items.size();
        const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
        if(items.size() != itemCount)
          broker.warning("item count changed after update.");
        BotProtocol::SessionItem& item2 = *items.find(item.entityId);
        if(item.balanceBase != item2.balanceBase)
          broker.warning("item update failed.");
        broker.removeItem(item2.entityId);
        const HashMap<uint32_t, BotProtocol::SessionItem>& items2 = broker.getItems();
        if(items2.size() != itemCount - 1)
          broker.warning("item remove failed.");
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
