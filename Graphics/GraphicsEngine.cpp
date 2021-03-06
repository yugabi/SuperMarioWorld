#include "GraphicsEngine.hpp"
#include "../Graphics/GraphicsEvents.hpp"
#include "../System/Listener/CharacterDiedListener.hpp"
#include "../System/Listener/CharacterPositionUpdateListener.hpp"
#include "../System/Listener/DebugInfoUpdatedListener.hpp"
#include "../System/Listener/ForegroundItemRemovedListener.hpp"
#include "../System/Listener/ForegroundItemUpdatedListener.hpp"
#include "../System/Listener/GotLevelInfoListener.hpp"
#include "../System/Listener/NewForegroundItemReadListener.hpp"
#include "../System/Listener/NewPipeReadListener.hpp"

const float GraphicsEngine::FramerateLimit = 60;

GraphicsEngine::GraphicsEngine(EventEngine *_eventEngine): Engine (_eventEngine)
{
	m_gameWindow = new sf::RenderWindow(sf::VideoMode(WIN_WIDTH, WIN_HEIGHT, 32), "Super Mario !", sf::Style::Titlebar | sf::Style::Close);

	m_spriteHandler = new SpriteHandler();
	m_spriteHandler->LoadTextures();

	m_tmpSprite = new sf::Sprite();

	CreateListeners();
#ifdef DEBUG_MODE
	m_font.loadFromFile("arial.ttf");
	m_debugText.setFont(m_font);
	m_debugText.setCharacterSize(15);
	m_debugText.setColor(sf::Color::Red);
#endif

}

void GraphicsEngine::CreateListeners()
{
	CharacterDiedListener* characterDiedListener = new CharacterDiedListener(this);
	m_eventEngine->addListener("game.character_died", characterDiedListener);
	m_createdListeners.push_back(characterDiedListener);

	CharacterPositionUpdateListener* characterPositionUpdateListener = new CharacterPositionUpdateListener(this);
	m_eventEngine->addListener("game.character_position_updated", characterPositionUpdateListener);
	m_createdListeners.push_back(characterPositionUpdateListener);
#ifdef DEBUG
	DebugInfoUpdatedListener* debugInfoUpdatedListener = new DebugInfoUpdatedListener(this);
	m_eventEngine->addListener("game.debug_info_updated", debugInfoUpdatedListener);
	m_createdListeners.push_back(debugInfoUpdatedListener);
#endif
	ForegroundItemRemovedListener* foregroundItemRemovedListener = new ForegroundItemRemovedListener(this);
	m_eventEngine->addListener("game.foreground_item_removed", foregroundItemRemovedListener);
	m_createdListeners.push_back(foregroundItemRemovedListener);

	ForegroundItemUpdatedListener* foregroundItemUpdatedListener = new ForegroundItemUpdatedListener(this);
	m_eventEngine->addListener("game.foreground_item_updated", foregroundItemUpdatedListener);
	m_createdListeners.push_back(foregroundItemUpdatedListener);

	GotLevelInfoListener* gotLevelInfoListener = new GotLevelInfoListener(this);
	m_eventEngine->addListener("game.got_level_info", gotLevelInfoListener);
	m_createdListeners.push_back(gotLevelInfoListener);

	NewForegroundItemReadListener* newForegroundItemReadListener = new NewForegroundItemReadListener(this);
	m_eventEngine->addListener("game.new_foreground_item_read", newForegroundItemReadListener);
	m_createdListeners.push_back(newForegroundItemReadListener);

	NewPipeReadListener* newPipeReadListener = new NewPipeReadListener(this);
	m_eventEngine->addListener("game.new_pipe_read", newPipeReadListener);
	m_createdListeners.push_back(newPipeReadListener);
}

GraphicsEngine::~GraphicsEngine()
{
	delete m_spriteHandler;
	m_gameWindow->close();
	delete m_gameWindow;
	
	for (std::map<unsigned int, InfoForDisplay*>::iterator it = m_animatedLevelItems.begin(); it != m_animatedLevelItems.end(); ++it)
		delete(it->second);
}

void GraphicsEngine::Frame()
{
	m_gameWindow->clear();
	ResetSpritesToDraw();
	UpdateAnimatedLevelSprites();
	if (m_gameWindow->isOpen())
		ProcessWindowEvents();
	DisplayWindow();
}

void GraphicsEngine::ResetSpritesToDraw()
{
	while (!m_backgroundToDraw.empty())
		m_backgroundToDraw.pop_back();
}

