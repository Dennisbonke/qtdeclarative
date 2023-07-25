// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "tst_qmlls_modules.h"
#include "QtQmlLS/private/qqmllsutils_p.h"
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

// Check if QTest already has a QTEST_CHECKED macro
#ifndef QTEST_CHECKED
#define QTEST_CHECKED(...) \
do { \
    __VA_ARGS__; \
    if (QTest::currentTestFailed()) \
        return; \
} while (false)
#endif

QT_USE_NAMESPACE
using namespace Qt::StringLiterals;
using namespace QLspSpecification;

static constexpr bool enable_debug_output = true;

tst_qmlls_modules::tst_qmlls_modules()
    : QQmlDataTest(QT_QMLTEST_DATADIR),
      m_protocol([this](const QByteArray &data) { m_server.write(data); })
{
    connect(&m_server, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray data = m_server.readAllStandardOutput();
        m_protocol.receiveData(data);
    });

    connect(&m_server, &QProcess::readyReadStandardError, this,
            [this]() {
        QProcess::ProcessChannel tmp = m_server.readChannel();
        m_server.setReadChannel(QProcess::StandardError);
        while (m_server.canReadLine())
            std::cerr << m_server.readLine().constData();
        m_server.setReadChannel(tmp);
    });

    m_qmllsPath =
            QLibraryInfo::path(QLibraryInfo::BinariesPath) + QLatin1String("/qmlls");
#ifdef Q_OS_WIN
    m_qmllsPath += QLatin1String(".exe");
#endif
    // allow overriding of the executable, to be able to use a qmlEcho script (as described in
    // qmllanguageservertool.cpp)
    m_qmllsPath = qEnvironmentVariable("QMLLS", m_qmllsPath);
    // qputenv("QT_LOGGING_RULES",
    // "qt.languageserver.codemodel.debug=true;qt.languageserver.codemodel.warning=true"); // helps
    // when using EditingRecorder
    m_server.setProgram(m_qmllsPath);
    // m_server.setArguments(QStringList() << u"-v"_s << u"-w"_s << u"8"_s);
    /*
    m_protocol.registerPublishDiagnosticsNotificationHandler([](const QByteArray &, auto) {
        // ignoring qmlint notifications
    });
    */
}

void tst_qmlls_modules::initTestCase()
{
    QQmlDataTest::initTestCase();
    if (!QFileInfo::exists(m_qmllsPath)) {
        QString message =
                QStringLiteral("qmlls executable not found (looked for %0)").arg(m_qmllsPath);
        QSKIP(qPrintable(message)); // until we add a feature for this we avoid failing here
    }
    m_server.start();
    InitializeParams clientInfo;
    clientInfo.rootUri = QUrl::fromLocalFile(dataDirectory() + "/default").toString().toUtf8();

    TextDocumentClientCapabilities tDoc;
    tDoc.typeDefinition = TypeDefinitionClientCapabilities{ false, false };

    PublishDiagnosticsClientCapabilities pDiag;
    tDoc.publishDiagnostics = pDiag;
    pDiag.versionSupport = true;
    clientInfo.capabilities.textDocument = tDoc;
    bool didInit = false;
    m_protocol.requestInitialize(clientInfo, [this, &didInit](const InitializeResult &serverInfo) {
        Q_UNUSED(serverInfo);
        m_protocol.notifyInitialized(InitializedParams());
        didInit = true;
    });
    QTRY_COMPARE_WITH_TIMEOUT(didInit, true, 10000);

    for (const QString &filePath :
         QStringList({ u"completions/Yyy.qml"_s, u"completions/fromBuildDir.qml"_s,
                       u"completions/SomeBase.qml"_s, u"findUsages/jsIdentifierUsages.qml"_s,
                       u"findDefinition/jsDefinitions.qml"_s, u"linting/SimpleItem.qml"_s,
                       u"formatting/rangeFormatting.qml"_s})) {
        QFile file(testFile(filePath));
        QVERIFY(file.open(QIODevice::ReadOnly));
        DidOpenTextDocumentParams oParams;
        TextDocumentItem textDocument;
        QByteArray uri = testFileUrl(filePath).toString().toUtf8();
        textDocument.uri = uri;
        textDocument.text = file.readAll();
        oParams.textDocument = textDocument;
        m_protocol.notifyDidOpenTextDocument(oParams);
        m_uriToClose.append(uri);
    }
}

