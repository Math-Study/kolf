#include <qcanvas.h>
#include <qcolor.h>
#include <qpen.h>

#include <kapplication.h>
#include <kdebug.h>

#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include "rtti.h"
#include "vector.h"
#include "canvasitem.h"
#include "game.h"
#include "ball.h"

Ball::Ball(QCanvas *canvas)
	: QCanvasEllipse(canvas)
{
	m_doDetect = true;
	m_collisionLock = false;
	setBeginningOfHole(false);
	setBlowUp(false);
	setPen(black);
	resetSize();
	collisionId = 0;
	m_addStroke = false;
	m_placeOnGround = false;
	frictionMultiplier = 1.0;

	// this sets z
	setState(Stopped);
}

void Ball::setState(BallState newState)
{
	state = newState;
	if (state == Stopped)
		setZ(1000);
	else
		setBeginningOfHole(false);
}

void Ball::advance(int phase)
{
	if (phase == 1 && m_blowUp)
	{
		if (blowUpCount >= 50)
		{
			//TODO make this a config option
			//setAddStroke(addStroke() + 1);
			setBlowUp(false);
			resetSize();
			return;
		}

		const double diff = 8;
		double randnum = kapp->random();
		const double width = 6 + randnum * (diff / RAND_MAX);
		randnum = kapp->random();
		const double height = 6 + randnum * (diff / RAND_MAX);
		setSize(width, height);
		blowUpCount++;
	}
}

void Ball::friction()
{
	if (state == Stopped || state == Holed || !isVisible()) { setVelocity(0, 0); return; }
	const double subtractAmount = .027 * frictionMultiplier;
	if (m_vector.magnitude() <= subtractAmount)
	{
		state = Stopped;
		setVelocity(0, 0);
		game->timeout();
		return;
	}
	m_vector.setMagnitude(m_vector.magnitude() - subtractAmount);
	setVector(m_vector);

	frictionMultiplier = 1.0;
}

void Ball::setVelocity(double vx, double vy)
{
	QCanvasEllipse::setVelocity(vx, vy);

	if (vx == 0 && vy == 0)
	{
		m_vector.setDirection(0);
		m_vector.setMagnitude(0);
		return;
	}

	double ballAngle = atan2(-vy, vx);

	m_vector.setDirection(ballAngle);
	m_vector.setMagnitude(sqrt(pow(vx, 2) + pow(vy, 2)));
}

void Ball::setVector(const Vector &newVector)
{
	m_vector = newVector;

	if (newVector.magnitude() == 0)
	{
		setVelocity(0, 0);
		return;
	}

	QCanvasEllipse::setVelocity(cos(newVector.direction()) * newVector.magnitude(), -sin(newVector.direction()) * newVector.magnitude());
}

void Ball::moveBy(double dx, double dy)
{
	QCanvasEllipse::moveBy(dx, dy);

	if (game && !game->isPaused())
		collisionDetect();
		
	if ((dx || dy) && game && game->curBall() == this)
		game->ballMoved();
}

void Ball::doAdvance()
{
	QCanvasEllipse::advance(1);
}

void Ball::collisionDetect()
{
	if (!isVisible() || state == Holed || !m_doDetect)
		return;

	if (collisionId >= INT_MAX - 1)
		collisionId = 0;
	else
		collisionId++;

	// every other time...
	// do friction
	if (collisionId % 2 && !(xVelocity() == 0 && yVelocity() == 0))
		friction();

	const double minSpeed = .06;

	QCanvasItemList m_list = collisions(true);

	// please don't ask why QCanvas doesn't actually sort its list;
	// it just doesn't.
	m_list.sort();

	this->m_list = m_list;

	if (m_list.isEmpty())
	{
		return;
	}

	QCanvasItem *item = 0;

	for (QCanvasItemList::Iterator it = m_list.begin(); it != m_list.end(); ++it)
	{
		item = *it;

		if (item->rtti() == Rtti_NoCollision || item->rtti() == Rtti_Putter)
			continue;

		if (item->rtti() == rtti() && !m_collisionLock)
		{
			// it's one of our own kind, a ball
			Ball *oball = dynamic_cast<Ball *>(item);
			if (oball->collisionLock())
				continue;
			oball->setCollisionLock(true);

			if ((oball->x() - x() != 0 && oball->y() - y() != 0) && state == Rolling && oball->curState() != Holed)
			{
				m_collisionLock = true;
				// move this ball to where it was barely touching
				double ballAngle = m_vector.direction();
				while (collisions(true).contains(item) > 0)
					move(x() - cos(ballAngle) / 2.0, y() + sin(ballAngle) / 2.0);
				// make a 1 pixel separation
				move(x() - cos(ballAngle), y() + sin(ballAngle));

				Vector bvector = oball->curVector();
				m_vector -= bvector;

				Vector unit1 = Vector(QPoint(x(), y()), QPoint(oball->x(), oball->y()));
				unit1 = unit1.unit();

				Vector unit2 = m_vector.unit();

				double cos = unit1 * unit2;

				unit1 *= m_vector.magnitude() * cos;
				m_vector -= unit1;
				m_vector += bvector;

				bvector += unit1;

				oball->setVector(bvector);
				setVector(m_vector);

				oball->setState(Rolling);
				setState(Rolling);

				oball->doAdvance();
			}

			continue;
		}

		if (!isVisible() || state == Holed)
			return;

		CanvasItem *citem = dynamic_cast<CanvasItem *>(item);
		if (citem)
		{
			if (!citem->terrainCollisions())
				if (!citem->collision(this, collisionId))
					goto end;
			break;
		}
	}


	for (QCanvasItemList::Iterator it = m_list.begin(); it != m_list.end(); ++it)
	{
		CanvasItem *citem = dynamic_cast<CanvasItem *>(*it);
		if (citem)
			if (citem->terrainCollisions())
				citem->collision(this, collisionId);
	}

	end:

	if (m_vector.magnitude() < minSpeed && m_vector.magnitude())
	{
		setVelocity(0, 0);
		setState(Stopped);
	}
}

BallState Ball::currentState()
{
	return state;
}

