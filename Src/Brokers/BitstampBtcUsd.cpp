
#include <nstd/Time.h>
#include <nstd/Thread.h>
#include <nstd/Math.h>

#include "Tools/Sha256.h"
#include "Tools/Hex.h"
#include "Tools/Json.h"
#include "Tools/ZlimdbConnection.h"

#include "BitstampBtcUsd.h"

BitstampBtcUsd::BitstampBtcUsd(const String& clientId, const String& key, const String& secret) :
  clientId(clientId), key(key), secret(secret),
  balanceLoaded(false), lastRequestTime(0), lastNonce(0), lastLiveTradeUpdateTime(0)/*, nextEntityId(1)*/ {}

bool_t BitstampBtcUsd::loadBalanceAndFee()
{
  if(balanceLoaded)
    return true;
  meguco_user_market_balance_entity balance;
  if(!loadBalance(balance))
    return false;
  return true;
}

bool_t BitstampBtcUsd::createOrder(uint64_t id, meguco_user_market_order_type type, double price, double amount, double total, meguco_user_market_order_entity& order)
{
  if(!loadBalanceAndFee())
    return false;

  bool buy = type == meguco_user_market_order_buy;

  if(amount == 0.)
  { // compute amount based on total
    if(buy)
    {
      //total = Math::ceil(amount * price * (1. + balance.fee) * 100.) / 100.;
      //total * 100. = Math::ceil(amount * price * (1. + balance.fee) * 100.);
      //total * 100. = amount * price * (1. + balance.fee) * 100.; // maximize amount
      //total = amount * price * (1. + balance.fee);
      //total / (price * (1. + balance.fee)) = amount;
      amount = total / (price * (1. + balance.fee));
      amount = Math::floor(amount * 100000000.) / 100000000.;
    }
    else
    {
      //total = Math::floor(amount * price * (1. - balance.fee) * 100.) / 100.;
      //total * 100. = Math::floor(amount * price * (1. - balance.fee) * 100.);
      //total * 100. = amount * price * (1. - balance.fee) * 100.; // minimize amount
      //total = amount * price * (1. - balance.fee);
      //total / (price * (1. - balance.fee)) = amount;
      amount = total / (price * (1. - balance.fee));
      amount = Math::ceil(amount * 100000000.) / 100000000.;
    }
  }
  else
  {
    if(buy)
    { // maximize buy amount
      total = Math::ceil(amount * price * (1. + balance.fee) * 100.) / 100.;
      amount = total / (price * (1. + balance.fee));
      amount = Math::floor(amount * 100000000.) / 100000000.;
    }
    else
    { // minimize sell amount
      total = Math::floor(amount * price * (1. - balance.fee) * 100.) / 100.;
      amount = total / (price * (1. - balance.fee));
      amount = Math::ceil(amount * 100000000.) / 100000000.;
    }
  }

  double maxAmount = Math::abs(buy ? getMaxBuyAmout(price) : getMaxSellAmout());
  if(Math::abs(amount) > maxAmount)
    amount = maxAmount;

  String priceStr, amountStr;
  priceStr.printf("%.2f", price);
  amountStr.printf("%.8f", Math::abs(amount));

  HashMap<String, Variant> args;
  args.append("amount", amountStr);
  args.append("price", priceStr);

  String url = buy ? String("https://www.bitstamp.net/api/buy/") : String("https://www.bitstamp.net/api/sell/");
  Variant result;
  if(!request(url, false, args, result))
    return false;

  const HashMap<String, Variant>& orderData = result.toMap();

  ZlimdbConnection::setEntityHeader(order.entity, 0, 0, sizeof(order));
  order.raw_id = orderData.find("id")->toUInt64();
  String btype = orderData.find("type")->toString();
  if(btype != "0" && btype != "1")
  {
    error = "Received invalid order type.";
    return false;
  }

  buy = btype == "0";
  order.type = buy ? meguco_user_market_order_buy : meguco_user_market_order_sell;

  String dateStr = orderData.find("datetime")->toString();
  const tchar_t* lastDot = dateStr.findLast('.');
  if(lastDot)
    dateStr.resize(lastDot - (const tchar_t*)dateStr);
  Time time(true);
  if(dateStr.scanf("%d-%d-%d %d:%d:%d", &time.year, &time.month, &time.day, &time.hour, &time.min, &time.sec) != 6)
  {
    error = "Received invalid order date.";
    return false;
  }
  order.entity.time = time.toTimestamp();

  order.price = orderData.find("price")->toDouble();
  order.amount = Math::abs(orderData.find("amount")->toDouble());
  order.total = Math::abs(getOrderCharge(buy ? order.amount : -order.amount, order.price));
  this->orders.append(order.raw_id, order);

  // update balance
  if(order.amount > 0) // buy order
  {
    balance.available_usd -= order.total;
    balance.reserved_usd += order.total;
  }
  else // sell order
  {
    balance.available_btc -= order.amount;
    balance.reserved_btc += order.amount;
  }
  return true;
}
/*
bool_t BitstampBtcUsd::getOrder(uint32_t entityId, BotProtocol::Order& order)
{
  HashMap<uint32_t, BotProtocol::Order>::Iterator it = orders.find(entityId);
  if(it == orders.end())
  {
    error = "Unknown order.";
    return false;
  }
  order = *it;
  return true;
}
*/
bool_t BitstampBtcUsd::cancelOrder(uint64_t id)
{
  HashMap<uint64_t, meguco_user_market_order_entity>::Iterator it = orders.find(id);
  if(it == orders.end())
  {
    error = "Unknown order.";
    return false;
  }
  const meguco_user_market_order_entity& order = *it;

  HashMap<String, Variant> args;
  args.append("id", id);
  Variant result;
  if(!request("https://www.bitstamp.net/api/cancel_order/", false, args, result))
    return false;
  if(!result.toBool())
  {
    // todo: check if order is still on the orders list... if not, than it is already canceled and we should return true
    error = "Could not find or cancel order.";
    return false;
  }

  // update balance
  if(order.type == meguco_user_market_order_buy) // buy order
  {
    double total = Math::abs(getOrderCharge(order.amount, order.price));
    balance.available_usd += total;
    balance.reserved_usd -= total;
  }
  else // sell order
  {
    balance.available_btc += order.amount;
    balance.reserved_btc -= order.amount;
  }
  orders.remove(it);
  return true;
}

