/*
 * Plugin Includes
 */
#include "include/obs/Collection.hpp"
#include "include/obs/Scene.hpp"
#include "include/obs/Item.hpp"
#include "include/obs/ItemGroup.hpp"

 /*
  * Qt Includes
  */
#include <QString>

/*
========================================================================================================
	Constructors / Destructor
========================================================================================================
*/

Item::Item(Scene* scene, uint16_t id, obs_sceneitem_t* item) :
	OBSStorable(id, obs_source_get_name(obs_sceneitem_get_source(item))),
	m_parentScene(scene),
	m_item(item),
	m_ownerItem(nullptr) {
	m_source = obs_sceneitem_get_source(m_item);
	m_visible = obs_sceneitem_visible(m_item);
}

Item::Item(Scene* scene, uint16_t id, const std::string& name) :
	OBSStorable(id, name),
	m_parentScene(scene),
	m_ownerItem(nullptr) {
}

Item::~Item() {
	if(m_sourceRef != nullptr)
		m_sourceRef->removeReference(const_cast<Source**>(&m_sourceRef));
}

/*
========================================================================================================
	Accessors
========================================================================================================
*/

Scene*
Item::scene() const {
	return m_parentScene;
}

const Source*
Item::source() const {
	return m_sourceRef;
}

obs_sceneitem_t*
Item::item() const {
	return m_item;
}

ItemGroup*
Item::owner() const {
	return m_ownerItem;
}

void
Item::item(obs_sceneitem_t* item) {
	if(m_ownerItem != nullptr)
		m_ownerItem->remove(this);
	m_ownerItem = nullptr;
	m_item = item;
	m_source = obs_sceneitem_get_source(item);
	m_visible = obs_sceneitem_visible(m_item);
}

const char*
Item::type() const {
	return "item";
}

bool
Item::visible() const {
	return m_parentScene->collection()->active && m_visible;
}

bool
Item::visible(bool toggle, bool rpc_action) {
	if(m_parentScene->collection()->active) {
		m_visible = toggle;
		if(rpc_action) {
			obs_sceneitem_set_visible(m_item, toggle);
			bool result = obs_sceneitem_visible(m_item);
			return result == toggle;
		}
		return true;
	}
	return false;
}