#ifndef MICROSOFTSTORETRANSACTION_H
#define MICROSOFTSTORETRANSACTION_H

#include <qt6purchasing/abstracttransaction.h>

// Forward declaration
namespace winrt::Windows::Services::Store {
    struct StorePurchaseResult;
}

class MicrosoftStoreTransaction : public AbstractTransaction
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Transaction)
    QML_UNCREATABLE("Transactions are created by the store backend")

public:
    enum MicrosoftStoreTransactionStatus {
        Succeeded = 0,
        UserCanceled = 1,
        NetworkError = 2,
        ServerError = 3,
        Unknown = 99
    };
    Q_ENUM(MicrosoftStoreTransactionStatus)

    MicrosoftStoreTransaction(const winrt::Windows::Services::Store::StorePurchaseResult& result,
                            const QString& productId, 
                            QObject * parent = nullptr);
    
    // Constructor for restored purchases
    MicrosoftStoreTransaction(const QString& orderId,
                            const QString& productId,
                            QObject * parent = nullptr);

    QString productId() const override;

private:
    winrt::Windows::Services::Store::StorePurchaseResult m_purchaseResult{nullptr};
    QString m_productId;
    
    static QString generateOrderId(const winrt::Windows::Services::Store::StorePurchaseResult& result, 
                                  const QString& productId);
};

#endif // MICROSOFTSTORETRANSACTION_H
