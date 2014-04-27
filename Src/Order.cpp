
#include <nstd/Time.h>

#include "Order.h"
#include "ClientHandler.h"
#include "Session.h"

Order::Order(Session& session, uint32_t id, double price, double amount, double fee, BotProtocol::Order::Type type) :
  session(session),
  id(id), price(price), amount(amount), fee(fee), type(type)
{
  date = Time::time();
}

Order::Order(Session& session, const Variant& variant) :
  session(session)
{
  const HashMap<String, Variant>& data = variant.toMap();
  id = data.find("id")->toUInt();
  price = data.find("price")->toDouble();
  amount = data.find("amount")->toDouble();
  fee = data.find("fee")->toDouble();
  type = (BotProtocol::Order::Type)data.find("type")->toUInt();
  date = data.find("date")->toInt64();
}

void_t Order::toVariant(Variant& variant)
{
  HashMap<String, Variant>& data = variant.toMap();
  data.append("id", id);
  data.append("price", price);
  data.append("amount", amount);
  data.append("fee", fee);
  data.append("type", (uint_t)type);
  data.append("date", date);
}

void_t Order::send(ClientHandler* client)
{
  BotProtocol::Order orderData;
  orderData.entityType = BotProtocol::sessionOrder;
  orderData.entityId = id;
  orderData.price = price;
  orderData.amount = amount;
  orderData.fee = fee;
  orderData.type = type;
  orderData.date = date;
  if(client)
    client->sendEntity(&orderData, sizeof(orderData));
  else
    session.sendEntity(&orderData, sizeof(orderData));
}
