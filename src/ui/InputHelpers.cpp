#include "InputHelpers.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QEvent>

UpperCaseValidator::UpperCaseValidator(const QRegularExpression &pattern, QObject *parent)
    : QRegularExpressionValidator(pattern, parent)
{
}

QValidator::State UpperCaseValidator::validate(QString &input, int &position) const
{
    // Hex alanlarında yalnızca ASCII harfler bulunur; Türkçe yerel ayarından bağımsız çeviri yapılır.
    for (QChar &character : input) {
        if (character >= QLatin1Char('a') && character <= QLatin1Char('z')) {
            character = QChar(character.unicode() - 'a' + 'A');
        }
    }
    return QRegularExpressionValidator::validate(input, position);
}

WheelLockFilter::WheelLockFilter(QObject *parent)
    : QObject(parent)
{
}

bool WheelLockFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        auto *combo = qobject_cast<QComboBox *>(watched);
        const bool closedCombo = combo != nullptr && !combo->view()->isVisible();
        if (closedCombo || qobject_cast<QAbstractSpinBox *>(watched) != nullptr) {
            // Olay reddedilir: QApplication tekerlek olayını üst widget'a (ör. kaydırma alanı) iletir.
            event->ignore();
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}
