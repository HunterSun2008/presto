#include <QDir>

#include "LightningModel.h"
#include "./3rdparty/qjsonrpc/src/qjsonrpcservicereply.h"

PeersModel *LightningModel::peersModel() const
{
    return m_peersModel;
}

PaymentsModel *LightningModel::paymentsModel() const
{
    return m_paymentsModel;
}

WalletModel *LightningModel::walletModel() const
{
    return m_walletModel;
}

InvoicesModel *LightningModel::invoicesModel() const
{
    return m_invoicesModel;
}

QString LightningModel::id() const
{
    return m_id;
}

int LightningModel::port() const
{
    return m_port;
}

QString LightningModel::address() const
{
    return m_address;
}

QString LightningModel::version() const
{
    return m_version;
}

int LightningModel::blockheight() const
{
    return m_blockheight;
}

QString LightningModel::network() const
{
    return m_network;
}

void LightningModel::updateInfo()
{
    QJsonRpcMessage message = QJsonRpcMessage::createRequest("getinfo", QJsonValue());
    QJsonRpcServiceReply* reply = m_rpcSocket->sendMessage(message);
    QObject::connect(reply, &QJsonRpcServiceReply::finished, this, &LightningModel::updateInfoRequestFinished);
}

void LightningModel::launchDaemon()
{
#ifdef Q_OS_ANDROID
    QDir::home().mkdir("bitcoin-data");
    QDir::home().mkdir("libexec");
    qDebug() << QDir::home().entryList();

    QDir libexecDir(QDir::homePath() + "/libexec");
    libexecDir.mkdir("c-lightning");

    QDir cLightningDir(libexecDir.absolutePath() + "/c-lightning");

    QDir programDir = QDir::home();
    programDir.cdUp();

    QStringList lightningBinaries;

    lightningBinaries << "lightning_channeld"
                      << "lightning_closingd"
                      << "lightning_gossipd"
                      << "lightning_hsmd"
                      << "lightning_onchaind"
                      << "lightning_openingd"
                      << "lightningd";

    foreach (QString binaryName, lightningBinaries) {
        QFile::copy(programDir.absolutePath() + "/lib/lib" + binaryName + ".so",
                    cLightningDir.absolutePath() + "/" + binaryName);
    }

    qDebug() << cLightningDir.entryList();

    QString program = cLightningDir.absolutePath() + "/lightningd";
#else
    QDir programDir = QDir::home();
    QString program = programDir.absolutePath() + "/lightningd";
#endif

    qDebug() << "Starting: " << program;
    QStringList arguments;
    arguments << "--network=testnet"
              << "--lightning-dir=" + QDir::homePath() + "/lightning-data"
              << "--pid-file=" + QDir::homePath() + "/lightning.pid"
              << "--bitcoin-cli=" + programDir.absolutePath() + "/lib/libbitcoin-cli.so"
              << "--bitcoin-datadir=" + QDir::homePath() + "/bitcoin-data"
              << "--bitcoin-rpcconnect=192.168.0.12"
              << "--bitcoin-rpcuser=igor"
              << "--bitcoin-rpcpassword=test"
              << "--rpc-file=" + QDir::homePath() + "/lightning-rpc"
              << "--log-level=debug";

    qDebug() << "Arguments: " << arguments;

    m_lightningDaemonProcess->start(program, arguments);
    if (m_lightningDaemonProcess->waitForStarted())
    {
        qDebug()<<"Daemon started";
    }
    else
    {
        qDebug() << "Couldn't start daemon: " << m_lightningDaemonProcess->error();
    }
}

QString LightningModel::serverName() const
{
    return m_serverName;
}

void LightningModel::setServerName(const QString &serverName)
{
    m_serverName = serverName;
}