void tst_qmlls_modules::completions_data()
{
    QTest::addColumn<QByteArray>("uri");
    QTest::addColumn<int>("lineNr");
    QTest::addColumn<int>("character");
    QTest::addColumn<ExpectedCompletions>("expected");
    QTest::addColumn<QStringList>("notExpected");

    QByteArray uri = testFileUrl("completions/Yyy.qml").toString().toUtf8();

    QTest::newRow("objEmptyLine") << uri << 8 << 0
                                  << ExpectedCompletions({
                                             { u"Rectangle"_s, CompletionItemKind::Class },
                                             { u"property"_s, CompletionItemKind::Keyword },
                                             { u"width"_s, CompletionItemKind::Property },
                                             { u"function"_s, CompletionItemKind::Keyword },
                                     })
                                  << QStringList({ u"QtQuick"_s, u"vector4d"_s });

    QTest::newRow("inBindingLabel") << uri << 5 << 9
                                    << ExpectedCompletions({
                                               { u"Rectangle"_s, CompletionItemKind::Class },
                                               { u"property"_s, CompletionItemKind::Keyword },
                                               { u"width"_s, CompletionItemKind::Property },
                                       })
                                    << QStringList({ u"QtQuick"_s, u"vector4d"_s });

    QTest::newRow("afterBinding") << uri << 5 << 10
                                  << ExpectedCompletions({
                                             { u"Rectangle"_s, CompletionItemKind::Field },
                                             { u"width"_s, CompletionItemKind::Field },
                                             { u"vector4d"_s, CompletionItemKind::Field },
                                     })
                                  << QStringList({ u"QtQuick"_s, u"property"_s });

    // suppress?
    QTest::newRow("afterId") << uri << 4 << 7
                             << ExpectedCompletions({
                                        { u"import"_s, CompletionItemKind::Keyword },
                                })
                             << QStringList({ u"QtQuick"_s, u"property"_s, u"Rectangle"_s,
                                              u"width"_s, u"vector4d"_s });

    QTest::newRow("fileStart") << uri << 0 << 0
                               << ExpectedCompletions({
                                          { u"Rectangle"_s, CompletionItemKind::Class },
                                          { u"import"_s, CompletionItemKind::Keyword },
                                  })
                               << QStringList({ u"QtQuick"_s, u"vector4d"_s, u"width"_s });

    QTest::newRow("importImport") << uri << 0 << 3
                                  << ExpectedCompletions({
                                             { u"Rectangle"_s, CompletionItemKind::Class },
                                             { u"import"_s, CompletionItemKind::Keyword },
                                     })
                                  << QStringList({ u"QtQuick"_s, u"vector4d"_s, u"width"_s });

    QTest::newRow("importModuleStart")
            << uri << 0 << 7
            << ExpectedCompletions({
                       { u"QtQuick"_s, CompletionItemKind::Module },
               })
            << QStringList({ u"vector4d"_s, u"width"_s, u"Rectangle"_s, u"import"_s });

    QTest::newRow("importVersionStart")
            << uri << 0 << 15
            << ExpectedCompletions({
                       { u"2"_s, CompletionItemKind::Constant },
                       { u"as"_s, CompletionItemKind::Keyword },
               })
            << QStringList({ u"Rectangle"_s, u"import"_s, u"vector4d"_s, u"width"_s });

    // QTest::newRow("importVersionMinor")
    //         << uri << 0 << 17
    //         << ExpectedCompletions({
    //                    { u"15"_s, CompletionItemKind::Constant },
    //            })
    //         << QStringList({ u"as"_s, u"Rectangle"_s, u"import"_s, u"vector4d"_s, u"width"_s });

    QTest::newRow("inScript") << uri << 6 << 14
                              << ExpectedCompletions({
                                         { u"Rectangle"_s, CompletionItemKind::Field },
                                         { u"vector4d"_s, CompletionItemKind::Field },
                                         { u"lala()"_s, CompletionItemKind::Function },
                                         { u"longfunction()"_s, CompletionItemKind::Function },
                                         { u"documentedFunction()"_s,
                                           CompletionItemKind::Function },
                                         { u"lala()"_s, CompletionItemKind { 0 } },
                                         { u"width"_s, CompletionItemKind::Field },
                                 })
                              << QStringList({ u"import"_s });

    QTest::newRow("expandBase1") << uri << 9 << 23
                                 << ExpectedCompletions({
                                            { u"width"_s, CompletionItemKind::Field },
                                            { u"foo"_s, CompletionItemKind::Field },
                                    })
                                 << QStringList({ u"import"_s, u"Rectangle"_s });

    QTest::newRow("expandBase2") << uri << 10 << 29
                                 << ExpectedCompletions({
                                            { u"width"_s, CompletionItemKind::Field },
                                            { u"color"_s, CompletionItemKind::Field },
                                    })
                                 << QStringList({ u"foo"_s, u"import"_s, u"Rectangle"_s });

    QTest::newRow("asCompletions")
            << uri << 25 << 8
            << ExpectedCompletions({
                       { u"Rectangle"_s, CompletionItemKind::Field },
               })
            << QStringList({ u"foo"_s, u"import"_s, u"lala()"_s, u"width"_s });
}

void tst_qmlls_modules::checkCompletions(QByteArray uri, int lineNr, int character,
                                            ExpectedCompletions expected, QStringList notExpected)
{
    CompletionParams cParams;
    cParams.position.line = lineNr;
    cParams.position.character = character;
    cParams.textDocument.uri = uri;
    std::shared_ptr<bool> didFinish = std::make_shared<bool>(false);
    auto clean = [didFinish]() { *didFinish = true; };

    m_protocol.requestCompletion(
            cParams,
            [clean, uri, expected, notExpected](auto res) {
                QScopeGuard cleanup(clean);
                const QList<CompletionItem> *cItems = std::get_if<QList<CompletionItem>>(&res);

                if (!cItems) {
                    return;
                }

                QSet<QString> labels;
                QDuplicateTracker<QByteArray> modulesTracker;
                QDuplicateTracker<QByteArray> keywordsTracker;
                QDuplicateTracker<QByteArray> classesTracker;
                QDuplicateTracker<QByteArray> fieldsTracker;
                QDuplicateTracker<QByteArray> propertiesTracker;

                for (const CompletionItem &c : *cItems) {
                    if (c.kind->toInt() == int(CompletionItemKind::Module)) {
                        QVERIFY2(!modulesTracker.hasSeen(c.label), "Duplicate module: " + c.label);
                    } else if (c.kind->toInt() == int(CompletionItemKind::Keyword)) {
                        QVERIFY2(!keywordsTracker.hasSeen(c.label),
                                 "Duplicate keyword: " + c.label);
                    } else if (c.kind->toInt() == int(CompletionItemKind::Class)) {
                        QVERIFY2(!classesTracker.hasSeen(c.label), "Duplicate class: " + c.label);
                    } else if (c.kind->toInt() == int(CompletionItemKind::Field)) {
                        QVERIFY2(!fieldsTracker.hasSeen(c.label), "Duplicate field: " + c.label);
                    } else if (c.kind->toInt() == int(CompletionItemKind::Property)) {
                        QVERIFY2(!propertiesTracker.hasSeen(c.label),
                                 "Duplicate property: " + c.label);
                        QVERIFY2(c.insertText == c.label + u": "_s,
                                 "a property should end with a colon with a space for "
                                 "'insertText', for better coding experience");
                    }
                    labels << c.label;
                }

                for (const ExpectedCompletion &exp : expected) {
                    QVERIFY2(labels.contains(exp.first),
                             u"no %1 in %2"_s
                                     .arg(exp.first,
                                          QStringList(labels.begin(), labels.end()).join(u", "_s))
                                     .toUtf8());
                    if (labels.contains(exp.first)) {
                        for (const CompletionItem &c : *cItems) {
                            const auto kind = static_cast<CompletionItemKind>(c.kind->toInt());

                            bool foundEntry = false;
                            bool hasCorrectKind = false;
                            for (const ExpectedCompletion &e : expected) {
                                if (c.label == e.first) {
                                    foundEntry = true;
                                    hasCorrectKind |= kind == e.second;
                                }
                            }

                            // Ignore QVERIFY for those completions not in the expected list.
                            if (!foundEntry)
                                continue;

                            QVERIFY2(hasCorrectKind,
                                     qPrintable(
                                             QString::fromLatin1(
                                                     "Completion item '%1' has wrong kind '%2'")
                                                     .arg(c.label)
                                                     .arg(QMetaEnum::fromType<CompletionItemKind>()
                                                                  .valueToKey(int(kind)))));
                        }
                    }
                }
                for (const QString &nexp : notExpected) {
                    QVERIFY2(!labels.contains(nexp),
                             u"found unexpected completion  %1"_s.arg(nexp).toUtf8());
                }
            },
            [clean](const ResponseError &err) {
                QScopeGuard cleanup(clean);
                ProtocolBase::defaultResponseErrorHandler(err);
                QVERIFY2(false, "error computing the completion");
            });
    QTRY_VERIFY_WITH_TIMEOUT(*didFinish, 30000);
}

