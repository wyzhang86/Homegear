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

#include "SQLite3.h"
#include "../GD/GD.h"

namespace BaseLib
{

namespace Database
{

SQLite3::SQLite3()
{
}

SQLite3::SQLite3(std::string databasePath, bool databaseSynchronous, bool databaseMemoryJournal) : SQLite3()
{
	if(databasePath.size() == 0) return;
    openDatabase(databasePath, databaseSynchronous, databaseMemoryJournal);
}

void SQLite3::init(std::string databasePath, bool databaseSynchronous, bool databaseMemoryJournal, std::string backupPath)
{
	if(_database) closeDatabase();
	if(!backupPath.empty()) GD::bl->hf.copyFile(databasePath, backupPath);
	if(databasePath.size() == 0) return;
    openDatabase(databasePath, databaseSynchronous, databaseMemoryJournal);
}

SQLite3::~SQLite3()
{
    closeDatabase();
}

void SQLite3::dispose()
{
	closeDatabase();
}

void SQLite3::openDatabase(std::string databasePath, bool databaseSynchronous, bool databaseMemoryJournal)
{
	try
	{
		_databaseMutex.lock();
		int result = sqlite3_open(databasePath.c_str(), &_database);
		if(result)
		{
			GD::out.printCritical("Can't open database: " + std::string(sqlite3_errmsg(_database)));
			sqlite3_close(_database);
			_database = nullptr;
			_databaseMutex.unlock();
			return;
		}
		sqlite3_extended_result_codes(_database, 1);
		char* errorMessage = nullptr;
		if(!databaseSynchronous)
		{
			sqlite3_exec(_database, "PRAGMA synchronous = OFF", 0, 0, &errorMessage);
			if(errorMessage)
			{
				GD::out.printError("Can't execute \"PRAGMA synchronous = OFF\": " + std::string(errorMessage));
				sqlite3_free(errorMessage);
			}
		}

		if(databaseMemoryJournal)
		{
			sqlite3_exec(_database, "PRAGMA journal_mode = MEMORY", 0, 0, &errorMessage);
			if(errorMessage)
			{
				GD::out.printError("Can't execute \"PRAGMA journal_mode = MEMORY\": " + std::string(errorMessage));
				sqlite3_free(errorMessage);
			}
		}

		sqlite3_exec(_database, "VACUUM", 0, 0, &errorMessage);
		if(errorMessage)
		{
			GD::out.printWarning("Warning: Can't execute \"VACUUM\": " + std::string(errorMessage));
			sqlite3_free(errorMessage);
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _databaseMutex.unlock();
}

void SQLite3::closeDatabase()
{
	try
	{
		if(!_database) return;
		_databaseMutex.lock();
		GD::out.printInfo("Closing database...");
		char* errorMessage = nullptr;
		sqlite3_exec(_database, "COMMIT", 0, 0, &errorMessage); //Release all savepoints
		if(errorMessage)
		{
			//Normally error is: No transaction is active, so no real error
			GD::out.printInfo("Can't execute \"COMMIT\": " + std::string(errorMessage));
			sqlite3_free(errorMessage);
		}
		sqlite3_exec(_database, "PRAGMA synchronous = FULL", 0, 0, &errorMessage);
		if(errorMessage)
		{
			GD::out.printError("Can't execute \"PRAGMA synchronous = FULL\": " + std::string(errorMessage));
			sqlite3_free(errorMessage);
		}
		sqlite3_exec(_database, "PRAGMA journal_mode = DELETE", 0, 0, &errorMessage);
		if(errorMessage)
		{
			GD::out.printError("Can't execute \"PRAGMA journal_mode = DELETE\": " + std::string(errorMessage));
			sqlite3_free(errorMessage);
		}
		sqlite3_close(_database);
		_database = nullptr;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _databaseMutex.unlock();
}

void SQLite3::getDataRows(sqlite3_stmt* statement, std::shared_ptr<DataTable>& dataRows)
{
	int32_t result;
	int32_t row = 0;
	while((result = sqlite3_step(statement)) == SQLITE_ROW)
	{
		for(int32_t i = 0; i < sqlite3_column_count(statement); i++)
		{
			std::shared_ptr<DataColumn> col(new DataColumn());
			col->index = i;
			int32_t columnType = sqlite3_column_type(statement, i);
			if(columnType == SQLITE_INTEGER)
			{
				col->dataType = DataColumn::DataType::Enum::INTEGER;
				col->intValue = sqlite3_column_int64(statement, i);
			}
			else if(columnType == SQLITE_FLOAT)
			{
				col->dataType = DataColumn::DataType::Enum::FLOAT;
				col->floatValue = sqlite3_column_double(statement, i);
			}
			else if(columnType == SQLITE_BLOB)
			{
				col->dataType = DataColumn::DataType::Enum::BLOB;
				char* binaryData = (char*)sqlite3_column_blob(statement, i);
				int32_t size = sqlite3_column_bytes(statement, i);
				if(size > 0) col->binaryValue.reset(new std::vector<char>(binaryData, binaryData + size));
			}
			else if(columnType == SQLITE_NULL)
			{
				col->dataType = DataColumn::DataType::Enum::NODATA;
			}
			else if(columnType == SQLITE_TEXT) //or SQLITE3_TEXT. As we are not using SQLite version 2 it doesn't matter
			{
				col->dataType = DataColumn::DataType::Enum::TEXT;
				col->textValue = std::string((const char*)sqlite3_column_text(statement, i));
			}
			if(i == 0) dataRows->insert(std::pair<uint32_t, std::map<uint32_t, std::shared_ptr<DataColumn>>>(row, std::map<uint32_t, std::shared_ptr<DataColumn>>()));
			dataRows->at(row)[i] = col;
		}
		row++;
	}
	if(result != SQLITE_DONE)
	{
		throw Exception("Can't execute command (Error-no.: " + std::to_string(result) + "): " + std::string(sqlite3_errmsg(_database)));
	}
}

void SQLite3::bindData(sqlite3_stmt* statement, DataRow& dataToEscape)
{
	//There is no try/catch block on purpose!
	int32_t result = 0;
	int32_t index = 1;
	std::for_each(dataToEscape.begin(), dataToEscape.end(), [&](std::shared_ptr<DataColumn> col)
	{
		switch(col->dataType)
		{
			case DataColumn::DataType::Enum::NODATA:
				result = sqlite3_bind_null(statement, index);
				break;
			case DataColumn::DataType::Enum::INTEGER:
				result = sqlite3_bind_int64(statement, index, col->intValue);
				break;
			case DataColumn::DataType::Enum::FLOAT:
				result = sqlite3_bind_double(statement, index, col->floatValue);
				break;
			case DataColumn::DataType::Enum::BLOB:
				if(col->binaryValue->empty()) result = sqlite3_bind_null(statement, index);
				else result = sqlite3_bind_blob(statement, index, &col->binaryValue->at(0), col->binaryValue->size(), SQLITE_STATIC);
				break;
			case DataColumn::DataType::Enum::TEXT:
				result = sqlite3_bind_text(statement, index, col->textValue.c_str(), -1, SQLITE_STATIC);
				break;
		}
		if(result)
		{
			throw(Exception(std::string(sqlite3_errmsg(_database))));
		}
		index++;
	});
}

uint32_t SQLite3::executeWriteCommand(std::string command, DataRow& dataToEscape)
{
	try
	{
		_databaseMutex.lock();
		if(!_database)
		{
			GD::out.printError("Error: Could not write to database. No database handle.");
			_databaseMutex.unlock();
			return 0;
		}
		sqlite3_stmt* statement = nullptr;
		int32_t result = sqlite3_prepare_v2(_database, command.c_str(), -1, &statement, NULL);
		if(result)
		{
			throw(Exception("Can't execute command \"" + command + "\": " + std::string(sqlite3_errmsg(_database))));
		}
		bindData(statement, dataToEscape);
		result = sqlite3_step(statement);
		if(result != SQLITE_DONE)
		{
			throw(Exception("Can't execute command: " + std::string(sqlite3_errmsg(_database))));
		}
		sqlite3_clear_bindings(statement);
		result = sqlite3_finalize(statement);
		if(result)
		{
			throw(Exception("Can't execute command \"" + command + "\": " + std::string(sqlite3_errmsg(_database))));
		}
		uint32_t rowID = sqlite3_last_insert_rowid(_database);
		_databaseMutex.unlock();
		return rowID;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
	_databaseMutex.unlock();
	return 0;
}

std::shared_ptr<DataTable> SQLite3::executeCommand(std::string command, DataRow& dataToEscape)
{
	std::shared_ptr<DataTable> dataRows(new DataTable());
	try
	{
		_databaseMutex.lock();
		if(!_database)
		{
			GD::out.printError("Error: Could not write to database. No database handle.");
			_databaseMutex.unlock();
			return dataRows;
		}
		sqlite3_stmt* statement = nullptr;
		int32_t result = sqlite3_prepare_v2(_database, command.c_str(), -1, &statement, NULL);
		if(result)
		{
			throw(Exception("Can't execute command \"" + command + "\": " + std::string(sqlite3_errmsg(_database))));
		}
		bindData(statement, dataToEscape);
		try
		{
			getDataRows(statement, dataRows);
		}
		catch(const Exception& ex)
		{
			if(command.compare(0, 7, "RELEASE") == 0)
			{
				GD::out.printInfo("Info: " + ex.what());
				sqlite3_clear_bindings(statement);
				_databaseMutex.unlock();
				return dataRows;
			}
			else GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		sqlite3_clear_bindings(statement);
		result = sqlite3_finalize(statement);
		if(result)
		{
			throw(Exception("Can't execute command \"" + command + "\": " + std::string(sqlite3_errmsg(_database))));
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
	_databaseMutex.unlock();
	return dataRows;
}

std::shared_ptr<DataTable> SQLite3::executeCommand(std::string command)
{
    std::shared_ptr<DataTable> dataRows(new DataTable());
    try
    {
    	_databaseMutex.lock();
    	if(!_database)
		{
			GD::out.printError("Error: Could not write to database. No database handle.");
			_databaseMutex.unlock();
			return dataRows;
		}
		sqlite3_stmt* statement = nullptr;
		int32_t result = sqlite3_prepare_v2(_database, command.c_str(), -1, &statement, NULL);
		if(result)
		{
			GD::out.printError("Can't execute command \"" + command + "\": " + std::string(sqlite3_errmsg(_database)));
			_databaseMutex.unlock();
			return dataRows;
		}
		try
		{
			getDataRows(statement, dataRows);
		}
		catch(const Exception& ex)
		{
			if(command.compare(0, 7, "RELEASE") == 0)
			{
				GD::out.printInfo("Info: " + ex.what());
				sqlite3_clear_bindings(statement);
				_databaseMutex.unlock();
				return dataRows;
			}
			else GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		sqlite3_clear_bindings(statement);
		result = sqlite3_finalize(statement);
		if(result)
		{
			GD::out.printError("Can't execute command \"" + command + "\" (Error-no.: " + std::to_string(result) + "): " + std::string(sqlite3_errmsg(_database)));
		}
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _databaseMutex.unlock();
    return dataRows;
}

/*
void SQLite3::benchmark1()
{
	//Duration for REPLACE in ms: 17836
	//Duration for UPDATE in ms: 14973
	std::vector<uint8_t> value({0});
	int64_t startTime = HelperFunctions::getTime();
	std::map<uint32_t, uint32_t> ids;
	for(uint32_t i = 0; i < 10000; ++i)
	{
		DataRow data;
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn(10000000)));
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn(1)));
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn(2)));
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn("TEST" + std::to_string(i))));
		value[0] = (i % 256);
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn(value)));
		ids[i] = executeWriteCommand("REPLACE INTO parameters VALUES(?, ?, ?, ?, ?, ?, ?, ?)", data);
	}
	int64_t duration = HelperFunctions::getTime() - startTime;
	std::cerr << "Duration for REPLACE in ms: " << duration << std::endl;
	startTime = HelperFunctions::getTime();
	for(uint32_t i = 0; i < 10000; ++i)
	{
		DataRow data;
		value[0] = (i % 256);
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn(value)));
		data.push_back(std::shared_ptr<DataColumn>(new DataColumn(ids[i])));
		executeWriteCommand("UPDATE parameters SET value=? WHERE parameterID=?", data);
	}
	duration = HelperFunctions::getTime() - startTime;
	std::cerr << "Duration for UPDATE in ms: " << duration << std::endl;
	executeCommand("DELETE FROM parameters WHERE peerID=10000000");
}

void SQLite3::benchmark2()
{
	try
	{
		//Duration for REPLACE in ms: 17002
		//Duration for UPDATE in ms: 14391
		sqlite3_stmt* statement = nullptr;
		int32_t result = sqlite3_prepare_v2(_database, "REPLACE INTO parameters VALUES(?, ?, ?, ?, ?, ?, ?, ?)", -1, &statement, NULL);
		if(result)
		{
			throw(Exception("Can't execute command: " + std::string(sqlite3_errmsg(_database))));
		}
		std::vector<uint8_t> value({0});
		int64_t startTime = HelperFunctions::getTime();
		std::map<uint32_t, uint32_t> ids;
		for(uint32_t i = 0; i < 10000; ++i)
		{
			DataRow data;
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(10000000)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(1)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(2)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn("TEST" + std::to_string(i))));
			value[0] = (i % 256);
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(value)));
			bindData(statement, data);
			ids[i] = sqlite3_step(statement);
			sqlite3_clear_bindings(statement);
			sqlite3_reset(statement);
		}
		sqlite3_finalize(statement);
		statement = nullptr;
		sqlite3_prepare_v2(_database, "UPDATE parameters SET value=? WHERE parameterID=?", -1, &statement, NULL);
		int64_t duration = HelperFunctions::getTime() - startTime;
		std::cerr << "Duration for REPLACE in ms: " << duration << std::endl;
		startTime = HelperFunctions::getTime();
		for(uint32_t i = 0; i < 10000; ++i)
		{
			DataRow data;
			value[0] = (i % 256);
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(value)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(ids[i])));
			bindData(statement, data);
			ids[i] = sqlite3_step(statement);
			sqlite3_clear_bindings(statement);
			sqlite3_reset(statement);
		}
		sqlite3_finalize(statement);
		duration = HelperFunctions::getTime() - startTime;
		std::cerr << "Duration for UPDATE in ms: " << duration << std::endl;
		executeCommand("DELETE FROM parameters WHERE peerID=10000000");
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void SQLite3::benchmark3()
{
	try
	{
		sqlite3_stmt* statement = nullptr;
		int32_t result = sqlite3_prepare_v2(_database, "REPLACE INTO parameters VALUES(?, ?, ?, ?, ?, ?, ?, ?)", -1, &statement, NULL);
		if(result)
		{
			throw(Exception("Can't execute command: " + std::string(sqlite3_errmsg(_database))));
		}
		std::vector<uint8_t> value({0});
		executeCommand("BEGIN IMMEDIATE TRANSACTION");
		int64_t startTime = HelperFunctions::getTime();
		std::map<uint32_t, uint32_t> ids;
		for(uint32_t i = 0; i < 10000; ++i)
		{
			DataRow data;
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(10000000)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(1)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(2)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn("TEST" + std::to_string(i))));
			value[0] = (i % 256);
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(value)));
			bindData(statement, data);
			ids[i] = sqlite3_step(statement);
			sqlite3_clear_bindings(statement);
			sqlite3_reset(statement);
		}
		sqlite3_finalize(statement);
		statement = nullptr;
		executeCommand("COMMIT TRANSACTION");
		int64_t duration = HelperFunctions::getTime() - startTime;
		std::cerr << "Duration for REPLACE in ms: " << duration << std::endl;
		sqlite3_prepare_v2(_database, "UPDATE parameters SET value=? WHERE parameterID=?", -1, &statement, NULL);
		executeCommand("BEGIN IMMEDIATE TRANSACTION");
		startTime = HelperFunctions::getTime();
		for(uint32_t i = 0; i < 10000; ++i)
		{
			DataRow data;
			value[0] = (i % 256);
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(value)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(ids[i])));
			bindData(statement, data);
			ids[i] = sqlite3_step(statement);
			sqlite3_clear_bindings(statement);
			sqlite3_reset(statement);
		}
		sqlite3_finalize(statement);
		executeCommand("COMMIT TRANSACTION");
		duration = HelperFunctions::getTime() - startTime;
		std::cerr << "Duration for UPDATE in ms: " << duration << std::endl;
		executeCommand("DELETE FROM parameters WHERE peerID=10000000");
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void SQLite3::benchmark4()
{
	try
	{
		sqlite3_stmt* statement = nullptr;
		int32_t result = sqlite3_prepare_v2(_database, "REPLACE INTO parameters VALUES(?, ?, ?, ?, ?, ?, ?, ?)", -1, &statement, NULL);
		if(result)
		{
			throw(Exception("Can't execute command: " + std::string(sqlite3_errmsg(_database))));
		}
		std::vector<uint8_t> value({0});
		executeCommand("SAVEPOINT peerConfig");
		int64_t startTime = HelperFunctions::getTime();
		std::map<uint32_t, uint32_t> ids;
		for(uint32_t i = 0; i < 10000; ++i)
		{
			DataRow data;
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(10000000)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(1)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(2)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn()));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn("TEST" + std::to_string(i))));
			value[0] = (i % 256);
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(value)));
			bindData(statement, data);
			ids[i] = sqlite3_step(statement);
			sqlite3_clear_bindings(statement);
			sqlite3_reset(statement);
		}
		sqlite3_finalize(statement);
		statement = nullptr;
		executeCommand("RELEASE peerConfig");
		int64_t duration = HelperFunctions::getTime() - startTime;
		std::cerr << "Duration for REPLACE in ms: " << duration << std::endl;
		sqlite3_prepare_v2(_database, "UPDATE parameters SET value=? WHERE parameterID=?", -1, &statement, NULL);
		executeCommand("SAVEPOINT peerConfig");
		startTime = HelperFunctions::getTime();
		for(uint32_t i = 0; i < 10000; ++i)
		{
			DataRow data;
			value[0] = (i % 256);
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(value)));
			data.push_back(std::shared_ptr<DataColumn>(new DataColumn(ids[i])));
			bindData(statement, data);
			ids[i] = sqlite3_step(statement);
			sqlite3_clear_bindings(statement);
			sqlite3_reset(statement);
		}
		sqlite3_finalize(statement);
		executeCommand("RELEASE peerConfig");
		duration = HelperFunctions::getTime() - startTime;
		std::cerr << "Duration for UPDATE in ms: " << duration << std::endl;
		executeCommand("DELETE FROM parameters WHERE peerID=10000000");
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}
*/
}
}
