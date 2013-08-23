#include "HM-CC-VD.h"
#include "../HelperFunctions.h"

HM_CC_VD::HM_CC_VD(std::string serialNumber, int32_t address) : HomeMaticDevice(serialNumber, address)
{
	try
	{
		_deviceType = HMDeviceTypes::HMCCVD;
		_firmwareVersion = 0x20;
		_deviceClass = 0x58;
		_channelMin = 0x01;
		_channelMax = 0x01;
		_deviceTypeChannels[0x39] = 1;

		_config[1][5][0x09] = 0;
		_offsetPosition = &(_config[1][5][0x09]);
		_config[1][5][0x0A] = 15;
		_errorPosition = &(_config[1][5][0x0A]);

		init();
	}
    catch(const std::exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

HM_CC_VD::HM_CC_VD() : HomeMaticDevice()
{
	init();
}

void HM_CC_VD::init()
{
	HomeMaticDevice::init();

    setUpBidCoSMessages();
}

HM_CC_VD::~HM_CC_VD()
{
	dispose();
}

std::string HM_CC_VD::serialize()
{
	try
	{
		std::string serializedBase = HomeMaticDevice::serialize();
		std::ostringstream stringstream;
		stringstream << std::hex << std::uppercase << std::setfill('0');
		stringstream << std::setw(8) << serializedBase.size() << serializedBase;
		stringstream << std::setw(2) << _valveState;
		stringstream << std::setw(1) << (int32_t)_valveDriveBlocked;
		stringstream << std::setw(1) << (int32_t)_valveDriveLoose;
		stringstream << std::setw(1) << (int32_t)_adjustingRangeTooSmall;
		return  stringstream.str();
	}
    catch(const std::exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return "";
}

void HM_CC_VD::unserialize(std::string serializedObject, uint8_t dutyCycleMessageCounter, int64_t lastDutyCycleEvent)
{
	try
	{
		HomeMaticDevice::unserialize(serializedObject.substr(8, std::stoll(serializedObject.substr(0, 8), 0, 16)), dutyCycleMessageCounter, lastDutyCycleEvent);
		uint32_t pos = 8 + std::stoll(serializedObject.substr(0, 8), 0, 16);
		_valveState = std::stoll(serializedObject.substr(pos, 2), 0, 16); pos += 2;
		_valveDriveBlocked = std::stoll(serializedObject.substr(pos, 1), 0, 16); pos += 1;
		_valveDriveLoose = std::stoll(serializedObject.substr(pos, 1), 0, 16); pos += 1;
		_adjustingRangeTooSmall = std::stoll(serializedObject.substr(pos, 1), 0, 16); pos += 1;
	}
	catch(const std::exception& ex)
    {
        HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
        HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HM_CC_VD::setUpBidCoSMessages()
{
	try
	{
		HomeMaticDevice::setUpBidCoSMessages();

		_messages->add(std::shared_ptr<BidCoSMessage>(new BidCoSMessage(0x58, this, ACCESSPAIREDTOSENDER, NOACCESS, &HomeMaticDevice::handleDutyCyclePacket)));
	}
    catch(const std::exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::string HM_CC_VD::handleCLICommand(std::string command)
{
	return HomeMaticDevice::handleCLICommand(command);
}

void HM_CC_VD::handleDutyCyclePacket(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		HomeMaticDevice::handleDutyCyclePacket(messageCounter, packet);
		std::shared_ptr<Peer> peer = getPeer(packet->senderAddress());
		if(!peer || peer->deviceType != HMDeviceTypes::HMCCTC) return;
		int32_t oldValveState = _valveState;
		_valveState = (packet->payload()->at(1) * 100) / 256;
		HelperFunctions::printInfo("Info: 0x" + HelperFunctions::getHexString(_address) + ": New valve state " + std::to_string(_valveState));
		if(packet->destinationAddress() != _address) return; //Unidirectional packet (more than three valve drives connected to one room thermostat) or packet to other valve drive
		sendDutyCycleResponse(packet->senderAddress(), oldValveState, packet->payload()->at(0));
		if(_justPairedToOrThroughCentral)
		{
			std::chrono::milliseconds sleepingTime(2); //Seems very short, but the real valve drive also sends the next packet after 2 ms already
			std::this_thread::sleep_for(sleepingTime);
			_messageCounter[0]++;
			sendConfigParamsType2(_messageCounter[0], packet->senderAddress());
			_justPairedToOrThroughCentral = false;
		}
	}
    catch(const std::exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HM_CC_VD::sendDutyCycleResponse(int32_t destinationAddress, unsigned char oldValveState, unsigned char adjustmentCommand)
{
	try
	{
		std::shared_ptr<Peer> peer = getPeer(destinationAddress);
		if(!peer) return;
		HomeMaticDevice::sendDutyCycleResponse(destinationAddress);

		std::vector<uint8_t> payload;
		payload.push_back(0x01); //Subtype
		payload.push_back(0x01); //Channel
		payload.push_back(oldValveState * 2);
		unsigned char byte3 = 0x00;
		switch(adjustmentCommand)
		{
			case 0: //Do nothing
				break;
			case 1: //?
				break;
			case 2: //OFF
				if(oldValveState > _valveState) byte3 |= 0b00100000; //Closing
				break;
			case 3: //ON or new valve state
				if(oldValveState <= _valveState) //When the valve state is unchanged and the command is 3, the HM-CC-TC is set to "ON" and the response needs to be "opening". That's the reason for the "<=".
				{
					byte3 |= 0b00010000; //Opening
				}
				else
				{
					byte3 |= 0b00100000; //Closing
				}
				break;
			case 4: //?
				break;
		}
		if(_valveDriveBlocked) byte3 |= 0b00000010;
		if(_valveDriveLoose) byte3 |= 0b00000100;
		if(_adjustingRangeTooSmall) byte3 |= 0b00000110;
		if(_lowBattery) byte3 |= 0b00001000;

		payload.push_back(byte3);
		payload.push_back(0x37);
		sendOKWithPayload(peer->messageCounter, destinationAddress, payload, true);
	}
    catch(const std::exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HM_CC_VD::sendConfigParamsType2(int32_t messageCounter, int32_t destinationAddress)
{
	try
	{
		HomeMaticDevice::sendConfigParamsType2(messageCounter, destinationAddress);

		std::vector<uint8_t> payload;
		payload.push_back(0x04);
		payload.push_back(0x00);
		payload.push_back(0x00);
		payload.push_back(0x00);
		payload.push_back(0x00);
		payload.push_back(0x00);
		payload.push_back(0x05);
		payload.push_back(0x09);
		payload.push_back(*_offsetPosition);
		payload.push_back(0x0A);
		payload.push_back(*_errorPosition);
		payload.push_back(0x00);
		payload.push_back(0x00);
		std::shared_ptr<BidCoSPacket> config(new BidCoSPacket(messageCounter, 0x80, 0x10, _address, destinationAddress, payload));
		sendPacket(config);
	}
    catch(const std::exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::shared_ptr<Peer> HM_CC_VD::createPeer(int32_t address, int32_t firmwareVersion, HMDeviceTypes deviceType, std::string serialNumber, int32_t remoteChannel, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		std::shared_ptr<Peer> peer(new Peer(false));
		peer->address = address;
		peer->firmwareVersion = firmwareVersion;
		peer->deviceType = deviceType;
		peer->messageCounter = 0;
		peer->remoteChannel = remoteChannel;
		if(deviceType == HMDeviceTypes::HMCCTC || deviceType == HMDeviceTypes::HMUNKNOWN) peer->localChannel = 1; else peer->localChannel = 0;
		peer->setSerialNumber(serialNumber);
		return peer;
	}
    catch(const std::exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return std::shared_ptr<Peer>();
}

void HM_CC_VD::reset()
{
	try
	{
		HomeMaticDevice::reset();
		*_errorPosition = 0x0F;
		*_offsetPosition = 0x00;
		_valveState = 0x00;
	}
    catch(const std::exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HM_CC_VD::handleConfigPeerAdd(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		HomeMaticDevice::handleConfigPeerAdd(messageCounter, packet);

		int32_t address = (packet->payload()->at(2) << 16) + (packet->payload()->at(3) << 8) + (packet->payload()->at(4));
		_peersMutex.lock();
		if(_peers.size() > 20)
		{
			_peersMutex.unlock();
			return;
		}
		_peers[address]->deviceType = HMDeviceTypes::HMCCTC;
		_peersMutex.unlock();
	}
	catch(const std::exception& ex)
    {
		_peersMutex.unlock();
        HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(Exception& ex)
    {
    	_peersMutex.unlock();
        HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_peersMutex.unlock();
        HelperFunctions::printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HM_CC_VD::setValveDriveBlocked(bool valveDriveBlocked)
{
    _valveDriveBlocked = valveDriveBlocked;
    sendDutyCycleResponse(0x1D8DDD, 0x00, 0x00);
}

void HM_CC_VD::setValveDriveLoose(bool valveDriveLoose)
{
    _valveDriveLoose = valveDriveLoose;
    sendDutyCycleResponse(0x1D8DDD, 0x00, 0x00);
}

void HM_CC_VD::setAdjustingRangeTooSmall(bool adjustingRangeTooSmall)
{
    _adjustingRangeTooSmall = adjustingRangeTooSmall;
    sendDutyCycleResponse(0x1D8DDD, 0x00, 0x00);
}
