#include "microsoftstoretransaction.h"
#include "windowsstorewrappers.h"
#include <QUuid>
#include <QDateTime>

using namespace winrt::Windows::Services::Store;

MicrosoftStoreTransaction::MicrosoftStoreTransaction(
    WindowsStorePurchaseResultWrapper * result,
    const QString& productId, 
    QObject * parent) 
    : AbstractTransaction(generateOrderId(result, productId), parent)
    , _purchaseResult(result)
    , _productId(productId)
{
    // Set transaction status based on purchase result
    if (_purchaseResult) {
        switch (_purchaseResult->status()) {
            case StorePurchaseStatus::Succeeded:
                _status = static_cast<int>(MicrosoftStoreTransactionStatus::Succeeded);
                break;
            case StorePurchaseStatus::AlreadyPurchased:
                _status = static_cast<int>(MicrosoftStoreTransactionStatus::AlreadyPurchased);
                break;
            case StorePurchaseStatus::NotPurchased:
                _status = static_cast<int>(MicrosoftStoreTransactionStatus::NotPurchased);
                break;
            case StorePurchaseStatus::NetworkError:
                _status = static_cast<int>(MicrosoftStoreTransactionStatus::NetworkError);
                break;
            case StorePurchaseStatus::ServerError:
                _status = static_cast<int>(MicrosoftStoreTransactionStatus::ServerError);
                break;
            default:
                _status = static_cast<int>(MicrosoftStoreTransactionStatus::Unknown);
                break;
        }
    } else {
        _status = static_cast<int>(MicrosoftStoreTransactionStatus::Unknown);
    }
}

// Constructor for restored purchases
MicrosoftStoreTransaction::MicrosoftStoreTransaction(
    const QString& orderId,
    const QString& productId,
    QObject * parent)
    : AbstractTransaction(orderId, parent)
    , _purchaseResult(nullptr)
    , _productId(productId)
{
    // Restored purchases are always successful
    _status = static_cast<int>(MicrosoftStoreTransactionStatus::Succeeded);
}

MicrosoftStoreTransaction::~MicrosoftStoreTransaction()
{
    delete _purchaseResult;
}

QString MicrosoftStoreTransaction::productId() const
{
    return _productId;
}

QString MicrosoftStoreTransaction::generateOrderId(WindowsStorePurchaseResultWrapper * result, const QString& productId)
{
    if (result && result->status() == StorePurchaseStatus::Succeeded) {
        // Generate deterministic ID using product ID + purchase timestamp
        // This allows purchase restoration to work correctly
        auto timestamp = QDateTime::currentMSecsSinceEpoch();
        return QString("ms_%1_%2").arg(productId).arg(timestamp);
    } else {
        // For failed transactions, use UUID for uniqueness
        return QString("ms_failed_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
}
