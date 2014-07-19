
#include "TestBot.h"

void TestBot::Session::setParameters(double* parameters)
{
}

void TestBot::Session::handle(const DataProtocol::Trade& trade, const Values& values)
{
  if(updateCount++ == 0)
  {
    broker.warning("Executing bot test...");
    String message;
    message.printf("balance is: base=%f, comm=%f, fee=%f", broker.getBalanceBase(), broker.getBalanceComm(), broker.getFee());
    broker.warning(message);

    BotProtocol::SessionItem item;
    item.entityType = BotProtocol::sessionItem;
    item.type = BotProtocol::SessionItem::buy;
    item.state = BotProtocol::SessionItem::waitSell;
    item.date = 89;
    item.price = 300;
    item.amount = 0.02;
    item.price = 320;
    item.profitablePrice = 330;
    item.flipPrice = 340;
    if(!broker.createItem(item))
      broker.warning("createItem returned false.");

    if(!broker.buy(300, 0.02, 30 * 1000))
      broker.warning("buy returned false.");
    if(!broker.sell(1000, 0.01, 25 * 1000))
      broker.warning("sell returned false.");
    if(broker.getOpenBuyOrderCount() != 1)
      broker.warning("buy order count is not 1.");
    if(broker.getOpenSellOrderCount() != 1)
      broker.warning("sell order count is not 1.");
    {
      const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
      if(items.size() == 0)
        broker.warning("items size is 0.");
      else
      {
        BotProtocol::SessionItem item = *items.begin();
        double newAmount = item.amount / 2.;
        item.amount /= newAmount;
        broker.updateItem(item);
        size_t itemCount = items.size();
        const HashMap<uint32_t, BotProtocol::SessionItem>& items = broker.getItems();
        if(items.size() != itemCount)
          broker.warning("item count changed after update.");
        BotProtocol::SessionItem& item2 = *items.find(item.entityId);
        if(item.amount != item2.amount)
          broker.warning("item update failed.");
        broker.removeItem(item2.entityId);
        const HashMap<uint32_t, BotProtocol::SessionItem>& items2 = broker.getItems();
        if(items2.size() != itemCount - 1)
          broker.warning("item remove failed.");
      }
    }
    broker.warning("finished test.");
  }
}

void TestBot::Session::handleBuy(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
}

void TestBot::Session::handleSell(uint32_t orderId, const BotProtocol::Transaction& transaction)
{
}