void LightningModel::updateInfoRequestFinished()
{
    QJsonRpcServiceReply *reply = static_cast<QJsonRpcServiceReply *>(sender());
    QJsonRpcMessage message = reply->response();

    if (message.type() == QJsonRpcMessage::Error)
    {
        //emit errorString(message.toObject().value("error").toObject().value("message").toString());
    }

    if (message.type() == QJsonRpcMessage::Response)
    {
        QJsonObject resultsObject = message.toObject().value("result").toObject();

        m_address = resultsObject.value("address").toString();
        m_blockheight = resultsObject.value("blockheight").toInt();
        m_id = resultsObject.value("id").toString();
        m_network = resultsObject.value("network").toString();
        m_port = resultsObject.value("port").toInt();
        m_version = resultsObject.value("version").toString();

        emit infoChanged();
    }
}

LightningModel::LightningModel(QString serverName, QObject *parent) {
    {
        Q_UNUSED(parent);

        if (serverName.isEmpty()) {
            m_serverName = QDir::homePath() + "/lightning-rpc";
        }
        else {
            m_serverName = serverName;
        }

        m_lightningDaemonProcess = new QProcess(this);

        m_connectedToDaemon = false;

        m_address = QString();
        m_blockheight = 0;
        m_id = QString();
        m_network = QString();
        m_port = 0;
        m_version = QString();

        m_connectionRetryTimer = new QTimer();
        m_unixSocket = new QLocalSocket();
        m_rpcSocket = new QJsonRpcSocket(m_unixSocket);

        m_peersModel = new PeersModel(m_rpcSocket);
        m_paymentsModel = new PaymentsModel(m_rpcSocket);
        m_walletModel = new WalletModel(m_rpcSocket);
        m_invoicesModel = new InvoicesModel(m_rpcSocket);

        QObject::connect(m_unixSocket, SIGNAL(error(QLocalSocket::LocalSocketError)),
                         this, SLOT(unixSocketError(QLocalSocket::LocalSocketError)));

        QObject::connect(m_rpcSocket, &QJsonRpcAbstractSocket::messageReceived, this, &LightningModel::rpcMessageReceived);

        m_unixSocket->connectToServer(m_serverName);

        if (m_unixSocket->waitForConnected())
        {
            rpcConnected();
        }
        else
        {
            // No daemon so lets launch our own
            launchDaemon();
            rpcNotConnected();
        }

    }
}

void LightningModel::rpcConnected()
{
    m_connectionRetryTimer->stop();
    m_connectedToDaemon = true;

    updateModels();
    m_updatesTimer = new QTimer();

    // Update the models every 15 secs
    m_updatesTimer->setInterval(15000);
    m_updatesTimer->setSingleShot(false);
    QObject::connect(m_updatesTimer, &QTimer::timeout, this, &LightningModel::updateModels);
    m_updatesTimer->start();
}

void LightningModel::rpcNotConnected()
{
    m_connectionRetryTimer->stop();

    // Let's retry every 30 secs
    m_connectionRetryTimer->setInterval(30000);
    m_connectionRetryTimer->setSingleShot(true);

    // Reentrant slot right here
    QObject::connect(m_connectionRetryTimer, &QTimer::timeout, this, &LightningModel::rpcNotConnected);
    m_connectionRetryTimer->start();

    m_unixSocket->connectToServer(m_serverName);
    if (m_unixSocket->waitForConnected())
    {
        rpcConnected();
    }
}

bool LightningModel::connectedToDaemon() const
{
    return m_connectedToDaemon;
}

void LightningModel::rpcMessageReceived(QJsonRpcMessage message)
{
    qDebug() << message.toJson();
}

void LightningModel::unixSocketError(QLocalSocket::LocalSocketError socketError)
{
    qDebug() << socketError;
}

void LightningModel::updateModels()
{
    m_paymentsModel->updatePayments();
    m_walletModel->updateFunds();
    m_invoicesModel->updateInvoices();
    updateInfo();
    // This calls needs to go last cause it hates concurrency
    m_peersModel->updatePeers();

    // Temporary daemon debug on android
#ifdef Q_OS_ANDROID
    qDebug() << m_lightningDaemonProcess->readAllStandardOutput();
#endif
}
