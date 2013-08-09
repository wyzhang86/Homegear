#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "Exception.h"

#include <iostream>
#include <string>

class Settings {
public:
	Settings();
	virtual ~Settings() {}
	void load(std::string filename);

	std::string rpcInterface() { return _rpcInterface; }
	int32_t rpcPort() { return _rpcPort; }
	int32_t debugLevel() { return _debugLevel; }
	std::string databasePath() { return _databasePath; }
	std::string rfDeviceType() { return _rfDeviceType; }
	std::string rfDevice() { return _rfDevice; }
	std::string logfilePath() { return _logfilePath; }
private:
	std::string _rpcInterface;
	int32_t _rpcPort = 2001;
	int32_t _debugLevel = 3;
	std::string _databasePath;
	std::string _rfDeviceType;
	std::string _rfDevice;
	std::string _logfilePath;

	void reset();
};

#endif /* SETTINGS_H_ */
