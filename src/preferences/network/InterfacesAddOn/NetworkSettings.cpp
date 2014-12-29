/*
 * Copyright 2004-2013 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de.
 *		Andre Alves Garzia, andre@andregarzia.com
 *		Alexander von Gluck, kallisti5@unixzen.com
 *		John Scipione, jscipione@gmail.com
 *		Vegard Wærp, vegarwa@online.no
 */


#include "NetworkSettings.h"

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <unistd.h>

#include <AutoDeleter.h>
#include <driver_settings.h>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <String.h>


NetworkSettings::NetworkSettings(const char* name)
	:
	fDisabled(false),
	fNameServers(5, true)
{
	fName = name;
	_DetectProtocols();

	fNetworkDevice = new BNetworkDevice(fName);
	fNetworkInterface = new BNetworkInterface(fName);

	ReadConfiguration();
}


NetworkSettings::~NetworkSettings()
{
	unsigned int index;
	for (index = 0; index < MAX_PROTOCOLS; index++)
	{
		int socket_id = fProtocols[index].socket_id;
		if (socket_id < 0)
			continue;

		close(socket_id);
	}

	delete fNetworkInterface;
}


status_t
NetworkSettings::_DetectProtocols()
{
	for (int index = 0; index < MAX_PROTOCOLS; index++) {
		fProtocols[index].name = NULL;
		fProtocols[index].present = false;
		fProtocols[index].socket_id = -1;
		fProtocols[index].inet_id = -1;
	}

	// First we populate the struct with the possible
	// protocols we could configure for an interface
	// (size limit of MAX_PROTOCOLS)
	fProtocols[0].name = "IPv4";
	fProtocols[0].inet_id = AF_INET;
	fProtocols[1].name = "IPv6";
	fProtocols[1].inet_id = AF_INET6;

	// Loop through each of the possible protocols and
	// ensure they are functional
	for (int index = 0; index < MAX_PROTOCOLS; index++) {
		int inet_id = fProtocols[index].inet_id;
		if (inet_id > 0)
		{
			fProtocols[index].socket_id = socket(inet_id, SOCK_DGRAM, 0);
			if (fProtocols[index].socket_id < 0) {
				printf("Protocol %s : not present\n", fProtocols[index].name);
				fProtocols[index].present = false;
			} else {
				printf("Protocol %s : present\n", fProtocols[index].name);
				fProtocols[index].present = true;
			}
		}
	}

	return B_OK;
}


// -- Interface address read code


