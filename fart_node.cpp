#include "fart_node.h"

FartNode::FartNode() {

}

String FartNode::say_hello() {
	return "Hello from FART NODE!";
}

void FartNode::_bind_methods(){
	ClassDB::bind_method(D_METHOD("say_hello"), &FartNode::say_hello);
}
