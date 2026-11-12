#include "DockTitleBar.h"

#include "Theme.h"

#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QVariant>

namespace {

// Tam pencere durumunun ayrı pencerede saklandığı özellikler.
const char *const NormalGeometryProperty = "dockNormalGeometry";
const char *const MaximizedGeometryProperty = "dockMaximizedGeometry";

} // namespace

DockTitleBar::DockTitleBar(QDockWidget *dock)
    : QWidget(dock)
    , m_dock(dock)
{
    m_titleLabel = new QLabel(dock->windowTitle(), this);
    m_titleLabel->setObjectName(QStringLiteral("MutedText"));
    // Etiket fare olaylarını almaz; tıklama / sürükleme / çift tıklama başlığa ulaşır.
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
    connect(m_floatButton, &QToolButton::clicked, this, [this]() {
        if (SeparateWindow() != nullptr) {
            RestoreIfMaximized();
            emit DockRequested();
        } else {
            m_dock->setFloating(true);
        }
    });
    connect(m_closeButton, &QToolButton::clicked, dock, &QDockWidget::close);
    connect(dock, &QDockWidget::windowTitleChanged, m_titleLabel, &QLabel::setText);
    connect(dock, &QDockWidget::topLevelChanged, this, &DockTitleBar::UpdateButtons);
    connect(dock, &QDockWidget::topLevelChanged, this, [this](bool floating) {
        if (floating) {
            QTimer::singleShot(0, this, &DockTitleBar::FitFloatingWindow);
        }
    });

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

void DockTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (SeparateWindow() != nullptr) {
        m_maximizedPressed = false;
        RestoreIfMaximized();
        emit DockRequested();
        event->accept();
        return;
    }
    event->ignore(); // ana pencerede: QDockWidget paneli ayrı pencereye alır
}

void DockTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dock->isFloating() && IsWindowMaximized(m_dock)) {
        // Tam penceredeki panel: sürükleme başlarsa önce eski boyutuna döner, sonra sürükleme
        // Qt'ye devredilir (yalnızca tıklamak boyutu değiştirmez).
        m_maximizedPressed = true;
        m_pressGlobal = event->globalPosition().toPoint();
        event->accept();
        return;
    }
    event->ignore(); // sürüklemeyi Qt yapar
}

void DockTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_maximizedPressed) {
        event->ignore();
        return;
    }
    event->accept();
    const QPoint cursor = event->globalPosition().toPoint();
    if (!(event->buttons() & Qt::LeftButton)) {
        m_maximizedPressed = false;
        return;
    }
    if ((cursor - m_pressGlobal).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }
    m_maximizedPressed = false;
    RestoreForDrag(m_dock, m_pressGlobal);
    UpdateButtons();
    // Basma olayı eski boyuttaki konuma göre panele yeniden gönderilir; sonraki olaylar
    // yok sayılarak panele (Qt'nin sürüklemesine) akar.
    QMouseEvent press(QEvent::MouseButtonPress, m_dock->mapFromGlobal(QPointF(m_pressGlobal)),
                      QPointF(m_pressGlobal), Qt::LeftButton, Qt::LeftButton, event->modifiers());
    QApplication::sendEvent(m_dock, &press);
    QMouseEvent move(QEvent::MouseMove, m_dock->mapFromGlobal(event->globalPosition()), event->globalPosition(),
                     Qt::NoButton, event->buttons(), event->modifiers());
    QApplication::sendEvent(m_dock, &move);
}

void DockTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_maximizedPressed) {
        m_maximizedPressed = false; // sürüklenmeden bırakıldı: boyut değişmez
        event->accept();
        return;
    }
    event->ignore();
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

bool DockTitleBar::IsWindowMaximized(const QWidget *window)
{
    if (window == nullptr) {
        return false;
    }
    const QVariant maximized = window->property(MaximizedGeometryProperty);
    return maximized.isValid() && maximized.toRect() == window->geometry();
}

