/*
 * STL Includes
 */
#include <type_traits>

/*
 * Plugin Includes
 */
#include "include/streamdeck/StreamdeckManager.hpp"

/*
========================================================================================================
	Streamdeck Management
========================================================================================================
*/

template<typename T>
bool
StreamdeckManager::setResult(Streamdeck* client, const rpc::response<T>& response) {
	QString resource = response.request != nullptr ?
		QString("%1.%2")
		.arg(response.request->serviceName.c_str())
		.arg(response.request->method.c_str()) :
		QString("%1.%2")
		.arg(response.serviceName)
		.arg(response.method);

	return client->sendResult(response.event, resource.toStdString(), response.data);
}

template<typename T>
bool
StreamdeckManager::setEvent(Streamdeck* client, const rpc::response<T>& response) {
	return client->sendEvent(response.event, response.data);
}

template<>
inline bool
StreamdeckManager::setEvent(Streamdeck* client, const rpc::response<void>& response) {
	return client->sendEvent(response.event);
}

/*
========================================================================================================
	Messages Handling
========================================================================================================
*/

template<typename T>
bool
StreamdeckManager::commit_to(
	rpc::response<T>& response, 
	bool(StreamdeckManager::*functor)(Streamdeck*, const rpc::response<T>&)
) {
	if(response.request == nullptr ||
		(response.request != nullptr && response.request->client == nullptr)) {

		return false;
	}

	if(!validate(response)) {
		return false;
	}

	bool result = (this->*functor)(response.request->client, response);

	if(!result) {
		this->close(response.request->client);
	}

	return result;
}

template<typename T>
bool
StreamdeckManager::commit_all(
	rpc::response<T>& response, 
	bool(StreamdeckManager::*functor)(Streamdeck*, const rpc::response<T>&)
) {
	bool result = this->validate(response);

	auto commit_func = [this, &response, &result, &functor](Streamdeck* streamdeck) {
		if((this->*functor)(streamdeck, response) == false) {
			this->close(streamdeck);
			result = false;
		}
	};

	for(auto i = m_streamdecks.begin(); i != m_streamdecks.end();) {
		Streamdeck* client = *i;
		++i;
		commit_func(client);
	}

	return result;
}

template<typename T>
bool
StreamdeckManager::commit_any(
	rpc::response<T>& response, 
	bool(StreamdeckManager::*functor)(Streamdeck*, const rpc::response<T>&)
) {
	bool result = this->validate(response);

	auto commit_func = [this, &response, &result, &functor](Streamdeck* streamdeck) {
		if((this->*functor)(streamdeck, response) == false) {
			this->close(streamdeck);
			result = false;
		}
	};

	Streamdeck* client = response.request == nullptr ? nullptr : response.request->client;
	if(client != nullptr) {
		commit_func(client);
	}
	else {
		for(auto i = m_streamdecks.begin(); i != m_streamdecks.end();) {
			client = *i;
			++i;
			commit_func(client);
		}
	}

	return result;
}