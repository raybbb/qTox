/*
    Copyright © 2017 by The qTox Project Contributors

    This file is part of qTox, a Qt-based graphical interface for Tox.

    qTox is libre software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    qTox is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with qTox.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "textformatter.h"

#include <QRegularExpression>

// clang-format off

static const QString SINGLE_SIGN_PATTERN = QStringLiteral("(?<=^|[\\s\\n])"
                                                          "[%1]"
                                                          "(?!\\s)"
                                                          "([^%1\\n]+?)"
                                                          "(?<!\\s)"
                                                          "[%1]"
                                                          "(?=$|[\\s\\n])");

static const QString SINGLE_SLASH_PATTERN = QStringLiteral("(?<=^|[\\s\\n])"
                                                           "/"
                                                           "(?!\\s)"
                                                           "([^/\\n]+?)"
                                                           "(?<!\\s)"
                                                           "/"
                                                           "(?=$|[\\s\\n])");

static const QString DOUBLE_SIGN_PATTERN = QStringLiteral("(?<=^|[\\s\\n])"
                                                          "[%1]{2}"
                                                          "(?!\\s)"
                                                          "([^\\n]+?)"
                                                          "(?<!\\s)"
                                                          "[%1]{2}"
                                                          "(?=$|[\\s\\n])");

static const QString MULTILINE_CODE = QStringLiteral("(?<=^|[\\s\\n])"
                                                     "```"
                                                     "(?!`)"
                                                     "((.|\\n)+?)"
                                                     "(?<!`)"
                                                     "```"
                                                     "(?=$|[\\s\\n])");

#define REGEXP_WRAPPER_PAIR(pattern, wrapper)\
{QRegularExpression(pattern,QRegularExpression::UseUnicodePropertiesOption),QStringLiteral(wrapper)}

static const QPair<QRegularExpression, QString> REGEX_TO_WRAPPER[] {
    REGEXP_WRAPPER_PAIR(SINGLE_SLASH_PATTERN, "<i>%1</i>"),
    REGEXP_WRAPPER_PAIR(SINGLE_SIGN_PATTERN.arg('*'), "<b>%1</b>"),
    REGEXP_WRAPPER_PAIR(SINGLE_SIGN_PATTERN.arg('_'), "<u>%1</u>"),
    REGEXP_WRAPPER_PAIR(SINGLE_SIGN_PATTERN.arg('~'), "<s>%1</s>"),
    REGEXP_WRAPPER_PAIR(SINGLE_SIGN_PATTERN.arg('`'), "<font color=#595959><code>%1</code></font>"),
    REGEXP_WRAPPER_PAIR(DOUBLE_SIGN_PATTERN.arg('*'), "<b>%1</b>"),
    REGEXP_WRAPPER_PAIR(DOUBLE_SIGN_PATTERN.arg('/'), "<i>%1</i>"),
    REGEXP_WRAPPER_PAIR(DOUBLE_SIGN_PATTERN.arg('_'), "<u>%1</u>"),
    REGEXP_WRAPPER_PAIR(DOUBLE_SIGN_PATTERN.arg('~'), "<s>%1</s>"),
    REGEXP_WRAPPER_PAIR(MULTILINE_CODE, "<font color=#595959><code>%1</code></font>"),
};

#undef REGEXP_WRAPPER_PAIR

static const QString HREF_WRAPPER = QStringLiteral(R"(<a href="%1">%1</a>)");

// based in this: https://tools.ietf.org/html/rfc3986#section-2
static const QString URL_PATH_PATTERN = QStringLiteral("[\\w:/?#\\[\\]@!$&'{}*+,;.~%=-]+");

static const QRegularExpression URL_PATTERNS[] = {
    QRegularExpression(QStringLiteral(R"(\b(www\.|((http[s]?)|ftp)://)%1)").arg(URL_PATH_PATTERN)),
    QRegularExpression(QStringLiteral(R"(\b(file|smb)://([\S| ]*))")),
    QRegularExpression(QStringLiteral(R"(\btox:[a-zA-Z\\d]{76})")),
    QRegularExpression(QStringLiteral(R"(\bmailto:\S+@\S+\.\S+)")),
    QRegularExpression(QStringLiteral(R"(\btox:\S+@\S+)")),
};

// clang-format on

/**
 * @brief Highlights URLs within passed message string
 * @param message Where search for URLs
 * @return Copy of message with highlighted URLs
 */
QString highlightURL(const QString& message)
{
    QString result = message;
    for (const QRegularExpression& exp : URL_PATTERNS) {
        const int startLength = result.length();
        int offset = 0;
        QRegularExpressionMatchIterator iter = exp.globalMatch(result);
        while (iter.hasNext()) {
            const QRegularExpressionMatch match = iter.next();
            const int startPos = match.capturedStart() + offset;
            const int length = match.capturedLength();
            const QString wrappedURL = HREF_WRAPPER.arg(match.captured());
            result.replace(startPos, length, wrappedURL);
            offset = result.length() - startLength;
        }
    }
    return result;
}

/**
 * @brief Checks HTML tags intersection while applying styles to the message text
 * @param str Checking string
 * @return True, if tag intersection detected
 */
static bool isTagIntersection(const QString& str)
{
    const QRegularExpression TAG_PATTERN("(?<=<)/?[a-zA-Z0-9]+(?=>)");

    int openingTagCount = 0;
    int closingTagCount = 0;

    QRegularExpressionMatchIterator iter = TAG_PATTERN.globalMatch(str);
    while (iter.hasNext()) {
        iter.next().captured()[0] == '/' ? ++closingTagCount : ++openingTagCount;
    }
    return openingTagCount != closingTagCount;
}

/**
 * @brief Applies markdown to passed message string
 * @param message Formatting string
 * @param showFormattingSymbols True, if it is supposed to include formatting symbols into resulting
 * string
 * @return Copy of message with markdown applied
 */
QString applyMarkdown(const QString& message, bool showFormattingSymbols)
{
    QString result = message;
    for (const QPair<QRegularExpression, QString>& pair : REGEX_TO_WRAPPER) {
        QRegularExpressionMatchIterator iter = pair.first.globalMatch(result);
        int offset = 0;
        while (iter.hasNext()) {
            const QRegularExpressionMatch match = iter.next();
            QString captured = match.captured(!showFormattingSymbols);
            if (isTagIntersection(captured)) {
                continue;
            }

            const int length = match.capturedLength();
            const QString wrappedText = pair.second.arg(captured);
            const int startPos = match.capturedStart() + offset;
            result.replace(startPos, length, wrappedText);
            offset += wrappedText.length() - length;
        }
    }
    return result;
}
