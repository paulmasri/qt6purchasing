#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QQmlEngine>
#include <QString>

struct Transaction
{
    Q_GADGET
    QML_VALUE_TYPE(transaction)
    
    Q_PROPERTY(QString orderId MEMBER orderId CONSTANT)
    Q_PROPERTY(QString productId MEMBER productId CONSTANT)
    
public:
    QString orderId;
    QString productId;
    
    // Platform-specific fields (not exposed to QML)
    QString purchaseToken;  // Android only - for purchase acknowledgment
};

#endif // TRANSACTION_H
