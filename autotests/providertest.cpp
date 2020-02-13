/*
    This file is part of KDE.

    SPDX-FileCopyrightText: 2018 Ralf Habacker <ralf.habacker@freenet.de>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QTest>
#include <QTimer>
#include <QEventLoop>

#include <QLoggingCategory>

#include <config.h>
#include <providermanager.h>


using namespace Attica;

class ProviderTest : public QObject
{
    Q_OBJECT
public:
    ProviderTest();
    virtual ~ProviderTest();

private:
    void initProvider(const QUrl &url);

private Q_SLOTS:
    void testFetchValidProvider();
    void testFetchInvalidProvider();

protected Q_SLOTS:
    void providerAdded(Attica::Provider p);
    void slotDefaultProvidersLoaded();
    void slotConfigResult(Attica::BaseJob *j);
    void slotTimeout();

private:
    Attica::ProviderManager *m_manager;
    QEventLoop *m_eventloop;
    QTimer m_timer;
    bool m_errorReceived = false;
};

ProviderTest::ProviderTest()
  : m_manager(nullptr),
    m_eventloop(new QEventLoop)
{
    QLoggingCategory::setFilterRules(QStringLiteral("org.kde.attica.debug=true"));
}

ProviderTest::~ProviderTest()
{
    delete m_manager;
    delete m_eventloop;
}

void ProviderTest::slotDefaultProvidersLoaded()
{
    qDebug() << "default providers loaded";
    m_eventloop->exit();
}

void ProviderTest::providerAdded(Attica::Provider p)
{
    qDebug() << "got provider" << p.name();
    m_eventloop->exit();
}

void ProviderTest::initProvider(const QUrl &url)
{
    m_errorReceived = false;
    delete m_manager;
    m_manager = new Attica::ProviderManager;
    m_manager->setAuthenticationSuppressed(true);
    connect(m_manager, &ProviderManager::defaultProvidersLoaded, this, &ProviderTest::slotDefaultProvidersLoaded);
    connect(m_manager, &ProviderManager::providerAdded, this, &ProviderTest::providerAdded);
    m_manager->addProviderFile(url);
    connect(m_manager, &Attica::ProviderManager::failedToLoad, this, [this]() {
        m_errorReceived = true;
        m_eventloop->quit();
    });
    m_timer.singleShot(5000, this, &ProviderTest::slotTimeout);

    m_eventloop->exec();
}

void ProviderTest::testFetchValidProvider()
{
    initProvider(QUrl(QLatin1String("https://autoconfig.kde.org/ocs/providers.xml")));
    Attica::Provider provider = m_manager->providers().at(0);
    ItemJob<Config>* job = provider.requestConfig();
    QVERIFY(job);
    connect(job, &BaseJob::finished, this, &ProviderTest::slotConfigResult);
    job->start();
    m_eventloop->exec();
}

void ProviderTest::slotConfigResult(Attica::BaseJob* j)
{
    if (j->metadata().error() == Metadata::NoError) {
        Attica::ItemJob<Config> *itemJob = static_cast<Attica::ItemJob<Config> *>( j );
        Attica::Config p = itemJob->result();
        qDebug() << QLatin1String("Config loaded - Server has version") << p.version();
    } else if (j->metadata().error() == Metadata::OcsError) {
        qDebug() << QString(QLatin1String("OCS Error: %1")).arg(j->metadata().message());
    } else if (j->metadata().error() == Metadata::NetworkError) {
        qDebug() << QString(QLatin1String("Network Error: %1")).arg(j->metadata().message());
    } else {
        qDebug() << QString(QLatin1String("Unknown Error: %1")).arg(j->metadata().message());
    }
    m_eventloop->exit();
    m_timer.stop();
    QVERIFY(j->metadata().error() == Metadata::NoError);
}

void ProviderTest::slotTimeout()
{
    QVERIFY(m_eventloop->isRunning());
    m_eventloop->exit();
    QFAIL("Timeout fetching provider");
}

void ProviderTest::testFetchInvalidProvider()
{
    initProvider(QUrl(QLatin1String("https://invalid-url.org/ocs/providers.xml")));
    QVERIFY(m_manager->providers().size() == 0);
    QVERIFY(m_errorReceived);
}

QTEST_GUILESS_MAIN(ProviderTest)

#include "providertest.moc"
