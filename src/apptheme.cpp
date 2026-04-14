#include "apptheme.hpp"

#include <QColor>

// ── Theme factory helpers ─────────────────────────────────────────────────────

static AppTheme make_dark_blue()
{
    AppTheme t;
    t.name           = "Dark Blue";
    t.bg0            = "#12121f";
    t.bg1            = "#0d0d1e";
    t.bg2            = "#191928";
    t.bg3            = "#1c1c2e";
    t.bg3h           = "#2a2a45";
    t.border         = "#2a2a45";
    t.borderh        = "#3a3a6a";
    t.accent         = "#00c9a7";
    t.accent2        = "#00a896";
    t.accenth        = "#00e0bb";
    t.accentbg       = "#1a2a1a";
    t.onaccent       = "#0a0a1a";
    t.text           = "#e0e0f0";
    t.textm          = "#7878aa";
    t.warning        = "#c8a020";
    t.warnbg         = "#1a1a2e";
    t.warnborder     = "#4a3a10";
    t.danger         = "#e05252";
    t.dangerbg       = "#1e1220";
    t.dangerborder   = "#5a2a2a";
    t.disabledbg     = "#1e1e35";
    t.disabledtext   = "#4a4a6a";
    t.disabledborder = "#2a2a45";
    return t;
}

static AppTheme make_black_orange()
{
    AppTheme t;
    t.name           = "Black & Orange";
    t.bg0            = "#0a0a0a";
    t.bg1            = "#111111";
    t.bg2            = "#181818";
    t.bg3            = "#1e1e1e";
    t.bg3h           = "#2a2a2a";
    t.border         = "#333333";
    t.borderh        = "#555555";
    t.accent         = "#ff6a00";
    t.accent2        = "#cc5500";
    t.accenth        = "#ff8c40";
    t.accentbg       = "#2a1800";
    t.onaccent       = "#0a0a0a";
    t.text           = "#f0f0f0";
    t.textm          = "#888888";
    t.warning        = "#e0b020";
    t.warnbg         = "#1a1400";
    t.warnborder     = "#4a3a00";
    t.danger         = "#e05252";
    t.dangerbg       = "#1a0a0a";
    t.dangerborder   = "#5a2020";
    t.disabledbg     = "#1a1a1a";
    t.disabledtext   = "#4a4a4a";
    t.disabledborder = "#333333";
    return t;
}

// ── Global current theme ──────────────────────────────────────────────────────

static AppTheme s_current = make_dark_blue();

// ── Themes namespace ──────────────────────────────────────────────────────────

const AppTheme& Themes::darkBlue()
{
    static AppTheme t = make_dark_blue();
    return t;
}

const AppTheme& Themes::blackOrange()
{
    static AppTheme t = make_black_orange();
    return t;
}

QStringList Themes::available()
{
    return { darkBlue().name, blackOrange().name };
}

void Themes::setCurrent(const QString& name)
{
    if      (name == darkBlue().name)    s_current = darkBlue();
    else if (name == blackOrange().name) s_current = blackOrange();
    else                                 s_current = darkBlue();
}

const AppTheme& Themes::current()
{
    return s_current;
}

// ── QSS template ─────────────────────────────────────────────────────────────
// Placeholders are substituted by buildStylesheet().  Ordering: longer/more-
// specific names first so partial-match collisions cannot occur.

static const char* kQssTemplate = R"(

/* === Central widget === */
QWidget#CW {
    background: $BG0;
    border: 1px solid $BORDER;
    border-radius: 10px;
}

/* === Title bar === */
QWidget#TB {
    background: $BG1;
    border-top-left-radius: 10px;
    border-top-right-radius: 10px;
    border-bottom: 1px solid $BORDER;
}

/* === Profile box === */
QWidget#ProfileBox {
    background: $BG1;
    border: 1px solid $BORDER;
    border-radius: 6px;
}
QWidget#ProfileBox:hover {
    border-color: $ACCENT;
    background: $BG2;
}

/* === Labels (base) === */
QLabel { background: transparent; color: $TEXT; }
QLabel#TitleLabel {
    font-size: 13px; font-weight: bold;
    color: $TEXT; background: transparent;
}
QLabel#StatusLabel  { font-size: 13px; color: $TEXT;  background: transparent; }
QLabel#StatsLabel   { color: $TEXTM;   font-size: 11px; background: transparent; }
QLabel#ProfileDisplayLabel { color: $TEXTM; font-size: 10px; background: transparent; }
QLabel#TouchpadKeyLabel    { color: $ACCENT; font-size: 12px; background: transparent; }

