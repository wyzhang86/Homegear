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

#ifndef HOMEMATICDEVICE_H
#define HOMEMATICDEVICE_H

#include "../Base/BaseLib.h"
#include "BidCoSQueue.h"
#include "BidCoSPeer.h"
#include "BidCoSMessage.h"
#include "BidCoSMessages.h"
#include "BidCoSQueueManager.h"
#include "BidCoSPacketManager.h"

#include <string>
#include <unordered_map>
#include <map>
#include <mutex>
#include <vector>
#include <queue>
#include <thread>
#include <chrono>
#include "pthread.h"

namespace BidCoS
{

class HomeMaticDevice : public BaseLib::Systems::LogicalDevice, public BidCoSQueueManager::IBidCoSQueueManagerEventSink
{
    public:
        //In table variables
        int32_t getFirmwareVersion() { return _firmwareVersion; }
        void setFirmwareVersion(int32_t value) { _firmwareVersion = value; saveVariable(0, value); }
        int32_t getCentralAddress() { return _centralAddress; }
        void setCentralAddress(int32_t value) { _centralAddress = value; saveVariable(1, value); }
        std::string getPhysicalInterfaceID() { return _physicalInterfaceID; }
		void setPhysicalInterfaceID(std::string);
        //End

        std::unordered_map<int32_t, uint8_t>* messageCounter() { return &_messageCounter; }
        virtual bool isCentral();
        static bool isDimmer(BaseLib::Systems::LogicalDeviceType type);
        static bool isSwitch(BaseLib::Systems::LogicalDeviceType type);
        virtual void reset();

        HomeMaticDevice(IDeviceEventSink* eventHandler);
        HomeMaticDevice(uint32_t deviceID, std::string serialNumber, int32_t address, IDeviceEventSink* eventHandler);
        virtual ~HomeMaticDevice();
        virtual void stopThreads();
        virtual void dispose(bool wait = true);
        virtual bool onPacketReceived(std::string& senderID, std::shared_ptr<BaseLib::Systems::Packet> packet);

        virtual void addPeer(std::shared_ptr<BidCoSPeer> peer);
        std::shared_ptr<BidCoSPeer> getPeer(int32_t address);
        std::shared_ptr<BidCoSPeer> getPeer(uint64_t id);
        std::shared_ptr<BidCoSPeer> getPeer(std::string serialNumber);
        std::shared_ptr<BidCoSQueue> getQueue(int32_t address);
        virtual void loadPeers();
        virtual void savePeers(bool full);
        virtual void loadVariables();
        virtual void saveVariables();
        virtual void saveMessageCounters();
        virtual void saveConfig();
        virtual void serializeMessageCounters(std::vector<uint8_t>& encodedData);
        virtual void unserializeMessageCounters(std::shared_ptr<std::vector<char>> serializedData);
        virtual void serializeConfig(std::vector<uint8_t>& encodedData);
        virtual void unserializeConfig(std::shared_ptr<std::vector<char>> serializedData);
        virtual void setLowBattery(bool);
        virtual void setChannelCount(uint32_t channelCount) {}
        virtual bool pairDevice(int32_t timeout);
        virtual bool isInPairingMode() { return _pairing; }
        virtual int32_t calculateCycleLength(uint8_t messageCounter);
        virtual int32_t getHexInput();
        virtual std::shared_ptr<BidCoSMessages> getMessages() { return _messages; }
        virtual std::string handleCLICommand(std::string command);
        virtual void sendPacket(std::shared_ptr<IBidCoSInterface> physicalInterface, std::shared_ptr<BidCoSPacket> packet, bool stealthy = false);
        virtual void sendPacketMultipleTimes(std::shared_ptr<IBidCoSInterface> physicalInterface, std::shared_ptr<BidCoSPacket> packet, int32_t peerAddress, int32_t count, int32_t delay, bool useCentralMessageCounter = false, bool isThread = false);
        std::shared_ptr<BidCoSPacket> getReceivedPacket(int32_t address) { return _receivedPackets.get(address); }
        std::shared_ptr<BidCoSPacket> getSentPacket(int32_t address) { return _sentPackets.get(address); }