void DockTitleBar::ToggleMaximized()
{
    QWidget *window = SeparateWindow();
    if (window == nullptr) {
        return;
    }
    if (IsWindowMaximized(window)) {
        const QRect normal = window->property(NormalGeometryProperty).toRect();
        window->setProperty(MaximizedGeometryProperty, QVariant());
        window->setProperty(NormalGeometryProperty, QVariant());
        if (normal.isValid()) {
            window->setGeometry(normal);
        }
    } else {
        QScreen *screen = window->screen();
        if (screen == nullptr) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen == nullptr) {
            return;
        }
        const QRect normal = window->geometry();
        window->setGeometry(screen->availableGeometry());
        // En küçük boyut ekrandan büyükse pencere istenen alanı tam kaplamayabilir; gerçek geometri saklanır.
        window->setProperty(NormalGeometryProperty, normal);
        window->setProperty(MaximizedGeometryProperty, window->geometry());
    }
    window->raise();
    UpdateButtons();
}

void DockTitleBar::RestoreIfMaximized()
{
    if (IsWindowMaximized(SeparateWindow())) {
        ToggleMaximized();
    }
}

void DockTitleBar::RestoreForDrag(QWidget *window, const QPoint &cursor)
{
    const QRect maximized = window->geometry();
    QRect normal = window->property(NormalGeometryProperty).toRect();
    window->setProperty(MaximizedGeometryProperty, QVariant());
    window->setProperty(NormalGeometryProperty, QVariant());
    if (!normal.isValid() || maximized.width() <= 0) {
        return;
    }
    // İmleç başlıkta aynı oransal konumda kalır (işletim sistemindeki büyütülmüş pencere davranışı).
    const int offsetX = (cursor.x() - maximized.x()) * normal.width() / maximized.width();
    const int offsetY = qMin(cursor.y() - maximized.y(), normal.height() - 1);
    normal.moveTopLeft(QPoint(cursor.x() - offsetX, cursor.y() - offsetY));
    window->setGeometry(normal);
}

bool DockTitleBar::CoversScreen(const QWidget *window)
{
    const QScreen *screen = window->screen();
    if (screen == nullptr) {
        return false;
    }
    const QRect area = screen->availableGeometry();
    return window->width() >= area.width() * 9 / 10 && window->height() >= area.height() * 9 / 10;
}

void DockTitleBar::FitFloatingWindow()
{
    // Sürükleme sürerken (sekmeden ayırma) boyuta dokunulmaz; tam pencere bilerek seçildiyse de.
    if (!m_dock->isFloating() || (QGuiApplication::mouseButtons() & Qt::LeftButton) || IsWindowMaximized(m_dock)
        || !CoversScreen(m_dock)) {
        return;
    }
    // Qt panelin son ayrı pencere boyutunu hatırlar; o sırada tam pencereyse panel ekranı kaplayarak
    // açılır. Ekranın üçte ikisi kadar, ortalanmış bir boyut verilir.
    const QRect area = m_dock->screen()->availableGeometry();
    QSize size = QSize(area.width() * 2 / 3, area.height() * 2 / 3).expandedTo(m_dock->minimumSize());
    QRect fitted(QPoint(0, 0), size.boundedTo(area.size()));
    fitted.moveCenter(area.center());
    m_dock->setGeometry(fitted);
    UpdateButtons();
}

void DockTitleBar::UpdateButtons()
{
    QWidget *window = SeparateWindow();
    if (window == nullptr) {
        // Panel ana pencerede: daha önce ayrı pencereyken kalan tam pencere kaydı silinir.
        m_dock->setProperty(MaximizedGeometryProperty, QVariant());
        m_dock->setProperty(NormalGeometryProperty, QVariant());
    }
    const bool maximized = IsWindowMaximized(window);
    m_maximizeButton->setVisible(!m_collapsed && window != nullptr);
    m_maximizeButton->setToolTip(maximized ? tr("Eski boyutuna döndür") : tr("Tam pencere (ekranı kapla)"));
    m_floatButton->setToolTip(window != nullptr ? tr("Ana pencereye yerleştir") : tr("Ayrı pencereye al"));
    RefreshIcons();
}

void DockTitleBar::RefreshIcons()
{
    QWidget *window = SeparateWindow();
    m_maximizeButton->setIcon(
        Theme::Icon(IsWindowMaximized(window) ? QStringLiteral("restore") : QStringLiteral("maximize")));
    m_floatButton->setIcon(Theme::Icon(window != nullptr ? QStringLiteral("layout") : QStringLiteral("undock")));
    m_closeButton->setIcon(Theme::Icon(QStringLiteral("close")));
}
