#include "microsoftstoretransaction.h"

MicrosoftStoreTransaction::MicrosoftStoreTransaction(
    const QString& orderId,
    const QString& productId,
    QObject * parent)
    : AbstractTransaction(orderId, parent)
    , _productId(productId)
{
    // All transactions created this way are successful
    _status = 0;
}

QString MicrosoftStoreTransaction::productId() const
{
    return _productId;
}
