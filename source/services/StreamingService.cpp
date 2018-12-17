/*
 * Plugin Includes
 */
#include "include/services/StreamingService.hpp"
#include "include/common/Logger.hpp"

/*
========================================================================================================
	Constructors / Destructor
========================================================================================================
*/

StreamingService::StreamingService() : 
	ServiceT("StreamingService", "StreamingService"),
	m_streamingOutput(nullptr) {
	this->setupEvent(obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STARTING,
		&StreamingService::onStreamStarting);

	this->setupEvent(obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_LAUNCHING, 
		&StreamingService::onStreamLaunching);

	this->setupEvent(obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STARTED,
		&StreamingService::onStreamStarted);

	this->setupEvent(obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STOPPING,
		&StreamingService::onStreamStopping);

	this->setupEvent(obs_frontend_event::OBS_FRONTEND_EVENT_STREAMING_STOPPED,
		&StreamingService::onStreamStopped);

	this->setupEvent<const rpc_event_data&>(
		Streamdeck::rpc_event::STREAMING_STATUS_CHANGED_SUBSCRIBE,
		&StreamingService::subscribeStreamStatusChange
	);

	this->setupEvent<const rpc_event_data&>(
		Streamdeck::rpc_event::START_STREAMING,
		&StreamingService::startStreaming);

	this->setupEvent<const rpc_event_data&>(
		Streamdeck::rpc_event::STOP_STREAMING,
		&StreamingService::stopStreaming);
}

StreamingService::~StreamingService() {
	disconnectOutputHandler();
}

/*
========================================================================================================
	RPC Event Handling
========================================================================================================
*/

bool
StreamingService::subscribeStreamStatusChange(const rpc_event_data& data) {
	rpc_adv_response<std::string> response = response_string(&data, "subscribeStreamStatusChange");
	if(data.event == Streamdeck::rpc_event::STREAMING_STATUS_CHANGED_SUBSCRIBE) {
		response.event = Streamdeck::rpc_event::STREAMING_STATUS_CHANGED_SUBSCRIBE;
		logInfo("Subscription to streaming event required.");

		if(!checkResource(&data, QRegExp("(.+)"))) {
			// This streamdeck doesn't provide any resource to warn on stream state change
			logError("Streamdeck didn't provide resourceId to subscribe.");
			return false;
		}

		response.data = QString("%1.%2")
			.arg(data.serviceName.c_str())
			.arg(data.method.c_str())
			.toStdString();

		return streamdeckManager()->commit_to(response, &StreamdeckManager::setSubscription);
	}

	logError("subscribeStreamStatusChange not called by STREAMING_STATUS_CHANGED_SUBSCRIBED");
	return false;
}

bool
StreamingService::startStreaming(const rpc_event_data& data) {
	rpc_adv_response<bool> response = response_bool(&data, "startStreaming");

	if(data.event == Streamdeck::rpc_event::START_STREAMING) {
		response.event = Streamdeck::rpc_event::START_STREAMING;
		logInfo("Streamdeck has required start streaming...");
		if(checkResource(&data, QRegExp("startStreaming"))) {
			if(obs_frontend_streaming_active() == false) {
				obs_frontend_streaming_start();
				// OBS triggers an event when stream is started/stopped.
				// We use this event to detect if an error occured on starting/stopping stream.
				// That's why we don't send any response to the streamdeck yet in that case.
				return true;
			}
		}
		logError("Error : StreamingService::startStreaming not called by StreamDeck. "
			"Starting stream aborted.");
	}
	else
		logError("startStreaming not called by START_STREAMING");

	return streamdeckManager()->commit_to(response, &StreamdeckManager::setError);
}

