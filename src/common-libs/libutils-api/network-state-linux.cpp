/* HopCast (Linux): network reachability for pls::NetworkState.
 *
 * Windows uses INetworkListManager (network-state.cpp), macOS uses NWPathMonitor
 * (network-state-mac.mm). On Linux this uses Qt's QNetworkInformation, which talks
 * to NetworkManager (or glib/portal backends) when available. If no backend can be
 * loaded we report "available" so the app never blocks itself offline by mistake.
 *
 * Original code, GPL-2.0-or-later.
 */
#include <private/qobject_p.h>
#include <QNetworkInformation>

#include "libutils-api.h"
#include "libutils-api-log.h"
#include "network-state.h"

#define NETWORK_STATE_MODULE "NetworkState"

namespace pls {

class NetworkStatePrivate : public QObjectPrivate {
	Q_DECLARE_PUBLIC(NetworkState)

public:
	bool m_available = true;
	bool m_started = false;

	static bool isReachable(QNetworkInformation::Reachability r)
	{
		/* Unknown means "no information": don't claim we're offline. */
		return r != QNetworkInformation::Reachability::Disconnected;
	}

	void setAvailable(bool available)
	{
		if (m_available == available)
			return;
		m_available = available;
		PLS_INFO(NETWORK_STATE_MODULE, "Network state is changed : %s", available ? "Connected" : "Disconnected");
		Q_Q(NetworkState);
		emit q->stateChanged(available);
	}

	bool start()
	{
		if (m_started)
			return true;

		bool loaded = QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability) ||
			      QNetworkInformation::loadDefaultBackend();
		QNetworkInformation *info = loaded ? QNetworkInformation::instance() : nullptr;
		if (!info) {
			PLS_WARN(NETWORK_STATE_MODULE, "%s", "No network information backend available; assuming connected");
			m_available = true;
			return false;
		}

		m_available = isReachable(info->reachability());
		Q_Q(NetworkState);
		QObject::connect(info, &QNetworkInformation::reachabilityChanged, q,
				 [this](QNetworkInformation::Reachability r) { setAvailable(isReachable(r)); });
		m_started = true;
		PLS_INFO(NETWORK_STATE_MODULE, "NetworkState started (backend: %s). network state : %s",
			 info->backendName().toUtf8().constData(), m_available ? "Connected" : "Disconnected");
		return true;
	}

	void stop()
	{
		if (!m_started)
			return;
		if (QNetworkInformation *info = QNetworkInformation::instance()) {
			Q_Q(NetworkState);
			QObject::disconnect(info, nullptr, q, nullptr);
		}
		m_started = false;
	}

	/* d_func() is private to NetworkState; NetworkStatePrivate is its friend. */
	static bool start(NetworkState *ns) { return ns->d_func()->start(); }
	static void stop(NetworkState *ns) { ns->d_func()->stop(); }
};

NetworkState::NetworkState() : QObject(*pls_new<NetworkStatePrivate>()) {}

NetworkState *NetworkState::instance()
{
	static NetworkState ns;
	return &ns;
}

bool NetworkState::isAvailable() const
{
	Q_D(const NetworkState);
	return d->m_available;
}

bool network_state_start()
{
	return NetworkStatePrivate::start(NetworkState::instance());
}

void network_state_stop()
{
	NetworkStatePrivate::stop(NetworkState::instance());
}

} // namespace pls
