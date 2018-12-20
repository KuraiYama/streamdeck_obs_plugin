/*
 * Plugin Includes
 */
#include "include/services/ApplicationService.hpp"
#include "include/common/Logger.hpp"

/*
========================================================================================================
	Constructors / Destructor
========================================================================================================
*/

ApplicationService::ApplicationService(QMainWindow* parent) :
	ServiceT("ApplicationService", ""),
	m_dialog(new InfoDialog(parent)),
	m_streamOutput(nullptr),
	m_recordOutput(nullptr) {
	m_streamingState = "offline";
	m_recordingState = "offline";

	/* OBS fully loaded event */
	this->setupEvent(OBS_FRONTEND_EVENT_FINISHED_LOADING, &ApplicationService::onApplicationLoaded);
	this->setupEvent(OBS_FRONTEND_EVENT_EXIT, &ApplicationService::onApplicationExit);
	this->setupEvent(OBS_FRONTEND_EVENT_STREAMING_LAUNCHING, &ApplicationService::onStreamLaunching);
	this->setupEvent(OBS_FRONTEND_EVENT_RECORDING_STARTING, &ApplicationService::onRecordStarting);

	this->setupEvent<const rpc_event_data&>(Streamdeck::rpc_event::GET_RECORD_STREAM_STATE,
		&ApplicationService::onGetRecordStreamState);
}

ApplicationService::~ApplicationService() {
	disconnectStreamOutputHandler();
	disconnectRecordOutputHandler();

	m_dialog->deleteLater();
}

/*
========================================================================================================
	Recording State Handler
========================================================================================================
*/

void
ApplicationService::onRecordStarting(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_recordOutput)) return;
	service->m_recordingState = "recording";
}

void
ApplicationService::onRecordStarted(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_recordOutput)) return;
	service->m_recordingState = "starting";
}

void
ApplicationService::onRecordStopping(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_recordOutput)) return;
	service->m_recordingState = "stopping";
}

void
ApplicationService::onRecordStopped(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_recordOutput)) return;
	service->m_recordingState = "offline";
	service->disconnectRecordOutputHandler();
}

/*
========================================================================================================
	Streaming State Handler
========================================================================================================
*/

void
ApplicationService::onStreamStarting(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_streamOutput)) return;
	service->m_streamingState = "starting";
}

void
ApplicationService::onStreamStarted(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_streamOutput)) return;
	service->m_streamingState = "live";
}

void
ApplicationService::onStreamStopping(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_streamOutput)) return;
	service->m_streamingState = "ending";
}

void
ApplicationService::onStreamStopped(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_streamOutput)) return;
	service->m_streamingState = "offline";
	service->disconnectStreamOutputHandler();
}

void
ApplicationService::onStreamReconnecting(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_streamOutput)) return;
	service->m_streamingState = "reconnecting";
}

void
ApplicationService::onStreamReconnected(void* streaming_service, calldata_t* data) {
	ApplicationService* service = reinterpret_cast<ApplicationService*>(streaming_service);
	if(!service->checkOutput(data, service->m_streamOutput)) return;
	service->m_streamingState = "live";
}

/*
========================================================================================================
	OBS Signals Helpers
========================================================================================================
*/

bool
ApplicationService::connectStreamOutputHandler() {
	if(m_streamOutput != nullptr)
		disconnectStreamOutputHandler();
	m_streamOutput = obs_frontend_get_streaming_output();
	if(m_streamOutput != nullptr) {
		signal_handler_t* signal_handler = obs_output_get_signal_handler(m_streamOutput);
		if(signal_handler != nullptr) {
			signal_handler_connect(signal_handler, "starting",
				ApplicationService::onStreamStarting, this);
			signal_handler_connect(signal_handler, "start",
				ApplicationService::onStreamStarted, this);
			signal_handler_connect(signal_handler, "stopping",
				ApplicationService::onStreamStopping, this);
			signal_handler_connect(signal_handler, "stop",
				ApplicationService::onStreamStopped, this);
			signal_handler_connect(signal_handler, "reconnect",
				ApplicationService::onStreamReconnecting, this);
			signal_handler_connect(signal_handler, "reconnect_success",
				ApplicationService::onStreamReconnected, this);

			return true;
		}
	}
	return false;
}

bool
ApplicationService::connectRecordOutputHandler() {
	if(m_recordOutput != nullptr)
		disconnectRecordOutputHandler();
	m_recordOutput = obs_frontend_get_recording_output();
	if(m_recordOutput != nullptr) {
		signal_handler_t* signal_handler = obs_output_get_signal_handler(m_recordOutput);
		if(signal_handler != nullptr) {
			signal_handler_connect(signal_handler, "starting",
				ApplicationService::onRecordStarting, this);
			signal_handler_connect(signal_handler, "start",
				ApplicationService::onRecordStarted, this);
			signal_handler_connect(signal_handler, "stopping",
				ApplicationService::onRecordStopping, this);
			signal_handler_connect(signal_handler, "stop",
				ApplicationService::onRecordStopped, this);

			return true;
		}
	}
	return false;
}

