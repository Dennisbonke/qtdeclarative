// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#include "testtypes.h"

#include <QUrl>
#include <QDir>
#include <QDebug>
#include <qtest.h>
#include <QPointer>
#include <QFileInfo>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQmlComponent>
#include <QQmlIncubator>
#include <private/qjsvalue_p.h>
#include <private/qqmlincubator_p.h>
#include <private/qqmlobjectcreator_p.h>
#include <QtQuickTestUtils/private/qmlutils_p.h>

class tst_qqmlincubator : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlincubator() : QQmlDataTest(QT_QMLTEST_DATADIR) {}

private slots:
    void initTestCase() override;

    void incubationMode();
    void objectDeleted();
    void clear();
    void noIncubationController();
    void forceCompletion();
    void setInitialState();
    void clearDuringCompletion();
    void objectDeletionAfterInit();
    void recursiveClear();
    void statusChanged();
    void asynchronousIfNested();
    void nestedComponent();
    void chainedAsynchronousIfNested();
    void chainedAsynchronousIfNestedOnCompleted();
    void chainedAsynchronousClear();
    void selfDelete();
    void contextDelete();
    void garbageCollection();
    void requiredProperties();
    void deleteInSetInitialState();

private:
    QQmlIncubationController controller;
    QQmlEngine engine;
};

#define VERIFY_ERRORS(component, errorfile) \
    if (!errorfile) { \
        if (qgetenv("DEBUG") != "" && !component.errors().isEmpty()) \
            qWarning() << "Unexpected Errors:" << component.errors(); \
        QVERIFY(!component.isError()); \
        QVERIFY(component.errors().isEmpty()); \
    } else { \
        QFile file(QQmlDataTest::instance()->testFile(errorfile)); \
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text)); \
        QByteArray data = file.readAll(); \
        file.close(); \
        QList<QByteArray> expected = data.split('\n'); \
        expected.removeAll(QByteArray("")); \
        QList<QQmlError> errors = component.errors(); \
        QList<QByteArray> actual; \
        for (int ii = 0; ii < errors.count(); ++ii) { \
            const QQmlError &error = errors.at(ii); \
            QByteArray errorStr = QByteArray::number(error.line()) + ':' +  \
                                  QByteArray::number(error.column()) + ':' + \
                                  error.description().toUtf8(); \
            actual << errorStr; \
        } \
        if (qgetenv("DEBUG") != "" && expected != actual) \
            qWarning() << "Expected:" << expected << "Actual:" << actual;  \
        QCOMPARE(expected, actual); \
    }

void tst_qqmlincubator::initTestCase()
{
    QQmlDataTest::initTestCase();
    registerTypes();
    engine.setIncubationController(&controller);
}

void tst_qqmlincubator::incubationMode()
{
    {
    QQmlIncubator incubator;
    QCOMPARE(incubator.incubationMode(), QQmlIncubator::Asynchronous);
    }
    {
    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    QCOMPARE(incubator.incubationMode(), QQmlIncubator::Asynchronous);
    }
    {
    QQmlIncubator incubator(QQmlIncubator::Synchronous);
    QCOMPARE(incubator.incubationMode(), QQmlIncubator::Synchronous);
    }
    {
    QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
    QCOMPARE(incubator.incubationMode(), QQmlIncubator::AsynchronousIfNested);
    }
}

void tst_qqmlincubator::objectDeleted()
{
    {
        QQmlEngine engine;
        QQmlIncubationController controller;
        engine.setIncubationController(&controller);
        SelfRegisteringType::clearMe();

        QQmlComponent component(&engine, testFileUrl("objectDeleted.qml"));
        QVERIFY(component.isReady());

        QQmlIncubator incubator;
        component.create(incubator);

        QCOMPARE(incubator.status(), QQmlIncubator::Loading);
        QVERIFY(!SelfRegisteringType::me());

        while (SelfRegisteringOuterType::me() == nullptr && incubator.isLoading()) {
            std::atomic<bool> b{false};
            controller.incubateWhile(&b);
        }

        QVERIFY(SelfRegisteringOuterType::me() != nullptr);
        QVERIFY(incubator.isLoading());

        while (SelfRegisteringType::me() == nullptr && incubator.isLoading()) {
            std::atomic<bool> b{false};
            controller.incubateWhile(&b);
        }

        delete SelfRegisteringType::me();

        {
            std::atomic<bool> b{true};
            controller.incubateWhile(&b);
        }

        QVERIFY(incubator.isError());
        VERIFY_ERRORS(incubator, "objectDeleted.errors.txt");
        QVERIFY(!incubator.object());
    }
    QVERIFY(SelfRegisteringOuterType::beenDeleted);
}