/*!	ReadConfiguration pulls the current interface settings
	from the interfaces via BNetworkInterface and friends
	and populates this classes private settings BAddresses
	with them.
*/
void
NetworkSettings::ReadConfiguration()
{
	fDisabled = (fNetworkInterface->Flags() & IFF_UP) == 0;

	for (int index = 0; index < MAX_PROTOCOLS; index++) {
		int inet_id = fProtocols[index].inet_id;

		if (fProtocols[index].present) {
			// --- Obtain IP Addresses
			int32 zeroAddr = fNetworkInterface->FindFirstAddress(inet_id);
			if (zeroAddr >= 0) {
				fNetworkInterface->GetAddressAt(zeroAddr,
					fInterfaceAddressMap[inet_id]);
				fAddress[inet_id].SetTo(
					fInterfaceAddressMap[inet_id].Address());
				fNetmask[inet_id].SetTo(
					fInterfaceAddressMap[inet_id].Mask());
			}

			// --- Obtain gateway
			// TODO : maybe in the future no ioctls?
			ifconf config;
			config.ifc_len = sizeof(config.ifc_value);
			// Populate config with size of routing table
			if (ioctl(fProtocols[index].socket_id, SIOCGRTSIZE,
				&config, sizeof(config)) < 0)
				return;

			uint32 size = (uint32)config.ifc_value;
			if (size == 0)
				return;

			// Malloc a buffer the size of the routing table
			void* buffer = malloc(size);
			if (buffer == NULL)
				return;

			MemoryDeleter bufferDeleter(buffer);
			config.ifc_len = size;
			config.ifc_buf = buffer;

			if (ioctl(fProtocols[index].socket_id, SIOCGRTTABLE,
				&config, sizeof(config)) < 0)
				return;

			ifreq* interface = (ifreq*)buffer;
			ifreq* end = (ifreq*)((uint8*)buffer + size);

			while (interface < end) {
				route_entry& route = interface->ifr_route;

				if ((route.flags & RTF_GATEWAY) != 0) {
					if (inet_id == AF_INET) {
						char addressOut[INET_ADDRSTRLEN];
						sockaddr_in* socketAddr
							= (sockaddr_in*)route.gateway;

						inet_ntop(inet_id, &socketAddr->sin_addr,
							addressOut, INET_ADDRSTRLEN);

						fGateway[inet_id].SetTo(addressOut);

					} else if (inet_id == AF_INET6) {
						char addressOut[INET6_ADDRSTRLEN];
						sockaddr_in6* socketAddr
							= (sockaddr_in6*)route.gateway;

						inet_ntop(inet_id, &socketAddr->sin6_addr,
							addressOut, INET6_ADDRSTRLEN);

						fGateway[inet_id].SetTo(addressOut);

					} else {
						printf("Cannot pull routes for unknown protocol: %d\n",
							inet_id);
						fGateway[inet_id].SetTo("");
					}

				}

				int32 addressSize = 0;
				if (route.destination != NULL)
					addressSize += route.destination->sa_len;
				if (route.mask != NULL)
					addressSize += route.mask->sa_len;
				if (route.gateway != NULL)
					addressSize += route.gateway->sa_len;

				interface = (ifreq *)((addr_t)interface + IF_NAMESIZE
					+ sizeof(route_entry) + addressSize);
			}

			// --- Obtain selfconfiguration options
			// TODO : This needs to be determined by protocol flags
			//        AutoConfiguration on the IP level doesn't exist yet
			//		  ( fInterfaceAddressMap[AF_INET].Flags() )
			if (fProtocols[index].socket_id >= 0) {
				fAutoConfigure[inet_id] = (fNetworkInterface->Flags()
					& (IFF_AUTO_CONFIGURED | IFF_CONFIGURING)) != 0;
			}
		}
	}

	// Read wireless network from interfaces
	fWirelessNetwork.SetTo(NULL);

	BPath path;
	find_directory(B_SYSTEM_SETTINGS_DIRECTORY, &path);
	path.Append("network");
	path.Append("interfaces");

	void* handle = load_driver_settings(path.Path());
	if (handle != NULL) {
		const driver_settings* settings = get_driver_settings(handle);
		if (settings != NULL) {
			for (int32 i = 0; i < settings->parameter_count; i++) {
				driver_parameter& top = settings->parameters[i];
				if (!strcmp(top.name, "interface")) {
					// The name of the interface can either be the value of
					// the "interface" parameter, or a separate "name" parameter
					const char* name = NULL;
					if (top.value_count > 0) {
						name = top.values[0];
						if (fName != name)
							continue;
					}

					// search "network" parameter
					for (int32 j = 0; j < top.parameter_count; j++) {
						driver_parameter& sub = top.parameters[j];
						if (name == NULL && !strcmp(sub.name, "name")
							&& sub.value_count > 0) {
							name = sub.values[0];
							if (fName != sub.values[0])
								break;
						}

						if (!strcmp(sub.name, "network")
							&& sub.value_count > 0) {
							fWirelessNetwork.SetTo(sub.values[0]);
							break;
						}
					}

					// We found our interface
					if (fName == name)
						break;
				}
			}
		}
		unload_driver_settings(handle);
	}

	// read resolv.conf for the dns.
	fNameServers.MakeEmpty();

	res_init();
	res_state state = __res_state();

	if (state != NULL) {
		for (int i = 0; i < state->nscount; i++) {
			fNameServers.AddItem(
				new BString(inet_ntoa(state->nsaddr_list[i].sin_addr)));
		}
		fDomain = state->dnsrch[0];
	}
}


/*!	SetConfiguration sets this classes current BNetworkAddress settings
	to the interface directly via BNetworkInterface and friends
*/
void
NetworkSettings::SetConfiguration()
{
	printf("Setting %s\n", Name());

	fNetworkDevice->JoinNetwork(WirelessNetwork());

	for (int index = 0; index < MAX_PROTOCOLS; index++) {
		int inet_id = fProtocols[index].inet_id;
		if (fProtocols[index].present) {
			int32 zeroAddr = fNetworkInterface->FindFirstAddress(inet_id);
			if (zeroAddr >= 0) {
				BNetworkInterfaceAddress interfaceConfig;
				fNetworkInterface->GetAddressAt(zeroAddr,
					interfaceConfig);
				interfaceConfig.SetAddress(fAddress[inet_id]);
				interfaceConfig.SetMask(fNetmask[inet_id]);
				fNetworkInterface->SetAddress(interfaceConfig);
			} else {
				// TODO : test this case (no address set for this protocol)
				printf("no zeroAddr found for %s(%d), found %" B_PRIu32 "\n",
					fProtocols[index].name, inet_id, zeroAddr);
				BNetworkInterfaceAddress interfaceConfig;
				interfaceConfig.SetAddress(fAddress[inet_id]);
				interfaceConfig.SetMask(fNetmask[inet_id]);
				fNetworkInterface->AddAddress(interfaceConfig);
			}

			// FIXME these flags shouldn't be interface-global, but specific to
			// each protocol. Only set them for AF_INET otherwise there is
			// confusion and freezes.
			if (fProtocols[index].inet_id == AF_INET) {
				int32 flags = fNetworkInterface->Flags();
				if (AutoConfigure(fProtocols[index].inet_id)) {
					flags |= IFF_AUTO_CONFIGURED;
					fNetworkInterface->SetFlags(flags);
					fNetworkInterface->AutoConfigure(fProtocols[index].inet_id);
				} else {
					flags &= ~(IFF_AUTO_CONFIGURED | IFF_CONFIGURING);
					fNetworkInterface->SetFlags(flags);
				}
			}
		}
	}
}