void
ApplicationService::disconnectStreamOutputHandler() {
	if(m_streamOutput != nullptr) {
		signal_handler_t* signal_handler = obs_output_get_signal_handler(m_streamOutput);
		if(signal_handler != nullptr) {
			signal_handler_disconnect(signal_handler, "starting",
				ApplicationService::onStreamStarting, this);
			signal_handler_disconnect(signal_handler, "start",
				ApplicationService::onStreamStarted, this);
			signal_handler_disconnect(signal_handler, "stopping",
				ApplicationService::onStreamStopping, this);
			signal_handler_disconnect(signal_handler, "stop",
				ApplicationService::onStreamStopped, this);
			signal_handler_disconnect(signal_handler, "reconnect",
				ApplicationService::onStreamReconnecting, this);
			signal_handler_disconnect(signal_handler, "reconnect_success",
				ApplicationService::onStreamReconnected, this);
		}
		obs_output_release(m_streamOutput);
		m_streamOutput = nullptr;
	}
}

void
ApplicationService::disconnectRecordOutputHandler() {
	if(m_recordOutput != nullptr) {
		signal_handler_t* signal_handler = obs_output_get_signal_handler(m_recordOutput);
		if(signal_handler != nullptr) {
			signal_handler_disconnect(signal_handler, "starting",
				ApplicationService::onRecordStarting, this);
			signal_handler_disconnect(signal_handler, "start",
				ApplicationService::onRecordStarted, this);
			signal_handler_disconnect(signal_handler, "stopping",
				ApplicationService::onRecordStopping, this);
			signal_handler_disconnect(signal_handler, "stop",
				ApplicationService::onRecordStopped, this);
		}
		obs_output_release(m_recordOutput);
		m_recordOutput = nullptr;
	}
}

bool
ApplicationService::checkOutput(calldata_t* data, obs_output_t* output) const {
	obs_output_t* data_output = nullptr;
	calldata_get_ptr(data, "output", &data_output);
	return data_output == output;
}

/*
========================================================================================================
	OBS Event Helpers
========================================================================================================
*/

bool
ApplicationService::onApplicationLoaded() {
	// setup tools menu action for show pluging info
	const char* label = obs_module_text("Elgato Remote Control for OBS Studio");
	QAction* action = (QAction*)obs_frontend_add_tools_menu_qaction(label);

	// Connect the new action to the pop dialog
	InfoDialog& dialog = *m_dialog;
	if(action != nullptr) {
		std::function<void (void)> f = [&dialog] {
			obs_frontend_push_ui_translation(obs_module_get_string);
			dialog.open();
			obs_frontend_pop_ui_translation();
		};
		action->connect(action, &QAction::triggered, f );
	}

	Logger::instance().output(dialog.logger());

	dialog.open();

	obsManager()->activeCollection(true)->resourceScenes();

	logInfo("Application Loaded.");

	return true;
}

bool
ApplicationService::onApplicationExit() {
	disconnectStreamOutputHandler();
	disconnectRecordOutputHandler();
	return true;
}

bool
ApplicationService::onStreamLaunching() {
	connectStreamOutputHandler();
	return true;
}

bool
ApplicationService::onRecordStarting() {
	connectRecordOutputHandler();
	return true;
}

/*
========================================================================================================
	RPC Event Helpers
========================================================================================================
*/

bool
ApplicationService::onGetRecordStreamState(const rpc_event_data& data) {
	connectRecordOutputHandler();
	connectStreamOutputHandler();

	rpc_adv_response<std::pair<std::string,std::string>> response =
		response_string2(&data, "getRecordStreamState");

	if(data.event == Streamdeck::rpc_event::GET_RECORD_STREAM_STATE) {
		response.event = Streamdeck::rpc_event::GET_RECORD_STREAM_STATE;
		logInfo("Streamdeck has required record and stream state...");

		if(data.serviceName.compare("StreamingService") == 0 && data.method.compare("getModel") == 0) {
			response.data = std::make_pair(m_streamingState, m_recordingState);
			return streamdeckManager()->commit_to(response, &StreamdeckManager::setRecordStreamState);
		}

		logWarning("GetRecordStreamState - Unknown RPC Service");
		return true;
	}

	logError("GetRecordStreamState not called by GET_RCORD_STREAM_STATE.");
	return false;
}