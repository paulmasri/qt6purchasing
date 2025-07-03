#ifndef ABSTRACTTRANSACTION_H
#define ABSTRACTTRANSACTION_H

#include <QJsonObject>
#include <QObject>
#include <QQmlEngine>

class AbstractStoreBackend;

class AbstractTransaction : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AbstractTransaction)
    QML_UNCREATABLE("AbstractTransaction is an abstract base class")

    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString orderId READ orderId CONSTANT)
    Q_PROPERTY(bool retained READ isRetained WRITE setRetained NOTIFY retainedChanged)

public:
    enum Status {
        PurchaseApproved,
        PurchaseFailed,
        PurchaseRestored,
        PurchaseConsumed
    };
    Q_ENUM(Status)

    int status() const { return _status; }
    QString orderId() const { return _orderId; }
    bool isRetained() const { return _retained; }
    void setRetained(bool retained);
    virtual QString productId() const = 0;

    Q_INVOKABLE void finalize();
    Q_INVOKABLE void retain();
    Q_INVOKABLE void destroy();

protected:
    explicit AbstractTransaction(QString orderId, QObject * parent = nullptr);
    AbstractStoreBackend * _store = nullptr;
    int _status;
    QString _orderId;
    bool _retained = false;

signals:
    void statusChanged();
    void retainedChanged();

};

#endif // ABSTRACTTRANSACTION_H
