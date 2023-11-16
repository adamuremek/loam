#include "gdnet.h"

void NetworkEntity::_notification(int n_type) {
	if(Engine::get_singleton()->is_editor_hint()){
		return;
	}

	switch (n_type) {
		case NOTIFICATION_READY: {
			break;
		}
		case NOTIFICATION_ENTER_TREE: {
			print_line("Cum fuckery");
			break;
		}
		case NOTIFICATION_EXIT_TREE: {
			print_line("Cum factory");
			break;
		}
	}
}