/*!	LoadConfiguration reads the current interface configuration
	file that NetServer looks for and populates this class with it
*/
void
NetworkSettings::LoadConfiguration()
{

}


/*!	GenerateConfiguration reads this classes settings and write them to
	a BString. This can then be put in the interfaces settings file to make
	the settings persistant.
*/
BString
NetworkSettings::GenerateConfiguration()
{
	// TODO should use ProtocolVersions to know which protocols are there
	bool autoConfigure = true;
	if (!IsDisabled()) {
		for (int i = 0; i < MAX_PROTOCOLS; i++) {
			if (fProtocols[i].present && strlen(IP(fProtocols[i].inet_id)) != 0
				&& !AutoConfigure(fProtocols[i].inet_id)) {
				autoConfigure = false;
			}
		}
	}

	// If the interface is fully autoconfigured, don't include it in the
	// settings file.
	if (autoConfigure)
		return BString();

	BString result;

	result.SetToFormat("interface %s {\n", Name());

	if (IsDisabled())
		result << "\tdisabled\ttrue\n";
	else {
		for (int i = 0; i < MAX_PROTOCOLS; i++) {
			if (fProtocols[i].present && strlen(IP(fProtocols[i].inet_id)) != 0
					&& !AutoConfigure(fProtocols[i].inet_id)) {
				result << "\taddress {\n";

				result << "\t\tfamily\t";
				if (fProtocols[i].inet_id == AF_INET)
					result << "inet\n";
				else if (fProtocols[i].inet_id == AF_INET6)
					result << "inet6\n";
				else {
					// FIXME the struct protocols should know the name to use.
					debugger("Unknown protocol found!");
				}

				result << "\t\taddress\t" << IP(fProtocols[i].inet_id) << "\n";
				result << "\t\tgateway\t" << Gateway(fProtocols[i].inet_id)
					<< "\n";
				result << "\t\tmask\t" << Netmask(fProtocols[i].inet_id)
					<< "\n";
				result << "\t}\n";
			}
		}
	}

#if 0
	if (!networks.empty()) {
		for (size_t index = 0; index < networks.size(); index++) {
			wireless_network& network = networks[index];
			fprintf(fp, "\tnetwork %s {\n", network.name);
			fprintf(fp, "\t\tmac\t%s\n",
				network.address.ToString().String());
			fprintf(fp, "\t}\n");
		}
	}
#endif

	result << "}\n\n";

	return result;
}


/*! RenegotiateAddresses performs a address renegotiation in an attempt to fix
	connectivity problems
*/
status_t
NetworkSettings::RenegotiateAddresses()
{
	for (int index = 0; index < MAX_PROTOCOLS; index++) {
		int inet_id = fProtocols[index].inet_id;
		if (fProtocols[index].present
			&& AutoConfigure(inet_id)) {
			// If protocol is active, and set to auto
			fNetworkInterface->AutoConfigure(inet_id);
				// Perform AutoConfiguration
		}
	}

	return B_OK;
}


const char*
NetworkSettings::HardwareAddress()
{
	BNetworkAddress macAddress;
	if (fNetworkInterface->GetHardwareAddress(macAddress) == B_OK)
		return macAddress.ToString();

	return NULL;
}


status_t
NetworkSettings::GetNextAssociatedNetwork(uint32& cookie,
	BNetworkAddress& address)
{
	return fNetworkDevice->GetNextAssociatedNetwork(cookie, address);
}


status_t
NetworkSettings::GetNextNetwork(uint32& cookie, wireless_network& network)
{
	return fNetworkDevice->GetNextNetwork(cookie, network);
}

