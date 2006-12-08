#include <qbitmap.h>
#include <QCheckBox>
#include <QLabel>
#include <qimage.h>
#include <qpixmapcache.h>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>

#include <kapplication.h>
#include <kcombobox.h>
#include <kconfig.h>
#include <knuminput.h>
#include <kpixmapeffect.h>
#include <kstandarddirs.h>

#include "slope.h"

Slope::Slope(QRect rect, QGraphicsItem * parent, QGraphicsScene *scene)
	: QGraphicsRectItem(rect, parent, scene), type(KImageEffect::VerticalGradient), grade(4), reversed(false), color(QColor("#327501"))
{
	setData(0, 1031);
	stuckOnGround = false;
	showingInfo = false;

	gradientKeys[KImageEffect::VerticalGradient] = "Vertical";
	gradientKeys[KImageEffect::HorizontalGradient] = "Horizontal";
	gradientKeys[KImageEffect::DiagonalGradient] = "Diagonal";
	gradientKeys[KImageEffect::CrossDiagonalGradient] = "Opposite Diagonal";
	gradientKeys[KImageEffect::EllipticGradient] = "Elliptic";

	gradientI18nKeys[KImageEffect::VerticalGradient] = i18n("Vertical");
	gradientI18nKeys[KImageEffect::HorizontalGradient] = i18n("Horizontal");
	gradientI18nKeys[KImageEffect::DiagonalGradient] = i18n("Diagonal");
	gradientI18nKeys[KImageEffect::CrossDiagonalGradient] = i18n("Opposite Diagonal");
	gradientI18nKeys[KImageEffect::EllipticGradient] = i18n("Circular");

	setZValue(-50);

	if (!QPixmapCache::find("grass", grass))
	{
		grass.load(KStandardDirs::locate("appdata", "pics/grass.png"));
		QPixmapCache::insert("grass", grass);
	}

	point = new RectPoint(color.light(), 0, parent, scene);

	QFont font(kapp->font());
	font.setPixelSize(18);
	text = new QGraphicsSimpleTextItem(0, scene);
	text->setZValue(99999.99);
	text->setFont(font);
	text->setBrush(Qt::white);

	editModeChanged(false);
	hideInfo();

	// this does updatePixmap
	setGradient("Vertical");
}

bool Slope::terrainCollisions() const
{
	// we are a terrain collision
	return true;
}

void Slope::showInfo()
{
	showingInfo = true;
	QList<Arrow *>::const_iterator arrow;
	for (arrow = arrows.constBegin(); arrow != arrows.constEnd(); ++arrow)
	{
		(*arrow)->setZValue(zValue() + .01);
		(*arrow)->setVisible(true);
	}
	text->setVisible(true);
}

void Slope::hideInfo()
{
	showingInfo = false;
	QList<Arrow *>::const_iterator arrow;
	for (arrow = arrows.constBegin(); arrow != arrows.constEnd(); ++arrow)
		(*arrow)->setVisible(false);
	text->setVisible(false);
}

void Slope::aboutToDie()
{
	delete point;
	clearArrows();
	delete text;
}

void Slope::clearArrows()
{
	QList<Arrow *>::const_iterator arrow;
	for (arrow = arrows.constBegin(); arrow != arrows.constEnd(); ++arrow)
	{
		(*arrow)->setVisible(false);
		(*arrow)->aboutToDie();
	}
        while (!arrows.isEmpty())
            delete arrows.takeFirst();
}

QList<QGraphicsItem *> Slope::moveableItems() const
{
	QList<QGraphicsItem *> ret;
	ret.append(point);
	return ret;
}

void Slope::setGrade(double newGrade)
{
	if (newGrade >= 0 && newGrade < 11)
	{
		grade = newGrade;
		updatePixmap();
	}
}

void Slope::setSize(int width, int height)
{
	newSize(width, height);
}