void tst_qmlls_modules::completions()
{
    QFETCH(QByteArray, uri);
    QFETCH(int, lineNr);
    QFETCH(int, character);
    QFETCH(ExpectedCompletions, expected);
    QFETCH(QStringList, notExpected);

    QTEST_CHECKED(checkCompletions(uri, lineNr, character, expected, notExpected));
}

void tst_qmlls_modules::function_documentations_data()
{
    QTest::addColumn<QByteArray>("uri");
    QTest::addColumn<int>("lineNr");
    QTest::addColumn<int>("character");
    QTest::addColumn<ExpectedDocumentations>("expectedDocs");

    QByteArray uri = testFileUrl("completions/Yyy.qml").toString().toUtf8();

    QTest::newRow("longfunction")
            << uri << 5 << 14
            << ExpectedDocumentations{
                   std::make_tuple(u"lala()"_s, u"returns void"_s, u"lala()"_s),
                   std::make_tuple(u"longfunction()"_s, u"returns string"_s,
                                   uR"(longfunction(a, b, c = "c", d = "d"))"_s),
                   std::make_tuple(u"documentedFunction()"_s, u"returns string"_s,
                                   uR"(// documentedFunction: is documented
// returns 'Good'
documentedFunction(arg1, arg2 = "Qt"))"_s),
               };
}

void tst_qmlls_modules::function_documentations()
{
    QFETCH(QByteArray, uri);
    QFETCH(int, lineNr);
    QFETCH(int, character);
    QFETCH(ExpectedDocumentations, expectedDocs);

    CompletionParams cParams;
    cParams.position.line = lineNr;
    cParams.position.character = character;
    cParams.textDocument.uri = uri;
    std::shared_ptr<bool> didFinish = std::make_shared<bool>(false);
    auto clean = [didFinish]() { *didFinish = true; };

    m_protocol.requestCompletion(
            cParams,
            [clean, uri, expectedDocs](auto res) {
                const QList<CompletionItem> *cItems = std::get_if<QList<CompletionItem>>(&res);

                if (!cItems) {
                    return;
                }

                for (const ExpectedDocumentation &exp : expectedDocs) {
                    bool hasFoundExpected = false;
                    const auto expectedLabel = std::get<0>(exp);
                    for (const CompletionItem &c : *cItems) {
                        if (c.kind->toInt() != int(CompletionItemKind::Function)) {
                            // Only check functions.
                            continue;
                        }

                        if (c.label == expectedLabel) {
                            hasFoundExpected = true;
                        }
                    }

                    QVERIFY2(hasFoundExpected,
                             qPrintable(u"expected completion label '%1' wasn't found"_s.arg(
                                     expectedLabel)));
                }

                for (const CompletionItem &c : *cItems) {
                    if (c.kind->toInt() != int(CompletionItemKind::Function)) {
                        // Only check functions.
                        continue;
                    }

                    QVERIFY(c.documentation != std::nullopt);
                    // We currently don't support 'MarkupContent', change this when we do.
                    QVERIFY(c.documentation->index() == 0);
                    const QByteArray cDoc = std::get<0>(*c.documentation);

                    for (const ExpectedDocumentation &exp : expectedDocs) {
                        const auto &[label, details, docs] = exp;

                        if (c.label != label)
                            continue;

                        QVERIFY2(c.detail == details,
                                 qPrintable(u"Completion item '%1' has wrong details '%2'"_s
                                                    .arg(label).arg(*c.detail)));
                        QVERIFY2(cDoc == docs,
                                 qPrintable(u"Completion item '%1' has wrong documentation '%2'"_s
                                                    .arg(label).arg(cDoc)));
                    }
                }
                clean();
            },
            [clean](const ResponseError &err) {
                ProtocolBase::defaultResponseErrorHandler(err);
                QVERIFY2(false, "error computing the completion");
                clean();
            });
    QTRY_VERIFY_WITH_TIMEOUT(*didFinish, 30000);
}

void tst_qmlls_modules::buildDir()
{
    QString filePath = u"completions/fromBuildDir.qml"_s;
    QByteArray uri = testFileUrl(filePath).toString().toUtf8();
    QTEST_CHECKED(checkCompletions(uri, 3, 0,
                     ExpectedCompletions({
                             { u"property"_s, CompletionItemKind::Keyword },
                             { u"function"_s, CompletionItemKind::Keyword },
                             { u"Rectangle"_s, CompletionItemKind::Class },
                     }),
                     QStringList({ u"BuildDirType"_s, u"QtQuick"_s, u"width"_s, u"vector4d"_s })));
    Notifications::AddBuildDirsParams bDirs;
    UriToBuildDirs ub;
    ub.baseUri = uri;
    ub.buildDirs.append(testFile("buildDir").toUtf8());
    bDirs.buildDirsToSet.append(ub);
    m_protocol.typedRpc()->sendNotification(QByteArray(Notifications::AddBuildDirsMethod), bDirs);
    DidChangeTextDocumentParams didChange;
    didChange.textDocument.uri = uri;
    didChange.textDocument.version = 2;
    TextDocumentContentChangeEvent change;
    QFile file(testFile(filePath));
    QVERIFY(file.open(QIODevice::ReadOnly));
    change.text = file.readAll();
    didChange.contentChanges.append(change);
    m_protocol.notifyDidChangeTextDocument(didChange);
    QTEST_CHECKED(checkCompletions(uri, 3, 0,
                     ExpectedCompletions({
                             { u"BuildDirType"_s, CompletionItemKind::Class },
                             { u"Rectangle"_s, CompletionItemKind::Class },
                             { u"property"_s, CompletionItemKind::Keyword },
                             { u"width"_s, CompletionItemKind::Property },
                             { u"function"_s, CompletionItemKind::Keyword },
                     }),
                     QStringList({ u"QtQuick"_s, u"vector4d"_s })));
}
void tst_qmlls_modules::cleanupTestCase()
{
    for (const QByteArray &uri : m_uriToClose) {
        DidCloseTextDocumentParams closeP;
        closeP.textDocument.uri = uri;
        m_protocol.notifyDidCloseTextDocument(closeP);
    }
    m_server.closeWriteChannel();
    QTRY_COMPARE(m_server.state(), QProcess::NotRunning);
    QCOMPARE(m_server.exitStatus(), QProcess::NormalExit);
}

