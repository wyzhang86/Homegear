/* Copyright 2013-2015 Sathya Laufer
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

#ifndef INSTEONCENTRAL_H_
#define INSTEONCENTRAL_H_

#include "../InsteonDevice.h"

#include <memory>
#include <mutex>
#include <string>

namespace Insteon
{

class InsteonCentral : public InsteonDevice, public BaseLib::Systems::Central
{
public:
	InsteonCentral(IDeviceEventSink* eventHandler);
	InsteonCentral(uint32_t deviceType, std::string serialNumber, int32_t address, IDeviceEventSink* eventHandler);
	virtual ~InsteonCentral();

	virtual bool onPacketReceived(std::string& senderID, std::shared_ptr<BaseLib::Systems::Packet> packet);
	virtual std::string handleCLICommand(std::string command);
	virtual uint64_t getPeerIDFromSerial(std::string serialNumber) { std::shared_ptr<InsteonPeer> peer = getPeer(serialNumber); if(peer) return peer->getID(); else return 0; }
	virtual void enqueuePendingQueues(int32_t deviceAddress);
	void unpair(uint64_t id);

	virtual void handleNak(std::shared_ptr<InsteonPacket> packet);
	virtual void handleAck(std::shared_ptr<InsteonPacket> packet);
	virtual void handleDatabaseOpResponse(std::shared_ptr<InsteonPacket> packet);
	virtual void handleLinkingModeResponse(std::shared_ptr<InsteonPacket> packet);
	virtual void handlePairingRequest(std::shared_ptr<InsteonPacket> packet);

	virtual bool knowsDevice(std::string serialNumber);
	virtual bool knowsDevice(uint64_t id);

	virtual std::shared_ptr<BaseLib::RPC::Variable> addDevice(int32_t clientID, std::string serialNumber);
	virtual std::shared_ptr<BaseLib::RPC::Variable> deleteDevice(int32_t clientID, std::string serialNumber, int32_t flags);
	virtual std::shared_ptr<BaseLib::RPC::Variable> deleteDevice(int32_t clientID, uint64_t peerID, int32_t flags);
	virtual std::shared_ptr<BaseLib::RPC::Variable> getDeviceInfo(int32_t clientID, uint64_t id, std::map<std::string, bool> fields);
	virtual std::shared_ptr<BaseLib::RPC::Variable> getInstallMode(int32_t clientID);
	virtual std::shared_ptr<BaseLib::RPC::Variable> putParamset(int32_t clientID, std::string serialNumber, int32_t channel, BaseLib::RPC::ParameterSet::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel, std::shared_ptr<BaseLib::RPC::Variable> paramset);
	virtual std::shared_ptr<BaseLib::RPC::Variable> putParamset(int32_t clientID, uint64_t peerID, int32_t channel, BaseLib::RPC::ParameterSet::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, std::shared_ptr<BaseLib::RPC::Variable> paramset);
	virtual std::shared_ptr<BaseLib::RPC::Variable> setInstallMode(int32_t clientID, bool on, uint32_t duration = 60, bool debugOutput = true);
protected:
	uint32_t _timeLeftInPairingMode = 0;
	void pairingModeTimer(int32_t duration, bool debugOutput = true);
	bool _stopPairingModeThread = false;
	bool _abortPairingModeThread = false;
	std::mutex _pairingModeThreadMutex;
	std::thread _pairingModeThread;
	int64_t _manualPairingModeStarted = -1;

	void addPeer(std::shared_ptr<InsteonPeer> peer);
	std::shared_ptr<InsteonPeer> createPeer(int32_t address, int32_t firmwareVersion, BaseLib::Systems::LogicalDeviceType deviceType, std::string serialNumber, bool save = true);
	void createPairingQueue(int32_t address, std::string interfaceID, std::shared_ptr<InsteonPeer> peer = nullptr);
	void deletePeer(uint64_t id);
	std::mutex _peerInitMutex;
	std::mutex _pairingMutex;
	std::mutex _enqueuePendingQueuesMutex;
	virtual void setUpInsteonMessages();
	virtual void worker();
	virtual void init();
	void enablePairingMode(std::string interfaceID = "");
	void disablePairingMode(std::string interfaceID = "");

	void addHomegearFeatures(std::shared_ptr<InsteonPeer> peer);
	void addHomegearFeaturesValveDrive(std::shared_ptr<InsteonPeer> peer);
};

}

#endif
