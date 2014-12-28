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

#ifndef VARIABLE_H_
#define VARIABLE_H_

#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <map>

namespace BaseLib
{
namespace RPC
{

enum class VariableType
{
	rpcVoid = 0x00,
	rpcInteger = 0x01,
	rpcBoolean = 0x02,
	rpcString = 0x03,
	rpcFloat = 0x04,
	rpcArray = 0x100,
	rpcStruct = 0x101,
	//rpcDate = 0x10,
	rpcBase64 = 0x11,
	rpcVariant = 0x1111
};

class Variable {
public:
	bool errorStruct = false;
	VariableType type;
	std::string stringValue;
	int32_t integerValue = 0;
	double floatValue = 0;
	bool booleanValue = false;
	std::shared_ptr<std::vector<std::shared_ptr<Variable>>> arrayValue;
	std::shared_ptr<std::map<std::string, std::shared_ptr<Variable>>> structValue;

	Variable() { type = VariableType::rpcVoid; arrayValue = std::shared_ptr<std::vector<std::shared_ptr<Variable>>>(new std::vector<std::shared_ptr<Variable>>()); structValue = std::shared_ptr<std::map<std::string, std::shared_ptr<Variable>>>(new std::map<std::string, std::shared_ptr<Variable>>()); }
	Variable(VariableType variableType) : Variable() { type = variableType; if(type == VariableType::rpcVariant) type = VariableType::rpcVoid; }
	Variable(uint8_t integer) : Variable() { type = VariableType::rpcInteger; integerValue = (int32_t)integer; }
	Variable(int32_t integer) : Variable() { type = VariableType::rpcInteger; integerValue = integer; }
	Variable(uint32_t integer) : Variable() { type = VariableType::rpcInteger; integerValue = (int32_t)integer; }
	Variable(std::string string) : Variable() { type = VariableType::rpcString; stringValue = string; }
	Variable(bool boolean) : Variable() { type = VariableType::rpcBoolean; booleanValue = boolean; }
	Variable(double floatVal) : Variable() { type = VariableType::rpcFloat; floatValue = floatVal; }
	virtual ~Variable();
	static std::shared_ptr<Variable> createError(int32_t faultCode, std::string faultString);
	void print();
	static std::string getTypeString(VariableType type);
	static std::shared_ptr<Variable> fromString(std::string value, VariableType type);
	bool operator==(const Variable& rhs);
	bool operator<(const Variable& rhs);
	bool operator<=(const Variable& rhs);
	bool operator>(const Variable& rhs);
	bool operator>=(const Variable& rhs);
	bool operator!=(const Variable& rhs);
private:
	void print(std::shared_ptr<Variable>, std::string indent);
	void printStruct(std::shared_ptr<std::map<std::string, std::shared_ptr<Variable>>> rpcStruct, std::string indent);
	void printArray(std::shared_ptr<std::vector<std::shared_ptr<Variable>>> rpcArray, std::string indent);
};

typedef std::pair<std::string, std::shared_ptr<Variable>> RPCStructElement;
typedef std::map<std::string, std::shared_ptr<Variable>> RPCStruct;
typedef std::vector<std::shared_ptr<Variable>> RPCArray;

}
}

#endif