void tst_qmlls_modules::goToTypeDefinition_data()
{
    QTest::addColumn<QByteArray>("uri");
    QTest::addColumn<int>("line");
    QTest::addColumn<int>("character");
    QTest::addColumn<QByteArray>("expectedUri");
    QTest::addColumn<int>("expectedStartLine");
    QTest::addColumn<int>("expectedStartCharacter");
    QTest::addColumn<int>("expectedEndLine");
    QTest::addColumn<int>("expectedEndCharacter");

    QByteArray yyyUri = testFileUrl("completions/Yyy.qml").toString().toUtf8();
    QByteArray zzzUri = testFileUrl("completions/Zzz.qml").toString().toUtf8();
    QByteArray someBaseUri = testFileUrl("completions/SomeBase.qml").toString().toUtf8();

    QTest::newRow("BaseOfYyy") << yyyUri << 3 << 1 << zzzUri << 2 << 0 << 9 << 1;
    QTest::newRow("BaseOfIC") << yyyUri << 29 << 19 << zzzUri << 2 << 0 << 9 << 1;

    QTest::newRow("PropertyType") << yyyUri << 30 << 14 << someBaseUri << 2 << 0 << 4 << 1;

    QTest::newRow("TypeInIC") << yyyUri << 29 << 36 << someBaseUri << 2 << 0 << 4 << 1;
    QTest::newRow("ICTypeDefinition") << yyyUri << 29 << 15 << yyyUri << 29 << 18 << 29 << 48;
}

void tst_qmlls_modules::goToTypeDefinition()
{
    QFETCH(QByteArray, uri);
    QFETCH(int, line);
    QFETCH(int, character);
    QFETCH(QByteArray, expectedUri);
    QFETCH(int, expectedStartLine);
    QFETCH(int, expectedStartCharacter);
    QFETCH(int, expectedEndLine);
    QFETCH(int, expectedEndCharacter);

    QVERIFY(uri.startsWith("file://"_ba));

    // TODO
    TypeDefinitionParams params;
    params.position.line = line;
    params.position.character = character;
    params.textDocument.uri = uri;

    std::shared_ptr<bool> didFinish = std::make_shared<bool>(false);
    auto clean = [didFinish]() { *didFinish = true; };

    m_protocol.requestTypeDefinition(
            params,
            [&](auto res) {
                QScopeGuard cleanup(clean);
                auto *result = std::get_if<QList<Location>>(&res);

                QVERIFY(result);

                QCOMPARE(result->size(), 1);

                Location l = result->front();
                QCOMPARE(l.uri, expectedUri);
                QCOMPARE(l.range.start.line, expectedStartLine);
                QCOMPARE(l.range.start.character, expectedStartCharacter);
                QCOMPARE(l.range.end.line, expectedEndLine);
                QCOMPARE(l.range.end.character, expectedEndCharacter);
            },
            [clean](const ResponseError &err) {
                QScopeGuard cleanup(clean);
                ProtocolBase::defaultResponseErrorHandler(err);
                QVERIFY2(false, "error computing the completion");
            });
    QTRY_VERIFY_WITH_TIMEOUT(*didFinish, 30000);
}

void tst_qmlls_modules::goToDefinition_data()
{
    QTest::addColumn<QByteArray>("uri");
    // keep in mind that line and character are starting at 1!
    QTest::addColumn<int>("line");
    QTest::addColumn<int>("character");

    QTest::addColumn<QByteArray>("expectedUri");
    // set to -1 when unchanged from above line and character. 0-based.
    QTest::addColumn<int>("expectedStartLine");
    QTest::addColumn<int>("expectedStartCharacter");
    QTest::addColumn<int>("expectedEndLine");
    QTest::addColumn<size_t>("expectedEndCharacter");

    const QByteArray JSDefinitionsQml =
            testFileUrl(u"findDefinition/jsDefinitions.qml"_s).toEncoded();
    const QByteArray noResultExpected;

    QTest::addRow("JSIdentifierX") << JSDefinitionsQml << 14 << 11 << JSDefinitionsQml << 13 << 13
                                   << 13 << 13 + strlen("x");
    QTest::addRow("propertyI") << JSDefinitionsQml << 14 << 14 << JSDefinitionsQml << 9 << 18 << 9
                               << 18 + strlen("i");
    QTest::addRow("qualifiedPropertyI")
            << JSDefinitionsQml << 15 << 21 << JSDefinitionsQml << 9 << 18 << 9 << 18 + strlen("i");
    QTest::addRow("id") << JSDefinitionsQml << 15 << 17 << JSDefinitionsQml << 7 << 9 << 7
                        << 9 + strlen("rootId");

    QTest::addRow("parameterA") << JSDefinitionsQml << 10 << 16 << noResultExpected << -1 << -1
                                << -1 << size_t{};
    QTest::addRow("parameterB") << JSDefinitionsQml << 10 << 28 << noResultExpected << -1 << -1
                                << -1 << size_t{};
    QTest::addRow("comment") << JSDefinitionsQml << 10 << 21 << noResultExpected << -1 << -1 << -1
                             << size_t{};
}

void tst_qmlls_modules::goToDefinition()
{
    QFETCH(QByteArray, uri);
    QFETCH(int, line);
    QFETCH(int, character);
    QFETCH(QByteArray, expectedUri);
    QFETCH(int, expectedStartLine);
    QFETCH(int, expectedStartCharacter);
    QFETCH(int, expectedEndLine);
    QFETCH(size_t, expectedEndCharacter);

    QVERIFY(uri.startsWith("file://"_ba));

    DefinitionParams params;
    params.position.line = line - 1;
    params.position.character = character - 1;
    params.textDocument.uri = uri;

    std::shared_ptr<bool> didFinish = std::make_shared<bool>(false);
    auto clean = [didFinish]() { *didFinish = true; };

    m_protocol.requestDefinition(
            params,
            [&](auto res) {
                QScopeGuard cleanup(clean);
                auto *result = std::get_if<QList<Location>>(&res);
                const QByteArray noResultExpected;

                QVERIFY(result);
                if (expectedUri == noResultExpected) {
                    QCOMPARE(result->size(), 0);
                } else {
                    QCOMPARE(result->size(), 1);

                    Location l = result->front();
                    QCOMPARE(l.uri, expectedUri);
                    QCOMPARE(l.range.start.line, expectedStartLine - 1);
                    QCOMPARE(l.range.start.character, expectedStartCharacter - 1);
                    QCOMPARE(l.range.end.line, expectedEndLine - 1);
                    QCOMPARE(l.range.end.character, (int)(expectedEndCharacter - 1));
                }
            },
            [clean](const ResponseError &err) {
                QScopeGuard cleanup(clean);
                ProtocolBase::defaultResponseErrorHandler(err);
                QVERIFY2(false, "error computing the completion");
            });
    QTRY_VERIFY_WITH_TIMEOUT(*didFinish, 30000);
}