/* === Group boxes === */
QGroupBox {
    background-color: $BG3;
    border: 1px solid $BORDER;
    border-radius: 10px;
    margin-top: 20px;
    padding: 14px 10px 10px 10px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 14px;
    padding: 2px 8px;
    color: $ACCENT;
    font-size: 10px;
    font-weight: bold;
    letter-spacing: 1px;
}

/* === Check boxes === */
QCheckBox { spacing: 8px; color: $TEXT; }
QCheckBox::indicator {
    width: 16px; height: 16px;
    border: 1.5px solid $BORDERH;
    border-radius: 4px;
    background: $BG2;
}
QCheckBox::indicator:checked { background-color: $ACCENT; border-color: $ACCENT; }
QCheckBox::indicator:hover   { border-color: $ACCENT; }

/* === Radio buttons === */
QRadioButton { spacing: 6px; color: $TEXT; }
QRadioButton::indicator {
    width: 14px; height: 14px;
    border: 1.5px solid $BORDERH;
    border-radius: 7px;
    background: $BG2;
}
QRadioButton::indicator:checked { background-color: $ACCENT; border-color: $ACCENT; }
QRadioButton::indicator:hover   { border-color: $ACCENT; }

/* === Spin boxes === */
QDoubleSpinBox {
    background: $BG2;
    color: $TEXT;
    border: 1px solid $BORDER;
    border-radius: 4px;
    padding: 2px 4px;
}
QDoubleSpinBox:focus { border-color: $ACCENT; }
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    background: $BG3;
    border: none;
    width: 16px;
}

/* === Line edits === */
QLineEdit {
    background: $BG1;
    color: $TEXT;
    border: 1px solid $BORDER;
    border-radius: 4px;
    padding: 3px 8px;
    font-size: 12px;
}
QLineEdit:focus { border-color: $ACCENT; }

/* === Combo boxes === */
QComboBox {
    background: $BG1;
    color: $TEXT;
    border: 1px solid $BORDER;
    border-radius: 4px;
    padding: 3px 8px;
    font-size: 12px;
    min-height: 22px;
}
QComboBox:focus { border-color: $ACCENT; }
QComboBox::drop-down { border: none; }
QComboBox QAbstractItemView {
    background: $BG1;
    color: $TEXT;
    border: 1px solid $BORDER;
    selection-background-color: $BG3H;
}

/* === List widget === */
QListWidget {
    background: $BG1;
    color: $TEXT;
    border: 1px solid $BORDER;
    border-radius: 4px;
    font-size: 12px;
}
QListWidget::item:selected     { background: $BG3; }
QListWidget::indicator:checked { color: $ACCENT; }

/* === Table widget === */
QTableWidget {
    background: $BG0;
    alternate-background-color: $BG3;
    border: none;
    gridline-color: $BG3;
}
QTableWidget::item          { padding: 4px; color: $TEXT; }
QTableWidget::item:selected { background: $BG3H; }
QHeaderView::section {
    background: $BG1;
    color: $TEXTM;
    border: none;
    padding: 4px;
    font-size: 10px;
    border-bottom: 1px solid $BORDER;
}

