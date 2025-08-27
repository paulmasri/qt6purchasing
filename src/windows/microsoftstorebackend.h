#ifndef MICROSOFTSTOREBACKEND_H
#define MICROSOFTSTOREBACKEND_H

#include <qt6purchasing/abstractstorebackend.h>
#include <QTimer>
#include <QVariantMap>
#include <QMap>
#include <windows.h>
#include <winrt/Windows.Services.Store.h>

class MicrosoftStoreBackend : public AbstractStoreBackend
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Store)

public:
    explicit MicrosoftStoreBackend(QObject * parent = nullptr);
    ~MicrosoftStoreBackend();

    void startConnection() override;
    void registerProduct(AbstractProduct * product) override;
    void purchaseProduct(AbstractProduct * product) override;
    void consumePurchase(Transaction transaction) override;
    void restorePurchases() override;
    bool canMakePurchases() const override;

    static MicrosoftStoreBackend * s_currentInstance;

private slots:
    void onProductQueried(AbstractProduct * product, bool success, const QVariantMap & productData);
    void onPurchaseComplete(AbstractProduct * product, winrt::Windows::Services::Store::StorePurchaseStatus status);
    void onRestoreComplete(const QList<QVariantMap> &restoredProducts);
    void onAllProductsQueried(const QList<QVariantMap> &products);

private:
    HWND _hwnd = nullptr;
    QMap<QString, AbstractProduct *> _registeredProducts; // Track products by identifier

    void initializeWindowHandle();
    void queryAllProducts();
    void onConsumableFulfillmentComplete(const QString & orderId, const QString & productId, bool success, const QString & result);
    static PurchaseError mapWindowsErrorToPurchaseError(uint32_t errorCode);
    static QString getWindowsErrorMessage(uint32_t errorCode);
};

#endif // MICROSOFTSTOREBACKEND_H
