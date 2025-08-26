#ifndef ABSTRACTSTOREBACKEND_H
#define ABSTRACTSTOREBACKEND_H

#include <QJsonDocument>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>
#include <QSharedPointer>

// Forward declaration for AbstractProduct to avoid circular dependency
class AbstractProduct;

// Need full definition for Transaction for member access and QML integration
#include <qt6purchasing/transaction.h>

class AbstractStoreBackend : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AbstractStoreBackend)
    QML_UNCREATABLE("AbstractStoreBackend is an abstract base class")

public:
    enum class PurchaseError {
        NoError,
        UserCanceled,
        NetworkError,
        ServiceUnavailable,
        ItemUnavailable,
        ItemNotOwned,
        AlreadyPurchased,
        DeveloperError,
        PaymentInvalid,
        NotAllowed,
        UnknownError
    };
    Q_ENUM(PurchaseError)

    Q_PROPERTY(QQmlListProperty<AbstractProduct> productsQml READ productsQml NOTIFY productsChanged)
    Q_CLASSINFO("DefaultProperty", "productsQml")
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged FINAL)
    Q_PROPERTY(bool canMakePurchases READ canMakePurchases NOTIFY canMakePurchasesChanged FINAL)

public:
    QQmlListProperty<AbstractProduct> productsQml();
    QList<AbstractProduct *> products() { return _products; }
    AbstractProduct * product(const QString &identifier);
    bool isConnected() const { return _connected; }
    virtual bool canMakePurchases() const = 0;

    virtual void startConnection() = 0;
    virtual void registerProduct(AbstractProduct * product) = 0;
    virtual void purchaseProduct(AbstractProduct * product) = 0;
    virtual void consumePurchase(QSharedPointer<Transaction> transaction) = 0;

    Q_INVOKABLE virtual void restorePurchases() = 0;
    Q_INVOKABLE virtual void finalize(QSharedPointer<Transaction> transaction);

protected:
    explicit AbstractStoreBackend(QObject * parent = nullptr);
    QList<AbstractProduct *> _products;
    bool _connected = false;
    bool _canMakePurchases = false;

    void setConnected(bool connected);
    void setCanMakePurchases(bool canMakePurchases);

private:
    static void appendProduct(QQmlListProperty<AbstractProduct> *list, AbstractProduct *product);
    static qsizetype productCount(QQmlListProperty<AbstractProduct> *list);
    static AbstractProduct *productAt(QQmlListProperty<AbstractProduct> *list, qsizetype index);
    static void clearProducts(QQmlListProperty<AbstractProduct> *list);

signals:
    void productsChanged();
    void connectedChanged();
    void canMakePurchasesChanged();

    void productRegistered(AbstractProduct * product);
    void purchaseSucceeded(QSharedPointer<Transaction> transaction);
    void purchasePending(QSharedPointer<Transaction> transaction);
    void purchaseRestored(QSharedPointer<Transaction> transaction);
    void purchaseFailed(const QString & productId, int error, int platformCode, const QString & message);
    void consumePurchaseSucceeded(QSharedPointer<Transaction> transaction);
    void consumePurchaseFailed(QSharedPointer<Transaction> transaction);
};

#endif // ABSTRACTSTOREBACKEND_H