void Slope::newSize(double width, double height)
{
	if (type == KImageEffect::EllipticGradient)
	{
		QGraphicsRectItem::setRect(rect().x(), rect().y(), width, width);
		// move point back to good spot
		moveBy(0, 0);

		if (game && game->isEditing())
			game->updateHighlighter();
	}
	else
		QGraphicsRectItem::setRect(rect().x(), rect().y(), width, height);

	updatePixmap();
	updateZ();
}

void Slope::moveBy(double dx, double dy)
{
	QGraphicsRectItem::moveBy(dx, dy);

	point->dontMove();
	point->setPos(x() + width(), y() + height());

	moveArrow();
	updateZ();
}

void Slope::moveArrow()
{
	int xavg = 0, yavg = 0;
	Q3PointArray r = areaPoints(); //still using Q3PointArray because I'm not sure of how to change this easily and this code will probably be replaced when the graphics are updated
	for (int i = 0; i < r.size(); ++i)
	{
		xavg += r[i].x();
		yavg += r[i].y();
	}
	xavg /= r.size();
	yavg /= r.size();

	QList<Arrow *>::const_iterator arrow;
	for (arrow = arrows.constBegin(); arrow != arrows.constEnd(); ++arrow)
		(*arrow)->setPos((double)xavg, (double)yavg);

	if (showingInfo)
		showInfo();
	else
		hideInfo();

	text->setPos((double)xavg - text->boundingRect().width() / 2, (double)yavg - text->boundingRect().height() / 2);
}

void Slope::editModeChanged(bool changed)
{
	point->setVisible(changed);
	moveBy(0, 0);
}

void Slope::updateZ(QGraphicsRectItem *vStrut)
{
	const double area = (height() * width());
	const int defaultz = -50;

	double newZ = 0;

	QGraphicsRectItem *rect = 0;
	if (!stuckOnGround)
		rect = vStrut? vStrut : onVStrut();

	if (rect)
	{
		if (area > (rect->rect().width() * rect->rect().height()))
			newZ = defaultz;
		else
			newZ = rect->zValue();
	}
	else
		newZ = defaultz;

	setZValue(((double)1 / (area == 0? 1 : area)) + newZ);
}

void Slope::load(KConfig *cfg)
{
	stuckOnGround = cfg->readEntry("stuckOnGround", stuckOnGround);
	grade = cfg->readEntry("grade", grade);
	reversed = cfg->readEntry("reversed", reversed);

	// bypass updatePixmap which newSize normally does
	QGraphicsRectItem::setRect(rect().x(), rect().y(), cfg->readEntry("width", width()), cfg->readEntry("height", height()));
	updateZ();

	QString gradientType = cfg->readEntry("gradient", gradientKeys[type]);
	setGradient(gradientType);
}

void Slope::save(KConfig *cfg)
{
	cfg->writeEntry("reversed", reversed);
	cfg->writeEntry("width", width());
	cfg->writeEntry("height", height());
	cfg->writeEntry("gradient", gradientKeys[type]);
	cfg->writeEntry("grade", grade);
	cfg->writeEntry("stuckOnGround", stuckOnGround);
}

void Slope::paint(QPainter *painter, const QStyleOptionGraphicsItem * /*option*/, QWidget * /*widget*/ ) 
{
	painter->drawPixmap(0, 0, pixmap);  
}

QPainterPath Slope::shape() const
{       
	switch (type)
	{
		case KImageEffect::CrossDiagonalGradient:
		{
			QPainterPath path;
			QPolygonF polygon(3);
			polygon[0] = QPointF(rect().x(), rect().y());
			polygon[1] = QPointF(rect().x() + width(), rect().y() + height());
			polygon[2] = reversed? QPointF(rect().x() + width(), rect().y()) : QPointF(rect().x(), rect().y() + height());
			path.addPolygon(polygon);
			return path;
		}

		case KImageEffect::DiagonalGradient:
		{
			QPainterPath path;
			QPolygonF polygon(3);
			polygon[0] = QPointF(rect().x() + width(), rect().y());
			polygon[1] = QPointF(rect().x(), rect().y() + height());
			polygon[2] = !reversed? QPointF(rect().x() + width(), rect().y() + height()) : QPointF(rect().x(), rect().y());
			path.addPolygon(polygon);
			return path;
		}

		case KImageEffect::EllipticGradient:
		{
			QPainterPath path;
			path.addEllipse(rect().x(), rect().y(), width(), height());
			return path;
		}

		default:
		{
			QPainterPath path;
			path.addRect(rect().x(), rect().y(), width(), height());
			return path;
		}
	}
}

