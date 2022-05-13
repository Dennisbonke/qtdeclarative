// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
#ifndef TEXTCURSOR_H
#define TEXTCURSOR_H

#include <QtCore/qstring.h>

namespace Utils {

class TextDocument;
class TextBlock;

class TextCursor
{
public:
    enum MoveOperation {
        NoMove,
        Start,
        PreviousCharacter,
        End,
        NextCharacter,
    };

    enum MoveMode { MoveAnchor, KeepAnchor };

    enum SelectionType { Document };

    TextCursor();
    TextCursor(const TextBlock &block);
    TextCursor(TextDocument *document);

    bool movePosition(MoveOperation op, MoveMode = MoveAnchor, int n = 1);
    int position() const;
    void setPosition(int pos, MoveMode mode = MoveAnchor);
    QString selectedText() const;
    void clearSelection();
    int anchor() const;
    TextDocument *document() const;
    void insertText(const QString &text);
    TextBlock block() const;
    int positionInBlock() const;
    int blockNumber() const;

    void select(SelectionType selection);

    bool hasSelection() const;

    void removeSelectedText();
    int selectionEnd() const;

    bool isNull() const;

private:
    TextDocument *m_document = nullptr;
    int m_position = 0;
    int m_anchor = 0;
};
} // namespace Utils

#endif // TEXTCURSOR_H
