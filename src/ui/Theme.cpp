#include "Theme.h"

#include <QApplication>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QStyle>
#include <QStyleFactory>
#include <QWidget>

namespace Theme {

namespace {

// Açık mod: fildişi zemin, koyu sıcak gri metin, terracotta vurgu.
Palette LightPalette()
{
    Palette p;
    p.window = QColor(0xFAF9F5);
    p.surface = QColor(0xF3F1EA);
    p.base = QColor(0xFFFFFF);
    p.alternateBase = QColor(0xFBFAF6);
    p.border = QColor(0xE3E0D6);
    p.text = QColor(0x141413);
    p.textSecondary = QColor(0x4A4945);
    p.textMuted = QColor(0x8A8880);
    p.accent = QColor(0xD97757);
    p.accentHover = QColor(0xC6613F);
    p.accentPressed = QColor(0xB05730);
    p.accentSoft = QColor(0xF6E3D9);
    p.onAccent = QColor(0xFFFFFF);
    p.success = QColor(0x5B8C51);
    p.danger = QColor(0xBF4D43);
    p.warning = QColor(0xB8891A);
    return p;
}

// Koyu mod: sıcak kömür zemin, krem metin, aynı terracotta vurgu.
Palette DarkPalette()
{
    Palette p;
    p.window = QColor(0x262624);
    p.surface = QColor(0x1F1E1D);
    p.base = QColor(0x2C2B28);
    p.alternateBase = QColor(0x302F2C);
    p.border = QColor(0x3E3D39);
    p.text = QColor(0xF5F4ED);
    p.textSecondary = QColor(0xC9C6BC);
    p.textMuted = QColor(0x8E8B82);
    p.accent = QColor(0xD97757);
    p.accentHover = QColor(0xE38B6D);
    p.accentPressed = QColor(0xC6613F);
    p.accentSoft = QColor(0x4A3128);
    p.onAccent = QColor(0xFFFFFF);
    p.success = QColor(0x82B36F);
    p.danger = QColor(0xE07A6F);
    p.warning = QColor(0xE0B84F);
    return p;
}

Mode g_mode = Mode::Light;
Palette g_palette = LightPalette();

// Stil sayfası şablonu; @token@ yer tutucuları renklerle değiştirilir.
const char *StyleSheetTemplate = R"QSS(
QWidget {
    color: @text@;
}
QMainWindow, QDialog {
    background: @window@;
}
QToolTip {
    background: @surface@;
    color: @text@;
    border: 1px solid @border@;
    padding: 4px 6px;
}

/* --- Menü ve araç çubuğu --- */
QMenuBar {
    background: @surface@;
    border-bottom: 1px solid @border@;
    padding: 2px 6px;
}
QMenuBar::item {
    padding: 5px 10px;
    border-radius: 6px;
    background: transparent;
}
QMenuBar::item:selected {
    background: @accentSoft@;
}
QMenu {
    background: @base@;
    border: 1px solid @border@;
    border-radius: 8px;
    padding: 4px;
}
QMenu::item {
    padding: 6px 24px 6px 14px;
    border-radius: 6px;
}
QMenu::item:selected {
    background: @accentSoft@;
    color: @text@;
}
QMenu::separator {
    height: 1px;
    background: @border@;
    margin: 4px 8px;
}
QToolBar {
    background: @surface@;
    border: none;
    border-bottom: 1px solid @border@;
    padding: 6px 10px;
    spacing: 6px;
}
QToolBar::separator {
    width: 1px;
    background: @border@;
    margin: 4px 8px;
}
QToolBar QLabel {
    color: @textSecondary@;
}
QLabel#BrandLabel {
    font-family: "Georgia", "Cambria", serif;
    font-size: 15pt;
    color: @text@;
    padding: 0 10px 0 2px;
}
QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 8px;
    padding: 6px 12px;
    color: @textSecondary@;
}
QToolButton:hover {
    background: @accentSoft@;
    color: @text@;
}
QToolButton:pressed {
    background: @border@;
}
QToolButton#PrimaryAction {
    background: @accent@;
    color: @onAccent@;
    font-weight: 600;
    padding: 6px 18px;
}
QToolButton#PrimaryAction:hover {
    background: @accentHover@;
}
QToolButton#PrimaryAction:pressed {
    background: @accentPressed@;
}
QToolButton#PrimaryAction[connected="true"] {
    background: transparent;
    color: @danger@;
    border: 1px solid @danger@;
}
QToolButton#PrimaryAction[connected="true"]:hover {
    background: @accentSoft@;
}

