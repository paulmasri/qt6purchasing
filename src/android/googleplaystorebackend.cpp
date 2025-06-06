#include "googleplaystorebackend.h"
#include "googleplaystoreproduct.h"
#include "googleplaystoretransaction.h"

#include <QJniEnvironment>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QCoreApplication>

GooglePlayStoreBackend * GooglePlayStoreBackend::s_currentInstance = nullptr;

GooglePlayStoreBackend::GooglePlayStoreBackend(QObject * parent) : AbstractStoreBackend(parent)
{
    _googlePlayBillingJavaClass = new QJniObject(
        "com/climbsmart/crimptronic/GooglePlayBilling",
        "(Landroid/content/Context;)V",
        QNativeInterface::QAndroidApplication::context()
    );

    JNINativeMethod methods[] {
        {"debugMessage", "(Ljava/lang/String;)V", reinterpret_cast<void *>(debugMessage)},
        {"billingResponseReceived", "(I)V", reinterpret_cast<void *>(billingResponseReceived)},
        {"connectedChangedHelper", "(Z)V", reinterpret_cast<void *>(connectedChangedHelper)},
        {"productRegistered", "(Ljava/lang/String;)V", reinterpret_cast<void *>(productRegistered)},
        {"purchaseSucceeded", "(Ljava/lang/String;)V", reinterpret_cast<void *>(purchaseSucceeded)},
        {"purchaseRestored", "(Ljava/lang/String;)V", reinterpret_cast<void *>(purchaseRestored)},
        {"purchaseFailed", "(I)V", reinterpret_cast<void *>(purchaseFailed)},
        {"purchaseConsumed", "(Ljava/lang/String;)V", reinterpret_cast<void *>(purchaseConsumed)},
    };
    QJniEnvironment env;
    jclass objectClass = env->GetObjectClass(_googlePlayBillingJavaClass->object<jobject>());
    env->RegisterNatives(objectClass,
                         methods,
                         sizeof(methods) / sizeof(methods[0]));
    env->DeleteLocalRef(objectClass);

    this->startConnection();

    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    s_currentInstance = this;
}

GooglePlayStoreBackend::~GooglePlayStoreBackend()
{
    if (s_currentInstance == this)
        s_currentInstance = nullptr;

    delete _googlePlayBillingJavaClass;
}

/*static*/ void GooglePlayStoreBackend::debugMessage(JNIEnv * env, jobject object, jstring message)
{
    const char* messageCStr = env->GetStringUTFChars(message, nullptr);
    qDebug() << messageCStr;
    env->ReleaseStringUTFChars(message, messageCStr);
}

/*static*/ void GooglePlayStoreBackend::billingResponseReceived(JNIEnv * env, jobject object, jint value)
{
    qDebug() << "Billing response received:" << static_cast<GooglePlayStoreBackend::BillingResponseCode>(value);
}

/*static*/ void GooglePlayStoreBackend::connectedChangedHelper(JNIEnv * env, jobject object, jboolean connected)
{
    GooglePlayStoreBackend * backend = GooglePlayStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Google Play billing callback received but backend instance is null";
        return;
    }
    backend->setConnected(connected);
}

void GooglePlayStoreBackend::startConnection()
{
    _googlePlayBillingJavaClass->callMethod<void>("startConnection");
}

void GooglePlayStoreBackend::registerProduct(AbstractProduct * product)
{
    _googlePlayBillingJavaClass->callMethod<void>(
        "registerProduct",
        "(Ljava/lang/String;)V",
        QJniObject::fromString( product->identifier() ).object<jstring>()
    );
}

