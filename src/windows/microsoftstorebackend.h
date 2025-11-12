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
    bool canMakePurchases() const override;

    // Transaction processing control
    void enableProcessing() override;

    static MicrosoftStoreBackend * s_currentInstance;

protected:
    void restorePurchasesImpl() override;

private slots:
    void onProductQuerySucceeded(AbstractProduct * product, const QVariantMap & productData);
    void onProductQueryFailed(AbstractProduct * product, uint32_t hresult, const QString & message);
    void onPurchaseComplete(AbstractProduct * product, winrt::Windows::Services::Store::StorePurchaseStatus status);
    void onRestoreSucceeded(const QList<QVariantMap> &restoredProducts);
    void onRestoreFailed(uint32_t errorCode, const QString & message);
    void onAllProductsQueried(const QList<QVariantMap> &products);
    void onAllProductsQueryFailed(uint32_t hresult, const QString & message);

private:
    HWND _hwnd = nullptr;
    QMap<QString, AbstractProduct *> _registeredProducts; // Track products by identifier

    void processQueuedTransactions();
    void processPurchase(AbstractProduct * product, winrt::Windows::Services::Store::StorePurchaseStatus status);
    void processRestoredProducts(const QList<QVariantMap> &restoredProducts);

    // Queued transaction data
    struct QueuedPurchase {
        AbstractProduct* product;
        winrt::Windows::Services::Store::StorePurchaseStatus status;
    };
    QList<QueuedPurchase> _queuedPurchases;
    QList<QVariantMap> _queuedRestores;

    void initializeWindowHandle();
    void queryAllProducts();
    static PurchaseError mapWindowsErrorToPurchaseError(uint32_t errorCode);
    static QString getWindowsErrorMessage(uint32_t errorCode);
    static PurchaseError mapHRESULTToPurchaseError(uint32_t hresult);
};

#endif // MICROSOFTSTOREBACKEND_H
