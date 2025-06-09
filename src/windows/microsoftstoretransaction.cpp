#include "microsoftstoretransaction.h"
#include <winrt/Windows.Services.Store.h>
#include <QUuid>
#include <QDateTime>

using namespace winrt::Windows::Services::Store;

MicrosoftStoreTransaction::MicrosoftStoreTransaction(
    const StorePurchaseResult& result,
    const QString& productId, 
    QObject * parent) 
    : AbstractTransaction(generateOrderId(result, productId), parent)
    , m_purchaseResult(result)
    , m_productId(productId)
{
    // Set transaction status based on purchase result
    switch (result.Status()) {
        case StorePurchaseStatus::Succeeded:
            _status = static_cast<int>(Succeeded);
            break;
        case StorePurchaseStatus::UserCanceled:
            _status = static_cast<int>(UserCanceled);
            break;
        case StorePurchaseStatus::NetworkError:
            _status = static_cast<int>(NetworkError);
            break;
        case StorePurchaseStatus::ServerError:
            _status = static_cast<int>(ServerError);
            break;
        default:
            _status = static_cast<int>(Unknown);
            break;
    }
}

// Constructor for restored purchases
MicrosoftStoreTransaction::MicrosoftStoreTransaction(
    const QString& orderId,
    const QString& productId,
    QObject * parent)
    : AbstractTransaction(orderId, parent)
    , m_purchaseResult{nullptr}
    , m_productId(productId)
{
    // Restored purchases are always successful
    _status = static_cast<int>(Succeeded);
}

QString MicrosoftStoreTransaction::productId() const
{
    return m_productId;
}

QString MicrosoftStoreTransaction::generateOrderId(const StorePurchaseResult& result, const QString& productId)
{
    if (result.Status() == StorePurchaseStatus::Succeeded) {
        // Generate deterministic ID using product ID + purchase timestamp
        // This allows purchase restoration to work correctly
        auto timestamp = QDateTime::currentMSecsSinceEpoch();
        return QString("ms_%1_%2").arg(productId).arg(timestamp);
    } else {
        // For failed transactions, use UUID for uniqueness
        return QString("ms_failed_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
}
