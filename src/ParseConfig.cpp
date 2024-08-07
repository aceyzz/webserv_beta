#include "ParseConfig.hpp"

ParseConfig::ParseConfig(std::string path)
{
	loadConfig(path);
}

ParseConfig::~ParseConfig()
{
}

void	ParseConfig::trimComments(std::string *str)
{
	size_t	pos = str->find("#");

	if (pos != std::string::npos)
		str->erase(pos);
}

void	ParseConfig::loadConfig(std::string path)
{
	std::ifstream	file(path.c_str());
	if (!file.is_open())
		throw std::runtime_error("could not open file " + path);

	std::string					line;
	std::string					currentConfig;
	std::vector<std::string>	configs;

	while (std::getline(file, line))
	{
		trimComments(&line);
		if (line.empty())
			continue;

		if (line.find("server {") != std::string::npos)
		{
			if (!currentConfig.empty())
				configs.push_back(currentConfig);
			currentConfig.clear();
		}
		currentConfig += line + "\n";
	}

	if (!currentConfig.empty())
		configs.push_back(currentConfig);

	file.close();

	for (size_t i = 0; i < configs.size(); i++)
		parseEachConfig(configs[i]);

	checkServerConfig();
}

void ParseConfig::parseEachConfig(std::string config)
{
	Config*	newConfig = new Config();

	std::istringstream			iss(config);
	std::string					line;
	std::vector<std::string>	locationBlocks;
	std::string					remainingConfig;

	while (std::getline(iss, line))
	{
		if (line.find("location") != std::string::npos)
		{
			std::string locationBlock = line + "\n";
			while (std::getline(iss, line) && line.find("}") == std::string::npos)
				locationBlock += line + "\n";
			locationBlock += line + "\n";
			locationBlocks.push_back(locationBlock);
		}
		else
			remainingConfig += line + "\n";
	}

	parseServerConfig(newConfig, remainingConfig);
	parseLocationBlocks(newConfig, locationBlocks);

	_configs.push_back(newConfig);
}

void	ParseConfig::parseServerConfig(Config* config, std::string remainingConfig)
{
	std::istringstream iss(remainingConfig);
	std::string line;

	config->addErrorPage(200, "errors/200.html");
	config->addErrorPage(201, "errors/201.html");
	config->addErrorPage(204, "errors/204.html");
	config->addErrorPage(301, "errors/301.html");
	config->addErrorPage(302, "errors/302.html");
	config->addErrorPage(303, "errors/303.html");
	config->addErrorPage(307, "errors/307.html");
	config->addErrorPage(400, "errors/400.html");
	config->addErrorPage(401, "errors/401.html");
	config->addErrorPage(403, "errors/403.html");
	config->addErrorPage(404, "errors/404.html");
	config->addErrorPage(405, "errors/405.html");
	config->addErrorPage(413, "errors/413.html");
	config->addErrorPage(414, "errors/414.html");
	config->addErrorPage(415, "errors/415.html");
	config->addErrorPage(429, "errors/429.html");
	config->addErrorPage(499, "errors/499.html");
	config->addErrorPage(500, "errors/500.html");
	config->addErrorPage(501, "errors/501.html");
	config->addErrorPage(502, "errors/502.html");
	config->addErrorPage(503, "errors/503.html");
	config->addErrorPage(504, "errors/504.html");
	config->addErrorPage(505, "errors/505.html");

	while (std::getline(iss, line))
	{
		if (line.find("listen") != std::string::npos) {
			size_t pos = line.find("listen") + 6;
			try {
				config->addPort(std::stoi(line.substr(pos, line.find(";") - pos)));
			} catch (std::exception &e) {
				config->addPort(-1);
				continue;
			}
		} else if (line.find("name") != std::string::npos) {
			size_t pos = line.find("name") + 4;
			if (trimWhitespaces(line.substr(pos, line.find(";") - pos)) == "localhost")
				config->setName("127.0.0.1");
			else
				config->setName(trimWhitespaces(line.substr(pos, line.find(";") - pos)));
		} else if (line.find("root") != std::string::npos) {
			size_t pos = line.find("root") + 4;
			config->setRoot(trimWhitespaces(line.substr(pos, line.find(";") - pos)));
		} else if (line.find("index") != std::string::npos) {
			size_t pos = line.find("index") + 5;
			config->setIndex(trimWhitespaces(line.substr(pos, line.find(";") - pos)));
		} else if (line.find("error_page") != std::string::npos) {
			size_t pos = line.find("error_page") + 10;
			std::istringstream errorStream(line.substr(pos, line.find(";", pos) - pos));
			int code;
			std::string path;
			errorStream >> code >> path;
			config->addErrorPage(code, trimWhitespaces(path));
		} else if (line.find("max_body_size") != std::string::npos) {
			size_t pos = line.find("max_body_size") + 13;
			try {
				config->setMaxBodySize(std::stoi(line.substr(pos, line.find(";", pos) - pos)));
			} catch (std::exception &e) {
				config->setMaxBodySize(-1);
			}
		} else if (line.find("listing") != std::string::npos) {
			size_t pos = line.find("listing") + 8;
			config->setAutoindex(line.substr(pos, line.find(";", pos) - pos) == "on");
		}
	}
}

