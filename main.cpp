 #include <SFML/Graphics.hpp>
 #include "Console.h"
 #include <thread>

int main()
{
    // Create the main window
    sf::RenderWindow window(sf::VideoMode(640, 480), "SFML window");
    //window.setKeyRepeatEnabled(false);

    Console console;
    bool windowOpen = true;
    std::thread networkThread([&] {console.updateSockets(windowOpen);} );
    networkThread.detach();

	// Start the game loop
    while (window.isOpen())
    {
        // Process events
        sf::Event event;
        while (window.pollEvent(event))
        {
            // Close window : exit
            if (event.type == sf::Event::Closed)
            {
                console.disconnect();
                windowOpen = false;
                window.close();
            }

            console.update(event);
        };

        // Clear screen
        window.clear();

        console.showUI(window);
        console.showChat(window);

        // Update the window
        window.display();
    }

    return EXIT_SUCCESS;
}
