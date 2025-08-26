#ifndef GOOGLEPLAYSTORETRANSACTION_H
#define GOOGLEPLAYSTORETRANSACTION_H

#include <qt6purchasing/transaction.h>

class GooglePlayStoreTransaction : public Transaction
{
    Q_OBJECT
    
public:
    explicit GooglePlayStoreTransaction(const QString & orderId, const QString & productId, 
                                       const QString & purchaseToken, QObject * parent = nullptr);
    
    QString purchaseToken() const { return _purchaseToken; }
    
private:
    QString _purchaseToken;
};

#endif // GOOGLEPLAYSTORETRANSACTION_H