void tst_qqmlincubator::clear()
{
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("clear.qml"));
    QVERIFY(component.isReady());

    // Clear in null state
    {
    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    incubator.clear(); // no effect
    QVERIFY(incubator.isNull());
    }

    // Clear in loading state
    {
    QQmlIncubator incubator;
    component.create(incubator);
    QVERIFY(incubator.isLoading());
    incubator.clear();
    QVERIFY(incubator.isNull());
    }

    // Clear mid load
    {
    QQmlIncubator incubator;
    component.create(incubator);

    while (SelfRegisteringType::me() == nullptr && incubator.isLoading()) {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(SelfRegisteringType::me() != nullptr);
    QPointer<SelfRegisteringType> srt = SelfRegisteringType::me();

    incubator.clear();
    QVERIFY(incubator.isNull());
    QVERIFY(srt.isNull());
    }

    // Clear in ready state
    {
    QQmlIncubator incubator;
    component.create(incubator);

    {
        std::atomic<bool> b{true};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != nullptr);
    QPointer<QObject> obj = incubator.object();

    incubator.clear();
    QVERIFY(incubator.isNull());
    QVERIFY(!incubator.object());
    QVERIFY(!obj.isNull());

    delete obj;
    QVERIFY(obj.isNull());
    }
}

void tst_qqmlincubator::noIncubationController()
{
    // All incubators should behave synchronously when there is no controller

    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("noIncubationController.qml"));

    QVERIFY(component.isReady());

    {
    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("testValue").toInt(), 1913);
    delete incubator.object();
    }

    {
    QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("testValue").toInt(), 1913);
    delete incubator.object();
    }

    {
    QQmlIncubator incubator(QQmlIncubator::Synchronous);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("testValue").toInt(), 1913);
    delete incubator.object();
    }
}

void tst_qqmlincubator::forceCompletion()
{
    QQmlComponent component(&engine, testFileUrl("forceCompletion.qml"));
    QVERIFY(component.isReady());

    {
    // forceCompletion on a null incubator does nothing
    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    incubator.forceCompletion();
    QVERIFY(incubator.isNull());
    }

    {
    // forceCompletion immediately after creating an asynchronous object completes it
    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    component.create(incubator);
    QVERIFY(incubator.isLoading());

    incubator.forceCompletion();

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != nullptr);
    QCOMPARE(incubator.object()->property("testValue").toInt(), 3499);

    delete incubator.object();
    }

    {
    // forceCompletion during creation completes it
    SelfRegisteringType::clearMe();

    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    component.create(incubator);
    QVERIFY(incubator.isLoading());

    while (SelfRegisteringType::me() == nullptr && incubator.isLoading()) {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != nullptr);
    QVERIFY(incubator.isLoading());

    incubator.forceCompletion();

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != nullptr);
    QCOMPARE(incubator.object()->property("testValue").toInt(), 3499);

    delete incubator.object();
    }

    {
    // forceCompletion on a ready incubator has no effect
    QQmlIncubator incubator;
    QVERIFY(incubator.isNull());
    component.create(incubator);
    QVERIFY(incubator.isLoading());

    incubator.forceCompletion();

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != nullptr);
    QCOMPARE(incubator.object()->property("testValue").toInt(), 3499);

    incubator.forceCompletion();

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object() != nullptr);
    QCOMPARE(incubator.object()->property("testValue").toInt(), 3499);

    delete incubator.object();
    }
}