Q3PointArray Slope::areaPoints() const //still using Q3PointArray areaPoints because it is needed to find the centre of the slope when placing arrows. Not sure of an easy alternative way to do this, and no need to because this code will probably be replaced when the graphics are updated
{
	switch (type)
	{
		case KImageEffect::CrossDiagonalGradient:
		{
			Q3PointArray ret(3);
			ret[0] = QPoint((int)x(), (int)y());
			ret[1] = QPoint((int)x() + (int)width(), (int)y() + (int)height());
			ret[2] = reversed? QPoint((int)x() + (int)width(), (int)y()) : QPoint((int)x(), (int)y() + (int)height());

			return ret;
		}

		case KImageEffect::DiagonalGradient:
		{
			Q3PointArray ret(3);
			ret[0] = QPoint((int)x() + (int)width(), (int)y());
			ret[1] = QPoint((int)x(), (int)y() + (int)height());
			ret[2] = !reversed? QPoint((int)x() + (int)width(), (int)y() + (int)height()) : QPoint((int)x(), (int)y());

			return ret;
		}

		case KImageEffect::EllipticGradient:
		{
			Q3PointArray ret;
			ret.makeEllipse((int)x(), (int)y(), (int)width(), (int)height());
			return ret;
		}

		default:
		{
			Q3PointArray ret(4);
			ret[0] = QPoint((int)x() + (int)width(), (int)y());
			ret[1] = QPoint((int)x(), (int)y());
			ret[2] = QPoint((int)x(), (int)y() + (int)height());
			ret[3] = QPoint((int)x() + (int)width(), (int)y() + (int)height());
			return ret;
		}
	}
}

bool Slope::collision(Ball *ball, long int /*id*/)
{
	if (grade <= 0)
		return false;

	double vx = ball->getXVelocity();
	double vy = ball->getYVelocity();
	double addto = 0.013 * grade;

	const bool diag = type == KImageEffect::DiagonalGradient || type == KImageEffect::CrossDiagonalGradient;
	const bool circle = type == KImageEffect::EllipticGradient;

	double slopeAngle = 0;

	if (diag) 
		slopeAngle = atan((double)width() / (double)height());
	else if (circle)
	{
		const QPointF start((x() + width() / 2.0), y() + height() / 2.0);
		const QPointF end(ball->x(), ball->y());

		Vector betweenVector(start, end);
		const double factor = betweenVector.magnitude() / ((double)width() / 2.0);
		slopeAngle = betweenVector.direction();

		// this little bit by Daniel
		addto *= factor * M_PI / 2;
		addto = sin(addto);
	}

	switch (type)
	{
		case KImageEffect::HorizontalGradient:
			reversed? vx += addto : vx -= addto;
		break;

		case KImageEffect::VerticalGradient:
			reversed? vy += addto : vy -= addto;
		break;

		case KImageEffect::DiagonalGradient:
		case KImageEffect::EllipticGradient:
			reversed? vx += cos(slopeAngle) * addto : vx -= cos(slopeAngle) * addto;
			reversed? vy += sin(slopeAngle) * addto : vy -= sin(slopeAngle) * addto;
		break;

		case KImageEffect::CrossDiagonalGradient:
			reversed? vx -= cos(slopeAngle) * addto : vx += cos(slopeAngle) * addto;
			reversed? vy += sin(slopeAngle) * addto : vy -= sin(slopeAngle) * addto;
		break;

		default:
		break;
	}

	ball->setVelocity(vx, vy);
	// check if the ball is at the center of a pit or mound
	// or has otherwise stopped.
	if (vx == 0 && vy ==0) 
		ball->setState(Stopped);
	else 
		ball->setState(Rolling);

	// do NOT do terrain collidingItems
	return false;
}