/* --- Düğmeler ve girişler --- */
QPushButton {
    background: @base@;
    border: 1px solid @border@;
    border-radius: 8px;
    padding: 6px 14px;
    color: @text@;
}
QPushButton:hover {
    border-color: @accent@;
    color: @accent@;
}
QPushButton:pressed {
    background: @accentSoft@;
}
QPushButton:disabled {
    color: @textMuted@;
    border-color: @border@;
    background: @window@;
}
QPushButton[primary="true"] {
    background: @accent@;
    border: 1px solid @accent@;
    color: @onAccent@;
    font-weight: 600;
}
QPushButton[primary="true"]:hover {
    background: @accentHover@;
    border-color: @accentHover@;
    color: @onAccent@;
}
QPushButton[primary="true"]:pressed {
    background: @accentPressed@;
}
QPushButton[primary="true"]:disabled {
    background: @border@;
    border-color: @border@;
    color: @textMuted@;
}
QPushButton:checked {
    background: @accentSoft@;
    border-color: @accent@;
    color: @accent@;
}
QLineEdit, QSpinBox, QComboBox, QPlainTextEdit {
    background: @base@;
    border: 1px solid @border@;
    border-radius: 8px;
    padding: 5px 8px;
    selection-background-color: @accent@;
    selection-color: @onAccent@;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus {
    border: 1px solid @accent@;
}
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled {
    background: @window@;
    color: @textMuted@;
}
QComboBox {
    padding-right: 28px;
}
QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: center right;
    border: none;
    width: 26px;
}
QComboBox::down-arrow {
    image: url(:/icons/chevron-down-@themeName@.svg);
    width: 12px;
    height: 12px;
}
QComboBox::down-arrow:disabled {
    image: url(:/icons/chevron-down-muted.svg);
}
QComboBox::down-arrow:on {
    image: url(:/icons/chevron-up-@themeName@.svg);
}
QComboBox QAbstractItemView {
    background: @base@;
    border: 1px solid @border@;
    selection-background-color: @accentSoft@;
    selection-color: @text@;
    outline: 0;
}
QSpinBox {
    padding-right: 22px;
}
QSpinBox::up-button, QSpinBox::down-button {
    subcontrol-origin: border;
    border: none;
    width: 20px;
}
QSpinBox::up-button {
    subcontrol-position: top right;
}
QSpinBox::down-button {
    subcontrol-position: bottom right;
}
QSpinBox::up-button:hover, QSpinBox::down-button:hover {
    background: @accentSoft@;
    border-radius: 4px;
}
QSpinBox::up-arrow {
    image: url(:/icons/chevron-up-@themeName@.svg);
    width: 9px;
    height: 9px;
}
QSpinBox::down-arrow {
    image: url(:/icons/chevron-down-@themeName@.svg);
    width: 9px;
    height: 9px;
}
QSpinBox::up-arrow:disabled, QSpinBox::up-arrow:off {
    image: url(:/icons/chevron-up-muted.svg);
}
QSpinBox::down-arrow:disabled, QSpinBox::down-arrow:off {
    image: url(:/icons/chevron-down-muted.svg);
}

/* --- Sekmeler --- */
QTabWidget::pane {
    border: none;
    border-top: 1px solid @border@;
    top: -1px;
}
QTabBar {
    background: transparent;
}
QTabBar::tab {
    background: transparent;
    color: @textMuted@;
    padding: 10px 20px;
    border: none;
    border-bottom: 2px solid transparent;
    font-weight: 600;
}
QTabBar::tab:hover:!selected {
    color: @textSecondary@;
}
QTabBar::tab:selected {
    color: @text@;
    border-bottom: 2px solid @accent@;
}

/* --- Tablolar ve ağaçlar --- */
QTableView, QTreeView, QTableWidget {
    background: @base@;
    alternate-background-color: @alternateBase@;
    border: 1px solid @border@;
    border-radius: 10px;
    gridline-color: @border@;
    selection-background-color: @accentSoft@;
    selection-color: @text@;
    outline: 0;
}
QTableView::item, QTreeView::item {
    padding: 0 6px;
}
QTreeView::item:hover, QTableView::item:hover {
    background: @alternateBase@;
}
QTreeView::item:selected, QTableView::item:selected {
    background: @accentSoft@;
    color: @text@;
}
QHeaderView {
    background: transparent;
}
QHeaderView::section {
    background: @surface@;
    color: @textSecondary@;
    border: none;
    border-bottom: 1px solid @border@;
    border-right: 1px solid @border@;
    padding: 6px 8px;
    font-weight: 600;
}
QHeaderView::section:last {
    border-right: none;
}
QTableCornerButton::section {
    background: @surface@;
    border: none;
}

QTreeView::item {
    padding: 3px 4px;
}

