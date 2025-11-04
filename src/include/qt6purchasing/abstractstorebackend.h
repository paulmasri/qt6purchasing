#ifndef ABSTRACTSTOREBACKEND_H
#define ABSTRACTSTOREBACKEND_H

#include <QJsonDocument>
#include <QObject>
#include <QQmlEngine>
#include <QQmlListProperty>

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
        Busy,
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
    Q_PROPERTY(bool processingEnabled READ processingEnabled NOTIFY processingEnabledChanged FINAL)
    Q_PROPERTY(bool isRestoringPurchases READ isRestoringPurchases NOTIFY isRestoringPurchasesChanged FINAL)

public:
    QQmlListProperty<AbstractProduct> productsQml();
    QList<AbstractProduct *> products() { return _products; }
    AbstractProduct * product(const QString &identifier);
    bool isConnected() const { return _connected; }
    virtual bool canMakePurchases() const = 0;
    bool processingEnabled() const { return _processingEnabled; }
    bool isRestoringPurchases() const { return _isRestoringPurchases; }

    virtual void startConnection() = 0;
    virtual void registerProduct(AbstractProduct * product) = 0;
    virtual void purchaseProduct(AbstractProduct * product) = 0;
    virtual void consumePurchase(Transaction transaction) = 0;

    Q_INVOKABLE virtual void restorePurchases();
    Q_INVOKABLE virtual void finalize(Transaction transaction);

    // Transaction processing control (cross-platform defensive programming)
    Q_INVOKABLE virtual void enableProcessing();

protected:
    explicit AbstractStoreBackend(QObject * parent = nullptr);
    QList<AbstractProduct *> _products;
    bool _connected = false;
    bool _canMakePurchases = false;
    bool _processingEnabled = false;
    bool _isRestoringPurchases = false;

    void setConnected(bool connected);
    void setCanMakePurchases(bool canMakePurchases);
    void setIsRestoringPurchases(bool restoring);

private:
    static void appendProduct(QQmlListProperty<AbstractProduct> *list, AbstractProduct *product);
    static qsizetype productCount(QQmlListProperty<AbstractProduct> *list);
    static AbstractProduct *productAt(QQmlListProperty<AbstractProduct> *list, qsizetype index);
    static void clearProducts(QQmlListProperty<AbstractProduct> *list);

signals:
    void productsChanged();
    void connectedChanged();
    void canMakePurchasesChanged();
    void processingEnabledChanged();
    void isRestoringPurchasesChanged();

    void productRegistered(AbstractProduct * product);
    void purchaseSucceeded(Transaction transaction);
    void purchasePending(Transaction transaction);
    void purchaseRestored(Transaction transaction);
    void purchaseFailed(const QString & productId, int error, int platformCode, const QString & message);
    void consumePurchaseSucceeded(Transaction transaction);
    void consumePurchaseFailed(Transaction transaction);
    void restorePurchasesSucceeded(int count);
    void restorePurchasesFailed(int error, int platformCode, const QString & message);
};

#endif // ABSTRACTSTOREBACKEND_H
