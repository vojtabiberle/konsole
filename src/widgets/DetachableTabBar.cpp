/*
    SPDX-FileCopyrightText: 2018 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DetachableTabBar.h"
#include "KonsoleSettings.h"
#include "widgets/ViewContainer.h"

#include <QApplication>
#include <QMimeData>
#include <QMouseEvent>

#include <KAcceleratorManager>

#include <QColor>
#include <QPainter>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionTab>

namespace Konsole
{
namespace
{
bool isVertical(QTabBar::Shape shape)
{
    return shape == QTabBar::RoundedWest || shape == QTabBar::RoundedEast || shape == QTabBar::TriangularWest || shape == QTabBar::TriangularEast;
}

class TabBarStyle : public QProxyStyle
{
public:
    TabBarStyle()
        : QProxyStyle(qApp->style()->objectName())
    {
        qApp->installEventFilter(this);
    }

    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::StyleChange && baseStyle()->objectName() != qApp->style()->objectName()) {
            if (auto *newStyle = QStyleFactory::create(qApp->style()->objectName())) {
                setBaseStyle(newStyle);
                QEvent styleChange(QEvent::StyleChange);
                QApplication::sendEvent(parent(), &styleChange);
            }
        }
        return QProxyStyle::eventFilter(object, event);
    }

    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const override
    {
        const auto *bar = qobject_cast<const QTabBar *>(widget);
        if (element == CE_TabBarTab && bar && isVertical(bar->shape())) {
            // Horizontal tab styles may paint into the page below the tab.
            // In a sidebar that area belongs to the next row instead.
            painter->save();
            painter->setClipRect(option->rect, Qt::IntersectClip);
            QProxyStyle::drawControl(element, option, painter, widget);
            painter->restore();
            return;
        }
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    QRect subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const override
    {
        const auto *bar = qobject_cast<const QTabBar *>(widget);
        const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
        if (bar && isVertical(bar->shape()) && tab && (element == SE_TabBarTabLeftButton || element == SE_TabBarTabRightButton)) {
            // Some styles assume horizontal tabs always start at y == 0.
            QStyleOptionTab localOption(*tab);
            localOption.rect.moveTopLeft(QPoint());
            return QProxyStyle::subElementRect(element, &localOption, widget).translated(tab->rect.topLeft());
        }
        return QProxyStyle::subElementRect(element, option, widget);
    }
};
}

DetachableTabBar::DetachableTabBar(QWidget *parent)
    : QTabBar(parent)
    , dragType(DragType::NONE)
    , _originalCursor(cursor())
    , tabId(-1)
    , _activityColor(QColor::Invalid)
{
    auto *tabStyle = new TabBarStyle;
    tabStyle->setParent(this);
    setStyle(tabStyle);
    setAcceptDrops(true);
    setElideMode(Qt::TextElideMode::ElideLeft);
    KAcceleratorManager::setNoAccel(this);
}

QSize DetachableTabBar::tabSizeHint(int index) const
{
    if (!isVertical(shape())) {
        return QTabBar::tabSizeHint(index);
    }

    QStyleOptionTab option;
    initStyleOption(&option, index);
    QSize size = fontMetrics().size(Qt::TextShowMnemonic, tabText(index));
    if (!option.icon.isNull()) {
        size.rwidth() += option.iconSize.width() + 4;
        size.setHeight(qMax(size.height(), option.iconSize.height()));
    }
    for (const QSize &buttonSize : {option.leftButtonSize, option.rightButtonSize}) {
        if (!buttonSize.isEmpty()) {
            size.rwidth() += buttonSize.width() + 4;
            size.setHeight(qMax(size.height(), buttonSize.height()));
        }
    }
    size.rwidth() += style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &option, this);
    size.rheight() += style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &option, this);
    // Keep long session titles from consuming the terminal's width. Apply the
    // style afterwards so explicit stylesheet dimensions are still respected.
    size.setWidth(qMin(size.width(), fontMetrics().averageCharWidth() * 30));
    return style()->sizeFromContents(QStyle::CT_TabBarTab, &option, size, this);
}

void DetachableTabBar::initStyleOption(QStyleOptionTab *option, int index) const
{
    QTabBar::initStyleOption(option, index);
    if (option == nullptr || !isVertical(shape())) {
        return;
    }

    // Keep QTabBar's vertical layout, scrolling and dragging, but let the style
    // lay out labels, icons and tab buttons as a horizontal row.
    option->shape = shape() == TriangularWest || shape() == TriangularEast ? TriangularNorth : RoundedNorth;
    const QRect textRect = style()->subElementRect(QStyle::SE_TabBarTabText, option, this);
    option->text = fontMetrics().elidedText(tabText(index), elideMode(), textRect.width(), Qt::TextShowMnemonic);
}

void DetachableTabBar::setColor(int idx, const QColor &color)
{
    DetachableTabData data = tabData(idx).value<DetachableTabData>();
    if (data.color != color) {
        data.color = color;
        setDetachableTabData(idx, data);
        update(tabRect(idx));
    }
}

void DetachableTabBar::setActivityColor(int idx, const QColor &color)
{
    Q_UNUSED(idx)
    _activityColor = color;
    update();
}

void DetachableTabBar::removeColor(int idx)
{
    DetachableTabData data = tabData(idx).value<DetachableTabData>();
    if (data.color.isValid()) {
        data.color = QColor();
        setDetachableTabData(idx, data);
        update(tabRect(idx));
    }
}

void DetachableTabBar::setProgress(int idx, const std::optional<int> &progress)
{
    DetachableTabData data = tabData(idx).value<DetachableTabData>();
    if (data.progress != progress) {
        data.progress = progress;
        setDetachableTabData(idx, data);
        update(tabRect(idx));
    }
}

void DetachableTabBar::setDetachableTabData(int idx, const DetachableTabData &data)
{
    if ((data.color.isValid() && data.color.alpha() > 0) || data.progress.has_value()) {
        setTabData(idx, QVariant::fromValue(data));
    } else {
        setTabData(idx, QVariant());
    }
}

void DetachableTabBar::mousePressEvent(QMouseEvent *event)
{
    QTabBar::mousePressEvent(event);
    _containers = window()->findChildren<Konsole::TabbedViewContainer *>();
}

void DetachableTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
    auto widgetAtPos = qApp->topLevelAt(event->globalPosition().toPoint());
    if (widgetAtPos != nullptr) {
        if (window() == widgetAtPos->window()) {
            if (dragType != DragType::NONE) {
                dragType = DragType::NONE;
                setCursor(_originalCursor);
            }
        } else {
            if (dragType != DragType::WINDOW) {
                dragType = DragType::WINDOW;
                setCursor(QCursor(Qt::DragMoveCursor));
            }
        }
    } else if (!contentsRect().adjusted(-30, -30, 30, 30).contains(event->pos())) {
        // Don't let it detach the last tab.
        if (count() == 1) {
            return;
        }
        if (dragType != DragType::OUTSIDE) {
            dragType = DragType::OUTSIDE;
            setCursor(QCursor(Qt::DragCopyCursor));
        }
    }
}

void DetachableTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    // Block signals on middle mouse release, to prevent QTabBar's own handling
    // closing the tab, which is not configurable.
    const bool signalsWereBlocked = blockSignals(event->button() == Qt::MiddleButton);
    QTabBar::mouseReleaseEvent(event);
    blockSignals(signalsWereBlocked);

    switch (event->button()) {
    case Qt::MiddleButton:
        tabId = tabAt(event->pos());
        if (tabId == -1) {
            Q_EMIT newTabRequest();
        } else if (KonsoleSettings::closeTabOnMiddleMouseButton()) {
            Q_EMIT closeTab(tabId);
        }
        break;
    case Qt::LeftButton:
        _containers = window()->findChildren<Konsole::TabbedViewContainer *>();
        break;
    default:
        break;
    }

    setCursor(_originalCursor);

    if (contentsRect().adjusted(-30, -30, 30, 30).contains(event->pos())) {
        return;
    }

    auto widgetAtPos = qApp->topLevelAt(event->globalPosition().toPoint());
    if (widgetAtPos == nullptr) {
        if (count() != 1) {
            Q_EMIT detachTab(currentIndex());
        }
    } else if (window() != widgetAtPos->window()) {
        if (_containers.size() == 1 || count() > 1) {
            Q_EMIT moveTabToWindow(currentIndex(), widgetAtPos);
        }
    }
}

void DetachableTabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QTabBar::mouseDoubleClickEvent(event);
    }
}

void DetachableTabBar::dragEnterEvent(QDragEnterEvent *event)
{
    const auto dragId = QStringLiteral("konsole/terminal_display");
    if (!event->mimeData()->hasFormat(dragId)) {
        return;
    }
    auto other_pid = event->mimeData()->data(dragId).toInt();
    // don't accept the drop if it's another instance of konsole
    if (qApp->applicationPid() != other_pid) {
        return;
    }
    event->accept();
}

void DetachableTabBar::dragMoveEvent(QDragMoveEvent *event)
{
    int tabIdx = tabAt(event->position().toPoint());
    if (tabIdx != -1) {
        setCurrentIndex(tabIdx);
    }
}

void DetachableTabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);
    if (!event->isAccepted()) {
        return; // Reduces repainting
    }

    QPainter painter(this);
    painter.setPen(Qt::NoPen);

    for (int tabIndex = 0; tabIndex < count(); tabIndex++) {
        const QVariant data = tabData(tabIndex);
        if (!data.isValid() || data.isNull()) {
            continue;
        }

        const DetachableTabData tabData = data.value<DetachableTabData>();

        const bool colorValid = tabData.color.isValid() && tabData.color.alpha() > 0;

        if (!colorValid && !tabData.progress.has_value()) {
            continue;
        }

        const QColor color = colorValid ? tabData.color : palette().highlight().color();

        painter.setBrush(color);
        QRect tRect = tabRect(tabIndex);
        if (!isTabVisible(tabIndex)) {
            continue;
        }
        tRect.setTop(tRect.top() + painter.fontMetrics().height() + 6); // Position relative to each tab, including vertically stacked tabs.
        tRect.setHeight(4);
        tRect.setLeft(tRect.left() + 6);
        tRect.setWidth(tRect.width() - 6);

        // Draw progress, if any, ontop of a faint bar.
        if (tabData.progress.has_value()) {
            painter.setOpacity(0.3);
            painter.drawRect(tRect);
            painter.setOpacity(1.0);

            tRect.setWidth(tRect.width() * tabData.progress.value() / 100.0);
            painter.drawRect(tRect);
        } else {
            painter.drawRect(tRect);
        }
    }
}

}

#include "moc_DetachableTabBar.cpp"