void tst_qqmlincubator::setInitialState()
{
    QQmlComponent component(&engine, testFileUrl("setInitialState.qml"));
    QVERIFY(component.isReady());

    struct MyIncubator : public QQmlIncubator
    {
        MyIncubator(QQmlIncubator::IncubationMode mode)
        : QQmlIncubator(mode) {}

        void setInitialState(QObject *o) override {
            QQmlProperty::write(o, "test2", 19);
            QQmlProperty::write(o, "testData1", 201);
        }
    };

    {
    MyIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);
    QVERIFY(incubator.isLoading());
    std::atomic<bool> b{true};
    controller.incubateWhile(&b);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("myValueFunctionCalled").toBool(), false);
    QCOMPARE(incubator.object()->property("test1").toInt(), 502);
    QCOMPARE(incubator.object()->property("test2").toInt(), 19);
    delete incubator.object();
    }

    {
    MyIncubator incubator(QQmlIncubator::Synchronous);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("myValueFunctionCalled").toBool(), false);
    QCOMPARE(incubator.object()->property("test1").toInt(), 502);
    QCOMPARE(incubator.object()->property("test2").toInt(), 19);
    delete incubator.object();
    }
}

void tst_qqmlincubator::clearDuringCompletion()
{
    CompletionRegisteringType::clearMe();
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("clearDuringCompletion.qml"));
    QVERIFY(component.isReady());

    QQmlIncubator incubator;
    component.create(incubator);

    QCOMPARE(incubator.status(), QQmlIncubator::Loading);
    QVERIFY(!CompletionRegisteringType::me());

    while (CompletionRegisteringType::me() == nullptr && incubator.isLoading()) {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(CompletionRegisteringType::me() != nullptr);
    QVERIFY(SelfRegisteringType::me() != nullptr);
    QVERIFY(incubator.isLoading());

    QPointer<QObject> srt = SelfRegisteringType::me();

    incubator.clear();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QVERIFY(incubator.isNull());
    QVERIFY(srt.isNull());
}

void tst_qqmlincubator::objectDeletionAfterInit()
{
    QQmlComponent component(&engine, testFileUrl("clear.qml"));
    QVERIFY(component.isReady());

    struct MyIncubator : public QQmlIncubator
    {
        MyIncubator(QQmlIncubator::IncubationMode mode)
        : QQmlIncubator(mode), obj(nullptr) {}

        void setInitialState(QObject *o) override {
            obj = o;
        }

        QObject *obj;
    };

    SelfRegisteringType::clearMe();
    MyIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);

    while (!incubator.obj && incubator.isLoading()) {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(SelfRegisteringType::me() != nullptr);

    delete incubator.obj;

    incubator.clear();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    QVERIFY(incubator.isNull());
}

class Switcher : public QObject
{
    Q_OBJECT
public:
    Switcher(QQmlEngine *e) : QObject(), engine(e) { }
    ~Switcher()
    {
        if (component)
            component->deleteLater();
        if (incubator && incubator->object())
            incubator->object()->deleteLater();
    }

    struct MyIncubator : public QQmlIncubator
    {
        MyIncubator(QQmlIncubator::IncubationMode mode, QObject *s)
        : QQmlIncubator(mode), switcher(s) {}

        void setInitialState(QObject *o) override {
            if (o->objectName() == "switchMe")
                connect(o, SIGNAL(switchMe()), switcher, SLOT(switchIt()));
        }

        QObject *switcher;
    };

    void start()
    {
        incubator.reset(new MyIncubator(QQmlIncubator::Synchronous, this));
        component = new QQmlComponent(engine, QQmlDataTest::instance()->testFileUrl("recursiveClear.1.qml"));
        component->create(*incubator);
    }

    QQmlEngine *engine;
    QScopedPointer<MyIncubator> incubator;
    QQmlComponent *component;

public slots:
    void switchIt() {
        component->deleteLater();
        incubator->clear();
        component = new QQmlComponent(engine, QQmlDataTest::instance()->testFileUrl("recursiveClear.2.qml"));
        component->create(*incubator);
    }
};

void tst_qqmlincubator::recursiveClear()
{
    Switcher switcher(&engine);
    switcher.start();
}

