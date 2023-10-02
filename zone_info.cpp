#include "gdnet.h"


ZoneInfo::ZoneInfo() {
	m_zoneId = -1;
}

//==Protected Methods==//

void ZoneInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_zone_id"), &ZoneInfo::get_zone_id);
}

//==Public Methods==//

int ZoneInfo::get_zone_id() {
	return m_zoneId;
}

void ZoneInfo::set_zone_id(int zoneId) {
	m_zoneId = zoneId;
}

String ZoneInfo::get_zone_name() {
	return m_zoneName;
}

void ZoneInfo::set_zone_name(String zoneName) {
	m_zoneName = zoneName;
}
