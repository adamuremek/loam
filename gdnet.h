#ifndef GDNET_SERVER_H
#define GDNET_SERVER_H

#include "scene/main/node.h"
#include <steam/steamnetworkingsockets.h>


//===============GDNetServer===============//

class GDNetServer : public Node {
	GDCLASS(GDNetServer, Node);

private:

protected:
	static void _bind_methods();

public:
	GDNetServer();
	~GDNetServer();

	void start_server(int port);
};


//===============GDNetClient===============//
class GDNetClient : public Node {
	GDCLASS(GDNetClient, Node);

private:

protected:
	static void _bind_methods();

public:
	GDNetClient();
	~GDNetClient();

	void connect_to_server(String host, int port);
};
#endif
