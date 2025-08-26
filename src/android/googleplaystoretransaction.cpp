#include "googleplaystoretransaction.h"

GooglePlayStoreTransaction::GooglePlayStoreTransaction(const QString & orderId, const QString & productId, 
                                                       const QString & purchaseToken, QObject * parent)
    : Transaction(orderId, productId, parent), _purchaseToken(purchaseToken)
{
}
