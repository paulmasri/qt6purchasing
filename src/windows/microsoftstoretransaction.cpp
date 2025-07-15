#include "microsoftstoretransaction.h"

MicrosoftStoreTransaction::MicrosoftStoreTransaction(
    const QString& orderId,
    const QString& productId,
    QObject * parent)
    : AbstractTransaction(orderId, parent)
    , _productId(productId)
{
    // All transactions created this way are successful
    _status = static_cast<int>(MicrosoftStoreTransactionStatus::Succeeded);
}

QString MicrosoftStoreTransaction::productId() const
{
    return _productId;
}