void Slope::setGradient(QString text)
{
	for (QMap<KImageEffect::GradientType, QString>::Iterator it = gradientKeys.begin(); it != gradientKeys.end(); ++it)
	{
		if (it.value() == text)
		{
			setType(it.key());
			return;
		}
	}

	// extra forgiveness ;-) (note it's i18n keys)
	for (QMap<KImageEffect::GradientType, QString>::Iterator it = gradientI18nKeys.begin(); it != gradientI18nKeys.end(); ++it)
	{
		if (it.value() == text)
		{
			setType(it.key());
			return;
		}
	}
}

void Slope::setType(KImageEffect::GradientType type)
{
	this->type = type;

	if (type == KImageEffect::EllipticGradient)
	{
		// calls updatePixmap
		newSize(width(), height());
	}
	else
		updatePixmap();
}

void Slope::updatePixmap()
{
	// make a gradient, make grass that's bright or dim
	// merge into this->pixmap. This is paintn in paint()

	// we update the arrows in this function
	clearArrows();

	const bool diag = type == KImageEffect::DiagonalGradient || type == KImageEffect::CrossDiagonalGradient;
	const bool circle = type == KImageEffect::EllipticGradient;

	const QColor darkColor = color.dark((int)(100 + grade * (circle? 20 : 10)));
	const QColor lightColor = diag || circle? color.light((int)(110 + (diag? 5 : .5) * grade)) : color;
	// hack only for circles
	const bool _reversed = circle? !reversed : reversed;
	QImage gradientImage = KImageEffect::gradient(QSize((int)width(), (int)height()), _reversed? darkColor : lightColor, _reversed? lightColor : darkColor, type);

	QPixmap qpixmap((int)width(), (int)height());
	QPainter p(&qpixmap);
	p.drawTiledPixmap(QRect(0, 0, (int)width(), (int)height()), grass);
	p.end();

	const double length = sqrt(double(width() * width() + height() * height())) / 4;

	if (circle)
	{
		const QColor otherLightColor = color.light((int)(110 + 15 * grade));
		const QColor otherDarkColor = darkColor.dark((int)(110 + 20 * grade));
		QImage otherGradientImage = KImageEffect::gradient(QSize((int)width(), (int)height()), reversed? otherDarkColor : otherLightColor, reversed? otherLightColor : otherDarkColor, KImageEffect::DiagonalGradient);

		QImage grassImage(qpixmap.toImage());

		QImage finalGradientImage = KImageEffect::blend(otherGradientImage, gradientImage, .60);
		pixmap = QPixmap::fromImage(KImageEffect::blend(grassImage, finalGradientImage, .40));

		// make arrows
		double angle = 0;
		for (int i = 0; i < 4; ++i)
		{
			angle += M_PI / 2;
			Arrow *arrow = new Arrow(0, scene());
			arrow->setLength(length);
			arrow->setAngle(angle);
			arrow->setReversed(reversed);
			arrow->updateSelf();
			arrows.append(arrow);
		}
	}
	else
	{
		Arrow *arrow = new Arrow(0, scene());

		float ratio = 0;
		float factor = 1;

		double angle = 0;

		switch (type)
		{
			case KImageEffect::HorizontalGradient:
				angle = 0;
				factor = .32;
				break;

			case KImageEffect::VerticalGradient:
				angle = M_PI / 2;
				factor = .32;
				break;

			case KImageEffect::DiagonalGradient:
				angle = atan((double)width() / (double)height());

				factor = .45;
				break;

			case KImageEffect::CrossDiagonalGradient:
				angle = atan((double)width() / (double)height());
				angle = M_PI - angle;

				factor = .05;
				break;

			default:
				break;
		}

		float factorPart = factor * 2;
		// gradePart is out of 1
		float gradePart = grade / 8.0;

		ratio = factorPart * gradePart;

		// reverse the reversed ones
		if (reversed)
			ratio *= -1;
		else
			angle += M_PI;

		QPixmap kpixmap = qpixmap;
		(void) KPixmapEffect::intensity(kpixmap, ratio);

		QImage grassImage(kpixmap.toImage());

		// okay, now we have a grass image that's
		// appropriately lit, and a gradient;
		// lets blend..
		pixmap = QPixmap::fromImage(KImageEffect::blend(gradientImage, grassImage, .42));
		arrow->setAngle(angle);
		arrow->setLength(length);
		arrow->updateSelf();

		arrows.append(arrow);
	}

	text->setText(QString::number(grade));

	if (diag || circle)
	{
		// make cleared bitmap
		QBitmap bitmap(pixmap.width(), pixmap.height());
                bitmap.clear();
		QPainter bpainter(&bitmap);
		bpainter.setBrush(Qt::color1);

		QPainterPath r = shape();
		r.moveTo(x(), y());
		bpainter.drawPath(r);


		// mask is paintn
		pixmap.setMask(bitmap);
	}

	moveArrow();
}