void GraphicsEngine::UpdateAnimatedLevelSprites()
{
	for (std::map<unsigned int, InfoForDisplay*>::iterator it = m_animatedLevelItems.begin(); it != m_animatedLevelItems.end(); ++it)
	{
		UpdateForegroundItem(it->second);
	}
}

// Process windows events that have happened since the last loop iteration (sent by SFML)
void GraphicsEngine::ProcessWindowEvents()
{
	sf::Event windowEvent;

	while (m_gameWindow->pollEvent(windowEvent))
	{
		switch (windowEvent.type)
		{
			case sf::Event::KeyPressed:
			case sf::Event::KeyReleased:
			{
				KeyboardEvent event(windowEvent);
				m_eventEngine->dispatch(KEY_EVENT, &event);
				break;
			}
			case sf::Event::Closed:
			{
				Event event;
				m_eventEngine->dispatch(GAME_STOP_REQUEST, &event);
				break;
			}
			default:
				break;
		}
	}
}

void GraphicsEngine::DisplayWindow()
{
	SetBackgroundToDraw();

	DrawGame();

#ifdef DEBUG_MODE
	DrawDebugInfo();
#endif

	m_gameWindow->display();
}

void GraphicsEngine::SetBackgroundToDraw()
{
	ResetTmpSprite();
	m_tmpSprite->setTexture(m_spriteHandler->GetTexture("background_" + m_currentBackgroundName));
	m_backgroundToDraw.push_back(*m_tmpSprite);
}

void GraphicsEngine::UpdateForegroundItem(const InfoForDisplay *_info)
{
	ResetTmpSprite();
	InfoForDisplay* infoToApplyOnSprite = new InfoForDisplay(*_info); // in order to not modify _info
	infoToApplyOnSprite->name = GetTextureName(_info->id, _info->name, _info->state);
	infoToApplyOnSprite->coordinates = AbsoluteToRelative(_info->coordinates);
	m_spriteHandler->SetDisplayInfoOnSprite(*infoToApplyOnSprite, m_tmpSprite);

	if (_info->name.find("pipe_") != std::string::npos)
	{
		m_pipeSpritesToDraw[_info->id] = *m_tmpSprite;
	}
	else
	{
		m_foregroundSpritesToDraw[_info->id] = *m_tmpSprite;
	}

	assert(m_spritesCurrentlyDisplayed.find(_info->id) != m_spritesCurrentlyDisplayed.end());
	if (m_spritesCurrentlyDisplayed[_info->id].state == Sprite::ANIMATED)
		AddOrUpdateAnimatedLevelItem(_info); // What if it goes from ANIMATED to something else ? It would need to be removed from the list

	// Tell GameEngine what is to be drawn (id and coordinates), so it can handle collisions (the sprite size might have changed)
	// TODO check that game.foreground_item_updated is not being sent back and forth
	Event tmpEvent(_info->id, RelativeToAbsolute(m_tmpSprite->getGlobalBounds()));
	m_eventEngine->dispatch("game.foreground_item_updated", &tmpEvent);
}

void GraphicsEngine::AddOrUpdateAnimatedLevelItem(const InfoForDisplay *_info)
{
	if (m_animatedLevelItems.find(_info->id) != m_animatedLevelItems.end())
		*(m_animatedLevelItems[_info->id]) = *_info;
	else
		m_animatedLevelItems[_info->id] = new InfoForDisplay(*_info); 
}


void GraphicsEngine::DeleteForegroundItem(unsigned int _id)
{
	m_foregroundSpritesToDraw.erase(_id);
}

void GraphicsEngine::SetDisplayableObjectToDraw(InfoForDisplay _info) /* Need to copy the object, otherwise (by reference) I'd modify it */
{
	ResetTmpSprite();
	_info.name = GetTextureName(_info.id, _info.name, _info.state);
	_info.coordinates = AbsoluteToRelative(_info.coordinates);
	m_spriteHandler->SetDisplayInfoOnSprite(_info, m_tmpSprite);

	/*	gfx can receive the information to display a character several times (if it has been hit for exemple, info is sent fron g to gfx right after the hit)
	Only the last one received will be displayed */
	m_displayableObjectsToDraw[_info.id] = *m_tmpSprite;

	// Tell GameEngine what is to be drawn (id and coordinates), so it can handle collisions (the sprite size might have changed)
	Event tmpEvent(_info.id, RelativeToAbsolute(m_tmpSprite->getGlobalBounds()));
	m_eventEngine->dispatch("game.foreground_item_updated", &tmpEvent);
}

