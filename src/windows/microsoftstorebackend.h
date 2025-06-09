#ifndef MICROSOFTSTOREBACKEND_H
#define MICROSOFTSTOREBACKEND_H

#include <qt6purchasing/abstractstorebackend.h>
#include <QTimer>

// Forward declarations to avoid WinRT headers in public interface
namespace winrt::Windows::Services::Store {
    struct StoreContext;
    struct StoreProductQueryResult;
    struct StorePurchaseResult;
}

class MicrosoftStoreBackend : public AbstractStoreBackend
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Store)

public:
    enum StoreErrorCode {
        Success = 0,
        NetworkError = 1,
        UserCanceled = 2,
        ServiceUnavailable = 3,
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

private:
    winrt::Windows::Services::Store::StoreContext m_storeContext{nullptr};

    void registerProductSync(AbstractProduct* product);
    void handleProductQueryResult(AbstractProduct* product, 
                                const winrt::Windows::Services::Store::StoreProductQueryResult& result);
    void handlePurchaseResult(const winrt::Windows::Services::Store::StorePurchaseResult& result, 
                            const QString& productId);
    StoreErrorCode mapStoreError(int32_t hresult);
};

#endif // MICROSOFTSTOREBACKEND_H
