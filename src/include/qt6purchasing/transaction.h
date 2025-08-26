#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <QObject>
#include <QQmlEngine>
#include <QString>

class Transaction : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Transaction objects are created by the store backend")
    
    Q_PROPERTY(QString orderId READ orderId CONSTANT)
    Q_PROPERTY(QString productId READ productId CONSTANT)
    
public:
    explicit Transaction(const QString & orderId, const QString & productId, QObject * parent = nullptr);
    
    QString orderId() const { return _orderId; }
    QString productId() const { return _productId; }
    
private:
    QString _orderId;
    QString _productId;
};

#endif // TRANSACTION_H