bool_t BitstampBtcUsd::loadOrders(List<meguco_user_market_order_entity>& orders)
{
  if(!loadBalanceAndFee())
    return false;

  Variant result;
  if(!request("https://www.bitstamp.net/api/open_orders/", false, HashMap<String, Variant>(), result))
    return false;

  const List<Variant>& ordersData = result.toList();
  this->orders.clear();
  meguco_user_market_order_entity order;
  ZlimdbConnection::setEntityHeader(order.entity, 0, 0, sizeof(order));
  Time time(true);
  for(List<Variant>::Iterator i = ordersData.begin(), end = ordersData.end(); i != end; ++i)
  {
    const Variant& orderDataVar = *i;
    HashMap<String, Variant> orderData = orderDataVar.toMap();
    
    order.raw_id = orderData.find("id")->toUInt64();
    String type = orderData.find("type")->toString();
    if(type != "0" && type != "1")
      continue;

    bool buy = type == "0";
    order.type = buy ? meguco_user_market_order_buy : meguco_user_market_order_sell;

    String dateStr = orderData.find("datetime")->toString();

    if(dateStr.scanf("%d-%d-%d %d:%d:%d", &time.year, &time.month, &time.day, &time.hour, &time.min, &time.sec) != 6)
      continue;
    order.entity.time = time.toTimestamp();

    order.price = orderData.find("price")->toDouble();
    order.amount = Math::abs(orderData.find("amount")->toDouble());
    order.total = Math::abs(getOrderCharge(buy ? order.amount : -order.amount, order.price));

    this->orders.append(order.raw_id, order);
    orders.append(order);
  }
  return true;
}

bool_t BitstampBtcUsd::loadBalance(meguco_user_market_balance_entity& balance)
{
  Variant result;
  if(!request("https://www.bitstamp.net/api/balance/", false, HashMap<String, Variant>(), result))
    return false;

  const HashMap<String, Variant>& balanceData = result.toMap();
  ZlimdbConnection::setEntityHeader(balance.entity, 0, 0, sizeof(balance));
  balance.reserved_usd = balanceData.find("usd_reserved")->toDouble();
  balance.reserved_btc = balanceData.find("btc_reserved")->toDouble();
  balance.available_usd = balanceData.find("usd_available")->toDouble();
  balance.available_btc = balanceData.find("btc_available")->toDouble();
  balance.fee =  balanceData.find("fee")->toDouble() * 0.01;
  this->balance = balance;
  this->balanceLoaded = true;
  return true;
}

bool_t BitstampBtcUsd::loadTransactions(List<meguco_user_market_transaction_entity>& transactions)
{
  Variant result;
  if(!request("https://www.bitstamp.net/api/user_transactions/", false, HashMap<String, Variant>(), result))
    return false;

  meguco_user_market_transaction_entity transaction;
  ZlimdbConnection::setEntityHeader(transaction.entity, 0, 0, sizeof(transaction));
  const List<Variant>& transactionData = result.toList();
  Time time(true);
  for(List<Variant>::Iterator i = transactionData.begin(), end = transactionData.end(); i != end; ++i)
  {
    const Variant& transactionDataVar = *i;
    const HashMap<String, Variant>& transactionData = transactionDataVar.toMap();
    
    transaction.raw_id = transactionData.find("id")->toUInt64();
    String type = transactionData.find("type")->toString();
    if(type != "2")
      continue;

    String dateStr = transactionData.find("datetime")->toString();
    if(dateStr.scanf("%d-%d-%d %d:%d:%d", &time.year, &time.month, &time.day, &time.hour, &time.min, &time.sec) != 6)
      continue;
    transaction.entity.time = time.toTimestamp();

    double fee = Math::abs(transactionData.find("fee")->toDouble());

    double value = transactionData.find("usd")->toDouble();
    bool buy = value < 0.;
    transaction.type = buy ? meguco_user_market_transaction_buy : meguco_user_market_transaction_sell;
    transaction.amount = Math::abs(transactionData.find("btc")->toDouble());
    transaction.total = buy ? (Math::abs(value) + fee) : (Math::abs(value) - fee);
    transaction.price = Math::abs(value) / Math::abs(transaction.amount);

    transactions.append(transaction);
  }

  return true;
}