void	ParseConfig::parseLocationBlocks(Config* config, std::vector<std::string> locationBlocks)
{
	for (size_t i = 0; i < locationBlocks.size(); i++)
	{
		Route* 				route = new Route();
		Cgi* 				cgi = new Cgi();
		std::istringstream	iss(locationBlocks[i]);
		std::string			line;
		bool				isCgiBlock = false;

		while (std::getline(iss, line))
		{
			if (line.find("location") != std::string::npos) {
				size_t pos = line.find("location") + 8;
				route->setUri(trimWhitespaces(line.substr(pos, line.find("{") - pos)));
			} else if (line.find("root") != std::string::npos) {
				size_t pos = line.find("root") + 4;
				route->setRoot(trimWhitespaces(line.substr(pos, line.find(";") - pos)));
			} else if (line.find("methods") != std::string::npos) {
				size_t pos = line.find("methods") + 7;
				std::istringstream methodStream(line.substr(pos, line.find(";") - pos));
				std::string method;
				std::vector<std::string> methods;
				while (methodStream >> method)
					methods.push_back(method);
				route->setMethod(methods);
			} else if (line.find("index") != std::string::npos) {
				size_t pos = line.find("index") + 5;
				route->setIndex(trimWhitespaces(line.substr(pos, line.find(";") - pos)));
			} else if (line.find("cgi_path") != std::string::npos) {
				size_t pos = line.find("cgi_path") + 8;
				cgi->setPath(trimWhitespaces(line.substr(pos, line.find(";", pos) - pos)));
				isCgiBlock = true;
			} else if (line.find("cgi_ext") != std::string::npos) {
				size_t pos = line.find("cgi_ext") + 7;
				cgi->setExtension(trimWhitespaces(line.substr(pos, line.find(";", pos) - pos)));
				isCgiBlock = true;
			} else if (line.find("return 301") != std::string::npos) {
				size_t pos = line.find("return 301") + 10;
				std::string redirPath = trimWhitespaces(line.substr(pos, line.find(";", pos) - pos));
				route->setRedir(true);
				route->setRedirPath(redirPath);
			}
		}

		if (isCgiBlock)
		{
			route->setCgiEnabled(true);
			route->setCgi(cgi);
		}
		else
			delete (cgi);

		// Appliquer les valeurs par défaut si elles ne sont pas définies
		if (route->getRoot().empty()) {
			route->setRoot(config->getRoot());
		}
		if (route->getIndex().empty()) {
			route->setIndex(config->getIndex());
		}
		config->addRoute(route);
	}
}

std::string	ParseConfig::trimWhitespaces(std::string str)
{
	std::string	result = str;

	result.erase(0, result.find_first_not_of(" \t\n\r\f\v"));
	result.erase(result.find_last_not_of(" \t\n\r\f\v") + 1);

	return (result);
}

void	ParseConfig::checkServerConfig()
{
	for (size_t i = 0; i < _configs.size(); i++)
	{
		Config* config = _configs[i];

		std::vector<int> ports = config->getPort();
		if (ports.empty())
			throw std::runtime_error("port not defined in server block " + config->getName());
		for (size_t j = 0; j < ports.size(); j++)
		{
			if (ports[j] < 0 || ports[j] > 65535)
				throw std::runtime_error("port bad defined in server block " + config->getName());
		}
		if (config->getName().empty())
			throw std::runtime_error("name not defined in server block " + config->getName());
		if (config->getRoot().empty())
			throw std::runtime_error("root not defined in server block " + config->getName());
		// if (config->getIndex().empty())
		// 	throw std::runtime_error("index not defined in server block " + config->getName());
		if (config->getMaxBodySize() == -1)
			throw std::runtime_error("max body size not defined in server block " + config->getName());
		if (config->getErrorPages().empty())
			throw std::runtime_error("error pages not defined in server block " + config->getName());

		std::map<std::string, Route*> routes = config->getRoutes();
		for (std::map<std::string, Route*>::iterator it = routes.begin(); it != routes.end(); it++)
			checkRouteConfig(it->second);
	}
}

void	ParseConfig::checkRouteConfig(Route *route)
{
	if (route->getUri().empty())
		throw std::runtime_error("uri not defined in location block " + route->getRoot());
	if (route->getRoot().empty())
		throw std::runtime_error("root not defined in location block " + route->getRoot());
	// if (route->getIndex().empty())
	// 	throw std::runtime_error("index not defined in location block " + route->getRoot());
	if (route->getIsRedir() && route->getRedirPath().empty())
		throw std::runtime_error("redir path not defined in location block " + route->getRoot());
	if (route->getMethod().empty() && !route->getIsRedir())
		throw std::runtime_error("methods not defined in location block " + route->getRoot());
	if (route->getCgiEnabled())
	{
		if (route->getCgiPath().empty())
			throw std::runtime_error("cgi path not defined in location block " + route->getRoot());
		if (route->getCgiExtension().empty())
			throw std::runtime_error("cgi extension not defined in location block " + route->getRoot());
	}

	std::vector<std::string> methods = route->getMethod();
	for (size_t i = 0; i < methods.size(); i++)
	{
		if (methods[i] != "GET" && methods[i] != "POST" && methods[i] != "DELETE" && !methods[i].empty())
			throw std::runtime_error("invalid method in location block for " + methods[i]);
	}

	if (route->getRoot().empty())
		route->setRoot(route->getRoot());
}
