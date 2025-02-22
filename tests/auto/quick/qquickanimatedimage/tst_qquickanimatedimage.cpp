// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#include <qtest.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/qquickview.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <private/qquickimage_p.h>
#include <private/qquickanimatedimage_p.h>
#include <QSignalSpy>
#include <QtQml/qqmlcontext.h>

#include <QtQuickTestUtils/private/testhttpserver_p.h>
#include <QtQuickTestUtils/private/qmlutils_p.h>

Q_DECLARE_METATYPE(QQuickImageBase::Status)

template <typename T> static T evaluate(QObject *scope, const QString &expression)
{
    QQmlExpression expr(qmlContext(scope), scope, expression);
    QVariant result = expr.evaluate();
    if (expr.hasError())
        qWarning() << expr.error().toString();
    return result.value<T>();
}

template <> void evaluate<void>(QObject *scope, const QString &expression)
{
    QQmlExpression expr(qmlContext(scope), scope, expression);
    expr.evaluate();
    if (expr.hasError())
        qWarning() << expr.error().toString();
}

class tst_qquickanimatedimage : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickanimatedimage() : QQmlDataTest(QT_QMLTEST_DATADIR) {}

private slots:
    void cleanup();
    void play();
    void pause();
    void stopped();
    void setFrame();
    void frameCount();
    void mirror_running();
    void mirror_notRunning();
    void mirror_notRunning_data();
    void remote();
    void remote_data();
    void sourceSize();
    void setSourceSize();
    void sourceSizeChanges();
    void sourceSizeChanges_intermediate();
    void invalidSource();
    void qtbug_16520();
    void progressAndStatusChanges();
    void playingAndPausedChanges();
    void noCaching();
    void sourceChangesOnFrameChanged();
    void currentFrame();
    void qtbug_120555();
};

void tst_qquickanimatedimage::cleanup()
{
    QQuickWindow window;
    window.releaseResources();
}

void tst_qquickanimatedimage::play()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickman.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());

    delete anim;
}

void tst_qquickanimatedimage::pause()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmanpause.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);

    QTRY_VERIFY(anim->isPaused());
    QTRY_VERIFY(anim->isPlaying());

    delete anim;
}

void tst_qquickanimatedimage::stopped()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmanstopped.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QTRY_VERIFY(!anim->isPlaying());
    QCOMPARE(anim->currentFrame(), 0);

    delete anim;
}

void tst_qquickanimatedimage::setFrame()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmanpause.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());
    QCOMPARE(anim->currentFrame(), 2);

    delete anim;
}

void tst_qquickanimatedimage::frameCount()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("colors.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QVERIFY(anim->isPlaying());
    QCOMPARE(anim->frameCount(), 3);

    QSignalSpy frameCountChangedSpy(anim, &QQuickAnimatedImage::frameCountChanged);

    const QUrl origSource = anim->source();
    anim->setSource(QUrl());
    QCOMPARE(anim->frameCount(), 0);
    QCOMPARE(frameCountChangedSpy.size(), 1);
    anim->setSource(origSource);
    QCOMPARE(anim->frameCount(), 3);
    QCOMPARE(frameCountChangedSpy.size(), 2);

    delete anim;
}