/*static*/ void GooglePlayStoreBackend::productRegistered(JNIEnv * env, jobject object, jstring message)
{
    GooglePlayStoreBackend* backend = GooglePlayStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Google Play product registration callback received but backend instance is null";
        return;
    }
    
    const char* jsonCStr = env->GetStringUTFChars(message, nullptr);
    QJsonObject json = QJsonDocument::fromJson(jsonCStr).object();
    env->ReleaseStringUTFChars(message, jsonCStr);
    
    GooglePlayStoreProduct * product = reinterpret_cast<GooglePlayStoreProduct *>(backend->product( json["productId"].toString() ));

    if (product) {
        product->setJson(json);
        product->setDescription(json["description"].toString());
        product->setPrice(json["price"].toString());
        product->setTitle(json["title"].toString());
        product->setStatus(AbstractProduct::Registered);

        emit backend->productRegistered(product);
    } else {
        qCritical() << "Registered a product that's not in the list of products. This is not handled.";
    }
}

void GooglePlayStoreBackend::purchaseProduct(AbstractProduct * product)
{
    QByteArray jsonSkuDetails = QJsonDocument(reinterpret_cast<GooglePlayStoreProduct*>(product)->json()).toJson();

    _googlePlayBillingJavaClass->callMethod<void>(
        "purchaseProduct",
        "(Landroid/app/Activity;Ljava/lang/String;)V",
        QNativeInterface::QAndroidApplication::context(),
        QJniObject::fromString(jsonSkuDetails).object<jstring>()
    );
}

void GooglePlayStoreBackend::consumePurchase(AbstractTransaction * transaction)
{
    QJsonObject jsonTransaction = reinterpret_cast<GooglePlayStoreTransaction *>(transaction)->json();

    _googlePlayBillingJavaClass->callMethod<void>(
        "consumePurchase",
        "(Ljava/lang/String;)V",
        QJniObject::fromString(QJsonDocument(jsonTransaction).toJson()).object<jstring>()
    );
}

/*static*/ void GooglePlayStoreBackend::purchaseSucceeded(JNIEnv *env, jobject object, jstring message)
{
    GooglePlayStoreBackend* backend = GooglePlayStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Google Play purchase callback received but backend instance is null";
        return;
    }

    const char* jsonCStr = env->GetStringUTFChars(message, nullptr);
    QJsonObject json = QJsonDocument::fromJson(jsonCStr).object();
    env->ReleaseStringUTFChars(message, jsonCStr);

    GooglePlayStoreTransaction * transaction = new GooglePlayStoreTransaction(json, backend);
    emit backend->purchaseSucceeded(transaction);
}

/*static*/ void GooglePlayStoreBackend::purchaseRestored(JNIEnv *env, jobject object, jstring message)
{
    GooglePlayStoreBackend* backend = GooglePlayStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Google Play purchase callback received but backend instance is null";
        return;
    }

    const char* jsonCStr = env->GetStringUTFChars(message, nullptr);
    QJsonObject json = QJsonDocument::fromJson(jsonCStr).object();
    env->ReleaseStringUTFChars(message, jsonCStr);

    GooglePlayStoreTransaction * transaction = new GooglePlayStoreTransaction(json, backend);
    emit backend->purchaseRestored(transaction);
}

/*static*/ void GooglePlayStoreBackend::purchaseFailed(JNIEnv *env, jobject object, jint billingResponseCode)
{
    GooglePlayStoreBackend* backend = GooglePlayStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Google Play purchase callback received but backend instance is null";
        return;
    }

    emit backend->purchaseFailed(billingResponseCode);
}

/*static*/ void GooglePlayStoreBackend::purchaseConsumed(JNIEnv *env, jobject object, jstring message)
{
    GooglePlayStoreBackend* backend = GooglePlayStoreBackend::s_currentInstance;
    if (!backend) {
        qCritical() << "Google Play purchase callback received but backend instance is null";
        return;
    }

    const char* jsonCStr = env->GetStringUTFChars(message, nullptr);
    QJsonObject json = QJsonDocument::fromJson(jsonCStr).object();
    env->ReleaseStringUTFChars(message, jsonCStr);

    GooglePlayStoreTransaction * transaction = new GooglePlayStoreTransaction(json, backend);
    emit backend->purchaseConsumed(transaction);
}
