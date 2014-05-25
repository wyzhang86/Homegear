/* Copyright 2013-2014 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#ifndef LOGICALDEVICE_H_
#define LOGICALDEVICE_H_

#include "../HelperFunctions/HelperFunctions.h"
#include "IPhysicalInterface.h"
#include "DeviceFamilies.h"
#include "Packet.h"
#include "../RPC/RPCVariable.h"
#include "../IEvents.h"
#include "Peer.h"

#include <memory>

namespace BaseLib
{
namespace Systems
{
class LogicalDevice : public Peer::IPeerEventSink, public IPhysicalInterface::IPhysicalInterfaceEventSink, public IEvents
{
public:
	//Event handling
	class IDeviceEventSink : public IEventSinkBase
	{
	public:
		//Database
			//General
			virtual void onCreateSavepoint(std::string name) = 0;
			virtual void onReleaseSavepoint(std::string name) = 0;

			//Metadata
			virtual void onDeleteMetadata(std::string objectID, std::string dataID = "") = 0;

			//Device
			virtual uint64_t onSaveDevice(uint64_t id, int32_t address, std::string serialNumber, uint32_t type, uint32_t family) = 0;
			virtual uint64_t onSaveDeviceVariable(Database::DataRow data) = 0;
			virtual void onDeletePeers(int32_t deviceID) = 0;
			virtual Database::DataTable onGetPeers(uint64_t deviceID) = 0;
			virtual Database::DataTable onGetDeviceVariables(uint64_t deviceID) = 0;

			//Peer
			virtual void onDeletePeer(uint64_t id) = 0;
			virtual uint64_t onSavePeer(uint64_t id, uint32_t parentID, int32_t address, std::string serialNumber) = 0;
			virtual uint64_t onSavePeerParameter(uint64_t peerID, Database::DataRow data) = 0;
			virtual uint64_t onSavePeerVariable(uint64_t peerID, Database::DataRow data) = 0;
			virtual Database::DataTable onGetPeerParameters(uint64_t peerID) = 0;
			virtual Database::DataTable onGetPeerVariables(uint64_t peerID) = 0;
			virtual void onDeletePeerParameter(uint64_t peerID, Database::DataRow data) = 0;
		//End database

		virtual void onRPCEvent(uint64_t id, int32_t channel, std::string deviceAddress, std::shared_ptr<std::vector<std::string>> valueKeys, std::shared_ptr<std::vector<std::shared_ptr<RPC::RPCVariable>>> values) = 0;
		virtual void onRPCUpdateDevice(uint64_t id, int32_t channel, std::string address, int32_t hint) = 0;
		virtual void onRPCNewDevices(std::shared_ptr<RPC::RPCVariable> deviceDescriptions) = 0;
		virtual void onRPCDeleteDevices(std::shared_ptr<RPC::RPCVariable> deviceAddresses, std::shared_ptr<RPC::RPCVariable> deviceInfo) = 0;
		virtual void onEvent(uint64_t peerID, int32_t channel, std::shared_ptr<std::vector<std::string>> variables, std::shared_ptr<std::vector<std::shared_ptr<BaseLib::RPC::RPCVariable>>> values) = 0;
	};
	//End event handling

	LogicalDevice(IDeviceEventSink* eventHandler);
	LogicalDevice(uint32_t deviceID, std::string serialNumber, int32_t address, IDeviceEventSink* eventHandler);
	virtual ~LogicalDevice();

	virtual DeviceFamilies deviceFamily();

	virtual int32_t getAddress() { return _address; }
	virtual uint64_t getID() { return _deviceID; }
    virtual std::string getSerialNumber() { return _serialNumber; }
    virtual uint32_t getDeviceType() { return _deviceType; }
	virtual std::string handleCLICommand(std::string command) { return ""; }
	virtual bool peerSelected() { return false; }
	virtual void dispose(bool wait = true) {}
	virtual void deletePeersFromDatabase();
	virtual void load();
	virtual void loadVariables() = 0;
	virtual void loadPeers() {}
	virtual void save(bool saveDevice);
	virtual void saveVariables() = 0;
	virtual void saveVariable(uint32_t index, int64_t intValue);
	virtual void saveVariable(uint32_t index, std::string& stringValue);
	virtual void saveVariable(uint32_t index, std::vector<uint8_t>& binaryValue);
	virtual void savePeers(bool full) {}
protected:
	uint64_t _deviceID = 0;
	int32_t _address = 0;
    std::string _serialNumber;
    uint32_t _deviceType = 0;
    std::map<uint32_t, uint32_t> _variableDatabaseIDs;
    bool _initialized = false;
    bool _disposing = false;
	bool _disposed = false;

	//Event handling
	//Database
		//General
		virtual void raiseCreateSavepoint(std::string name);
		virtual void raiseReleaseSavepoint(std::string name);

		//Metadata
		virtual void raiseDeleteMetadata(std::string objectID, std::string dataID = "");

		//Device
		virtual uint64_t raiseSaveDevice();
		virtual uint64_t raiseSaveDeviceVariable(Database::DataRow data);
		virtual void raiseDeletePeers(int32_t deviceID);
		virtual Database::DataTable raiseGetPeers();
		virtual Database::DataTable raiseGetDeviceVariables();

		//Peer
		virtual void raiseDeletePeer(uint64_t id);
		virtual uint64_t raiseSavePeer(uint64_t id, uint32_t parentID, int32_t address, std::string serialNumber);
		virtual uint64_t raiseSavePeerParameter(uint64_t peerID, Database::DataRow data);
		virtual uint64_t raiseSavePeerVariable(uint64_t peerID, Database::DataRow data);
		virtual Database::DataTable raiseGetPeerParameters(uint64_t peerID);
		virtual Database::DataTable raiseGetPeerVariables(uint64_t peerID);
		virtual void raiseDeletePeerParameter(uint64_t peerID, Database::DataRow data);
	//End database

	virtual void raiseRPCEvent(uint64_t id, int32_t channel, std::string deviceAddress, std::shared_ptr<std::vector<std::string>> valueKeys, std::shared_ptr<std::vector<std::shared_ptr<RPC::RPCVariable>>> values);
	virtual void raiseRPCUpdateDevice(uint64_t id, int32_t channel, std::string address, int32_t hint);
	virtual void raiseRPCNewDevices(std::shared_ptr<RPC::RPCVariable> deviceDescriptions);
	virtual void raiseRPCDeleteDevices(std::shared_ptr<RPC::RPCVariable> deviceAddresses, std::shared_ptr<RPC::RPCVariable> deviceInfo);
	virtual void raiseEvent(uint64_t peerID, int32_t channel, std::shared_ptr<std::vector<std::string>> variables, std::shared_ptr<std::vector<std::shared_ptr<BaseLib::RPC::RPCVariable>>> values);
	//End event handling

	//Physical device event handling
	virtual bool onPacketReceived(std::string& senderID, std::shared_ptr<Packet> packet) = 0;
	//End physical device event handling

	//Peer event handling
	//Database
		//General
		virtual void onCreateSavepoint(std::string name);
		virtual void onReleaseSavepoint(std::string name);

		//Metadata
		virtual void onDeleteMetadata(std::string objectID, std::string dataID = "");

		//Peer
		virtual void onDeletePeer(uint64_t id);
		virtual uint64_t onSavePeer(uint64_t id, uint32_t parentID, int32_t address, std::string serialNumber);
		virtual uint64_t onSavePeerParameter(uint64_t peerID, Database::DataRow data);
		virtual uint64_t onSavePeerVariable(uint64_t peerID, Database::DataRow data);
		virtual Database::DataTable onGetPeerParameters(uint64_t peerID);
		virtual Database::DataTable onGetPeerVariables(uint64_t peerID);
		virtual void onDeletePeerParameter(uint64_t peerID, Database::DataRow data);
	//End database

	virtual void onRPCEvent(uint64_t id, int32_t channel, std::string deviceAddress, std::shared_ptr<std::vector<std::string>> valueKeys, std::shared_ptr<std::vector<std::shared_ptr<RPC::RPCVariable>>> values);
	virtual void onRPCUpdateDevice(uint64_t id, int32_t channel, std::string address, int32_t hint);
	virtual void onEvent(uint64_t peerID, int32_t channel, std::shared_ptr<std::vector<std::string>> variables, std::shared_ptr<std::vector<std::shared_ptr<BaseLib::RPC::RPCVariable>>> values);
	//End Peer event handling
};

}
}
#endif /* LOGICALDEVICE_H_ */