double BitstampBtcUsd::getMaxSellAmout() const
{
  return balance.available_btc;
}

double BitstampBtcUsd::getMaxBuyAmout(double price) const
{
  double fee = balance.fee; // e.g. 0.0044
  double availableUsd = balance.available_usd;
  double additionalAvailableUsd = 0.; //floor(canceledAmount * canceledPrice * (1. + fee) * 100.) / 100.;
  double usdAmount = availableUsd + additionalAvailableUsd;
  double result = Math::floor(((100. / ( 100. + (fee * 100.))) * usdAmount) * 100.) / 100.;
  result /= price;
  result = Math::floor(result * 100000000.) / 100000000.;
  return result;
}

double BitstampBtcUsd::getOrderCharge(double amount, double price) const
{
  //if(amount < 0.) // sell order
  //  return Math::floor(-amount * price / (1. + balance.fee) * 100.) / 100.;
  //else // buy order
  //  return Math::floor(amount * price * (1. + balance.fee) * -100.) / 100.;

  if(amount >= 0.) // buy order
  {
    double result = amount * price;
    result = result * balance.fee + result;
    result = Math::ceil(result * 100.) / 100.;
    return result;
  }
  else // sell order
  {
    double result = -amount * price;
    result = result - result * balance.fee;
    //result = Math::ceil(result * 100.) / 100.; // bitstamp's website computes the total of a sell order this way, but its probably incorrect
    result = Math::floor(result * 100.) / 100.;
    return result;
  }
}

bool_t BitstampBtcUsd::request(const String& url, bool_t isPublic, const HashMap<String, Variant>& params, Variant& result)
{
  avoidSpamming();

  Buffer buffer;
  if (isPublic)
  {
    if(!httpRequest.get(url, buffer))
    {
      error = httpRequest.getErrorString();
      return false;
    }
  }
  else
  {
    uint64_t newNonce = Time::time() / 1000LL;
    if(newNonce <= lastNonce)
      newNonce = lastNonce + 1;
    lastNonce = newNonce;

    String nonce;
    nonce.printf("%llu", newNonce);
    String message = nonce + clientId + key;
    byte_t signatureBuffer[Sha256::digestSize];
    Sha256::hmac((const byte_t*)(const char_t*)secret, secret.length(), (const byte_t*)(const char_t*)message, message.length(), signatureBuffer);
    String signature = Hex::toString(signatureBuffer, sizeof(signatureBuffer));
    signature.toUpperCase();
    
    HashMap<String, String> formData;
    formData.append("key", key);
    formData.append("signature", signature);
    formData.append("nonce", nonce);
    for(HashMap<String, Variant>::Iterator j = params.begin(), end = params.end(); j != end; ++j)
      formData.append(j.key(), j->toString());

    if(!httpRequest.post(url, formData, buffer))
    {
      error = httpRequest.getErrorString();
      return false;
    }
  }

  if(!Json::parse(buffer, result) || result.isNull())
  {
    error = "Received unparsable data.";
    return false;
  }
  else if(result.getType() == Variant::mapType && result.toMap().contains("error"))
  {
    List<String> errors;
    struct ErrorStringCollector
    {
      static void collect(const Variant& var, List<String>& errors)
      {
        switch(var.getType())
        {
        case Variant::stringType:
          errors.append(var.toString());
          break;
        case Variant::listType:
          {
            const List<Variant>& list = var.toList();
            for(List<Variant>::Iterator i = list.begin(), end = list.end(); i != end; ++i)
              collect(*i, errors);
          }
          break;
        case Variant::mapType:
          {
            const HashMap<String, Variant>& map = var.toMap();
            for(HashMap<String, Variant>::Iterator i = map.begin(), end = map.end(); i != end; ++i)
              collect(*i, errors);
          }
          break;
        default:
          break;
        }
      }
    };
    ErrorStringCollector::collect(result, errors);
    error.clear();
    if(!errors.isEmpty())
      for(List<String>::Iterator i = errors.begin(), end = errors.end();; ++i)
      {
        error += *i;
        if(i == end)
          break;
        error += ' ';
      }
    return false;
  }
  else
    return true;
}

void_t BitstampBtcUsd::avoidSpamming()
{
  const timestamp_t queryDelay = 1337LL;
  timestamp_t now = Time::time();
  timestamp_t elapsed = lastRequestTime == 0 ? queryDelay : (now - lastRequestTime);
  if(elapsed < queryDelay)
  {
    timestamp_t sleepMs = queryDelay - elapsed;
    Thread::sleep(sleepMs);
    lastRequestTime = now + sleepMs;
  }
  else
    lastRequestTime = now;
  // TODO: allow more than 1 request per second but limit requests to 600 per 10 minutes
}
