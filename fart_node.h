#ifndef FART_NODE_H
#define FART_NODE_H

#include "scene/main/node.h";

class FartNode : public Node {
	GDCLASS(FartNode, Node);

protected:
	static void _bind_methods();

public:
	FartNode();

	String say_hello();
};

#endif // FART_NODE_H