bool
StreamingService::stopStreaming(const rpc_event_data& data) {
	rpc_adv_response<bool> response = response_bool(&data, "stopStreaming");

	if(data.event == Streamdeck::rpc_event::STOP_STREAMING) {
		response.event = Streamdeck::rpc_event::STOP_STREAMING;
		logInfo("Streamdeck has required stop streaming...");
		if(checkResource(&data, QRegExp("stopStreaming"))) {
			if(obs_frontend_streaming_active() == true) {
				obs_frontend_streaming_stop();
				// OBS triggers an event when stream is started/stopped.
				// We use this event to detect if an error occured on starting/stopping stream.
				// That's why we don't send any response to the streamdeck yet in that case.
				return true;
			}
		}
		logError("Error : StreamingService::stopStreaming not called by StreamDeck. "
			"Stopping stream aborted.");
	}
	else
		logError("stopStreaming not called by STOP_STREAMING");

	return streamdeckManager()->commit_to(response, &StreamdeckManager::setError);
}

/*
========================================================================================================
	OBS Signals Helpers
========================================================================================================
*/

bool
StreamingService::connectOutputHandler() {
	if(m_streamingOutput != nullptr)
		disconnectOutputHandler();
	m_streamingOutput = obs_frontend_get_streaming_output();
	if(m_streamingOutput != nullptr) {
		signal_handler_t* signal_handler = obs_output_get_signal_handler(m_streamingOutput);
		if(signal_handler != nullptr) {
			signal_handler_connect(signal_handler, "starting",
				StreamingService::onStreamStarting, this);
			signal_handler_connect(signal_handler, "start",
				StreamingService::onStreamStarted, this);
			signal_handler_connect(signal_handler, "stopping",
				StreamingService::onStreamStopping, this);
			signal_handler_connect(signal_handler, "stop",
				StreamingService::onStreamStopped, this);
			signal_handler_connect(signal_handler, "reconnect",
				StreamingService::onStreamReconnecting, this);
			signal_handler_connect(signal_handler, "reconnect_success",
				StreamingService::onStreamReconnected, this);
			return true;
		}
	}
	return false;
}

void
StreamingService::disconnectOutputHandler() {
	if(m_streamingOutput != nullptr) {
		signal_handler_t* signal_handler = obs_output_get_signal_handler(m_streamingOutput);
		if(signal_handler != nullptr) {
			signal_handler_disconnect(signal_handler, "starting",
				StreamingService::onStreamStarting, this);
			signal_handler_disconnect(signal_handler, "start",
				StreamingService::onStreamStarted, this);
			signal_handler_disconnect(signal_handler, "stopping",
				StreamingService::onStreamStopping, this);
			signal_handler_disconnect(signal_handler, "stop",
				StreamingService::onStreamStopped, this);
			signal_handler_disconnect(signal_handler, "reconnect",
				StreamingService::onStreamReconnecting, this);
			signal_handler_disconnect(signal_handler, "reconnect_success",
				StreamingService::onStreamReconnected, this);
		}
		m_streamingOutput = nullptr;
	}
}

bool
StreamingService::checkOutput(calldata_t* data) const {
	obs_output_t* output = nullptr;
	calldata_get_ptr(data, "output", &output);
	if(output != m_streamingOutput) {
		logError("Error: StreamingOutput received by the callback is different from "
			"the registered one.");
		obs_frontend_streaming_stop();
		return false;
	}

	return true;
}

/*
========================================================================================================
	OBS Event Handling
========================================================================================================
*/

bool
StreamingService::onStreamLaunching() {
	logInfo("OBS output is ready. OBS is launching stream.");

	if(connectOutputHandler())
		return true;

	logError("Error: Output is NULL.");
	obs_frontend_streaming_stop();
	return false;
}

bool
StreamingService::onStreamStarting() {
	logInfo("OBS is starting stream.");
	return true;
}

void
StreamingService::onStreamStarting(void* streamingService, calldata_t* data) {
	StreamingService* service = reinterpret_cast<StreamingService*>(streamingService);

	if(!service->checkOutput(data)) return;

	service->logInfo("OBS output is starting stream.");
	rpc_adv_response<std::string> response = service->response_string(nullptr, "onStreamingStarting");
	response.event = Streamdeck::rpc_event::STREAMING_STATUS_CHANGED_SUBSCRIBE;
	response.data = "starting";
	service->streamdeckManager()->commit_all(response, &StreamdeckManager::setEvent);
}

