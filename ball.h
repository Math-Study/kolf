#ifndef _BALL_H
#define _BALL_H

#include <qcanvas.h>
#include <qcolor.h>

#include <math.h>

#include "vector.h"
#include "rtti.h"

enum BallState { Rolling = 0, Stopped, Holed };

class Ball : public QCanvasEllipse, public CanvasItem
{
public:
	Ball(QCanvas *canvas);
	BallState currentState();

	virtual void resetSize() { setSize(7, 7); }
	virtual void advance(int phase);
	virtual void doAdvance();
	virtual void moveBy(double dx, double dy);
	virtual void setVelocity(double vx, double vy);

	virtual bool canBeMovedByOthers() const { return true; }

	BallState curState() { return state; }
	void setState(BallState newState);

	QColor color() { return m_color; }
	void setColor(QColor color) { m_color = color; setBrush(color); }

	void setMoved(bool yes) { m_moved = yes; }
	bool moved() { return m_moved; }
	void setBlowUp(bool yes) { m_blowUp = yes; blowUpCount = 0; }
	bool blowUp() { return m_blowUp; }

	void setFrictionMultiplier(double news) { frictionMultiplier = news; };
	void friction();
	void collisionDetect();

	virtual int rtti() const { return Rtti_Ball; };

	bool addStroke() { return m_addStroke; }
	bool placeOnGround(Vector &v) { v = oldVector; }
	void setAddStroke(int newStrokes) { m_addStroke = newStrokes; }
	void setPlaceOnGround(bool placeOnGround) { m_placeOnGround = placeOnGround; oldVector = m_vector; }

	bool beginningOfHole() { return m_beginningOfHole; }
	void setBeginningOfHole(bool yes) { m_beginningOfHole = yes; }

	Vector curVector() { return m_vector; }
	void setVector(Vector newVector);

	bool collisionLock() { return m_collisionLock; }
	void setCollisionLock(bool yes) { m_collisionLock = yes; }
	virtual void fastAdvanceDone() { setCollisionLock(false); }

private:
	BallState state;
	QColor m_color;
	long int collisionId;
	double frictionMultiplier;

	bool m_blowUp;
	int blowUpCount;
	int m_addStroke;
	bool m_placeOnGround;
	double m_oldvx;
	double m_oldvy;

	bool m_moved;
	bool m_beginningOfHole;

	Vector m_vector;
	Vector oldVector;
	bool m_collisionLock;
};


inline double rad2deg(double theDouble)
{
    return ((360L / (2L * M_PI)) * theDouble);
}

inline double deg2rad(double theDouble)
{
	return (((2L * M_PI) / 360L) * theDouble);
}

#endif
