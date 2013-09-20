/* Copyright 2013 Sathya Laufer
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

#ifndef EVENTHANDLER_H_
#define EVENTHANDLER_H_

#include <memory>
#include <string>
#include <map>

#include "Exception.h"
#include "Database.h"
#include "RPC/RPCVariable.h"

class Event
{
public:
	struct Type
	{
		enum Enum { triggered, timed };
	};

	struct Trigger
	{
		enum Enum { none, change, update, value, belowThreshold, aboveThreshold };
	};

	struct Operation
	{
		enum Enum { none, addition, substraction, multiplication, division };
	};

	uint32_t id = 0;
	Type::Enum type = Type::Enum::triggered;
	std::string name;
	std::string address;
	std::string variable;
	Trigger::Enum trigger = Trigger::Enum::none;
	std::string eventMethod;
	std::shared_ptr<RPC::RPCVariable> eventMethodParameters;
	uint32_t resetAfter = 0;
	uint32_t initialTime = 0;
	Operation::Enum incrementOperator = Operation::Enum::none;
	double incrementBy = 0;
	uint32_t maxTime = 0;
	std::string resetMethod;
	std::shared_ptr<RPC::RPCVariable> resetMethodParameters;
	uint32_t eventTime = 0;
	uint32_t recurAfter = 0;

	Event() {}
	virtual ~Event() {}
};

class EventHandler
{
public:
	EventHandler();
	virtual ~EventHandler();

	std::shared_ptr<RPC::RPCVariable> add(std::shared_ptr<RPC::RPCVariable> eventDescription);
	std::shared_ptr<RPC::RPCVariable> remove(std::string name);
protected:
	std::vector<std::shared_ptr<Event>> _timedEvents;
	std::map<std::string, std::map<std::string, std::vector<std::shared_ptr<Event>>>> _triggeredEvents;
};
#endif /* EVENTHANDLER_H_ */