// startLine and startCharacter start at 1, not 0
static QLspSpecification::Range rangeFrom(const QString &code, quint32 startLine,
                                          quint32 startCharacter, quint32 length)
{
    QLspSpecification::Range range;

    // the LSP works with lines and characters starting at 0
    range.start.line = startLine - 1;
    range.start.character = startCharacter - 1;

    quint32 startOffset = QQmlLSUtils::textOffsetFrom(code, startLine - 1, startCharacter - 1);
    auto end = QQmlLSUtils::textRowAndColumnFrom(code, startOffset + length);
    range.end.line = end.line;
    range.end.character = end.character;

    return range;
}

// startLine and startCharacter start at 1, not 0
static QLspSpecification::Location locationFrom(const QByteArray fileName, const QString &code,
                                                quint32 startLine, quint32 startCharacter,
                                                quint32 length)
{
    QLspSpecification::Location location;
    location.uri = QQmlLSUtils::qmlUrlToLspUri(fileName);
    location.range = rangeFrom(code, startLine, startCharacter, length);
    return location;
}

void tst_qmlls_modules::findUsages_data()
{
    QTest::addColumn<QByteArray>("uri");
    QTest::addColumn<int>("line");
    QTest::addColumn<int>("character");
    QTest::addColumn<QList<QLspSpecification::Location>>("expectedUsages");

    QByteArray jsIdentifierUsagesUri = testFileUrl("findUsages/jsIdentifierUsages.qml").toEncoded();

    QString jsIdentifierUsagesContent;
    {
        QFile file(testFile("findUsages/jsIdentifierUsages.qml").toUtf8());
        QVERIFY(file.open(QIODeviceBase::ReadOnly));
        jsIdentifierUsagesContent = QString::fromUtf8(file.readAll());
    }

    // line and character start at 1!
    const QList<QLspSpecification::Location> sumUsages = {
        locationFrom(jsIdentifierUsagesUri, jsIdentifierUsagesContent, 8, 13, strlen("sum")),
        locationFrom(jsIdentifierUsagesUri, jsIdentifierUsagesContent, 10, 13, strlen("sum")),
        locationFrom(jsIdentifierUsagesUri, jsIdentifierUsagesContent, 10, 19, strlen("sum")),
    };
    QVERIFY(sumUsages.front().uri.startsWith("file://"_ba));

    // line and character start at 1!
    QTest::addRow("sumUsagesFromUsage") << jsIdentifierUsagesUri << 10 << 14 << sumUsages;
    QTest::addRow("sumUsagesFromUsage2") << jsIdentifierUsagesUri << 10 << 20 << sumUsages;
    QTest::addRow("sumUsagesFromDefinition") << jsIdentifierUsagesUri << 8 << 14 << sumUsages;
}

static bool operator==(const QLspSpecification::Location &a, const QLspSpecification::Location &b)
{
    return std::tie(a.uri, a.range.start.character, a.range.start.line, a.range.end.character,
                    a.range.end.line)
            == std::tie(b.uri, b.range.start.character, b.range.start.line, b.range.end.character,
                        b.range.end.line);
}

static bool locationListsAreEqual(const QList<QLspSpecification::Location> &a,
                                  const QList<QLspSpecification::Location> &b)
{
    return std::equal(
            a.cbegin(), a.cend(), b.cbegin(), b.cend(),
            [](const QLspSpecification::Location &a, const QLspSpecification::Location &b) {
                return a == b; // as else std::equal will not find the above implementation of
                               // operator==
            });
}

static QString locationToString(const QLspSpecification::Location &l)
{
    QString s = u"%1: (%2, %3) - (%4, %5)"_s.arg(l.uri)
                        .arg(l.range.start.line)
                        .arg(l.range.start.character)
                        .arg(l.range.end.line)
                        .arg(l.range.end.character);
    return s;
}

void tst_qmlls_modules::findUsages()
{
    QFETCH(QByteArray, uri);
    // line and character start at 1!
    QFETCH(int, line);
    QFETCH(int, character);
    QFETCH(QList<QLspSpecification::Location>, expectedUsages);

    QVERIFY(uri.startsWith("file://"_ba));

    ReferenceParams params;
    params.position.line = line - 1;
    params.position.character = character - 1;
    params.textDocument.uri = uri;
    std::shared_ptr<bool> didFinish = std::make_shared<bool>(false);
    auto clean = [didFinish]() { *didFinish = true; };
    m_protocol.requestReference(
            params,
            [&](auto res) {
                QScopeGuard cleanup(clean);
                auto *result = std::get_if<QList<Location>>(&res);

                QVERIFY(result);
                if constexpr (enable_debug_output) {
                    if (!locationListsAreEqual(*result, expectedUsages)) {
                        qDebug() << "Got following locations:";
                        for (auto &x : *result) {
                            qDebug() << locationToString(x);
                        }
                        qDebug() << "But expected:";
                        for (auto &x : expectedUsages) {
                            qDebug() << locationToString(x);
                        }
                    }
                }

                QVERIFY(locationListsAreEqual(*result, expectedUsages));
            },
            [clean](const ResponseError &err) {
                QScopeGuard cleanup(clean);
                ProtocolBase::defaultResponseErrorHandler(err);
                QVERIFY2(false, "error computing the completion");
            });
    QTRY_VERIFY_WITH_TIMEOUT(*didFinish, 3000);
}

