#include "DockTitleBar.h"

#include "Theme.h"

#include <QDockWidget>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QScreen>
#include <QToolButton>

DockTitleBar::DockTitleBar(QDockWidget *dock)
    : QWidget(dock)
    , m_dock(dock)
{
    m_titleLabel = new QLabel(dock->windowTitle(), this);
    m_titleLabel->setObjectName(QStringLiteral("MutedText"));
    // Etiket fare olaylarını almaz; tıklama / sürükleme / çift tıklama panele ulaşır.
    m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    const auto makeButton = [this]() {
        auto *button = new QToolButton(this);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(QSize(14, 14));
        return button;
    };
    m_maximizeButton = makeButton();
    m_floatButton = makeButton();
    m_closeButton = makeButton();
    m_closeButton->setToolTip(tr("Paneli kapat (Görünüm menüsünden yeniden açılır)"));

    connect(m_maximizeButton, &QToolButton::clicked, this, &DockTitleBar::ToggleMaximized);
    connect(m_floatButton, &QToolButton::clicked, dock, [dock]() { dock->setFloating(!dock->isFloating()); });
    connect(m_closeButton, &QToolButton::clicked, dock, &QDockWidget::close);
    connect(dock, &QDockWidget::windowTitleChanged, m_titleLabel, &QLabel::setText);
    connect(dock, &QDockWidget::topLevelChanged, this, &DockTitleBar::UpdateButtons);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 2, 2);
    layout->setSpacing(2);
    layout->addWidget(m_titleLabel, 1);
    layout->addWidget(m_maximizeButton);
    layout->addWidget(m_floatButton);
    layout->addWidget(m_closeButton);
    UpdateButtons();
}

void DockTitleBar::SetCollapsed(bool collapsed)
{
    if (m_collapsed != collapsed) {
        m_collapsed = collapsed;
        m_titleLabel->setVisible(!collapsed);
        m_floatButton->setVisible(!collapsed);
        m_closeButton->setVisible(!collapsed);
        updateGeometry();
        if (QLayout *dockLayout = m_dock->layout()) {
            dockLayout->invalidate();
        }
    }
    UpdateButtons();
}

bool DockTitleBar::IsCollapsed() const
{
    return m_collapsed;
}

QSize DockTitleBar::sizeHint() const
{
    if (m_collapsed) {
        return QSize(0, 0);
    }
    return QSize(QWidget::sizeHint().width(), qMax(24, fontMetrics().height() + 10));
}

QSize DockTitleBar::minimumSizeHint() const
{
    return m_collapsed ? QSize(0, 0) : QSize(0, sizeHint().height());
}

void DockTitleBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange) {
        RefreshIcons();
    }
    QWidget::changeEvent(event);
}

QWidget *DockTitleBar::SeparateWindow() const
{
    QWidget *window = m_dock->window();
    return qobject_cast<QMainWindow *>(window) == nullptr ? window : nullptr;
}

void DockTitleBar::ToggleMaximized()
{
    QWidget *window = SeparateWindow();
    if (window == nullptr) {
        return;
    }
    if (m_maximized && window == m_maximizedWindow) {
        window->setGeometry(m_normalGeometry);
        m_maximized = false;
        m_maximizedWindow = nullptr;
    } else {
        QScreen *screen = window->screen();
        if (screen == nullptr) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen == nullptr) {
            return;
        }
        m_normalGeometry = window->geometry();
        window->setGeometry(screen->availableGeometry());
        m_maximized = true;
        m_maximizedWindow = window;
    }
    window->raise();
    UpdateButtons();
}

void DockTitleBar::UpdateButtons()
{
    QWidget *window = SeparateWindow();
    // Panel ana pencereye döndüyse veya başka bir ayrı pencereye geçtiyse büyütme durumu geçersizdir.
    if (window == nullptr || window != m_maximizedWindow) {
        m_maximized = false;
        m_maximizedWindow = nullptr;
    }
    m_maximizeButton->setVisible(!m_collapsed && window != nullptr);
    m_maximizeButton->setToolTip(m_maximized ? tr("Eski boyutuna döndür") : tr("Tam pencere (ekranı kapla)"));
    m_floatButton->setToolTip(window != nullptr ? tr("Ana pencereye yerleştir") : tr("Ayrı pencereye al"));
    RefreshIcons();
}

void DockTitleBar::RefreshIcons()
{
    const bool separate = SeparateWindow() != nullptr;
    m_maximizeButton->setIcon(Theme::Icon(m_maximized ? QStringLiteral("restore") : QStringLiteral("maximize")));
    m_floatButton->setIcon(Theme::Icon(separate ? QStringLiteral("layout") : QStringLiteral("undock")));
    m_closeButton->setIcon(Theme::Icon(QStringLiteral("close")));
}
