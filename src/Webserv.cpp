#include "Webserv.hpp"

Webserver::Webserver(std::vector<Config*> config) : _kqueue(-1), _configs(config)
{
	for (size_t i = 0; i < _configs.size(); i++)
		_configsByPort[_configs[i]->getPort()] = _configs[i];
}

Webserver::~Webserver()
{
	for (std::map<int, Socket*>::iterator it = _serverSockets.begin(); it != _serverSockets.end(); it++)
		if (it->second != NULL)
			delete it->second;
	_serverSockets.clear();
	for (size_t i = 0; i < _clientSockets.size(); i++)
		if (_clientSockets[i] != NULL)
			delete _clientSockets[i];
	_clientSockets.clear();
	for (std::map<int, Request*>::iterator it = _requests.begin(); it != _requests.end(); it++)
		if (it->second != NULL)
			delete it->second;
	_requests.clear();
	for (std::map<int, Response*>::iterator it = _responses.begin(); it != _responses.end(); it++)
		if (it->second != NULL)
			delete it->second;
	_responses.clear();
	if (_kqueue != -1)
		close(_kqueue);
}

void	Webserver::printConfigs()
{
	for (size_t j = 0; j < _configs.size(); j++)
	{
		Config* config = _configs[j];
		std::cout << std::endl;
		std::cout << CYAN << "PRINT CONFIG (CLASS WEBSERVER):" << RST << std::endl;
		std::cout << GRY2 "Address of config: " << config << std::endl;
		std::cout << YLLW << "Name: " << RST << config->getName() << std::endl;
		std::cout << YLLW << "Port: " << RST << config->getPort() << std::endl;
		std::cout << YLLW << "Root: " << RST << config->getRoot() << std::endl;
		std::cout << YLLW << "Index: " << RST << config->getIndex() << std::endl;
		std::cout << YLLW << "Error pages: " << RST << std::endl;
		std::map<int, std::string> errorPages = config->getErrorPages();
		for (std::map<int, std::string>::iterator it = errorPages.begin(); it != errorPages.end(); it++)
			std::cout << "  " << it->first << " -> " << it->second << std::endl;
		std::cout << YLLW << "Autoindex: " << RST << (config->getAutoindex() ? "true" : "false") << std::endl;
		std::cout << YLLW << "Max body size: " << RST << config->getMaxBodySize() << std::endl;
		std::cout << YLLW << "Routes: " << RST << std::endl;
		std::map<std::string, Route*> routes = config->getRoutes();
		int i = 0;
		for (std::map<std::string, Route*>::iterator it = routes.begin(); it != routes.end(); it++)
		{
			Route* route = it->second;
			std::cout << "  " << GRY1 "Route [" << i+1 << "] " RST << std::endl;
			std::cout << "    " << YLLW << "Root: " << RST << route->getRoot() << std::endl;
			std::cout << "    " << YLLW << "Uri: " << RST << route->getUri() << std::endl;
			std::cout << "    " << YLLW << "Methods: " << RST << std::endl;
			std::vector<std::string> methods = route->getMethod();
			for (size_t k = 0; k < methods.size(); k++)
				std::cout << "      " << methods[k] << std::endl;
			std::cout << "    " << YLLW << "Index: " << RST << route->getIndex() << std::endl;
			std::cout << "    " << YLLW << "Cgi enabled: " << RST << (route->getCgiEnabled() ? "true" : "false") << std::endl;
			if (route->getCgiEnabled())
			{
				std::cout << "    " << YLLW << "Cgi path: " << RST << route->getCgiPath() << std::endl;
				std::cout << "    " << YLLW << "Cgi extension: " << RST << route->getCgiExtension() << std::endl;
			}
			std::cout << "    " << YLLW << "Is redir: " << RST << (route->getIsRedir() ? "true" : "false") << std::endl;
			if (route->getIsRedir())
				std::cout << "    " << YLLW << "Redir path: " << RST << route->getRedirPath() << std::endl;
		}
		std::cout << std::endl;
	}
}

void	Webserver::printConfigByPort()
{
	std::cout << std::endl;
	std::cout << GOLD << "PRINT CONFIG BY PORT (CLASS WEBSERVER):" << RST << std::endl;
	for (std::map<int, Config*>::iterator it = _configsByPort.begin(); it != _configsByPort.end(); it++)
	{
		Config* config = it->second;
		std::cout << GRY2 "Address of config: " << config << std::endl;
		std::cout << CYAN << "Port: " << RST << config->getPort() << std::endl;
		std::cout << CYAN << "Name: " << RST << config->getName() << std::endl;
	}
	std::cout << std::endl;
}

void	Webserver::printSockets()
{
	std::cout << GOLD "PRINT SERVER SOCKETS (CLASS WEBSERVER):" RST << std::endl;
	for (size_t i = 0; i < _serverSockets.size(); i++)
	{
		Socket* socket = _serverSockets[i];
		socket->printSocket();
	}
}

