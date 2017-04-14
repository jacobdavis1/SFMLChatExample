#include "Console.h"
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>
#include <string>
#include <vector>
#include <iostream>

Console::Console()
{
    userName = "No Name";
    font.loadFromFile("opensans.ttf");

    socket.setBlocking(false);

    server.port = 12000;

    clientReady = false;
    serverReady = false;

    role = Role::Disconnected;
}

Console::~Console()
{
    //dtor
}

//Main update function
void Console::update(const sf::Event& event)
{
    getInput(event);
}

//Functions used by or related to the update function below here
void Console::getInput(const sf::Event& event)
{
    if (event.type == sf::Event::KeyPressed)
    {
        if (event.key.code == sf::Keyboard::Return)
        {
            if (userText.size() > 0)
                sendMessage();
        }

        else if (event.key.code == sf::Keyboard::BackSpace)
        {
            if (userText.size() > 0)
                userText.pop_back();
        }

        else if (event.key.code == sf::Keyboard::Space)
        {
            userText += ' ';
        }
    }

    else if (event.type == sf::Event::TextEntered)
    {
        if (32 < event.text.unicode && event.text.unicode < 128)
            userText += (char)event.text.unicode;
    }
}

void Console::sendMessage()
{
    if (parseCommand())
        return;

    std::string finalString = userName + ": " + userText;
    sf::Packet packet;
    userText.clear();

    switch (role)
    {
        case Role::Disconnected:
            sendSystemMessage(finalString);
            break;

        default:
            messagesM.lock();
            messagesToSend.push_back(finalString);
            messagesM.unlock();
            break;
    }
}

void Console::sendSystemMessage(std::string text)
{
    chatM.lock();
    chat.push_back(makeText(text));
    chatM.unlock();
}

bool Console::parseCommand()
{
    std::string::iterator itor;

    //Replace with generic parsing function:
    //std::string parseUntil(std::string fullCommand, char stop, std::string::iterator &itor);
    //From:

    std::string command;

    for (itor = userText.begin(); *itor != ' ' && itor != userText.end(); ++itor)
    {
        command += *itor;
    }

    ++itor;

    //To here. Returns command, itor is moved along since it's a reference.

    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (command == "connect")
    {
        std::string fullIP = userText.substr(std::distance(userText.begin(), itor));

        server.ipaddress = fullIP;
        sendSystemMessage((std::string)"Attempting to connect to " + server.ipaddress.toString() + ".");
        userText.clear();

        role = Role::Client;

        return true;
    }

    else if (command == "listen")
    {
        sendSystemMessage("Beginning server.");
        userText.clear();

        role = Role::Server;

        return true;
    }

    //Would like name saved locally.
    else if (command == "setname")
    {
        userName = userText.substr(std::distance(userText.begin(), itor));
        sendSystemMessage((std::string)"Name changed to \'" + userName + "\'.");
        userText.clear();

        return true;
    }

    return false;
}

//Main show (draw) function
void Console::show(sf::RenderWindow& window)
{
    showUI(window);

    showChat(window);
}

//Functions called by or related to the show function below this line
void Console::showChat(sf::RenderWindow& window)
{
    chatM.lock();
    if (chat.size() > 0)
    {
        std::vector<sf::Text>::reverse_iterator rev;

        int count = 0;
        for (rev = chat.rbegin(); rev != chat.rend(); ++rev)
        {
            rev -> setPosition(20, 412 - count*34);
            ++count;

            window.draw(*rev);
        }
    }
    chatM.unlock();
}

void Console::showUI(sf::RenderWindow& window)
{
    std::string stringVersion = userName + ": " + userText;
    sf::Text text = makeText(stringVersion);

    text.setPosition(20, 446);

    window.draw(text);
}

sf::Text Console::makeText(const std::string str)
{
    sf::Text text;
    text.setFont(font);
    text.setCharacterSize(14);
    text.setFillColor(sf::Color::White);
    text.setString(str);

    return text;
}

//Main networking function
void Console::updateSockets(const bool& windowOpen)
{
    while (windowOpen)
    {
        switch (role)
        {
            default:
                break;

            case Role::Client:
                {
                    attemptClientConnect();

                    clientSend();

                    clientReceive();

                    break;
                }


            case Role::Server:
                {
                    attemptServerStart();

                    serverReceive();

                    serverSend();

                    break;
                }

        }
    }
}

//Functions called by or related to the networking function below this line
void Console::attemptClientConnect()
{
    if (!clientReady)
    {
        if (socket.bind(sf::UdpSocket::AnyPort) == sf::UdpSocket::Done)
        {
            clientReady = true;

            char port[50];
            sprintf(port, "%d", socket.getLocalPort());

            std::string firstMessage = userName + " is connecting...";

            messagesM.lock();
            messagesToSend.push_back(firstMessage);
            messagesM.unlock();

            sendSystemMessage((std::string)"Success. Socket bound on " + port + ". Say hello!");
        }
        else
        {
            role = Role::Disconnected;

            sendSystemMessage("Binding the socket has failed.");
        }
    }
}

