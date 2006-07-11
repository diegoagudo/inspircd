/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspsocket.h"
#include "helperfuncs.h"
#include "httpd.h"

/* $ModDesc: Provides HTTP serving facilities to modules */

class ModuleHttp;

static Server *Srv;
static ModuleHttp* HttpModule;
extern time_t TIME;

enum HttpState
{
	HTTP_LISTEN = 0,
	HTTP_SERVE_WAIT_REQUEST = 1,
	HTTP_SERVE_SEND_DATA = 2
};

class HttpSocket : public InspSocket
{
	FileReader* index;
	HttpState InternalState;
	std::stringstream headers;

 public:

	HttpSocket(std::string host, int port, bool listening, unsigned long maxtime, FileReader* index_page) : InspSocket(host, port, listening, maxtime), index(index_page)
	{
		log(DEBUG,"HttpSocket constructor");
		InternalState = HTTP_LISTEN;
	}

	HttpSocket(int newfd, char* ip, FileReader* ind) : InspSocket(newfd, ip), index(ind)
	{
		InternalState = HTTP_SERVE_WAIT_REQUEST;
	}

	virtual int OnIncomingConnection(int newsock, char* ip)
	{
		if (InternalState == HTTP_LISTEN)
		{
			HttpSocket* s = new HttpSocket(newsock, ip, index);
			Srv->AddSocket(s);
		}
		return true;
	}

	virtual void OnClose()
	{
	}

	void SendHeaders(unsigned long size)
	{
		struct tm *timeinfo = localtime(&TIME);
		this->Write("HTTP/1.1 200 OK\r\nDate: ");
		this->Write(asctime(timeinfo));
		this->Write("Server: InspIRCd/m_httpd.so/1.1\r\nContent-Length: "+ConvToStr(size)+
				"\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n");
	}

	virtual bool OnDataReady()
	{
		char* data = this->Read();
		std::string request_type;

		/* Check that the data read is a valid pointer and it has some content */
		if (data && *data)
		{
			headers << data;

			if (headers.str().find("\r\n\r\n") != std::string::npos)
			{
				/* Headers are complete */
				InternalState = HTTP_SERVE_SEND_DATA;

				headers >> request_type;
				headers >> uri;

				if ((request_type == "GET") && (uri = "/"))
				{
					SendHeaders(index->ContentSize());
					this->Write(index->Contents());
				}
				else
				{
					HttpRequest httpr(request_type,uri,headers,this,this->GetIP());
					Event e(uri, HttpModule, "httpd_url");
				}

				return false;
			}
			return true;
		}
		else
		{
			/* Bastard client closed the socket on us!
			 * Oh wait, theyre SUPPOSED to do that!
			 */
			return false;
		}
	}
};

class ModuleHttp : public Module
{
	int port;
	std::string host;
	std::string bindip;
	std::string indexfile;

	FileReader index;

	HttpSocket* http;

 public:

	void ReadConfig()
	{
		ConfigReader c;
		this->host = c.ReadValue("http", "host", 0);
		this->bindip = c.ReadValue("http", "ip", 0);
		this->port = c.ReadInteger("http", "port", 0, true);
		this->indexfile = c.ReadValue("http", "index", 0);

		index.LoadFile(this->indexfile);
	}

	void CreateListener()
	{
		http = new HttpSocket(this->bindip, this->port, true, 0, &index);
		if ((http) && (http->GetState() == I_LISTENING))
		{
			Srv->AddSocket(http);
		}
	}

	ModuleHttp(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		ReadConfig();
		CreateListener();
	}

	void OnEvent(Event* event)
	{
	}

	char* OnRequest(Request* request)
	{
		return NULL;
	}

	void Implements(char* List)
	{
		List[I_OnEvent] = List[I_OnRequest] = 1;
	}

	virtual ~ModuleHttp()
	{
		Srv->DelSocket(http);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleHttpFactory : public ModuleFactory
{
 public:
	ModuleHttpFactory()
	{
	}
	
	~ModuleHttpFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		ModuleHttp = new ModuleHttp(Me);
		return new HttpModule;
	}
};


extern "C" void * init_module( void )
{
	return new ModuleHttpFactory;
}