bool	Webserver::isServerSocket(int fd)
{
	return (_serverSockets.find(fd) != _serverSockets.end());
}

void	Webserver::addServerSocket(Socket *socket)
{
	_serverSockets[socket->getFD()] = socket;
}

void	Webserver::addClientSocket(Socket *socket)
{
	_clientSockets[socket->getFD()] = socket;
}

void	Webserver::initServer()
{
	_kqueue = kqueue();
	if (_kqueue == -1)
		throw std::runtime_error("kqueue() failed");

	// Creation des sockets serveurs
	for (size_t i = 0; i < _configs.size(); i++)
	{
		Config* config = _configs[i];
		Socket* socket = new Socket(SERVER, AF_INET, SOCK_STREAM, 0, config->getPort(), config->getName());
		_serverSockets.insert(std::pair<int, Socket*>(socket->getFD(), socket));

		if (fcntl(socket->getFD(), F_SETFL, O_NONBLOCK) == -1)
			throw std::runtime_error("fcntl() failed: " + std::string(strerror(errno)));

		struct kevent	event;
		EV_SET(&event, socket->getFD(), EVFILT_READ, EV_ADD, 0, 0, NULL);
		if (kevent(_kqueue, &event, 1, NULL, 0, NULL) == -1)
			throw std::runtime_error("kevent() failed: " + std::string(strerror(errno)));
	}

	// Bind et listen des sockets serveurs
	for (std::map<int, Socket*>::iterator it = _serverSockets.begin(); it != _serverSockets.end(); ++it)
	{
		Socket* socket = it->second;
		socket->bindSocket();
		socket->listenSocket();
	}
}

void	Webserver::runServer()
{
	std::cout << CLRALL;
	std::cout << LIME "Server is running..." RST << std::endl;
	while (g_signal)
	{
		// Creation du vecteur d'evenements
		std::vector<struct kevent> events(MAX_EVENTS);
		// On attend les evenements
		int nbEvents = kevent(_kqueue, NULL, 0, &events[0], MAX_EVENTS, NULL);
		if (nbEvents == -1 && g_signal)
			throw std::runtime_error("kevent() failed: " + std::string(strerror(errno)));
		// On parcourt les evenements
		for (int i = 0; i < nbEvents; i++)
		{
			// Si EV_ERROR (erreur dans kevent) alors on continue
			if (events[i].flags & EV_ERROR)
			{
				std::cerr << "Error in kevent" << std::endl;
				continue;
			}
			// Si c'est un socket serveur en comparant avec le FD de l'evenement
			if (isServerSocket(events[i].ident))
			{
				if (events[i].filter == EVFILT_READ)
					acceptNewClient(events[i].ident);
			}
			// Sinon c'est un socket client
			else
			{
				// Si c'est en lecture
				if (events[i].filter == EVFILT_READ)
				{
					if (receiveRequest(events[i].ident))
						parseAndHandleRequest(events[i].ident);
				}
				// Sinon c'est en ecriture
				else if (events[i].filter == EVFILT_WRITE)
				{
					if (responseManager(events[i].ident))
						if (_responses[events[i].ident]->getStatus() == SENT)
							closeClient(events[i].ident);
				}
			}
		}
	}
}

bool	Webserver::receiveRequest(int clientFD)
{
	char	buffer[BUFFER_SIZE];
	ssize_t	nbBytes = recv(clientFD, buffer, sizeof(buffer) - 1, 0);
	static ssize_t	nbBytesTotal = 0;

	if (nbBytes <= 0)
	{
		// Error handling or connection closed by client
		close(clientFD);
		_requests.erase(clientFD);
		_clientSockets.erase(clientFD);
		return (false);
	}

	buffer[nbBytes] = '\0';
	Request*	request = _requests[clientFD];
	if (!request)
	{
		// Find the client socket based on clientFD
		Socket*	clientSocket = _clientSockets[clientFD];
		if (!clientSocket)
		{
			std::cerr << "Client socket not found for FD: " << clientFD << std::endl;
			return (false);
		}

		request = new Request(clientFD, clientSocket->getIp());
		_requests[clientFD] = request;
	}

	request->appendRawRequest(buffer);

	// Find the Content-Length in the headers
	std::string rawRequestLocal = request->getRawRequest();
	size_t posHeadersEnd = rawRequestLocal.find("\r\n\r\n");
	size_t contentLength = 0;
	if (posHeadersEnd != std::string::npos)
	{
		std::string headers = rawRequestLocal.substr(0, posHeadersEnd);
		size_t contentLengthPos = headers.find("Content-Length: ");
		if (contentLengthPos != std::string::npos)
		{
			contentLengthPos += 16; // length of "Content-Length: "
			size_t endPos = headers.find("\r\n", contentLengthPos);
			if (endPos != std::string::npos)
			{
				std::string contentLengthStr = headers.substr(contentLengthPos, endPos - contentLengthPos);
				try
				{
					contentLength = std::stoul(contentLengthStr);
				}
				catch (const std::exception& e) { std::cerr << "Failed to parse Content-Length: " << e.what() << std::endl; }
			}
		}
	}

	if (DEBUG)
		std::cout << "Received " << nbBytes << " bytes from client: " << request->getClientIp() << " (FD: " << clientFD << ")" << std::endl;

	nbBytesTotal += nbBytes;
	if (contentLength == 0 || static_cast<size_t>(nbBytesTotal) >= contentLength)
	{
		request->setStatus(COMPLETE);
		return (true);
	}

	return (false);
}