        virtual void handleAck(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet) {}
        virtual void handlePairingRequest(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleDutyCyclePacket(int32_t messageCounter, std::shared_ptr<BidCoSPacket>) {}
        virtual void handleConfigStart(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleConfigWriteIndex(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleConfigPeerAdd(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleConfigParamRequest(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleConfigParamResponse(int32_t messageCounter, std::shared_ptr<BidCoSPacket>) {};
        virtual void handleConfigRequestPeers(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleReset(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleConfigEnd(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleConfigPeerDelete(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleWakeUp(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
        virtual void handleSetPoint(int32_t messageCounter, std::shared_ptr<BidCoSPacket>) {}
        virtual void handleSetValveState(int32_t messageCounter, std::shared_ptr<BidCoSPacket>) {}
        virtual void handleStateChangeRemote(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet) {}
        virtual void handleStateChangeMotionDetector(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet) {}
        virtual void handleTimeRequest(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);

        virtual void sendPairingRequest();
        virtual void sendDirectedPairingRequest(int32_t messageCounter, int32_t controlByte, std::shared_ptr<BidCoSPacket> packet);
        virtual void sendOK(int32_t messageCounter, int32_t destinationAddress);
        virtual void sendStealthyOK(int32_t messageCounter, int32_t destinationAddress);
        virtual void sendOKWithPayload(int32_t messageCounter, int32_t destinationAddress, std::vector<uint8_t> payload, bool isWakeMeUpPacket);
        virtual void sendNOK(int32_t messageCounter, int32_t destinationAddress);
        virtual void sendNOKTargetInvalid(int32_t messageCounter, int32_t destinationAddress);
        virtual void sendConfigParams(int32_t messageCounter, int32_t destinationAddress, std::shared_ptr<BidCoSPacket> packet);
        virtual void sendConfigParamsType2(int32_t messageCounter, int32_t destinationAddress) {}
        virtual void sendPeerList(int32_t messageCounter, int32_t destinationAddress, int32_t channel);
        virtual void sendDutyCycleResponse(int32_t destinationAddress);
        virtual void sendRequestConfig(int32_t messageCounter, int32_t controlByte, std::shared_ptr<BidCoSPacket> packet) {}
    protected:
        //In table variables
        int32_t _firmwareVersion = 0;
        int32_t _centralAddress = 0;
        std::unordered_map<int32_t, uint8_t> _messageCounter;
        std::unordered_map<int32_t, std::unordered_map<int32_t, std::map<int32_t, int32_t>>> _config;
        std::string _physicalInterfaceID;
        //End

        bool _stopWorkerThread = false;
        std::thread _workerThread;

        int32_t _deviceClass = 0;
        int32_t _channelMin = 0;
        int32_t _channelMax = 0;
        int32_t _lastPairingByte = 0;
        int32_t _currentList = 0;
        std::unordered_map<int32_t, int32_t> _deviceTypeChannels;
        bool _pairing = false;
        bool _justPairedToOrThroughCentral = false;
        BidCoSQueueManager _bidCoSQueueManager;
        BidCoSPacketManager _receivedPackets;
        BidCoSPacketManager _sentPackets;
        std::shared_ptr<BidCoSMessages> _messages;
        std::shared_ptr<IBidCoSInterface> _physicalInterface;

        bool _lowBattery = false;

        virtual std::shared_ptr<BidCoSPeer> createPeer(int32_t address, int32_t firmwareVersion, BaseLib::Systems::LogicalDeviceType deviceType, std::string serialNumber, int32_t remoteChannel, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet = std::shared_ptr<BidCoSPacket>(), bool save = true);
        virtual std::shared_ptr<BidCoSPeer> createTeam(int32_t address, BaseLib::Systems::LogicalDeviceType deviceType, std::string serialNumber);
        virtual std::shared_ptr<BaseLib::Systems::Central> getCentral();
        virtual std::shared_ptr<HomeMaticDevice> getDevice(int32_t address);
        virtual std::shared_ptr<IBidCoSInterface> getPhysicalInterface(int32_t peerAddress);
        virtual void worker() {}

        virtual void init();
        virtual void setUpBidCoSMessages();
        virtual void setUpConfig();

        //BidCoSQueueManager event handling
		virtual void onQueueCreateSavepoint(std::string name);
		virtual void onQueueReleaseSavepoint(std::string name);
		//End BidCoSQueueManager
};

}
#endif // HOMEMATICDEVICE_H