/* Figures out which sprite to display, ie the name of the sprite in the RECT file. The name is fetched only if it's an animation or if the state has changed. */
std::string GraphicsEngine::GetTextureName(unsigned int _id, std::string _currentName, State _state)
{
	std::string newTextureName;
	std::string fullStateName = m_spriteHandler->GetFullStateName(_currentName, _state);
	int nbTextures = m_spriteHandler->HowManyLoadedTexturesContainThisName(fullStateName);

	if (nbTextures < 0)
	{
		std::cerr << "ERROR: " << nbTextures << " texture for " << fullStateName << " (name: \"" << _currentName << "\", state: \"" << _state << "\")" << std::endl;
		assert(false);
	}

	if (m_spritesCurrentlyDisplayed.find(_id) == m_spritesCurrentlyDisplayed.end())
	{
		m_spritesCurrentlyDisplayed[_id].name = "";
		m_spritesCurrentlyDisplayed[_id].framesSinceLastChange = 0;
		m_spritesCurrentlyDisplayed[_id].state = nbTextures > 1 ? Sprite::ANIMATED : Sprite::NEW_STATIC;
	}

	switch (m_spritesCurrentlyDisplayed[_id].state)
	{
		case Sprite::UNKNOWN:
		case Sprite::ANIMATED:
		case Sprite::NEW_STATIC:
			newTextureName = m_spriteHandler->GetTextureNameFromStateName(fullStateName, m_spritesCurrentlyDisplayed[_id], nbTextures);
			break;
		case Sprite::STATIC:
			if (fullStateName == m_spritesCurrentlyDisplayed[_id].name) // Sprite same as previously
				newTextureName = fullStateName;
			else
				newTextureName = m_spriteHandler->GetTextureNameFromStateName(fullStateName, m_spritesCurrentlyDisplayed[_id], nbTextures);
			break;
		default:
			throw std::exception();
	}

	m_spritesCurrentlyDisplayed[_id].name = newTextureName;
	return newTextureName;
}

void GraphicsEngine::UpdateAnimationStates(unsigned int _id, std::string _stateFullName, int nbTextures)
{
	if (nbTextures == 1)			// Static
	{
		m_spritesCurrentlyDisplayed[_id].state = Sprite::STATIC;

		if (_stateFullName != m_spritesCurrentlyDisplayed[_id].name)
			m_spritesCurrentlyDisplayed[_id].state = Sprite::NEW_STATIC;
	}
	else							// Animation
	{
		m_spritesCurrentlyDisplayed[_id].state = Sprite::ANIMATED;
	}
}

// Draw the layers in the correct order
void GraphicsEngine::DrawGame()
{
	for (unsigned int i = 0; i < m_backgroundToDraw.size(); i++)
		m_gameWindow->draw(m_backgroundToDraw[i]);
	for (std::map<unsigned int, sf::Sprite>::iterator it = m_foregroundSpritesToDraw.begin(); it != m_foregroundSpritesToDraw.end(); ++it)
		m_gameWindow->draw(it->second);
	for (std::map<unsigned int, sf::Sprite>::iterator it = m_pipeSpritesToDraw.begin(); it != m_pipeSpritesToDraw.end(); ++it)
		m_gameWindow->draw(it->second);
	for (std::map<unsigned int, sf::Sprite>::iterator it = m_displayableObjectsToDraw.begin(); it != m_displayableObjectsToDraw.end(); ++it)
		m_gameWindow->draw(it->second);
}

void GraphicsEngine::RceiveLevelInfo(LevelInfo *_info)
{
	StoreLevelInfo(_info);
	InitCameraPosition(_info->size.y);
}

void GraphicsEngine::StoreLevelInfo(LevelInfo *_info)
{
	m_currentBackgroundName = _info->backgroundName;
	m_levelSize = _info->size;
}

void GraphicsEngine::InitCameraPosition(float _levelHeight)
{
	m_cameraPosition.x = 0;
	m_cameraPosition.y = _levelHeight - WIN_HEIGHT;
}

void GraphicsEngine::ReceiveCharacterPosition(InfoForDisplay* _info)
{
	SetDisplayableObjectToDraw(*_info);
	if (_info->name == "mario")
	{
		MoveCameraOnMario(_info->coordinates);
#ifdef DEBUG_MODE
		m_posMario.x = _info->coordinates.left;
		m_posMario.y = _info->coordinates.top;
#endif
	}
}

