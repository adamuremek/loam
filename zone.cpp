#include "gdnet.h"


//===============Zone Implementation===============//

Zone::Zone() {
	print_line("POOP FART AHAHWHDAHWD");
}

Zone::~Zone() {

}

//==Protected Methods==//

void Zone::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_zone_scene"), &Zone::get_zone_scene);
	ClassDB::bind_method(D_METHOD("set_zone_scene", "zone_scene"), &Zone::set_zone_scene);

	//Expose zones to the inspector as an array
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "Zone Scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_zone_scene", "get_zone_scene");
}

//==Public Methods==//

Ref<PackedScene> Zone::get_zone_scene() const {
	return m_zoneScene;
}

void Zone::set_zone_scene(const Ref<PackedScene>& zoneScene) {
	m_zoneScene = zoneScene;
}

Ref<ZoneInfo> Zone::get_zone_info() const {
	return m_zoneInfo;
}

void Zone::set_zone_info(const Ref<ZoneInfo>& zoneInfo) {
	m_zoneInfo = zoneInfo;

	print_line(vformat("ZONE %s WITH ID %d HAS BEEN REGISTERED", m_zoneInfo->get_zone_name(), m_zoneInfo->get_zone_id()));
}