/* === Scroll bars === */
QScrollBar:vertical {
    width: 6px;
    background: $BG2;
    border-radius: 3px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: $BG3H;
    border-radius: 3px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover        { background: $ACCENT; }
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical            { height: 0; }

QScrollBar:horizontal {
    height: 8px;
    background: $BG1;
    border: none;
    border-radius: 4px;
}
QScrollBar::handle:horizontal {
    background: $BORDER;
    border-radius: 4px;
    min-width: 20px;
}
QScrollBar::add-line:horizontal,
QScrollBar::sub-line:horizontal          { width: 0; }

/* === Scroll area === */
QScrollArea {
    background: $BG1;
    border: 1px solid $BORDER;
    border-radius: 4px;
}

/* === Tab widget === */
QTabWidget::pane {
    border: 1px solid $BORDER;
    background: $BG0;
    border-radius: 0 4px 4px 4px;
}
QTabBar::tab {
    background: $BG2;
    color: $TEXTM;
    padding: 6px 16px;
    border: 1px solid $BORDER;
    border-bottom: none;
    border-radius: 4px 4px 0 0;
    margin-right: 2px;
}
QTabBar::tab:selected { background: $BG0; color: $TEXT; }
QTabBar::tab:hover    { color: $TEXT; }

/* === Menus === */
QMenu {
    background-color: $BG3;
    border: 1px solid $BORDER;
    border-radius: 6px;
    padding: 4px;
}
QMenu::item                { padding: 6px 20px; border-radius: 4px; color: $TEXT; }
QMenu::item:selected       { background: $BG3H; color: $ACCENT; }
QMenu::item:checked        { color: $ACCENT; font-weight: bold; }

/* === Tooltips === */
QToolTip {
    background-color: $BG3;
    color: $TEXT;
    border: 1px solid $BORDER;
    border-radius: 4px;
    padding: 4px 8px;
}

/* === Dialogs === */
QDialog { background: $BG0; }

/* === Push buttons — generic fallback === */
QPushButton {
    background: $BG3;
    color: $TEXT;
    border: 1px solid $BORDER;
    border-radius: 6px;
    padding: 4px 12px;
}
QPushButton:hover    { background: $BG3H; }
QPushButton:disabled {
    background: $DISABLEDBG;
    color: $DISABLEDTEXT;
    border-color: $DISABLEDBORDER;
}

/* === Title-bar buttons === */
QPushButton#MinBtn {
    background: transparent; color: $TEXTM;
    border: none; font-size: 14px; border-radius: 4px;
}
QPushButton#MinBtn:hover   { background: $BG3H; color: $TEXT; }
QPushButton#CloseBtn {
    background: transparent; color: $TEXTM;
    border: none; font-size: 11px; border-radius: 4px;
}
QPushButton#CloseBtn:hover { background: #c0392b; color: #ffffff; }

/* === Start / Pause / Stop === */
QPushButton#StartBtn {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
        stop:0 $ACCENT, stop:1 $ACCENT2);
    color: $ONACCENT; border: none; border-radius: 8px;
    font-weight: bold; font-size: 13px;
}
QPushButton#StartBtn:hover {
    background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
        stop:0 $ACCENTH, stop:1 $ACCENT);
}
QPushButton#StartBtn:disabled {
    background: $DISABLEDBG; color: $DISABLEDTEXT; border: none;
}

QPushButton#PauseBtn {
    background: $WARNBG; color: $WARNING;
    border: 1px solid $WARNBORDER;
    border-radius: 8px; font-weight: bold; font-size: 13px;
}
QPushButton#PauseBtn:hover    { background: $WARNING; color: $ONACCENT; border-color: $WARNING; }
QPushButton#PauseBtn:disabled {
    background: $DISABLEDBG; color: $DISABLEDTEXT; border-color: $DISABLEDBORDER;
}

QPushButton#StopBtn {
    background: $DANGERBG; color: $DANGER;
    border: 1px solid $DANGERBORDER;
    border-radius: 8px; font-weight: bold; font-size: 13px;
}
QPushButton#StopBtn:hover    { background: $DANGER; color: #ffffff; border-color: $DANGER; }
QPushButton#StopBtn:disabled {
    background: $DISABLEDBG; color: $DISABLEDTEXT; border-color: $DISABLEDBORDER;
}

/* === Accent outline button (Load, etc.) === */
QPushButton#AccentBtn {
    background: $BG3; color: $ACCENT;
    border: 1px solid $BORDER; border-radius: 6px;
}
QPushButton#AccentBtn:hover    { background: $ACCENT; color: $ONACCENT; }
QPushButton#AccentBtn:disabled {
    background: $DISABLEDBG; color: $DISABLEDTEXT; border-color: $DISABLEDBORDER;
}

/* === Neutral button (Refresh, Save As, Set Key, …) === */
QPushButton#NeutralBtn {
    background: $BG3; color: $TEXTM;
    border: 1px solid $BORDER; border-radius: 6px;
}
QPushButton#NeutralBtn:hover { background: $BG3H; color: $TEXT; }

/* === Danger button (Delete, Clear, Cancel, …) === */
QPushButton#DangerBtn {
    background: $DANGERBG; color: $DANGER;
    border: 1px solid $DANGERBORDER; border-radius: 6px;
}
QPushButton#DangerBtn:hover    { background: $DANGER; color: #ffffff; }
QPushButton#DangerBtn:disabled {
    background: $DISABLEDBG; color: $DISABLEDTEXT; border-color: $DISABLEDBORDER;
}