void tst_qquickanimatedimage::mirror_running()
{
    // test where mirror is set to true after animation has started

    QQuickView window;
    window.setSource(testFileUrl("hearts.qml"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(window.rootObject());
    QVERIFY(anim);

    int width = anim->property("width").toInt();

    QCOMPARE(anim->frameCount(), 2);

    QCOMPARE(anim->currentFrame(), 0);
    QImage frame0 = window.grabWindow();

    anim->setCurrentFrame(1);
    QCOMPARE(anim->currentFrame(), 1);
    QImage frame1 = window.grabWindow();

    anim->setCurrentFrame(0);

    QSignalSpy spy(anim, SIGNAL(frameChanged()));
    QVERIFY(spy.isValid());
    anim->setPlaying(true);

    QTRY_VERIFY(spy.size() == 1); spy.clear();
    anim->setMirror(true);

    QCOMPARE(anim->currentFrame(), 1);
    QImage frame1_flipped = window.grabWindow();

    QTRY_VERIFY(spy.size() == 1); spy.clear();
    QCOMPARE(anim->currentFrame(), 0);  // animation only has 2 frames, should cycle back to first
    QImage frame0_flipped = window.grabWindow();

    QTransform transform;
    transform.translate(width, 0).scale(-1, 1.0);
    QImage frame0_expected = frame0.transformed(transform);
    QImage frame1_expected = frame1.transformed(transform);

    if (window.devicePixelRatio() != 1.0 && window.rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
        QSKIP("QTBUG-53823");
    QCOMPARE(frame0_flipped, frame0_expected);
    if (window.devicePixelRatio() != 1.0 && window.rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
        QSKIP("QTBUG-53823");
    QCOMPARE(frame1_flipped, frame1_expected);

    delete anim;
}

void tst_qquickanimatedimage::mirror_notRunning()
{
    QFETCH(QUrl, fileUrl);

    QQuickView window;
    window.setSource(fileUrl);
    window.show();
    QTRY_VERIFY(window.isExposed());

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(window.rootObject());
    QVERIFY(anim);

    int width = anim->property("width").toInt();
    QImage screenshot = window.grabWindow();

    QTransform transform;
    transform.translate(width, 0).scale(-1, 1.0);
    QImage expected = screenshot.transformed(transform);

    int frame = anim->currentFrame();
    bool playing = anim->isPlaying();
    bool paused = anim->isPaused();

    anim->setProperty("mirror", true);
    screenshot = window.grabWindow();

    screenshot.save("screen.png");
    if (window.devicePixelRatio() != 1.0 && window.rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
        QSKIP("QTBUG-53823");
    QCOMPARE(screenshot, expected);

    // mirroring should not change the current frame or playing status
    QCOMPARE(anim->currentFrame(), frame);
    QCOMPARE(anim->isPlaying(), playing);
    QCOMPARE(anim->isPaused(), paused);

    delete anim;
}

void tst_qquickanimatedimage::mirror_notRunning_data()
{
    QTest::addColumn<QUrl>("fileUrl");

    QTest::newRow("paused") << testFileUrl("stickmanpause.qml");
    QTest::newRow("stopped") << testFileUrl("stickmanstopped.qml");
}

void tst_qquickanimatedimage::remote_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<bool>("paused");

    QTest::newRow("playing") << "stickman.qml" << false;
    QTest::newRow("paused") << "stickmanpause.qml" << true;
}

void tst_qquickanimatedimage::remote()
{
    QFETCH(QString, fileName);
    QFETCH(bool, paused);

    ThreadedTestHTTPServer server(dataDirectory());

    QQmlEngine engine;
    QQmlComponent component(&engine, server.url(fileName));
    QTRY_VERIFY(component.isReady());

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);

    QTRY_VERIFY(anim->isPlaying());
    if (paused) {
        QTRY_VERIFY(anim->isPaused());
        QCOMPARE(anim->currentFrame(), 2);
    }
    QVERIFY(anim->status() != QQuickAnimatedImage::Error);

    delete anim;
}

void tst_qquickanimatedimage::sourceSize()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmanscaled.qml"));
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);
    QCOMPARE(anim->width(),240.0);
    QCOMPARE(anim->height(),180.0);
    QCOMPARE(anim->sourceSize(),QSize(160,120));

    delete anim;
}

void tst_qquickanimatedimage::setSourceSize()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("stickmansourcesized.qml"));
    QScopedPointer<QQuickAnimatedImage> anim(qobject_cast<QQuickAnimatedImage *>(component.create()));
    QVERIFY(anim);
    QCOMPARE(anim->sourceSize(), QSize(80, 60));

    anim->setSourceSize(QSize(40, 30));
    QCOMPARE(anim->sourceSize(), QSize(40, 30));

    anim->setSourceSize(QSize());
    QCOMPARE(anim->sourceSize(), QSize(160, 120));
}