void	Webserver::acceptNewClient(int serverFD)
{
	sockaddr_in	clientAddr;
	socklen_t	clientAddrLen = sizeof(clientAddr);

	// On accepte la connexion du client
	int			clientFD = accept(serverFD, (struct sockaddr*)&clientAddr, &clientAddrLen);
	if (clientFD == -1)
	{
		std::cerr << "accept() failed: " << strerror(errno) << std::endl;
		return;
	}
	// On met le client en mode non-bloquant
	if (fcntl(clientFD, F_SETFL, O_NONBLOCK) == -1)
	{
		std::cerr << "fcntl() failed: " << strerror(errno) << std::endl;
		close(clientFD);
		return;
	}
	// On ajoute le client au kqueue
	struct kevent event;
	EV_SET(&event, clientFD, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
	if (kevent(_kqueue, &event, 1, NULL, 0, NULL) == -1)
	{
		std::cerr << "kevent() failed: " << strerror(errno) << std::endl;
		close(clientFD);
		return;
	}

	// On ajoute le client à la liste des sockets clients
	Socket* socket = new Socket(CLIENT, AF_INET, clientFD, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
	socket->setServerPort(_serverSockets[serverFD]->getPort());
	_clientSockets[clientFD] = socket;

	if (DEBUG)
		std::cout << "New client connected: " << socket->getIp() << ":" << socket->getServerPort()  << " with fd: " << socket->getFD() << std::endl;
}

void	Webserver::parseAndHandleRequest(int fd)
{
	Request* request = _requests[fd];

	if (!request)
	{
		std::cerr << "Request not found" << std::endl;
		close(fd);
		_requests.erase(fd);
		return ;
	}

	// Parser la requete et init des valeurs de la classe Request
	request->parseRequest(request->getRawRequest());

	if (DEBUG)
		request->printRequest();

	if (request->expectsContinue())
	{
		sendContinueResponse(fd);
		request->setStatus(CONTINUE);
		return;
	}

	// Mise a jour de l'evenement kqueue pour l'ecriture
	struct kevent	event;
	EV_SET(&event, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
	if (kevent(_kqueue, &event, 1, NULL, 0, NULL) == -1)
	{
		std::cerr << "kevent() failed: " << strerror(errno) << std::endl;
		close(fd);
		_requests.erase(fd);
		return ;
	}
}

bool	Webserver::responseManager(int clientFD)
{
	// Obtenir la requête du client
	Request* request = _requests[clientFD];
	if (!request)
	{
		std::cerr << "Request of client not found" << std::endl;
		return false;
	}

	// Obtenir la configuration correspondante
	Config* config = getConfigForClient(clientFD);
	if (!config)
	{
		std::cerr << "Config for client not found" << std::endl;
		return false;
	}

	// Récupérer la réponse, ou la créer si non existante
	Response* response = _responses[clientFD];
	if (!response)
	{
		response = new Response(request, config, _clientSockets[clientFD], _kqueue);
		_responses[clientFD] = response;
	}

	// Interpréter la requête
	response->interpretRequest();

	if (DEBUG && response->getStatus() == READY)
		response->printResponse();

	if (response->getStatus() == READY || response->getStatus() == WRITING)
	{
		response->sendResponse();
		if (response->getStatus() == SENT)
			return true;
	}

	return false;
}

void	Webserver::closeClient(int fd)
{
	if (fd != -1)
		close(fd);

	if (_requests[fd])
	{
		delete _requests[fd];
		_requests.erase(fd);
	}
	if (_responses[fd])
	{
		delete _responses[fd];
		_responses.erase(fd);
	}
	if (_clientSockets[fd])
	{
		delete _clientSockets[fd];
		_clientSockets.erase(fd);
	}

	if (DEBUG)
		std::cout << "Client closed: " << fd << std::endl;
}

Config*	Webserver::getConfigForClient(int clientFD)
{
	Socket* clientSocket = _clientSockets[clientFD];
	if (!clientSocket)
	{
		std::cerr << "Client socket not found for FD: " << clientFD << std::endl;
		return (NULL);
	}

	int	serverPort = clientSocket->getServerPort();

	Config* config = _configsByPort[serverPort];
	if (!config)
		return (NULL);

	return (config);
}

void	Webserver::sendContinueResponse(int clientFD)
{
	const char *continueMessage = "HTTP/1.1 100 Continue\r\n\r\n";
	send(clientFD, continueMessage, strlen(continueMessage), 0);
}
