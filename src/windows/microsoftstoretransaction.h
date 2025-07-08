#ifndef MICROSOFTSTORETRANSACTION_H
#define MICROSOFTSTORETRANSACTION_H

#include <qt6purchasing/abstracttransaction.h>

// Forward declaration
class WindowsStorePurchaseResultWrapper;

class MicrosoftStoreTransaction : public AbstractTransaction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Transaction)
    QML_UNCREATABLE("Transactions are created by the store backend")

public:
    enum MicrosoftStoreTransactionStatus {
        Succeeded = 0,
        AlreadyPurchased = 1,
        NotPurchased = 2,
        NetworkError = 3,
        ServerError = 4,
        Unknown = 99
    };
    Q_ENUM(MicrosoftStoreTransactionStatus)

    MicrosoftStoreTransaction(WindowsStorePurchaseResultWrapper * result,
                            const QString& productId, 
                            QObject * parent = nullptr);
    
    // Constructor for restored purchases
    MicrosoftStoreTransaction(const QString& orderId,
                            const QString& productId,
                            QObject * parent = nullptr);
    
    ~MicrosoftStoreTransaction();

    QString productId() const override;

private:
    WindowsStorePurchaseResultWrapper * _purchaseResult = nullptr;
    QString _productId;
    
    static QString generateOrderId(WindowsStorePurchaseResultWrapper * result, 
                                  const QString& productId);
};

#endif // MICROSOFTSTORETRANSACTION_H