void GraphicsEngine::RemoveDisplayableObject(unsigned int _id)
{
	m_displayableObjectsToDraw.erase(_id);
}

void GraphicsEngine::ResetTmpSprite()
{
	delete m_tmpSprite;
	m_tmpSprite = new sf::Sprite();
}

void GraphicsEngine::MoveCameraOnMario(sf::FloatRect _coordsMario)
{
	sf::Vector2f oldCameraPosition = m_cameraPosition;

	float newCameraX = _coordsMario.left - WIN_WIDTH / 2;
	if (newCameraX < 0)
		m_cameraPosition.x = 0;
	else if (newCameraX > m_levelSize.x - WIN_WIDTH)
		m_cameraPosition.x = m_levelSize.x - WIN_WIDTH;
	else
		m_cameraPosition.x = newCameraX;

	float newCameraY = _coordsMario.top - WIN_HEIGHT / 2;
	if (newCameraY < 0)
		m_cameraPosition.y = 0;
	else if (newCameraY > m_levelSize.y - WIN_HEIGHT)
		m_cameraPosition.y = m_levelSize.y - WIN_HEIGHT;
	else
		m_cameraPosition.y = newCameraY;

	if (m_cameraPosition != oldCameraPosition)
		MoveEverythingBy(oldCameraPosition - m_cameraPosition);
}

void GraphicsEngine::MoveEverythingBy(sf::Vector2f _vect)
{
	for (std::map<unsigned int, sf::Sprite>::iterator it = m_foregroundSpritesToDraw.begin(); it != m_foregroundSpritesToDraw.end(); ++it)
		it->second.move(_vect);
	for (std::map<unsigned int, sf::Sprite>::iterator it = m_pipeSpritesToDraw.begin(); it != m_pipeSpritesToDraw.end(); ++it)
		it->second.move(_vect);
	for (std::map<unsigned int, sf::Sprite>::iterator it = m_displayableObjectsToDraw.begin(); it != m_displayableObjectsToDraw.end(); ++it)
		it->second.move(_vect);
}

#ifdef DEBUG_MODE
void GraphicsEngine::DrawDebugInfo()
{
	std::string toWrite;
	sf::Vector2f playerPos = m_posMario;
	sf::Vector2f playerVel = m_debugInfo->velocity;
	sf::Vector2f playerAcc = m_debugInfo->acceleration;

	toWrite += (" Framerate = " + std::to_string(floor((int)1 / m_clock.getElapsedTime().asSeconds())) + "\n");
	toWrite += (" Position: { " + std::to_string(playerPos.x) + "; " + std::to_string(playerPos.y) + " }\n");
	toWrite += (" Velocity: { " + std::to_string(playerVel.x) + "; " + std::to_string(playerVel.y) + " }\n");
	toWrite += (" Acceleration: { " + std::to_string(playerAcc.x) + "; " + std::to_string(playerAcc.y) + " }\n");
	toWrite += (" State: " + Debug::GetTextForState(m_debugInfo->state) + "\n");
	toWrite += (" Jump state: " + Debug::GetTextForJumpState(m_debugInfo->jumpState) + "\n");

	m_clock.restart();
	m_debugText.setString(toWrite);
	m_gameWindow->draw(m_debugText);
}
#endif

float GraphicsEngine::GetFramerateLimit()
{
	return GraphicsEngine::FramerateLimit;
}

sf::Vector2f GraphicsEngine::RelativeToAbsolute(sf::Vector2f _rel)
{
	sf::Vector2f abs(_rel.x + m_cameraPosition.x, _rel.y + m_cameraPosition.y);
	return abs;
}

sf::FloatRect GraphicsEngine::RelativeToAbsolute(sf::FloatRect _rel)
{
	_rel.left += m_cameraPosition.x;
	_rel.top += m_cameraPosition.y;
	return _rel;
}


sf::Vector2f GraphicsEngine::AbsoluteToRelative(sf::Vector2f _abs)
{
	sf::Vector2f rel(_abs.x + m_cameraPosition.x, _abs.y + m_cameraPosition.y);
	return rel;
}

sf::FloatRect GraphicsEngine::AbsoluteToRelative(sf::FloatRect _abs)
{
	_abs.left -= m_cameraPosition.x;
	_abs.top -= m_cameraPosition.y;
	return _abs;
}