void tst_qquickanimatedimage::invalidSource()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n AnimatedImage { source: \"no-such-file.gif\" }", QUrl::fromLocalFile("relative"));
    QVERIFY(component.isReady());

    QTest::ignoreMessage(QtWarningMsg, "file:relative:2:2: QML AnimatedImage: Error Reading Animated Image File file:no-such-file.gif");

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());
    QVERIFY(anim);

    QVERIFY(anim->isPlaying());
    QVERIFY(!anim->isPaused());
    QCOMPARE(anim->currentFrame(), 0);
    QCOMPARE(anim->frameCount(), 0);
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Error);

    delete anim;
}

void tst_qquickanimatedimage::sourceSizeChanges()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nAnimatedImage { source: srcImage }", QUrl::fromLocalFile(""));
    QTRY_VERIFY(component.isReady());
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", "");
    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage*>(component.create());
    QVERIFY(anim != nullptr);

    QSignalSpy sourceSizeSpy(anim, SIGNAL(sourceSizeChanged()));

    // Local
    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Null);
    QTRY_COMPARE(sourceSizeSpy.size(), 0);

    ctxt->setContextProperty("srcImage", testFileUrl("hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.size(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.size(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("hearts_copy.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.size(), 1);

    ctxt->setContextProperty("srcImage", testFileUrl("colors.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.size(), 2);

    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Null);
    QTRY_COMPARE(sourceSizeSpy.size(), 3);

    // Remote
    ctxt->setContextProperty("srcImage", server.url("/hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.size(), 4);

    ctxt->setContextProperty("srcImage", server.url("/hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.size(), 4);

    ctxt->setContextProperty("srcImage", server.url("/hearts_copy.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.size(), 4);

    ctxt->setContextProperty("srcImage", server.url("/colors.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(sourceSizeSpy.size(), 5);

    ctxt->setContextProperty("srcImage", QUrl(""));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Null);
    QTRY_COMPARE(sourceSizeSpy.size(), 6);

    delete anim;
}

void tst_qquickanimatedimage::sourceSizeChanges_intermediate()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nAnimatedImage { readonly property int testWidth: status === AnimatedImage.Ready ? sourceSize.width : -1; source: srcImage }", QUrl::fromLocalFile(""));
    QTRY_VERIFY(component.isReady());
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", "");

    QScopedPointer<QQuickAnimatedImage> anim(qobject_cast<QQuickAnimatedImage*>(component.create()));
    QVERIFY(anim != nullptr);

    ctxt->setContextProperty("srcImage", testFileUrl("hearts.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(anim->property("testWidth").toInt(), anim->sourceSize().width());

    ctxt->setContextProperty("srcImage", testFileUrl("hearts_copy.gif"));
    QTRY_COMPARE(anim->status(), QQuickAnimatedImage::Ready);
    QTRY_COMPARE(anim->property("testWidth").toInt(), anim->sourceSize().width());
}


void tst_qquickanimatedimage::qtbug_16520()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("qtbug-16520.qml"));
    QTRY_VERIFY(component.isReady());

    QQuickRectangle *root = qobject_cast<QQuickRectangle *>(component.create());
    QVERIFY(root);
    QQuickAnimatedImage *anim = root->findChild<QQuickAnimatedImage*>("anim");
    QVERIFY(anim != nullptr);

    anim->setProperty("source", server.urlString("/stickman.gif"));
    QTRY_COMPARE(anim->opacity(), qreal(0));
    QTRY_COMPARE(anim->opacity(), qreal(1));

    delete anim;
    delete root;
}

void tst_qquickanimatedimage::progressAndStatusChanges()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QString componentStr = "import QtQuick 2.0\nAnimatedImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", testFileUrl("stickman.gif"));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickImage *obj = qobject_cast<QQuickImage*>(component.create());
    QVERIFY(obj != nullptr);
    QCOMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);

    qRegisterMetaType<QQuickImageBase::Status>();
    QSignalSpy sourceSpy(obj, SIGNAL(sourceChanged(QUrl)));
    QSignalSpy progressSpy(obj, SIGNAL(progressChanged(qreal)));
    QSignalSpy statusSpy(obj, SIGNAL(statusChanged(QQuickImageBase::Status)));

    // Same image
    ctxt->setContextProperty("srcImage", testFileUrl("stickman.gif"));
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.size(), 0);
    QTRY_COMPARE(progressSpy.size(), 0);
    QTRY_COMPARE(statusSpy.size(), 0);

    // Loading local file
    ctxt->setContextProperty("srcImage", testFileUrl("colors.gif"));
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.size(), 1);
    QTRY_COMPARE(progressSpy.size(), 0);
    QTRY_COMPARE(statusSpy.size(), 1);

    // Loading remote file
    ctxt->setContextProperty("srcImage", server.url("/stickman.gif"));
    QTRY_COMPARE(obj->status(), QQuickImage::Loading);
    QTRY_COMPARE(obj->progress(), 0.0);
    QTRY_COMPARE(obj->status(), QQuickImage::Ready);
    QTRY_COMPARE(obj->progress(), 1.0);
    QTRY_COMPARE(sourceSpy.size(), 2);
    QTRY_VERIFY(progressSpy.size() > 1);
    QTRY_COMPARE(statusSpy.size(), 3);

    ctxt->setContextProperty("srcImage", "");
    QTRY_COMPARE(obj->status(), QQuickImage::Null);
    QTRY_COMPARE(obj->progress(), 0.0);
    QTRY_COMPARE(sourceSpy.size(), 3);
    QTRY_VERIFY(progressSpy.size() > 2);
    QTRY_COMPARE(statusSpy.size(), 4);

    delete obj;
}

void tst_qquickanimatedimage::playingAndPausedChanges()
{
    QQmlEngine engine;
    QString componentStr = "import QtQuick 2.0\nAnimatedImage { source: srcImage }";
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("srcImage", QUrl(""));
    QQmlComponent component(&engine);
    component.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
    QQuickAnimatedImage *obj = qobject_cast<QQuickAnimatedImage*>(component.create());
    QVERIFY(obj != nullptr);
    QCOMPARE(obj->status(), QQuickAnimatedImage::Null);
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QSignalSpy playingSpy(obj, SIGNAL(playingChanged()));
    QSignalSpy pausedSpy(obj, SIGNAL(pausedChanged()));

    // initial state
    obj->setProperty("playing", true);
    obj->setProperty("paused", false);
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.size(), 0);
    QTRY_COMPARE(pausedSpy.size(), 0);

    obj->setProperty("playing", false);
    obj->setProperty("paused", true);
    QTRY_VERIFY(!obj->isPlaying());
    QTRY_VERIFY(obj->isPaused());
    QTRY_COMPARE(playingSpy.size(), 1);
    QTRY_COMPARE(pausedSpy.size(), 1);

    obj->setProperty("playing", true);
    obj->setProperty("paused", false);
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.size(), 2);
    QTRY_COMPARE(pausedSpy.size(), 2);

    ctxt->setContextProperty("srcImage", testFileUrl("stickman.gif"));
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.size(), 2);
    QTRY_COMPARE(pausedSpy.size(), 2);

    obj->setProperty("paused", true);
    QTRY_VERIFY(obj->isPlaying());
    QTRY_VERIFY(obj->isPaused());
    QTRY_COMPARE(playingSpy.size(), 2);
    QTRY_COMPARE(pausedSpy.size(), 3);

    obj->setProperty("playing", false);
    QTRY_VERIFY(!obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.size(), 3);
    QTRY_COMPARE(pausedSpy.size(), 4);

    obj->setProperty("playing", true);

    // Cannot animate this image, playing will be false
    ctxt->setContextProperty("srcImage", testFileUrl("green.png"));
    QTRY_VERIFY(!obj->isPlaying());
    QTRY_VERIFY(!obj->isPaused());
    QTRY_COMPARE(playingSpy.size(), 5);
    QTRY_COMPARE(pausedSpy.size(), 4);

    delete obj;
}

