#ifndef CONSOLE_H
#define CONSOLE_H

#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <string>
#include <list>
#include <mutex>

struct IpPair
{
    sf::IpAddress ipaddress;
    unsigned short port;
};

class Console
{
    public:
        Console();
        virtual ~Console();

        void update(const sf::Event& event);
        void getInput(const sf::Event& event);
        void sendMessage();
        void sendSystemMessage(std::string text);
        bool parseCommand();

        void show(sf::RenderWindow& window);
        void showChat(sf::RenderWindow& window);
        void showUI(sf::RenderWindow& window);
        sf::Text makeText(const std::string str);

        void updateSockets(const bool& windowOpen);
        void attemptClientConnect();
        void clientSend();
        void clientDisconnectSend();
        void clientReceive();
        void attemptServerStart();
        void serverReceive();
        void serverSend();
        void serverDisconnectSend();
        void disconnect();

    protected:

    private:
        sf::Font font;

        std::vector<sf::Text> chat;
        std::string userText;
        std::string userName;

        enum Role {Disconnected, Client, Server} role;
        enum MessageType {Normal, Disconnecting} messageType;

        sf::UdpSocket socket;
        IpPair server;
        bool clientReady, serverReady;
        std::vector<std::string> messagesToSend;
        std::vector<IpPair> clients;

        std::mutex messagesM, chatM;
};

#endif // CONSOLE_H
