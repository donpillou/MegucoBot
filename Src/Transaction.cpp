
#include "Transaction.h"

Transaction::Transaction(uint32_t id, double price, double amount, double fee, BotProtocol::Transaction::Type type) :
  id(id), price(price), amount(amount), fee(fee), type(type) {}

Transaction::Transaction(const Variant& variant)
{
  const HashMap<String, Variant>& data = variant.toMap();
  id = data.find("id")->toUInt();
  price = data.find("price")->toDouble();
  amount = data.find("amount")->toDouble();
  fee = data.find("fee")->toDouble();
  type = (BotProtocol::Transaction::Type)data.find("type")->toUInt();
}

void_t Transaction::toVariant(Variant& variant)
{
  HashMap<String, Variant>& data = variant.toMap();
  data.append("id", id);
  data.append("price", price);
  data.append("amount", amount);
  data.append("fee", fee);
  data.append("type", (uint_t)type);
}