void tst_qqmlincubator::statusChanged()
{
    class MyIncubator : public QQmlIncubator
    {
    public:
        MyIncubator(QQmlIncubator::IncubationMode mode = QQmlIncubator::Asynchronous)
        : QQmlIncubator(mode) {}

        QList<int> statuses;
    protected:
        void statusChanged(Status s) override { statuses << s; }
        void setInitialState(QObject *) override { statuses << -1; }
    };

    {
    QQmlComponent component(&engine, testFileUrl("statusChanged.qml"));
    QVERIFY(component.isReady());

    MyIncubator incubator(QQmlIncubator::Synchronous);
    component.create(incubator);
    QVERIFY(incubator.isReady());
    QCOMPARE(incubator.statuses.size(), 3);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));
    QCOMPARE(incubator.statuses.at(1), -1);
    QCOMPARE(incubator.statuses.at(2), int(QQmlIncubator::Ready));
    delete incubator.object();
    }

    {
    QQmlComponent component(&engine, testFileUrl("statusChanged.qml"));
    QVERIFY(component.isReady());

    MyIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);
    QVERIFY(incubator.isLoading());
    QCOMPARE(incubator.statuses.size(), 1);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));

    {
    std::atomic<bool> b{true};
    controller.incubateWhile(&b);
    }

    QCOMPARE(incubator.statuses.size(), 3);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));
    QCOMPARE(incubator.statuses.at(1), -1);
    QCOMPARE(incubator.statuses.at(2), int(QQmlIncubator::Ready));
    delete incubator.object();
    }

    {
    QQmlComponent component2(&engine, testFileUrl("statusChanged.nested.qml"));
    QVERIFY(component2.isReady());

    MyIncubator incubator(QQmlIncubator::Asynchronous);
    component2.create(incubator);
    QVERIFY(incubator.isLoading());
    QCOMPARE(incubator.statuses.size(), 1);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));

    {
    std::atomic<bool> b{true};
    controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isReady());
    QCOMPARE(incubator.statuses.size(), 3);
    QCOMPARE(incubator.statuses.at(0), int(QQmlIncubator::Loading));
    QCOMPARE(incubator.statuses.at(1), -1);
    QCOMPARE(incubator.statuses.at(2), int(QQmlIncubator::Ready));
    delete incubator.object();
    }
}

void tst_qqmlincubator::asynchronousIfNested()
{
    // Asynchronous if nested within a finalized context behaves synchronously
    {
    QQmlComponent component(&engine, testFileUrl("asynchronousIfNested.1.qml"));
    QVERIFY(component.isReady());

    QScopedPointer<QObject> object { component.create() };
    QVERIFY(object);
    QCOMPARE(object->property("a").toInt(), 10);

    QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
    component.create(incubator, nullptr, qmlContext(object.get()));

    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("a").toInt(), 10);
    delete incubator.object();
    }

    // Asynchronous if nested within an executing context behaves asynchronously, but prevents
    // the parent from finishing
    {
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("asynchronousIfNested.2.qml"));
    QVERIFY(component.isReady());

    QQmlIncubator incubator;
    component.create(incubator);

    QVERIFY(incubator.isLoading());
    QVERIFY(!SelfRegisteringType::me());
    while (SelfRegisteringType::me() == nullptr && incubator.isLoading()) {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != nullptr);
    QVERIFY(incubator.isLoading());

    QQmlIncubator nested(QQmlIncubator::AsynchronousIfNested);
    component.create(nested, nullptr, qmlContext(SelfRegisteringType::me()));
    QVERIFY(nested.isLoading());

    while (nested.isLoading()) {
        QVERIFY(incubator.isLoading());
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(nested.isReady());
    QVERIFY(incubator.isLoading());

    {
        std::atomic<bool> b{true};
        controller.incubateWhile(&b);
    }

    QVERIFY(nested.isReady());
    QVERIFY(incubator.isReady());

    delete nested.object();
    delete incubator.object();
    }

    // AsynchronousIfNested within a synchronous AsynchronousIfNested behaves synchronously
    {
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("asynchronousIfNested.3.qml"));
    QVERIFY(component.isReady());

    struct CallbackData {
        CallbackData(QQmlEngine *e) : engine(e), pass(false) {}
        QQmlEngine *engine;
        bool pass;
        static void callback(CallbackRegisteringType *o, void *data) {
            CallbackData *d = (CallbackData *)data;

            QQmlComponent c(d->engine, QQmlDataTest::instance()->testFileUrl("asynchronousIfNested.1.qml"));
            if (!c.isReady()) return;

            QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
            c.create(incubator, nullptr, qmlContext(o));

            if (!incubator.isReady()) return;

            if (incubator.object()->property("a").toInt() != 10) return;

            d->pass = true;
            incubator.object()->deleteLater();
        }
    };

    CallbackData cd(&engine);
    CallbackRegisteringType::registerCallback(&CallbackData::callback, &cd);

    QQmlIncubator incubator(QQmlIncubator::AsynchronousIfNested);
    component.create(incubator);

    QVERIFY(incubator.isReady());
    QCOMPARE(cd.pass, true);

    delete incubator.object();
    }
}