void tst_qmlls_modules::documentFormatting_data()
{
    QTest::addColumn<QString>("originalFile");
    QTest::addColumn<QString>("expectedFile");

    QDir directory(QT_QMLFORMATTEST_DATADIR);

    // Exclude some test files which require options support
    QStringList excludedFiles;
    excludedFiles << "tests/auto/qml/qmlformat/data/checkIdsNewline.qml";
    excludedFiles << "tests/auto/qml/qmlformat/data/normalizedFunctionsSpacing.qml";
    excludedFiles << "tests/auto/qml/qmlformat/data/normalizedObjectsSpacing.qml";

    const auto shouldSkip = [&excludedFiles](const QString &fileName) {
        for (const QString &file : excludedFiles) {
            if (fileName.endsWith(file))
                return true;
        }
        return false;
    };

    // TODO: move into a separate member function
    const auto registerFile = [this](const QString &filePath) {
        QFile testFile(filePath);
        QVERIFY(testFile.open(QIODevice::ReadOnly));
        const auto fileUri = QUrl::fromLocalFile(filePath).toEncoded();
        DidOpenTextDocumentParams oParams;
        oParams.textDocument = TextDocumentItem{ fileUri, testFile.readAll() };
        m_protocol.notifyDidOpenTextDocument(oParams);
        m_uriToClose.append(fileUri);
    };

    // Filter to include files contain .formatted.
    const auto formattedFilesInfo =
            directory.entryInfoList(QStringList{ { "*.formatted.qml" } }, QDir::Files);
    for (const auto &formattedFileInfo : formattedFilesInfo) {
        const QFileInfo unformattedFileInfo(directory, formattedFileInfo.fileName().remove(".formatted"));
        const auto unformattedFilePath = unformattedFileInfo.canonicalFilePath();
        if (shouldSkip(unformattedFilePath))
            continue;

        registerFile(unformattedFilePath);
        QTest::newRow(qPrintable(unformattedFileInfo.fileName()))
                << unformattedFilePath << formattedFileInfo.canonicalFilePath();
    }

    // Extra tests
    const QString blanklinesPath = testFile("formatting/blanklines.qml");
    registerFile(blanklinesPath);
    QTest::newRow("leading-and-trailing-blanklines")
            << testFile("formatting/blanklines.qml")
            << testFile("formatting/blanklines.formatted.qml");
}

void tst_qmlls_modules::documentFormatting()
{
    QFETCH(QString, originalFile);
    QFETCH(QString, expectedFile);

    DocumentFormattingParams params;
    params.textDocument.uri = QUrl::fromLocalFile(originalFile).toEncoded();

    const auto lineCount = [](const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Error while opening the file " << filePath;
            return -1;
        }
        int lineCount = 0;
        QString line;
        while (!file.atEnd()) {
            line = file.readLine();
            ++lineCount;
        }
        if (line.endsWith('\n'))
            ++lineCount;
        return lineCount;
    };

    std::shared_ptr<bool> didFinish = std::make_shared<bool>(false);
    auto clean = [didFinish]() { *didFinish = true; };
    auto &&responseHandler = [&](auto response) {
        QScopeGuard cleanup(clean);
        if (std::holds_alternative<QList<TextEdit>>(response)) {
            const auto results = std::get<QList<TextEdit>>(response);
            QVERIFY(results.size() == 1);
            QFile file(expectedFile);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qWarning() << "Error while opening the file " << expectedFile;
                return;
            }

            const auto &textEdit = results.first();
            QCOMPARE(textEdit.range.start.line, 0);
            QCOMPARE(textEdit.range.start.character, 0);
            QCOMPARE(textEdit.range.end.line, lineCount(originalFile));
            QCOMPARE(textEdit.range.end.character, 0);
            QCOMPARE(textEdit.newText, file.readAll());
        }
    };

    auto &&errorHandler = [&clean](const ResponseError &err) {
        QScopeGuard cleanup(clean);
        ProtocolBase::defaultResponseErrorHandler(err);
        QVERIFY2(false, "error computing the completion");
    };
    m_protocol.requestDocumentFormatting(params, std::move(responseHandler),
                                         std::move(errorHandler));

    QTRY_VERIFY_WITH_TIMEOUT(*didFinish, 50000);
}

void tst_qmlls_modules::renameUsages_data()
{
    QTest::addColumn<QByteArray>("uri");
    QTest::addColumn<int>("line");
    QTest::addColumn<int>("character");
    QTest::addColumn<QString>("newName");
    QTest::addColumn<QLspSpecification::WorkspaceEdit>("expectedEdit");
    QTest::addColumn<QLspSpecification::ResponseError>("expectedError");

    QLspSpecification::ResponseError noError;

    QByteArray jsIdentifierUsagesUri = testFileUrl("findUsages/jsIdentifierUsages.qml").toEncoded();

    QString jsIdentifierUsagesContent;
    {
        QFile file(testFile("findUsages/jsIdentifierUsages.qml").toUtf8());
        QVERIFY(file.open(QIODeviceBase::ReadOnly));
        jsIdentifierUsagesContent = QString::fromUtf8(file.readAll());
    }

    // TODO: create workspace edit for the tests
    QLspSpecification::WorkspaceEdit sumRenames{
        std::nullopt, // TODO
        QList<TextDocumentEdit>{
                TextDocumentEdit{
                        OptionalVersionedTextDocumentIdentifier{ { jsIdentifierUsagesUri } },
                        {
                                TextEdit{
                                        rangeFrom(jsIdentifierUsagesContent, 8, 13, strlen("sum")),
                                        "specialSum" },
                                TextEdit{
                                        rangeFrom(jsIdentifierUsagesContent, 10, 13, strlen("sum")),
                                        "specialSum" },
                                TextEdit{
                                        rangeFrom(jsIdentifierUsagesContent, 10, 19, strlen("sum")),
                                        "specialSum" },
                        } },
        }
    };

    // line and character start at 1!
    QTest::addRow("sumRenameFromUsage")
            << jsIdentifierUsagesUri << 10 << 14 << u"specialSum"_s << sumRenames << noError;
    QTest::addRow("sumRenameFromUsage2")
            << jsIdentifierUsagesUri << 10 << 20 << u"specialSum"_s << sumRenames << noError;
    QTest::addRow("sumRenameFromDefinition")
            << jsIdentifierUsagesUri << 8 << 14 << u"specialSum"_s << sumRenames << noError;
    QTest::addRow("invalidSumRenameFromDefinition")
            << jsIdentifierUsagesUri << 8 << 14 << u"function"_s << sumRenames
            << QLspSpecification::ResponseError{
                   0,
                   "Invalid EcmaScript identifier!",
                   std::nullopt,
               };
}

