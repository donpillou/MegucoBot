
#include <nstd/Time.h>

#include "Order.h"

Order::Order(uint32_t id, double price, double amount, double fee, BotProtocol::Order::Type type) :
  id(id), price(price), amount(amount), fee(fee), type(type)
{
  date = Time::time();
}

Order::Order(const Variant& variant)
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
