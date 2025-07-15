#ifndef MICROSOFTSTORETRANSACTION_H
#define MICROSOFTSTORETRANSACTION_H

#include <qt6purchasing/abstracttransaction.h>

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

    MicrosoftStoreTransaction(const QString& orderId,
                            const QString& productId,
                            QObject * parent = nullptr);

    QString productId() const override;

private:
    QString _productId;
};

#endif // MICROSOFTSTORETRANSACTION_H