/* --- Onay kutuları --- */
QCheckBox {
    spacing: 8px;
}
QCheckBox::indicator, QTableView::indicator, QTreeView::indicator {
    width: 15px;
    height: 15px;
    border: 1px solid @textMuted@;
    border-radius: 4px;
    background: @base@;
}
QCheckBox::indicator:hover, QTableView::indicator:hover {
    border-color: @accent@;
}
QCheckBox::indicator:checked, QTableView::indicator:checked, QTreeView::indicator:checked {
    background: @accent@;
    border-color: @accent@;
    image: url(:/icons/check.svg);
}
QCheckBox::indicator:disabled {
    background: @window@;
    border-color: @border@;
}
QRadioButton {
    spacing: 8px;
}
QRadioButton::indicator {
    width: 14px;
    height: 14px;
    border: 1px solid @textMuted@;
    border-radius: 8px;
    background: @base@;
}
QRadioButton::indicator:hover {
    border-color: @accent@;
}
QRadioButton::indicator:checked {
    width: 8px;
    height: 8px;
    border: 4px solid @accent@;
    background: @base@;
}
QRadioButton::indicator:disabled {
    background: @window@;
    border-color: @border@;
}
QRadioButton:disabled {
    color: @textMuted@;
}

/* --- Gruplar, paneller --- */
QGroupBox {
    background: @surface@;
    border: 1px solid @border@;
    border-radius: 10px;
    margin-top: 16px;
    padding: 14px 10px 10px 10px;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 4px;
    color: @textSecondary@;
}
QGroupBox QLabel, QGroupBox QCheckBox {
    font-weight: normal;
}
QDockWidget::title {
    background: @surface@;
    padding: 7px 10px;
    border-bottom: 1px solid @border@;
    font-weight: 600;
}
QMainWindow::separator {
    background: @border@;
    width: 4px;
    height: 4px;
}
QMainWindow::separator:hover {
    background: @accent@;
}
QMainWindow > QTabBar {
    background: @surface@;
    border-bottom: 1px solid @border@;
}
QMainWindow > QTabBar::tab {
    padding: 8px 18px;
}
QDockWidget > QWidget {
    background: @window@;
}
QSplitter::handle {
    background: transparent;
}
QSplitter::handle:hover {
    background: @accentSoft@;
}

/* --- Durum çubuğu --- */
QStatusBar {
    background: @surface@;
    border-top: 1px solid @border@;
    color: @textSecondary@;
}
QStatusBar::item {
    border: none;
}
QStatusBar QLabel {
    color: @textSecondary@;
    padding: 2px 10px;
}

/* --- Kaydırma çubukları --- */
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
    background: @border@;
    border-radius: 4px;
    min-height: 30px;
    min-width: 30px;
}
QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {
    background: @textMuted@;
}
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0;
    height: 0;
}
QScrollBar::add-page, QScrollBar::sub-page {
    background: transparent;
}

/* --- Metin rolleri --- */
QLabel#SectionTitle {
    font-family: "Georgia", "Cambria", serif;
    font-size: 12pt;
    color: @text@;
}
QLabel#MutedText {
    color: @textMuted@;
}
QLabel#ErrorText {
    color: @danger@;
}
QLabel#CounterBadge {
    background: @accentSoft@;
    color: @accent@;
    border-radius: 9px;
    padding: 1px 9px;
    font-weight: 600;
}
)QSS";

// Şablondaki renk tokenlarını paletteki değerlerle değiştirir.
QString BuildStyleSheet(const Palette &p)
{
    const QHash<QString, QColor> tokens = {
        {QStringLiteral("window"), p.window},
        {QStringLiteral("surface"), p.surface},
        {QStringLiteral("base"), p.base},
        {QStringLiteral("alternateBase"), p.alternateBase},
        {QStringLiteral("border"), p.border},
        {QStringLiteral("text"), p.text},
        {QStringLiteral("textSecondary"), p.textSecondary},
        {QStringLiteral("textMuted"), p.textMuted},
        {QStringLiteral("accent"), p.accent},
        {QStringLiteral("accentHover"), p.accentHover},
        {QStringLiteral("accentPressed"), p.accentPressed},
        {QStringLiteral("accentSoft"), p.accentSoft},
        {QStringLiteral("onAccent"), p.onAccent},
        {QStringLiteral("success"), p.success},
        {QStringLiteral("danger"), p.danger},
        {QStringLiteral("warning"), p.warning},
    };

    QString sheet = QString::fromUtf8(StyleSheetTemplate);
    sheet.replace(QStringLiteral("@themeName@"), g_mode == Mode::Dark ? QStringLiteral("dark") : QStringLiteral("light"));
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
        sheet.replace(QLatin1Char('@') + it.key() + QLatin1Char('@'), it.value().name());
    }
    return sheet;
}

