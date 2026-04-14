#pragma once

#include <QPalette>
#include <QString>
#include <QStringList>

// ── AppTheme ──────────────────────────────────────────────────────────────────
// All color slots used across the application.  Build via Themes::darkBlue() /
// Themes::blackOrange(), then apply with Themes::buildStylesheet() /
// Themes::buildPalette() and optionally Themes::current() anywhere else.

struct AppTheme {
    QString name;
    // Backgrounds (darkest → lightest)
    QString bg0;            // main window / table background
    QString bg1;            // title bar, input fields, list/combo views
    QString bg2;            // input widget bg (spin box, profile box hover)
    QString bg3;            // group box bg, generic button bg
    QString bg3h;           // hovered element bg
    // Borders
    QString border;         // default border
    QString borderh;        // focused / hover border
    // Accent family
    QString accent;         // primary accent
    QString accent2;        // darker accent (gradient second stop)
    QString accenth;        // lighter accent (hover)
    QString accentbg;       // tinted bg for accent-coloured active buttons
    QString onaccent;       // text drawn *on* a filled accent surface
    // Text
    QString text;           // primary text
    QString textm;          // muted / secondary text
    // Warning (pause / confirm)
    QString warning;
    QString warnbg;
    QString warnborder;
    // Danger (stop / delete / cancel)
    QString danger;
    QString dangerbg;
    QString dangerborder;
    // Disabled state
    QString disabledbg;
    QString disabledtext;
    QString disabledborder;
};

// ── Themes namespace ──────────────────────────────────────────────────────────
namespace Themes {
    const AppTheme& darkBlue();
    const AppTheme& blackOrange();

    QStringList     available();                    // ordered display names
    void            setCurrent(const QString& name);
    const AppTheme& current();                      // returns darkBlue() if unknown

    QString  buildStylesheet(const AppTheme& t);
    QPalette buildPalette   (const AppTheme& t);
}