void tst_qquickanimatedimage::noCaching()
{
    QQuickView window, window_nocache;
    window.setSource(testFileUrl("colors.qml"));
    window_nocache.setSource(testFileUrl("colors_nocache.qml"));
    window.show();
    window_nocache.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(QTest::qWaitForWindowExposed(&window_nocache));

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(window.rootObject());
    QVERIFY(anim);

    QQuickAnimatedImage *anim_nocache = qobject_cast<QQuickAnimatedImage *>(window_nocache.rootObject());
    QVERIFY(anim_nocache);

    QCOMPARE(anim->frameCount(), anim_nocache->frameCount());

    // colors.gif only has 3 frames so this should be fast
    for (int loops = 0; loops <= 2; ++loops) {
        for (int frame = 0; frame < anim->frameCount(); ++frame) {
            anim->setCurrentFrame(frame);
            anim_nocache->setCurrentFrame(frame);

            QImage image_cache = window.grabWindow();
            QImage image_nocache = window_nocache.grabWindow();

            QCOMPARE(image_cache, image_nocache);
        }
    }
}

void tst_qquickanimatedimage::sourceChangesOnFrameChanged()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("colors.qml"));
    QVector<QQuickAnimatedImage*> images;

    // Run multiple animations in parallel, this should be fast
    for (int loops = 0; loops < 25; ++loops) {
        QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(component.create());

        // QTBUG-67427: this should not produce a segfault
        QObject::connect(anim,
                         &QQuickAnimatedImage::frameChanged,
                         [this, anim]() { anim->setSource(testFileUrl("hearts.gif")); });

        QVERIFY(anim);
        QVERIFY(anim->isPlaying());

        images.append(anim);
    }

    for (auto *anim : images)
        QTRY_COMPARE(anim->source(), testFileUrl("hearts.gif"));

    qDeleteAll(images);
}