// Fusion stilinin kullandığı QPalette'i tema renkleriyle doldurur.
QPalette BuildQPalette(const Palette &p)
{
    QPalette palette;
    palette.setColor(QPalette::Window, p.window);
    palette.setColor(QPalette::WindowText, p.text);
    palette.setColor(QPalette::Base, p.base);
    palette.setColor(QPalette::AlternateBase, p.alternateBase);
    palette.setColor(QPalette::Text, p.text);
    palette.setColor(QPalette::Button, p.base);
    palette.setColor(QPalette::ButtonText, p.text);
    palette.setColor(QPalette::BrightText, p.onAccent);
    palette.setColor(QPalette::Highlight, p.accent);
    palette.setColor(QPalette::HighlightedText, p.onAccent);
    palette.setColor(QPalette::ToolTipBase, p.surface);
    palette.setColor(QPalette::ToolTipText, p.text);
    palette.setColor(QPalette::PlaceholderText, p.textMuted);
    palette.setColor(QPalette::Link, p.accent);
    palette.setColor(QPalette::LinkVisited, p.accentPressed);
    palette.setColor(QPalette::Light, p.base);
    palette.setColor(QPalette::Midlight, p.surface);
    palette.setColor(QPalette::Mid, p.border);
    palette.setColor(QPalette::Dark, p.border.darker(120));
    palette.setColor(QPalette::Shadow, p.border.darker(150));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, p.textMuted);
    palette.setColor(QPalette::Disabled, QPalette::Text, p.textMuted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, p.textMuted);
    return palette;
}

// Renklendirilmiş ikonların önbelleği (anahtar: ad + renk); tema değişince temizlenir.
QHash<QString, QIcon> &IconCache()
{
    static QHash<QString, QIcon> cache;
    return cache;
}

} // namespace

void Apply(QApplication &app, Mode mode)
{
    g_mode = mode;
    g_palette = mode == Mode::Dark ? DarkPalette() : LightPalette();

    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        QApplication::setStyle(fusion);
    }

    QFont font = QApplication::font();
    font.setFamilies({QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"), QStringLiteral("Noto Sans")});
    font.setPointSize(10);
    QApplication::setFont(font);

    QApplication::setPalette(BuildQPalette(g_palette));
    app.setStyleSheet(BuildStyleSheet(g_palette));
    IconCache().clear();
}

Mode CurrentMode()
{
    return g_mode;
}

const Palette &Colors()
{
    return g_palette;
}

QFont MonospaceFont()
{
    QFont font;
    font.setFamilies({QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans Mono"),
                      QStringLiteral("Courier New")});
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(10);
    return font;
}

QIcon CreateAppIcon()
{
    QIcon icon;
    const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    for (const int size : sizes) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        const qreal radius = size * 0.22;
        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, size, size), radius, radius);
        painter.fillPath(path, QColor(0xD97757));

        // İki CAN hattını (CAN_H / CAN_L) simgeleyen dalga çizgileri
        QPen pen(QColor(0xFAF9F5));
        pen.setWidthF(qMax(1.5, size * 0.075));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        const qreal left = size * 0.2;
        const qreal right = size * 0.8;
        const qreal step = (right - left) / 4.0;
        for (int line = 0; line < 2; ++line) {
            const qreal high = size * (line == 0 ? 0.30 : 0.56);
            const qreal low = high + size * 0.14;
            QPainterPath wave;
            wave.moveTo(left, low);
            wave.lineTo(left + step, low);
            wave.lineTo(left + step, high);
            wave.lineTo(left + 2 * step, high);
            wave.lineTo(left + 2 * step, low);
            wave.lineTo(left + 3 * step, low);
            wave.lineTo(left + 3 * step, high);
            wave.lineTo(right, high);
            painter.drawPath(wave);
        }
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

QIcon Icon(const QString &name, const QColor &color)
{
    const QColor effective = color.isValid() ? color : g_palette.textSecondary;
    const QString key = name + QLatin1Char('|') + effective.name();
    QHash<QString, QIcon> &cache = IconCache();
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    QIcon icon;
    QFile file(QStringLiteral(":/icons/%1.svg").arg(name));
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray svg = file.readAll();
        svg.replace("currentColor", effective.name().toLatin1());
        QPixmap pixmap;
        // SVG, Qt'nin "svg" resim biçimi eklentisiyle (qsvg) çizilir; ek modül bağlamak gerekmez.
        if (pixmap.loadFromData(svg, "SVG")) {
            icon.addPixmap(pixmap);
        }
    }
    cache.insert(key, icon);
    return icon;
}

void Repolish(QWidget *widget)
{
    if (widget == nullptr) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QString StatusDotText(const QColor &color, const QString &text)
{
    return QStringLiteral("<span style=\"color:%1;\">&#9679;</span>&nbsp;%2").arg(color.name(), text.toHtmlEscaped());
}

} // namespace Theme