void tst_qqmlincubator::nestedComponent()
{
    QQmlComponent component(&engine, testFileUrl("nestedComponent.qml"));
    QVERIFY(component.isReady());

    QObject *object = component.create();

    QQmlComponent *nested = object->property("c").value<QQmlComponent*>();
    QVERIFY(nested);
    QVERIFY(nested->isReady());

    // Test without incubator
    {
    QObject *nestedObject = nested->create();
    QCOMPARE(nestedObject->property("value").toInt(), 19988);
    delete nestedObject;
    }

    // Test with incubator
    {
    QQmlIncubator incubator(QQmlIncubator::Synchronous);
    nested->create(incubator);
    QVERIFY(incubator.isReady());
    QVERIFY(incubator.object());
    QCOMPARE(incubator.object()->property("value").toInt(), 19988);
    delete incubator.object();
    }

    delete object;
}

// Checks that a new AsynchronousIfNested incubator can be correctly started in the
// statusChanged() callback of another.
void tst_qqmlincubator::chainedAsynchronousIfNested()
{
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("chainedAsynchronousIfNested.qml"));
    QVERIFY(component.isReady());

    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);

    QVERIFY(incubator.isLoading());
    QVERIFY(!SelfRegisteringType::me());

    while (SelfRegisteringType::me() == nullptr && incubator.isLoading()) {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != nullptr);
    QVERIFY(incubator.isLoading());

    struct MyIncubator : public QQmlIncubator {
        MyIncubator(MyIncubator *next, QQmlComponent *component, QQmlContext *ctxt)
        : QQmlIncubator(AsynchronousIfNested), next(next), component(component), ctxt(ctxt) {}

    protected:
        void statusChanged(Status s) override {
            if (s == Ready && next)
                component->create(*next, nullptr, ctxt);
        }

    private:
        MyIncubator *next;
        QQmlComponent *component;
        QQmlContext *ctxt;
    };

    MyIncubator incubator2(nullptr, &component, nullptr);
    MyIncubator incubator1(&incubator2, &component, qmlContext(SelfRegisteringType::me()));

    component.create(incubator1, nullptr, qmlContext(SelfRegisteringType::me()));

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isLoading());
    QVERIFY(incubator2.isNull());

    while (incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isLoading());
        QVERIFY(incubator2.isNull());

        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isLoading());

    while (incubator2.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isReady());
        QVERIFY(incubator2.isLoading());

        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    if (incubator.isLoading()) {
        std::atomic<bool> b{true};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isReady());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    incubator.object()->deleteLater();
    incubator1.object()->deleteLater();
    incubator2.object()->deleteLater();
}