void tst_qmlls_modules::renameUsages()
{
    QFETCH(QByteArray, uri);
    // line and character start at 1!
    QFETCH(int, line);
    QFETCH(int, character);
    QFETCH(QString, newName);
    QFETCH(QLspSpecification::WorkspaceEdit, expectedEdit);
    QFETCH(QLspSpecification::ResponseError, expectedError);

    QVERIFY(uri.startsWith("file://"_ba));

    RenameParams params;
    params.position.line = line - 1;
    params.position.character = character - 1;
    params.textDocument.uri = uri;
    params.newName = newName.toUtf8();

    std::shared_ptr<bool> didFinish = std::make_shared<bool>(false);
    auto clean = [didFinish]() { *didFinish = true; };
    m_protocol.requestRename(
            params,
            [&](auto res) {
                QScopeGuard cleanup(clean);
                auto *result = std::get_if<QLspSpecification::WorkspaceEdit>(&res);

                QVERIFY(result);
                QCOMPARE(result->changes.has_value(), expectedEdit.changes.has_value());
                QCOMPARE(result->changeAnnotations.has_value(),
                         expectedEdit.changeAnnotations.has_value());
                QCOMPARE(result->documentChanges.has_value(),
                         expectedEdit.documentChanges.has_value());

                std::visit(
                        [&expectedError](auto &&documentChanges, auto &&expectedDocumentChanges) {
                            if (!expectedError.message.isEmpty())
                                QVERIFY2(false, "No expected error was thrown.");

                            QCOMPARE(documentChanges.size(), expectedDocumentChanges.size());
                            using U = std::decay_t<decltype(documentChanges)>;
                            using V = std::decay_t<decltype(expectedDocumentChanges)>;

                            if constexpr (std::conjunction_v<
                                                  std::is_same<U, V>,
                                                  std::is_same<U, QList<TextDocumentEdit>>>) {
                                for (qsizetype i = 0; i < expectedDocumentChanges.size(); ++i) {
                                    QCOMPARE(documentChanges[i].textDocument.uri,
                                             expectedDocumentChanges[i].textDocument.uri);
                                    QVERIFY(documentChanges[i].textDocument.uri.startsWith(
                                            "file://"));
                                    QCOMPARE(documentChanges[i].textDocument.version,
                                             expectedDocumentChanges[i].textDocument.version);
                                    QCOMPARE(documentChanges[i].edits.size(),
                                             expectedDocumentChanges[i].edits.size());

                                    for (qsizetype j = 0; j < documentChanges[i].edits.size();
                                         ++j) {
                                        std::visit(
                                                [](auto &&textEdit, auto &&expectedTextEdit) {
                                                    using U = std::decay_t<decltype(textEdit)>;
                                                    using V = std::decay_t<
                                                            decltype(expectedTextEdit)>;

                                                    if constexpr (std::conjunction_v<
                                                                          std::is_same<U, V>,
                                                                          std::is_same<U,
                                                                                       TextEdit>>) {
                                                        QCOMPARE(textEdit.range.start.line,
                                                                 expectedTextEdit.range.start.line);
                                                        QCOMPARE(textEdit.range.start.character,
                                                                 expectedTextEdit.range.start
                                                                         .character);
                                                        QCOMPARE(textEdit.range.end.line,
                                                                 expectedTextEdit.range.end.line);
                                                        QCOMPARE(textEdit.range.end.character,
                                                                 expectedTextEdit.range.end
                                                                         .character);
                                                        QCOMPARE(textEdit.newText,
                                                                 expectedTextEdit.newText);
                                                    } else {
                                                        QFAIL("Comparison not implemented");
                                                    }
                                                },
                                                documentChanges[i].edits[j],
                                                expectedDocumentChanges[i].edits[j]);
                                    }
                                }

                            } else {
                                QFAIL("Comparison not implemented");
                            }
                        },
                        result->documentChanges.value(), expectedEdit.documentChanges.value());
            },
            [clean, &expectedError](const ResponseError &err) {
                QScopeGuard cleanup(clean);
                ProtocolBase::defaultResponseErrorHandler(err);
                if (expectedError.message.isEmpty())
                    QVERIFY2(false, "unexpected error computing the completion");
                QCOMPARE(err.code, expectedError.code);
                QCOMPARE(err.message, expectedError.message);
            });
    QTRY_VERIFY_WITH_TIMEOUT(*didFinish, 3000);
}

struct EditingRecorder
{
    QList<DidChangeTextDocumentParams> actions;
    QHash<int, QString> diagnosticsPerFileVersions;

    /*!
    All the indexes passed here must start at 1!

    If you want to make sure that your own written changes make sense, use
    \code
    qputenv("QT_LOGGING_RULES",
    "qt.languageserver.codemodel.debug=true;qt.languageserver.codemodel.warning=true"); \endcode
    before starting qmlls. It will print the differences between the different versions, and helps
    when some indices are off.
    */
    void changeText(int startLine, int startCharacter, int endLine, int endCharacter,
                    QString newText)
    {
        // The LSP starts at 0
        QVERIFY(startLine > 0);
        QVERIFY(startCharacter > 0);
        QVERIFY(endLine > 0);
        QVERIFY(endCharacter > 0);

        --startLine;
        --startCharacter;
        --endLine;
        --endCharacter;

        DidChangeTextDocumentParams params;
        params.textDocument =
                VersionedTextDocumentIdentifier{ { lastFileUri.toUtf8() }, ++version };
        params.contentChanges.append({
                Range{ Position{ startLine, startCharacter }, Position{ endLine, endCharacter } },
                std::nullopt, // deprecated range length
                newText.toUtf8(),
        });
        actions.append(params);
    }

    void setFile(const QString &uri) { lastFileUri = uri; }

    void setCurrentExpectedDiagnostic(const QString &diagnostic)
    {
        Q_ASSERT(diagnosticsPerFileVersions.find(version) == diagnosticsPerFileVersions.end());
        diagnosticsPerFileVersions[version] = diagnostic;
    }

    QString lastFileUri;
    int version = 0;
};

static constexpr int characterAfter(const char *line)
{
    return std::char_traits<char>::length(line) + 1;
}

