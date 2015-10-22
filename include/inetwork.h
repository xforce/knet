#include "network_helper_types.h"

namespace net
{
	class INetwork
	{
	public:
		virtual ~INetwork() = default;
		virtual void Start(const NetworkInformation &info) = 0;
		virtual void Connect(const NetworkConnectInfo& info) = 0;
		virtual void Stop() = 0;
		virtual void CloseConnection(const NetworkAddress &address, const bool sendNotification = true) = 0;

		virtual void Process() = 0;

		virtual void Shutdown() = 0;

	};
};