// Checks that new AsynchronousIfNested incubators can be correctly chained if started in
// componentCompleted().
void tst_qqmlincubator::chainedAsynchronousIfNestedOnCompleted()
{
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("chainInCompletion.qml"));
    QVERIFY(component.isReady());

    QQmlComponent c1(&engine, testFileUrl("chainedAsynchronousIfNested.qml"));
    QVERIFY(c1.isReady());

    struct MyIncubator : public QQmlIncubator {
        MyIncubator(MyIncubator *next, QQmlComponent *component, QQmlContext *ctxt)
        : QQmlIncubator(AsynchronousIfNested), next(next), component(component), ctxt(ctxt) {}

    protected:
        void statusChanged(Status s) override {
            if (s == Ready && next) {
                component->create(*next, nullptr, ctxt);
            }
        }

    private:
        MyIncubator *next;
        QQmlComponent *component;
        QQmlContext *ctxt;
    };

    struct CallbackData {
        CallbackData(QQmlComponent *c, MyIncubator *i, QQmlContext *ct)
            : component(c), incubator(i), ctxt(ct) {}
        QQmlComponent *component;
        MyIncubator *incubator;
        QQmlContext *ctxt;
        static void callback(CompletionCallbackType *, void *data) {
            CallbackData *d = (CallbackData *)data;
            d->component->create(*d->incubator, nullptr, d->ctxt);
        }
    };

    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);

    QVERIFY(incubator.isLoading());
    QVERIFY(!SelfRegisteringType::me());

    while (SelfRegisteringType::me() == nullptr && incubator.isLoading()) {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != nullptr);
    QVERIFY(incubator.isLoading());

    MyIncubator incubator3(nullptr, &c1, qmlContext(SelfRegisteringType::me()));
    MyIncubator incubator2(&incubator3, &c1, qmlContext(SelfRegisteringType::me()));
    MyIncubator incubator1(&incubator2, &c1, qmlContext(SelfRegisteringType::me()));

    // start incubator1 in componentComplete
    CallbackData cd(&c1, &incubator1, qmlContext(SelfRegisteringType::me()));
    CompletionCallbackType::registerCallback(&CallbackData::callback, &cd);

    while (!incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator2.isNull());
        QVERIFY(incubator3.isNull());

        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isLoading());
    QVERIFY(incubator2.isNull());
    QVERIFY(incubator3.isNull());

    while (incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isLoading());
        QVERIFY(incubator2.isNull());
        QVERIFY(incubator3.isNull());

        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isLoading());
    QVERIFY(incubator3.isNull());

    while (incubator2.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isReady());
        QVERIFY(incubator2.isLoading());
        QVERIFY(incubator3.isNull());

        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    QVERIFY(incubator3.isLoading());

    while (incubator3.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isReady());
        QVERIFY(incubator2.isReady());
        QVERIFY(incubator3.isLoading());

        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    {
    std::atomic<bool> b{true};
    controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isReady());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    QVERIFY(incubator3.isReady());

    incubator.object()->deleteLater();
    incubator1.object()->deleteLater();
    incubator2.object()->deleteLater();
    incubator3.object()->deleteLater();
}

