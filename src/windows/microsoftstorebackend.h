#ifndef MICROSOFTSTOREBACKEND_H
#define MICROSOFTSTOREBACKEND_H

#include <qt6purchasing/abstractstorebackend.h>
#include <QTimer>

// Forward declaration to avoid WinRT headers in public interface
class WindowsStoreManager;

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
    WindowsStoreManager * _storeManager = nullptr;

    void registerProductSync(AbstractProduct* product);
    StoreErrorCode mapStoreError(uint32_t hresult);
};

#endif // MICROSOFTSTOREBACKEND_H