/* === Add mapping button (accent fill) === */
QPushButton#AddMappingBtn {
    background: $ACCENT; color: $ONACCENT;
    border: none; border-radius: 6px;
    font-weight: bold; font-size: 12px;
}
QPushButton#AddMappingBtn:hover    { background: $ACCENTH; }
QPushButton#AddMappingBtn:disabled { background: $DISABLEDBG; color: $DISABLEDTEXT; }

/* === Save mapping button (disabled appearance; accent when dirty/enabled) === */
QPushButton#SaveMappingBtn {
    background: $DISABLEDBG; color: $DISABLEDTEXT;
    border: 1px solid $DISABLEDBORDER; border-radius: 6px; font-size: 12px;
}
QPushButton#SaveMappingBtn:enabled {
    background: $ACCENTBG; color: $ACCENT; border-color: $ACCENT;
}
QPushButton#SaveMappingBtn:enabled:hover { background: $ACCENT; color: $ONACCENT; }

/* === OK button in dialogs === */
QPushButton#OkBtn {
    background: $ACCENT; color: $ONACCENT;
    border: none; border-radius: 6px;
    font-weight: bold; padding: 4px 16px;
}
QPushButton#OkBtn:hover { background: $ACCENTH; }

)";

// ── Substitution helper ───────────────────────────────────────────────────────
// Replace longer / more-specific placeholders first to avoid partial collisions
// (e.g. $BG3H must be substituted before $BG3, $ACCENTH before $ACCENT, etc.)

QString Themes::buildStylesheet(const AppTheme& t)
{
    QString css = QString::fromLatin1(kQssTemplate);

    // Backgrounds (longer names first)
    css.replace("$BG3H", t.bg3h);
    css.replace("$BG3",  t.bg3);
    css.replace("$BG2",  t.bg2);
    css.replace("$BG1",  t.bg1);
    css.replace("$BG0",  t.bg0);

    // Borders (longer first)
    css.replace("$BORDERH", t.borderh);
    css.replace("$BORDER",  t.border);

    // Accent family (longer first)
    css.replace("$ACCENTBG",  t.accentbg);
    css.replace("$ACCENTH",   t.accenth);
    css.replace("$ACCENT2",   t.accent2);
    css.replace("$ACCENT",    t.accent);
    css.replace("$ONACCENT",  t.onaccent);

    // Text (longer first)
    css.replace("$TEXTM", t.textm);
    css.replace("$TEXT",  t.text);

    // Warning (longer first)
    css.replace("$WARNBORDER", t.warnborder);
    css.replace("$WARNBG",     t.warnbg);
    css.replace("$WARNING",    t.warning);

    // Danger (longer first)
    css.replace("$DANGERBORDER", t.dangerborder);
    css.replace("$DANGERBG",     t.dangerbg);
    css.replace("$DANGER",       t.danger);

    // Disabled (longer first)
    css.replace("$DISABLEDBORDER", t.disabledborder);
    css.replace("$DISABLEDTEXT",   t.disabledtext);
    css.replace("$DISABLEDBG",     t.disabledbg);

    return css;
}

// ── Palette builder ───────────────────────────────────────────────────────────

QPalette Themes::buildPalette(const AppTheme& t)
{
    QPalette p;
    p.setColor(QPalette::Window,          QColor(t.bg0));
    p.setColor(QPalette::WindowText,      QColor(t.text));
    p.setColor(QPalette::Base,            QColor(t.bg2));
    p.setColor(QPalette::AlternateBase,   QColor(t.bg3));
    p.setColor(QPalette::ToolTipBase,     QColor(t.bg3));
    p.setColor(QPalette::ToolTipText,     QColor(t.text));
    p.setColor(QPalette::Text,            QColor(t.text));
    p.setColor(QPalette::Button,          QColor(t.bg3));
    p.setColor(QPalette::ButtonText,      QColor(t.text));
    p.setColor(QPalette::BrightText,      QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Link,            QColor(t.accent));
    p.setColor(QPalette::Highlight,       QColor(t.accent));
    p.setColor(QPalette::HighlightedText, QColor(t.onaccent));
    p.setColor(QPalette::Mid,             QColor(t.bg3h));
    p.setColor(QPalette::Dark,            QColor(t.bg3));
    p.setColor(QPalette::Shadow,          QColor(t.bg1));
    return p;
}