void tst_qquickanimatedimage::currentFrame()
{
    QQuickView window;
    window.setSource(testFileUrl("currentframe.qml"));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage *>(window.rootObject());
    QVERIFY(anim);
    QSignalSpy frameChangedSpy(anim, SIGNAL(frameChanged()));
    QSignalSpy currentFrameChangedSpy(anim, SIGNAL(currentFrameChanged()));

    anim->setCurrentFrame(1);
    QCOMPARE(anim->currentFrame(), 1);
    QCOMPARE(frameChangedSpy.size(), 1);
    QCOMPARE(currentFrameChangedSpy.size(), 1);
    QCOMPARE(anim->property("currentFrameChangeCount"), 1);
    QCOMPARE(anim->property("frameChangeCount"), 1);

    evaluate<void>(anim, "scriptedSetCurrentFrame(2)");
    QCOMPARE(anim->currentFrame(), 2);
    QCOMPARE(frameChangedSpy.size(), 2);
    QCOMPARE(currentFrameChangedSpy.size(), 2);
    QCOMPARE(anim->property("currentFrameChangeCount"), 2);
    QCOMPARE(anim->property("frameChangeCount"), 2);
}

void tst_qquickanimatedimage::qtbug_120555()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nAnimatedImage {}", {});

    QQuickAnimatedImage *anim = qobject_cast<QQuickAnimatedImage*>(component.create());
    QVERIFY(anim);

    anim->setSource(server.url("/stickman.gif"));
    QTRY_COMPARE(anim->status(), QQuickImage::Loading);

    anim->setFillMode(QQuickImage::PreserveAspectFit);
    QCOMPARE(anim->fillMode(), QQuickImage::PreserveAspectFit);
    anim->setMipmap(true);
    QCOMPARE(anim->mipmap(), true);
    anim->setCache(false);
    QCOMPARE(anim->cache(), false);
    anim->setSourceSize(QSize(200, 200));
    QCOMPARE(anim->sourceSize(), QSize(200, 200));

    QTRY_COMPARE(anim->status(), QQuickImage::Ready);

    delete anim;
}

QTEST_MAIN(tst_qquickanimatedimage)

#include "tst_qquickanimatedimage.moc"