/////////////////////////

SlopeConfig::SlopeConfig(Slope *slope, QWidget *parent)
	: Config(parent)
{
	this->slope = slope;
	QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setMargin( marginHint() );
        layout->setSpacing( spacingHint() );
	KComboBox *gradient = new KComboBox(this);
	QStringList items;
	QString curText;
	for (QMap<KImageEffect::GradientType, QString>::Iterator it = slope->gradientI18nKeys.begin(); it != slope->gradientI18nKeys.end(); ++it)
	{
		if (it.key() == slope->curType())
			curText = it.value();
		items.append(it.value());
	}
	gradient->addItems(items);
	gradient->setCurrentText(curText);
	layout->addWidget(gradient);
	connect(gradient, SIGNAL(activated(const QString &)), this, SLOT(setGradient(const QString &)));

	layout->addStretch();

	QCheckBox *reversed = new QCheckBox(i18n("Reverse direction"), this);
	reversed->setChecked(slope->isReversed());
	layout->addWidget(reversed);
	connect(reversed, SIGNAL(toggled(bool)), this, SLOT(setReversed(bool)));

	QHBoxLayout *hlayout = new QHBoxLayout;
        hlayout->setSpacing( spacingHint() );
        layout->addLayout( hlayout );
	hlayout->addWidget(new QLabel(i18n("Grade:"), this));
	KDoubleNumInput *grade = new KDoubleNumInput(this);
	grade->setRange(0, 8, 1, true);
	grade->setValue(slope->curGrade());
	hlayout->addWidget(grade);
	connect(grade, SIGNAL(valueChanged(double)), this, SLOT(gradeChanged(double)));

	QCheckBox *stuck = new QCheckBox(i18n("Unmovable"), this);
	stuck->setWhatsThis( i18n("Whether or not this slope can be moved by other objects, like floaters."));
	stuck->setChecked(slope->isStuckOnGround());
	layout->addWidget(stuck);
	connect(stuck, SIGNAL(toggled(bool)), this, SLOT(setStuckOnGround(bool)));
}

void SlopeConfig::setGradient(const QString &text)
{
	slope->setGradient(text);
	changed();
}

void SlopeConfig::setReversed(bool yes)
{
	slope->setReversed(yes);
	changed();
}

void SlopeConfig::setStuckOnGround(bool yes)
{
	slope->setStuckOnGround(yes);
	changed();
}

void SlopeConfig::gradeChanged(double newgrade)
{
	slope->setGrade(newgrade);
	changed();
}

#include "slope.moc"
