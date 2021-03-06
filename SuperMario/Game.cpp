#include "Game.hpp"
#include "../System/Listener/CloseRequestListener.hpp"

Game::Game()
{
    m_running = true;

    m_eventEngine = new EventEngine();

    // Creating engines
    m_g = new GameEngine (m_eventEngine);
    m_gfx = new GraphicsEngine (m_eventEngine);
    m_s = new SoundEngine (m_eventEngine);

    CloseRequestListener* closeRequestListener = new CloseRequestListener(this);
    m_eventEngine->addListener("graphics.stop_request", closeRequestListener);
    m_createdListeners.push_back(closeRequestListener);

    // Creating links between engines
    m_g->Attach_Engine ("gfx", m_gfx);
    m_g->Attach_Engine ("s", m_s);
    m_gfx->Attach_Engine ("g", m_g);
    m_gfx->Attach_Engine ("s", m_s);
    m_s->Attach_Engine ("g", m_g);
    m_s->Attach_Engine ("s", m_s);
}

Game::~Game()
{
    delete m_g;
    delete m_gfx;
    delete m_s;
    for (unsigned int i = 0; i < m_createdListeners.size(); i++)
	{
        delete m_createdListeners[i];
    }
	delete m_eventEngine;
}

void Game::Run()
{
	bool running = m_running;
	sf::Clock clock;

	while (running)
	{
		m_g->Frame(clock.getElapsedTime().asSeconds());
		clock.restart();
		m_gfx->Frame();
        m_s->Frame();

        m_running_mutex.lock();
        running = m_running;
        m_running_mutex.unlock();

		// Enforce the framerateLimit (doesn't take into account the time spent in m_g->Frame; could be improved with a second clock)
		float framerateLimit = m_gfx->GetFramerateLimit();
		while (clock.getElapsedTime().asSeconds() < 1 / framerateLimit)
			sf::sleep(sf::seconds( (1.f / framerateLimit - clock.getElapsedTime().asSeconds()) / 2.f));
	}
}

void Game::Stop()
{
	m_running_mutex.lock();
	m_running = false;
	m_running_mutex.unlock();
}
