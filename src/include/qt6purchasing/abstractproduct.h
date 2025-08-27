#ifndef ABSTRACTPRODUCT_H
#define ABSTRACTPRODUCT_H

#include <QObject>
#include <QQmlEngine>

// Forward declaration for AbstractStoreBackend to avoid circular dependency
class AbstractStoreBackend;

// Need full definition for Transaction due to signal parameters
#include <qt6purchasing/transaction.h>

class AbstractProduct : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(AbstractProduct)
    QML_UNCREATABLE("AbstractProduct is an abstract base class")

    // writable properties
    Q_PROPERTY(QString identifier READ identifier WRITE setIdentifier NOTIFY identifierChanged REQUIRED)
    Q_PROPERTY(ProductType type READ productType WRITE setProductType NOTIFY productTypeChanged REQUIRED)
    Q_PROPERTY(QString microsoftStoreId READ microsoftStoreId WRITE setMicrosoftStoreId NOTIFY microsoftStoreIdChanged)
    // read only properties
    Q_PROPERTY(ProductStatus status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString description READ description NOTIFY descriptionChanged)
    Q_PROPERTY(QString price READ price NOTIFY priceChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)

public:
    enum ProductType {
        Consumable,
        Unlockable
    };
    Q_ENUM(ProductType)
    enum ProductStatus {
        Uninitialized,
        PendingRegistration,
        Registered,
        IncorrectProductType,
        Unknown
    };
    Q_ENUM(ProductStatus)

    ProductStatus status() const { return _status; }
    QString identifier() const { return _identifier; }
    QString description() const { return _description; }
    QString price() const { return _price; }
    ProductType productType() const { return _productType; }
    QString title() const { return _title; }
    QString microsoftStoreId() const { return _microsoftStoreId; }

    void setIdentifier(const QString &value);
    void setProductType(ProductType type);
    void setStatus(ProductStatus status);
    void setDescription(QString value);
    void setPrice(const QString &value);
    void setTitle(const QString &value);
    void setMicrosoftStoreId(const QString &value);

    void registerInStore();

    Q_INVOKABLE void purchase();

protected:
    explicit AbstractProduct(QObject * parent = nullptr);

    ProductStatus _status = ProductStatus::Uninitialized;
    QString _identifier;
    QString _description;
    QString _price;
    ProductType _productType;
    QString _title;
    QString _microsoftStoreId;

private:
    AbstractStoreBackend * findStoreBackend() const;

signals:
    void statusChanged();
    void identifierChanged();
    void descriptionChanged();
    void priceChanged();
    void productTypeChanged();
    void titleChanged();
    void microsoftStoreIdChanged();

    void purchaseSucceeded(Transaction transaction);
    void purchasePending(Transaction transaction);
    void purchaseFailed(int error, int platformCode, const QString & message);
    void purchaseRestored(Transaction transaction);
    void consumePurchaseSucceeded(Transaction transaction);
    void consumePurchaseFailed(Transaction transaction);
};

#endif // ABSTRACTPRODUCT_H