static EditingRecorder propertyTypoScenario(const QString &uri)
{
    EditingRecorder propertyTypo;
    propertyTypo.setFile(uri);

    propertyTypo.changeText(5, 1, 5, 1, u"    property int t"_s);

    // replace property by propertyt and expect a complaint from the parser
    propertyTypo.changeText(5, characterAfter("    property"), 5, characterAfter("    property"),
                            u"t"_s);
    propertyTypo.setCurrentExpectedDiagnostic(u"Expected token"_s);

    // replace propertyt back to property and expect no complaint from the parser
    propertyTypo.changeText(5, characterAfter("    property"), 5, characterAfter("    propertyt"),
                            u""_s);

    // replace property by propertyt and expect a complaint from the parser
    propertyTypo.changeText(5, characterAfter("    property"), 5, characterAfter("    property"),
                            u"t"_s);
    propertyTypo.setCurrentExpectedDiagnostic(u"Expected token"_s);

    // now, simulate some slow typing and expect the previous warning to not disappear
    const QString data = u"Item {}\n"_s;
    for (int i = 0; i < data.size(); ++i) {
        propertyTypo.changeText(6, i + 1, 6, i + 1, data[i]);
        propertyTypo.setCurrentExpectedDiagnostic(u"Expected token"_s);
    }

    // replace propertyt back to property and expect no complaint from the parser
    propertyTypo.changeText(5, characterAfter("    property"), 5, characterAfter("    propertyt"),
                            u""_s);

    return propertyTypo;
}

void tst_qmlls_modules::linting_data()
{
    QTest::addColumn<EditingRecorder>("recorder");

    QTest::addRow("property-typo")
            << propertyTypoScenario(testFileUrl(u"linting/SimpleItem.qml"_s).toEncoded());
}

void tst_qmlls_modules::linting()
{
    QFETCH(EditingRecorder, recorder);
    bool diagnosticOk = false;
    m_protocol.registerPublishDiagnosticsNotificationHandler(
            [&recorder, &diagnosticOk](const QByteArray &, const PublishDiagnosticsParams &p) {
                if (p.uri != recorder.lastFileUri || !p.version)
                    return;
                auto expectedMessage = recorder.diagnosticsPerFileVersions.find(*p.version);
                if (expectedMessage == recorder.diagnosticsPerFileVersions.end()) {
                    if constexpr (enable_debug_output) {
                        if (p.diagnostics.size() > 0)
                            qDebug() << "Did not expect message" << p.diagnostics.front().message;
                    }

                    QVERIFY(p.diagnostics.size() == 0);
                    diagnosticOk = true;
                    return;
                }
                QVERIFY(p.diagnostics.size() > 0);
                if constexpr (enable_debug_output) {
                    if (!p.diagnostics.front().message.contains(expectedMessage->toUtf8())) {
                        qDebug() << "expected a message with" << *expectedMessage << "but got"
                                 << p.diagnostics.front().message;
                    }
                }
                QVERIFY(p.diagnostics.front().message.contains(expectedMessage->toUtf8()));
                diagnosticOk = true;
            });
    for (const auto &action : recorder.actions) {
        m_protocol.notifyDidChangeTextDocument(action);
        QTRY_VERIFY_WITH_TIMEOUT(diagnosticOk, 5000);
        diagnosticOk = false;
    }
}

void tst_qmlls_modules::rangeFormatting_data()
{
    QTest::addColumn<QByteArray>("uri");
    QTest::addColumn<QLspSpecification::Range>("selectedRange");
    QTest::addColumn<QLspSpecification::Range>("expectedRange");
    QTest::addColumn<QString>("expectedAfterFormat");

    const QByteArray filePath = testFileUrl("formatting/rangeFormatting.qml").toEncoded();

    {
        QLspSpecification::Range selectedRange = { { 5, 0 }, { 9, 0 } };
        QLspSpecification::Range expectedRange = { { 0, 0 }, { 24, 0 } };
        QTest::addRow("selectRegion1") << filePath << selectedRange << expectedRange
                                       << testFile("formatting/rangeFormatting.formatted1.qml");
    }

    {
        QLspSpecification::Range selectedRange = { { 10, 25 }, { 23, 0 } };
        QLspSpecification::Range expectedRange = { { 0, 0 }, { 24, 0 } };
        QTest::addRow("selecteRegion2") << filePath << selectedRange << expectedRange
                                        << testFile("formatting/rangeFormatting.formatted2.qml");
    }

    {
        QLspSpecification::Range selectedRange = { { 14, 36 }, { 14, 45 } };
        QLspSpecification::Range expectedRange = { { 0, 0 }, { 24, 0 } };
        QTest::addRow("selectSingleLine") << filePath << selectedRange << expectedRange
                                          << testFile("formatting/rangeFormatting.formatted3.qml");
    }

    {
        QLspSpecification::Range selectedRange = { { 0, 0 }, { 24, 0 } };
        QLspSpecification::Range expectedRange = { { 0, 0 }, { 24, 0 } };
        QTest::addRow("selecteEntireFile") << filePath << selectedRange << expectedRange
                                           << testFile("formatting/rangeFormatting.formatted4.qml");
    }

    {
        QLspSpecification::Range selectedRange = { { 10, 3 }, { 20, 4 } };
        QLspSpecification::Range expectedRange = { { 0, 0 }, { 24, 0 } };
        QTest::addRow("selectUnbalanced") << filePath << selectedRange << expectedRange
                                          << testFile("formatting/rangeFormatting.formatted5.qml");
    }
}

void tst_qmlls_modules::rangeFormatting()
{
    QFETCH(QByteArray, uri);
    QFETCH(QLspSpecification::Range, selectedRange);
    QFETCH(QLspSpecification::Range, expectedRange);
    QFETCH(QString, expectedAfterFormat);

    QLspSpecification::DocumentRangeFormattingParams params;
    params.textDocument.uri = uri;
    params.range = selectedRange;
    std::shared_ptr<bool> didFinish = std::make_shared<bool>(false);
    const auto clean = [didFinish]() { *didFinish = true; };

    auto &&responseHandler = [&](auto res) {
        Q_UNUSED(res);
        QScopeGuard cleanup(clean);
        auto result = std::get_if<QList<TextEdit>>(&res);
        QVERIFY(result);

        QFile file(expectedAfterFormat);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            QFAIL("Error while opening the file ");

        const auto text = result->first();
        QCOMPARE(text.range.start.line, expectedRange.start.line);
        QCOMPARE(text.range.start.character, expectedRange.start.character);
        QCOMPARE(text.range.end.line, expectedRange.end.line);
        QCOMPARE(text.range.end.character, expectedRange.end.character);
        QCOMPARE(text.newText, file.readAll());
    };

    auto &&errorHandler = [&clean](auto &error) {
        QScopeGuard cleanup(clean);
        ProtocolBase::defaultResponseErrorHandler(error);
        QVERIFY2(false, "error occurred while range formatting");
    };

    m_protocol.requestDocumentRangeFormatting(params, std::move(responseHandler),
                                              std::move(errorHandler));
    QTRY_VERIFY_WITH_TIMEOUT(*didFinish, 10000);
}

QTEST_MAIN(tst_qmlls_modules)

#include <tst_qmlls_modules.moc>