bool
StreamingService::onStreamStarted() {
	logInfo("OBS has started stream.");
	return true;
}

void
StreamingService::onStreamStarted(void* streamingService, calldata_t* data) {
	StreamingService* service = reinterpret_cast<StreamingService*>(streamingService);

	if(!service->checkOutput(data)) return;

	service->logInfo("OBS output has started stream");
	rpc_adv_response<std::string> response = service->response_string(nullptr, "onStreamingStarted");
	response.event = Streamdeck::rpc_event::STREAMING_STATUS_CHANGED_SUBSCRIBE;
	response.data = "live";
	service->streamdeckManager()->commit_all(response, &StreamdeckManager::setEvent);

	rpc_adv_response<bool> action = service->response_bool(nullptr, "onStreamingStarted");
	action.event = Streamdeck::rpc_event::START_STREAMING;
	action.data = false;
	service->streamdeckManager()->commit_all(action, &StreamdeckManager::setError);
}

bool
StreamingService::onStreamStopping() {
	logInfo("OBS is stopping stream.");
	return true;
}

void
StreamingService::onStreamStopping(void* streamingService, calldata_t* data) {
	StreamingService* service = reinterpret_cast<StreamingService*>(streamingService);

	if(!service->checkOutput(data)) return;

	service->logInfo("OBS output is stopping stream");
	rpc_adv_response<std::string> response = service->response_string(nullptr, "onStreamingStopping");
	response.event = Streamdeck::rpc_event::STREAMING_STATUS_CHANGED_SUBSCRIBE;
	response.data = "ending";
	service->streamdeckManager()->commit_all(response, &StreamdeckManager::setEvent);
}

bool
StreamingService::onStreamStopped() {
	logInfo("OBS has stopped stream.");
	return true;
}

void
StreamingService::onStreamStopped(void* streamingService, calldata_t* data) {
	StreamingService* service = reinterpret_cast<StreamingService*>(streamingService);

	if(!service->checkOutput(data)) return;

	service->disconnectOutputHandler();

	long long code = -1;
	calldata_get_int(data, "code", &code);

	service->logInfo("OBS output has stopped stream.");
	rpc_adv_response<std::string> response = service->response_string(nullptr, "onStreamingStopped");
	response.event = Streamdeck::rpc_event::STREAMING_STATUS_CHANGED_SUBSCRIBE;
	response.data = "offline";
	service->streamdeckManager()->commit_all(response, &StreamdeckManager::setEvent);

	rpc_adv_response<bool> action = service->response_bool(nullptr, "onStreamingStopped");
	if(code == 0) {
		action.event = Streamdeck::rpc_event::STOP_STREAMING;
		action.data = false;
	}
	else {
		action.event = Streamdeck::rpc_event::START_STREAMING;
		action.data = true;
	}
	service->streamdeckManager()->commit_all(action, &StreamdeckManager::setError);
}

void
StreamingService::onStreamReconnecting(void* streamingService, calldata_t* data) {
	StreamingService* service = reinterpret_cast<StreamingService*>(streamingService);

	if(!service->checkOutput(data)) return;

	service->logInfo("OBS output is reconnecting stream");
	rpc_adv_response<std::string> response = service->response_string(nullptr, "onStreamingReconnecting");
	response.event = Streamdeck::rpc_event::STREAMING_STATUS_CHANGED_SUBSCRIBE;
	response.data = "reconnecting";
	service->streamdeckManager()->commit_all(response, &StreamdeckManager::setEvent);
}

void
StreamingService::onStreamReconnected(void* streamingService, calldata_t* data) {
	StreamingService* service = reinterpret_cast<StreamingService*>(streamingService);

	if(!service->checkOutput(data)) return;

	service->logInfo("OBS output has reconnected stream");
	rpc_adv_response<std::string> response = service->response_string(nullptr, "onStreamingReconnected");
	response.event = Streamdeck::rpc_event::STREAMING_STATUS_CHANGED_SUBSCRIBE;
	response.data = "live";
	service->streamdeckManager()->commit_all(response, &StreamdeckManager::setEvent);
}