// Checks that new AsynchronousIfNested incubators can be correctly cleared if started in
// componentCompleted().
void tst_qqmlincubator::chainedAsynchronousClear()
{
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("chainInCompletion.qml"));
    QVERIFY(component.isReady());

    QQmlComponent c1(&engine, testFileUrl("chainedAsynchronousIfNested.qml"));
    QVERIFY(c1.isReady());

    struct MyIncubator : public QQmlIncubator {
        MyIncubator(MyIncubator *next, QQmlComponent *component, QQmlContext *ctxt)
        : QQmlIncubator(AsynchronousIfNested), next(next), component(component), ctxt(ctxt) {}

    protected:
        void statusChanged(Status s) override {
            if (s == Ready && next) {
                component->create(*next, nullptr, ctxt);
            }
        }

    private:
        MyIncubator *next;
        QQmlComponent *component;
        QQmlContext *ctxt;
    };

    struct CallbackData {
        CallbackData(QQmlComponent *c, MyIncubator *i, QQmlContext *ct)
            : component(c), incubator(i), ctxt(ct) {}
        QQmlComponent *component;
        MyIncubator *incubator;
        QQmlContext *ctxt;
        static void callback(CompletionCallbackType *, void *data) {
            CallbackData *d = (CallbackData *)data;
            d->component->create(*d->incubator, nullptr, d->ctxt);
        }
    };

    QQmlIncubator incubator(QQmlIncubator::Asynchronous);
    component.create(incubator);

    QVERIFY(incubator.isLoading());
    QVERIFY(!SelfRegisteringType::me());

    while (SelfRegisteringType::me() == nullptr && incubator.isLoading()) {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != nullptr);
    QVERIFY(incubator.isLoading());

    MyIncubator incubator3(nullptr, &c1, qmlContext(SelfRegisteringType::me()));
    MyIncubator incubator2(&incubator3, &c1, qmlContext(SelfRegisteringType::me()));
    MyIncubator incubator1(&incubator2, &c1, qmlContext(SelfRegisteringType::me()));

    // start incubator1 in componentComplete
    CallbackData cd(&c1, &incubator1, qmlContext(SelfRegisteringType::me()));
    CompletionCallbackType::registerCallback(&CallbackData::callback, &cd);

    while (!incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator2.isNull());
        QVERIFY(incubator3.isNull());

        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isLoading());
    QVERIFY(incubator2.isNull());
    QVERIFY(incubator3.isNull());

    while (incubator1.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isLoading());
        QVERIFY(incubator2.isNull());
        QVERIFY(incubator3.isNull());

        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isLoading());
    QVERIFY(incubator3.isNull());

    while (incubator2.isLoading()) {
        QVERIFY(incubator.isLoading());
        QVERIFY(incubator1.isReady());
        QVERIFY(incubator2.isLoading());
        QVERIFY(incubator3.isNull());

        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(incubator.isLoading());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    QVERIFY(incubator3.isLoading());

    // Any in loading state will become null when cleared.
    incubator.clear();

    QVERIFY(incubator.isNull());
    QVERIFY(incubator1.isReady());
    QVERIFY(incubator2.isReady());
    QVERIFY(incubator3.isNull());


    incubator1.object()->deleteLater();
    incubator2.object()->deleteLater();
}

void tst_qqmlincubator::selfDelete()
{
    struct MyIncubator : public QQmlIncubator {
        MyIncubator(bool *done, Status status, IncubationMode mode)
        : QQmlIncubator(mode), done(done), status(status) {}

    protected:
        void statusChanged(Status s) override {
            if (s == status) {
                *done = true;
                if (s == Ready) delete object();
                delete this;
            }
        }

    private:
        bool *done;
        Status status;
    };

    {
    QQmlComponent component(&engine, testFileUrl("selfDelete.qml"));

#define DELETE_TEST(status, mode) { \
    bool done = false; \
    component.create(*(new MyIncubator(&done, status, mode))); \
    std::atomic<bool> True{true}; \
    controller.incubateWhile(&True); \
    QVERIFY(done == true); \
    }

    DELETE_TEST(QQmlIncubator::Loading, QQmlIncubator::Synchronous);
    DELETE_TEST(QQmlIncubator::Ready, QQmlIncubator::Synchronous);
    DELETE_TEST(QQmlIncubator::Loading, QQmlIncubator::Asynchronous);
    DELETE_TEST(QQmlIncubator::Ready, QQmlIncubator::Asynchronous);

#undef DELETE_TEST
    }

    // Delete within error status
    {
    SelfRegisteringType::clearMe();

    QQmlComponent component(&engine, testFileUrl("objectDeleted.qml"));
    QVERIFY(component.isReady());

    bool done = false;
    MyIncubator *incubator = new MyIncubator(&done, QQmlIncubator::Error,
                                             QQmlIncubator::Asynchronous);
    component.create(*incubator);

    QCOMPARE(incubator->QQmlIncubator::status(), QQmlIncubator::Loading);
    QVERIFY(!SelfRegisteringType::me());

    while (SelfRegisteringType::me() == nullptr && incubator->isLoading()) {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }

    QVERIFY(SelfRegisteringType::me() != nullptr);
    QVERIFY(incubator->isLoading());

    // We have to cheat and manually remove it from the creator->allCreatedObjects
    // otherwise we will do a double delete
    QQmlIncubatorPrivate *incubatorPriv = QQmlIncubatorPrivate::get(incubator);
    incubatorPriv->creator->allCreatedObjects().pop();
    delete SelfRegisteringType::me();

    {
    std::atomic<bool> b{true};
    controller.incubateWhile(&b);
    }

    QVERIFY(done);
    }
}

