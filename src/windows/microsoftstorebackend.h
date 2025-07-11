#ifndef MICROSOFTSTOREBACKEND_H
#define MICROSOFTSTOREBACKEND_H

#include <qt6purchasing/abstractstorebackend.h>
#include <QTimer>
#include <QVariantMap>
#include <QMap>
#include <windows.h>

class MicrosoftStoreBackend : public AbstractStoreBackend
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Store)

public:
    enum StoreErrorCode {
        Success = 0,
        AlreadyPurchased = 1,
        NetworkError = 2,
        UserCanceled = 3,
        ServiceUnavailable = 4,
        UnknownError = 99
    };
    Q_ENUM(StoreErrorCode)

    explicit MicrosoftStoreBackend(QObject * parent = nullptr);
    ~MicrosoftStoreBackend();

    void startConnection() override;
    void registerProduct(AbstractProduct * product) override;
    void purchaseProduct(AbstractProduct * product) override;
    void consumePurchase(AbstractTransaction * transaction) override;
    void restorePurchases();

    static MicrosoftStoreBackend * s_currentInstance;

private slots:
    void onProductQueried(AbstractProduct* product, bool success, const QVariantMap& productData);
    void onPurchaseComplete(AbstractProduct* product, bool success, int status, const QString& result);
    void onRestoreComplete(const QList<QVariantMap> &restoredProducts);
    void onAllProductsQueried(const QList<QVariantMap> &products);

private:
    HWND _hwnd = nullptr;
    QMap<QString, AbstractProduct*> _registeredProducts; // Track products by identifier

    void initializeWindowHandle();
    void queryAllProducts();
    void onConsumableFulfillmentComplete(const QString& orderId, const QString& productId, bool success, const QString& result);
    StoreErrorCode mapStoreError(uint32_t hresult);
    static PurchaseError mapStoreErrorToPurchaseError(StoreErrorCode errorCode);
    static QString getStoreErrorMessage(StoreErrorCode errorCode);
};

#endif // MICROSOFTSTOREBACKEND_H
