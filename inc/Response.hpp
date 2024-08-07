#pragma once

#include "Webserv.hpp"

enum eResponseStatus
{
	WRITING = 69,
	BUILDING,
	READY,
	SENT
};

class	Response
{
	private:
		int									_kqueue;

		Request*							_request;
		Config*								_config;
		Socket*								_clientSocket;
		CgiHandler*							_cgiHandler;

		size_t								_bytesSent;
		size_t								_totalBytes;

		size_t								_currentChunkOffset;
		std::string							_responseChunk;

		bool								_streamer;
		std::ifstream						*_fileStream;

		std::string							_cookie;

	public:

		eResponseStatus						_status;
		int									_HTTPcode;
		std::string							_statusMessage;
		std::map<std::string, std::string>	_headers;
		std::string							_body;
		std::string							_resultResponse;

		Response(Request* request, Config* config, Socket* clientSocket, int kqueue);
		~Response();

		// Getters
		eResponseStatus						getStatus() const { return (_status); };
		int									getHTTPcode() const { return (_HTTPcode); };
		std::string							getResultResponse() const { return (_resultResponse); };
		size_t								getBytesSent() const { return (_bytesSent); };
		std::map<std::string, std::string>	getHeaders() const { return (_headers); };
		std::string							getBody() const { return (_body); };
		std::string							getStatusMessage() const { return (_statusMessage); };
		Socket*								getClientSocket() const { return (_clientSocket); };
		CgiHandler*							getCgiHandler() const { return (_cgiHandler); };
		std::string							getCookie() const { return (_cookie); };

		void	setResultResponse(std::string resultResponse) { _resultResponse = resultResponse; };

		// Setters
		void	setStatus(eResponseStatus status) { _status = status; };

		// Methods
		void	interpretRequest();
		void	sendResponse();

		// Orchestrales pour GET
		void	handleGet(const std::string &path);

		// Orchestrales pour DELETE
		void	handleDelete(const std::string &path);

		// Utils
		void	buildErrorPage(int errorCode);
		void	formatResponseToStr();
		void	buildListingPage();
		int		isFileOrDir(const std::string &str);
		bool	isAllowedMethod(const std::string &method, Route *route);
		bool	handleRequestTooLarge();
		bool	handleUriTooLarge(const std::string &uri);

		// Cookies
		void	generateAndSetCookie();

		// Debug
		void	printResponse();
};
