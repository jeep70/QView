#include "logicalpixelfitter.h"
#include <QtMath>

LogicalPixelFitter::LogicalPixelFitter(const qreal logicalScale, const QPoint offset)
    : logicalScale(logicalScale), offset(offset)
{
}

int LogicalPixelFitter::snapWidth(const qreal value) const
{
    return snap(value + offset.x(), logicalScale) - offset.x();
}

int LogicalPixelFitter::snapHeight(const qreal value) const
{
    return snap(value + offset.y(), logicalScale) - offset.y();
}

QSize LogicalPixelFitter::snapSize(const QSizeF size) const
{
    return QSize(snapWidth(size.width()), snapHeight(size.height()));
}

qreal LogicalPixelFitter::unsnapWidth(const int value) const
{
    return unsnap(value + offset.x(), logicalScale) - offset.x();
}

qreal LogicalPixelFitter::unsnapHeight(const int value) const
{
    return unsnap(value + offset.y(), logicalScale) - offset.y();
}

QSizeF LogicalPixelFitter::unsnapSize(const QSize size) const
{
    return QSizeF(unsnapWidth(size.width()), unsnapHeight(size.height()));
}

int LogicalPixelFitter::snap(const qreal value, const qreal logicalScale)
{
    const int valueRoundedDown = qFloor(value);
    const int valueRoundedUp = valueRoundedDown + 1;
    const int physicalPixelsDrawn = qRound(value * logicalScale);
    const int physicalPixelsShownIfRoundingUp = qRound(valueRoundedUp * logicalScale);
    return physicalPixelsDrawn >= physicalPixelsShownIfRoundingUp ? valueRoundedUp : valueRoundedDown;
}

qreal LogicalPixelFitter::unsnap(const int value, const qreal logicalScale)
{
    // For a given input value, its physical pixels fall within [value-0.5,value+0.5), so
    // calculate the first physical pixel of the next value (rounding up if between pixels),
    // and the pixel prior to that is the last one within the current value.
    const int maxPhysicalPixelForValue = qCeil((value + 0.5) * logicalScale) - 1;
    return maxPhysicalPixelForValue / logicalScale;
}