void Console::clientSend()
{
    messagesM.lock();
    for (int i = 0; i < messagesToSend.size(); ++i)
    {
        sf::Packet packet;
        packet << MessageType::Normal << messagesToSend.at(i);

        for (int i = 0; i < 3; ++i)
        {
            if (socket.send(packet, server.ipaddress, server.port) != sf::UdpSocket::Done)
            {
                std::cout << "Send failed. Try " << i << " out of 3." << std::endl
                << "Server IP: " << server.ipaddress.toString() << std::endl;
            }
            else break;
        }
    }

    messagesToSend.clear();
    messagesM.unlock();
}

void Console::clientDisconnectSend()
{
    sf::Packet packet;
    packet << MessageType::Disconnecting << userName + " has disconnected.";

    messagesM.lock();
    for (int i = 0; i < 3; ++i)
    {
        if (socket.send(packet, server.ipaddress, server.port) != sf::UdpSocket::Done)
        {
            std::cout << "Send failed. Try " << i << " out of 3." << std::endl
            << "Server IP: " << server.ipaddress.toString() << std::endl;
        }
        else break;
    }
    messagesM.unlock();
}

void Console::clientReceive()
{
    sf::Packet packet;
    IpPair ip;
    int mt;

    if (socket.receive(packet, ip.ipaddress, ip.port) == sf::UdpSocket::Done)
    {
        packet >> mt;

        if (mt == MessageType::Disconnecting)
        {
            role = Role::Disconnected;
        }

        std::string finalString;
        if (packet >> finalString)
            sendSystemMessage(finalString);
        else
            sendSystemMessage((std::string)"Packet read failed: " + ip.ipaddress.toString());
    }
}


void Console::attemptServerStart()
{
    if (!serverReady)
    {
        if (socket.bind(server.port) == sf::UdpSocket::Done)
        {
            char port[50];
            sprintf(port, "%d", socket.getLocalPort());
            sendSystemMessage((std::string)"Success! Listening on port " + port);

            serverReady = true;

            userText.clear();
        }

        else
        {
            sendSystemMessage("Binding the socket has failed.");

            role = Role::Disconnected;

            userText.clear();
        }
    }
}

void Console::serverReceive()
{
    IpPair ip;
    sf::Packet packet;

    if (socket.receive(packet, ip.ipaddress, ip.port) == sf::UdpSocket::Done)
    {
        bool found = false;

        int mt;
        packet >> mt;

        for (int i = 0; i < clients.size(); ++i)
        {
            if (clients.at(i).ipaddress == ip.ipaddress
                && clients.at(i).port == ip.port)
            {
                if (mt == MessageType::Disconnecting)
                {
                    clients.erase(clients.begin() + i);
                }

                found = true;
                break;
            }
        }

        if(!found)
        {
            sendSystemMessage((std::string)"Incoming connection: " + ip.ipaddress.toString());
            clients.push_back(ip);
            sendSystemMessage("Added " + ip.ipaddress.toString());
        }

        messagesM.lock();
        std::string msg;
        if(packet >> msg)
        {
            messagesToSend.push_back(msg);
        }
        messagesM.unlock();

    }
}

void Console::serverSend()
{
    messagesM.lock();
    for (int i = 0; i < messagesToSend.size(); ++i)
    {
        sendSystemMessage(messagesToSend.at(i));

        for (int j = 0; j < clients.size(); ++j)
        {
            bool good = true;

            sf::Packet sendPacket;

            sendPacket << MessageType::Normal << messagesToSend.at(i);

            for (int i = 0; i < 3; ++i)
            {
                if (socket.send(sendPacket, clients.at(j).ipaddress, clients.at(j).port) != sf::UdpSocket::Done)
                {
                    std::cout << "Send failed for " << clients.at(j).ipaddress << ":" << clients.at(j).port << ". Try " << i << " out of 3." << std::endl;

                    if (i == 2)
                    {
                        good = false;
                    }
                }
                else break;
            }

            if (!good)
            {
                messagesM.lock();
                messagesToSend.push_back((std::string)"A client has been disconnected.");
                messagesM.unlock();

                clients.erase(clients.begin() + j);
            }
        }
    }

    messagesToSend.clear();
    messagesM.unlock();
}

void Console::serverDisconnectSend()
{
    sf::Packet packet;
    packet << MessageType::Disconnecting << (std::string)"The server has shut down.";

    messagesM.lock();
    for (int j = 0; j < clients.size(); ++j)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (socket.send(packet, clients.at(j).ipaddress, clients.at(j).port) != sf::UdpSocket::Done)
            {
                std::cout << "Send failed for " << clients.at(j).ipaddress << ":" << clients.at(j).port << ". Try " << i << " out of 3." << std::endl;
            }
            else break;
        }
    }
    messagesM.unlock();
}

void Console::disconnect()
{
    switch (role)
    {
    case Role::Client:
        clientDisconnectSend();
        break;

    case Role::Server:
        serverDisconnectSend();
        break;

    default:
        break;
    }
}
