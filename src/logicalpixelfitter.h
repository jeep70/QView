#ifndef LOGICALPIXELFITTER_H
#define LOGICALPIXELFITTER_H

#include <QPoint>
#include <QSize>

class LogicalPixelFitter
{
public:
    LogicalPixelFitter(const qreal logicalScale, const QPoint offset);

    int snapWidth(const qreal value) const;

    int snapHeight(const qreal value) const;

    QSize snapSize(const QSizeF size) const;

    qreal unsnapWidth(const int value) const;

    qreal unsnapHeight(const int value) const;

    QSizeF unsnapSize(const QSize size) const;

    static int snap(const qreal value, const qreal logicalScale);

    static qreal unsnap(const int value, const qreal logicalScale);

private:
    const qreal logicalScale;
    const QPoint offset;
};

#endif // LOGICALPIXELFITTER_H