// Test that QML doesn't crash if the context is deleted prior to the incubator
// first executing.
void tst_qqmlincubator::contextDelete()
{
    QQmlContext *context = new QQmlContext(engine.rootContext());
    QQmlComponent component(&engine, testFileUrl("contextDelete.qml"));

    QQmlIncubator incubator;
    component.create(incubator, context);

    delete context;

    {
        std::atomic<bool> b{false};
        controller.incubateWhile(&b);
    }
}

// QTBUG-53111
void tst_qqmlincubator::garbageCollection()
{
    QQmlComponent component(&engine, testFileUrl("garbageCollection.qml"));
    QScopedPointer<QObject> obj(component.create());

    gc(engine);

    std::atomic<bool> b{true};
    controller.incubateWhile(&b);

    // verify incubation completed (the incubator was not prematurely collected)
    QVariant incubatorVariant;
    QMetaObject::invokeMethod(obj.data(), "getAndClearIncubator", Q_RETURN_ARG(QVariant, incubatorVariant));
    QJSValue strongRef = incubatorVariant.value<QJSValue>();
    QVERIFY(!strongRef.isNull() && !strongRef.isUndefined());

    // turn the last strong reference to the incubator into a weak one and collect
    QV4::WeakValue weakIncubatorRef;
    weakIncubatorRef.set(QQmlEnginePrivate::getV4Engine(&engine), QJSValuePrivate::asReturnedValue(&strongRef));
    strongRef = QJSValue();
    incubatorVariant.clear();

    // verify incubator is correctly collected now that incubation is complete and all references are gone
    gc(engine);
    QVERIFY(weakIncubatorRef.isNullOrUndefined());
}

void tst_qqmlincubator::requiredProperties()
{
    {
        QQmlComponent component(&engine, testFileUrl("requiredProperty.qml"));
        QVERIFY(component.isReady());
        // forceCompletion immediately after creating an asynchronous object completes it
        QQmlIncubator incubator;
        incubator.setInitialProperties({{"requiredProperty", 42}});
        QVERIFY(incubator.isNull());
        component.create(incubator);
        QVERIFY(incubator.isLoading());

        incubator.forceCompletion();

        QVERIFY(incubator.isReady());
        QVERIFY(incubator.object() != nullptr);
        QCOMPARE(incubator.object()->property("requiredProperty").toInt(), 42);

        delete incubator.object();
    }
    {
        QQmlComponent component(&engine, testFileUrl("requiredProperty.qml"));
        QVERIFY(component.isReady());
        // forceCompletion immediately after creating an asynchronous object completes it
        QQmlIncubator incubator;
        QVERIFY(incubator.isNull());
        component.create(incubator);
        QVERIFY(incubator.isLoading());

        incubator.forceCompletion();

        QVERIFY(incubator.isError());
        auto error = incubator.errors().first();
        QVERIFY(error.description().contains(QLatin1String("Required property requiredProperty was not initialized")));
        QVERIFY(incubator.object() == nullptr);
    }
}

class DeletingIncubator : public QQmlIncubator
{


    // QQmlIncubator interface
protected:
    void statusChanged(Status) override
    {

    }
    void setInitialState(QObject *obj) override
    {
        delete obj;
        clear();
    }
};

void tst_qqmlincubator::deleteInSetInitialState()
{
    QQmlComponent component(&engine, testFileUrl("requiredProperty.qml"));
    QVERIFY(component.isReady());
    // forceCompletion immediately after creating an asynchronous object completes it
    DeletingIncubator incubator;
    incubator.setInitialProperties({{"requiredProperty", 42}});
    QVERIFY(incubator.isNull());
    component.create(incubator);
    QVERIFY(incubator.isLoading());
    incubator.forceCompletion(); // no crash
    QVERIFY(incubator.isNull());
    QCOMPARE(incubator.object(), nullptr); // object was deleted
}

QTEST_MAIN(tst_qqmlincubator)

#include "tst_qqmlincubator.moc"
