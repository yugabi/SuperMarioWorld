#include "Enemy.hpp"

Enemy::Enemy(EventEngine *_eventEngine, std::string _name, sf::Vector2f _coord, Direction _dir) : MovingObject(_eventEngine, _name, _coord, WALK)
{
	m_facing = _dir;
	Init();
}

Enemy::Enemy(EventEngine *_eventEngine, std::string _name, float _x, float _y, Direction _dir) : MovingObject(_eventEngine, _name, _x, _y, WALK)
{
	m_facing = _dir;
	Init();
}

void Enemy::Init()
{
	m_class = ENEMY;
}

InfoForDisplay Enemy::GetInfoForDisplay()
{
	m_reverseSprite = (m_facing == DRIGHT);
	return MovingObject::GetInfoForDisplay();
}

void Enemy::UpdateAfterCollisionWithMapEdge(CollisionDirection _dir, float _gap)
{
	switch (_dir)
	{
		case LEFT: // Enemy on the left (= right hand edge)
			m_facing = DRIGHT;
			break;
		case RIGHT:
			m_facing = DLEFT;
			break;
		default:
			break;
	}
	MovingObject::UpdateAfterCollisionWithMapEdge(_dir, _gap);
}

void Enemy::Move(Instruction _inst)
{
	switch (_inst)
	{
		case GO_LEFT:
			m_facing = DLEFT;
			break;
		case GO_RIGHT:
			m_facing = DRIGHT;
			break;
		default:
			break;
	}
}
