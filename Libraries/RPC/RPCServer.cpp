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

#include "RPCServer.h"
#include "../GD/GD.h"
#include "../../Modules/Base/BaseLib.h"

using namespace RPC;

RPCServer::Client::Client()
{
	 socket = std::shared_ptr<BaseLib::SocketOperations>(new BaseLib::SocketOperations(GD::bl.get()));
	 fileDescriptor = std::shared_ptr<BaseLib::FileDescriptor>(new BaseLib::FileDescriptor());
}

RPCServer::RPCServer()
{
	_rpcDecoder = std::unique_ptr<BaseLib::RPC::RPCDecoder>(new BaseLib::RPC::RPCDecoder(GD::bl.get()));
	_rpcEncoder = std::unique_ptr<BaseLib::RPC::RPCEncoder>(new BaseLib::RPC::RPCEncoder(GD::bl.get()));
	_xmlRpcDecoder = std::unique_ptr<BaseLib::RPC::XMLRPCDecoder>(new BaseLib::RPC::XMLRPCDecoder(GD::bl.get()));
	_xmlRpcEncoder = std::unique_ptr<BaseLib::RPC::XMLRPCEncoder>(new BaseLib::RPC::XMLRPCEncoder(GD::bl.get()));

	_settings.reset(new ServerSettings::Settings());
	_rpcMethods.reset(new std::map<std::string, std::shared_ptr<RPCMethod>>);
	_serverFileDescriptor.reset(new BaseLib::FileDescriptor);
	_threadPriority = GD::bl->settings.rpcServerThreadPriority();
	_threadPolicy = GD::bl->settings.rpcServerThreadPolicy();
}

RPCServer::~RPCServer()
{
	stop();
	_rpcMethods->clear();
}

void RPCServer::start(std::shared_ptr<ServerSettings::Settings>& settings)
{
	try
	{
		_stopServer = false;
		_settings = settings;
		if(!_settings)
		{
			GD::out.printError("Error: settings is nullptr.");
			return;
		}
		if(_settings->ssl)
		{
			SSL_load_error_strings();
			SSLeay_add_ssl_algorithms();
			_sslCTXMutex.lock();
			_sslCTX = SSL_CTX_new(SSLv23_server_method());
			if(!_sslCTX)
			{
				GD::out.printError("Error: Could not start RPC Server with SSL support." + std::string(ERR_reason_error_string(ERR_get_error())));
				_sslCTXMutex.unlock();
				return;
			}

			DH* dh = nullptr;
			if(GD::bl->settings.loadDHParamsFromFile())
			{
				BIO* bio = BIO_new_file(GD::bl->settings.dhParamPath().c_str(), "r");
				if(!bio)
				{
					GD::out.printError("Error: Could not start RPC Server with SSL support. Could not load Diffie-Hellman parameter file (" + GD::bl->settings.dhParamPath() + "): " + std::string(ERR_reason_error_string(ERR_get_error())));
					BIO_free(bio);
					SSL_CTX_free(_sslCTX);
					_sslCTX = nullptr;
					_sslCTXMutex.unlock();
					return;
				}

				dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
				if(!dh)
				{
					GD::out.printError("Error: Could not start RPC Server with SSL support. Reading of Diffie-Hellman parameters failed: " + std::string(ERR_reason_error_string(ERR_get_error())));
					BIO_free(bio);
					SSL_CTX_free(_sslCTX);
					_sslCTX = nullptr;
					_sslCTXMutex.unlock();
					return;
				}

				BIO_free(bio);
			}
			else
			{
				dh = DH_new();
				if(!dh)
				{
					GD::out.printError("Error: Could not start RPC Server with SSL support. Initialization of Diffie-Hellman parameters failed: " + std::string(ERR_reason_error_string(ERR_get_error())));
					SSL_CTX_free(_sslCTX);
					_sslCTX = nullptr;
					_sslCTXMutex.unlock();
					return;
				}
				GD::out.printInfo("Generating temporary Diffie-Hellman parameters. This might take a long time...");
				if(!DH_generate_parameters_ex(dh, settings->diffieHellmanKeySize, DH_GENERATOR_5, NULL))
				{
					GD::out.printError("Error: Could not start RPC Server with SSL support. Could not generate Diffie Hellman parameters: " + std::string(ERR_reason_error_string(ERR_get_error())));
					SSL_CTX_free(_sslCTX);
					_sslCTX = nullptr;
					_sslCTXMutex.unlock();
					return;
				}
			}
			int32_t codes = 0;
			if(!DH_check(dh, &codes))
			{
				GD::out.printError("Error: Could not start RPC Server with SSL support. Diffie Hellman check failed: " + std::string(ERR_reason_error_string(ERR_get_error())));
				SSL_CTX_free(_sslCTX);
				_sslCTX = nullptr;
				_sslCTXMutex.unlock();
				return;
			}
			if(!DH_generate_key(dh))
			{
				GD::out.printError("Error: Could not start RPC Server with SSL support. Could not generate Diffie Hellman key: " + std::string(ERR_reason_error_string(ERR_get_error())));
				SSL_CTX_free(_sslCTX);
				_sslCTX = nullptr;
				_sslCTXMutex.unlock();
				return;
			}
			SSL_CTX_set_options(_sslCTX, SSL_OP_NO_SSLv2);
			SSL_CTX_set_tmp_dh(_sslCTX, dh);
			if(SSL_CTX_use_certificate_file(_sslCTX, GD::bl->settings.certPath().c_str(), SSL_FILETYPE_PEM) < 1)
			{
				SSL_CTX_free(_sslCTX);
				_sslCTX = nullptr;
				GD::out.printError("Error: Could not load certificate file: " + std::string(ERR_reason_error_string(ERR_get_error())));
				_sslCTXMutex.unlock();
				return;
			}
			if(SSL_CTX_use_PrivateKey_file(_sslCTX, GD::bl->settings.keyPath().c_str(), SSL_FILETYPE_PEM) < 1)
			{
				SSL_CTX_free(_sslCTX);
				_sslCTX = nullptr;
				GD::out.printError("Error: Could not load key from certificate file: " + std::string(ERR_reason_error_string(ERR_get_error())));
				_sslCTXMutex.unlock();
				return;
			}
			if(!SSL_CTX_check_private_key(_sslCTX))
			{
				SSL_CTX_free(_sslCTX);
				_sslCTX = nullptr;
				GD::out.printError("Error: Private key does not match the public key");
				_sslCTXMutex.unlock();
				return;
			}
			_sslCTXMutex.unlock();
		}
		_mainThread = std::thread(&RPCServer::mainThread, this);
		BaseLib::Threads::setThreadPriority(GD::bl.get(), _mainThread.native_handle(), _threadPriority, _threadPolicy);
		_stopped = false;
		return;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _sslCTXMutex.unlock();
}

void RPCServer::stop()
{
	try
	{
		if(_stopped) return;
		_stopped = true;
		_stopServer = true;
		if(_mainThread.joinable()) _mainThread.join();
		_sslCTXMutex.lock();
		if(_sslCTX)
		{
			SSL_CTX_free(_sslCTX);
			_sslCTX = nullptr;
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
	_sslCTXMutex.unlock();
}

uint32_t RPCServer::connectionCount()
{
	try
	{
		_stateMutex.lock();
		uint32_t connectionCount = _clients.size();
		_stateMutex.unlock();
		return connectionCount;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _stateMutex.unlock();
    return 0;
}

void RPCServer::registerMethod(std::string methodName, std::shared_ptr<RPCMethod> method)
{
	try
	{
		if(_rpcMethods->find(methodName) != _rpcMethods->end())
		{
			GD::out.printWarning("Warning: Could not register RPC method, because a method with this name already exists.");
			return;
		}
		_rpcMethods->insert(std::pair<std::string, std::shared_ptr<RPCMethod>>(methodName, method));
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void RPCServer::closeClientConnection(std::shared_ptr<Client> client)
{
	try
	{
		removeClient(client->id);
		//Never ever call SSL_free before closing the socket!!! => segfault
		GD::bl->fileDescriptorManager.shutdown(client->fileDescriptor);
		if(client->ssl)
		{
			SSL_free(client->ssl);
			client->ssl = nullptr;
		}
	}
	catch(const std::exception& ex)
    {
		_stateMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_stateMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_stateMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void RPCServer::mainThread()
{
	try
	{
		getFileDescriptor();
		while(!_stopServer)
		{
			try
			{
				if(!_serverFileDescriptor || _serverFileDescriptor->descriptor < 0)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(5000));
					getFileDescriptor();
					continue;
				}
				std::shared_ptr<BaseLib::FileDescriptor> clientFileDescriptor = getClientFileDescriptor();
				if(!clientFileDescriptor || clientFileDescriptor->descriptor < 0) continue;
				_stateMutex.lock();
				if(_clients.size() >= _maxConnections)
				{
					_stateMutex.unlock();
					GD::out.printError("Error: Client connection rejected, because there are too many clients connected to me.");
					GD::bl->fileDescriptorManager.shutdown(clientFileDescriptor);
					continue;
				}
				std::shared_ptr<Client> client(new Client());
				client->id = _currentClientID++;
				client->fileDescriptor = clientFileDescriptor;
				_clients[client->id] = client;
				_stateMutex.unlock();

				if(_settings->ssl)
				{
					getSSLFileDescriptor(client);
					if(!client->ssl)
					{
						//Remove client from _clients again. Socket is already closed.
						closeClientConnection(client);
						continue;
					}
				}
				client->socket = std::shared_ptr<BaseLib::SocketOperations>(new BaseLib::SocketOperations(GD::bl.get(), client->fileDescriptor, client->ssl));

				client->readThread = std::thread(&RPCServer::readClient, this, client);
				BaseLib::Threads::setThreadPriority(GD::bl.get(), client->readThread.native_handle(), _threadPriority, _threadPolicy);
				client->readThread.detach();
			}
			catch(const std::exception& ex)
			{
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
				_stateMutex.unlock();
			}
			catch(BaseLib::Exception& ex)
			{
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
				_stateMutex.unlock();
			}
			catch(...)
			{
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
				_stateMutex.unlock();
			}
		}
		GD::bl->fileDescriptorManager.close(_serverFileDescriptor);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

bool RPCServer::clientValid(std::shared_ptr<Client>& client)
{
	try
	{
		if(client->fileDescriptor->descriptor < 0) return false;
		return true;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _stateMutex.unlock();
    return false;
}

void RPCServer::sendRPCResponseToClient(std::shared_ptr<Client> client, std::shared_ptr<std::vector<char>> data, bool keepAlive)
{
	try
	{
		if(_stopped) return;
		if(!clientValid(client)) return;
		if(!data || data->empty()) return;
		bool error = false;
		try
		{
			client->socket->proofwrite(data);
		}
		catch(BaseLib::SocketDataLimitException& ex)
		{
			GD::out.printWarning("Warning: " + ex.what());
		}
		catch(BaseLib::SocketOperationException& ex)
		{
			GD::out.printError("Error: " + ex.what());
			error = true;
		}
		if(!keepAlive || error) closeClientConnection(client);
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void RPCServer::analyzeRPC(std::shared_ptr<Client> client, std::shared_ptr<std::vector<char>> packet, PacketType::Enum packetType, bool keepAlive)
{
	try
	{
		if(_stopped) return;
		std::string methodName;
		std::shared_ptr<std::vector<std::shared_ptr<BaseLib::RPC::RPCVariable>>> parameters;
		if(packetType == PacketType::Enum::binaryRequest) parameters = _rpcDecoder->decodeRequest(packet, methodName);
		else if(packetType == PacketType::Enum::xmlRequest) parameters = _xmlRpcDecoder->decodeRequest(packet, methodName);
		if(!parameters)
		{
			GD::out.printWarning("Warning: Could not decode RPC packet.");
			return;
		}
		PacketType::Enum responseType = (packetType == PacketType::Enum::binaryRequest) ? PacketType::Enum::binaryResponse : PacketType::Enum::xmlResponse;
		if(!parameters->empty() && parameters->at(0)->errorStruct)
		{
			sendRPCResponseToClient(client, parameters->at(0), responseType, keepAlive);
			return;
		}
		callMethod(client, methodName, parameters, responseType, keepAlive);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void RPCServer::sendRPCResponseToClient(std::shared_ptr<Client> client, std::shared_ptr<BaseLib::RPC::RPCVariable> variable, PacketType::Enum responseType, bool keepAlive)
{
	try
	{
		if(_stopped) return;
		std::shared_ptr<std::vector<char>> data;
		if(responseType == PacketType::Enum::xmlResponse)
		{
			data = _xmlRpcEncoder->encodeResponse(variable);
			std::string header = getHttpResponseHeader(data->size());
			data->push_back('\r');
			data->push_back('\n');
			data->insert(data->begin(), header.begin(), header.end());
			if(GD::bl->debugLevel >= 5)
			{
				GD::out.printDebug("Response packet: " + std::string(&data->at(0), data->size()));
			}
			sendRPCResponseToClient(client, data, keepAlive);
		}
		else if(responseType == PacketType::Enum::binaryResponse)
		{
			data = _rpcEncoder->encodeResponse(variable);
			if(GD::bl->debugLevel >= 5)
			{
				GD::out.printDebug("Response binary:");
				GD::out.printBinary(data);
			}
			sendRPCResponseToClient(client, data, keepAlive);
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::shared_ptr<BaseLib::RPC::RPCVariable> RPCServer::callMethod(std::string& methodName, std::shared_ptr<BaseLib::RPC::RPCVariable>& parameters)
{
	try
	{
		if(!parameters) parameters = std::shared_ptr<BaseLib::RPC::RPCVariable>(new BaseLib::RPC::RPCVariable(BaseLib::RPC::RPCVariableType::rpcArray));
		if(_stopped) return BaseLib::RPC::RPCVariable::createError(100000, "Server is stopped.");
		if(_rpcMethods->find(methodName) == _rpcMethods->end())
		{
			GD::out.printError("Warning: RPC method not found: " + methodName);
			return BaseLib::RPC::RPCVariable::createError(-32601, ": Requested method not found.");
		}
		if(GD::bl->debugLevel >= 4)
		{
			GD::out.printInfo("Info: Method called: " + methodName + " Parameters:");
			for(std::vector<std::shared_ptr<BaseLib::RPC::RPCVariable>>::iterator i = parameters->arrayValue->begin(); i != parameters->arrayValue->end(); ++i)
			{
				(*i)->print();
			}
		}
		std::shared_ptr<BaseLib::RPC::RPCVariable> ret = _rpcMethods->at(methodName)->invoke(parameters->arrayValue);
		if(GD::bl->debugLevel >= 5)
		{
			GD::out.printDebug("Response: ");
			ret->print();
		}
		return ret;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return BaseLib::RPC::RPCVariable::createError(-32500, ": Unknown application error.");
}

void RPCServer::callMethod(std::shared_ptr<Client> client, std::string methodName, std::shared_ptr<std::vector<std::shared_ptr<BaseLib::RPC::RPCVariable>>> parameters, PacketType::Enum responseType, bool keepAlive)
{
	try
	{
		if(_stopped) return;
		if(_rpcMethods->find(methodName) == _rpcMethods->end())
		{
			GD::out.printError("Warning: RPC method not found: " + methodName);
			sendRPCResponseToClient(client, BaseLib::RPC::RPCVariable::createError(-32601, ": Requested method not found."), responseType, keepAlive);
			return;
		}
		if(GD::bl->debugLevel >= 4)
		{
			GD::out.printInfo("Info: Method called: " + methodName + " Parameters:");
			for(std::vector<std::shared_ptr<BaseLib::RPC::RPCVariable>>::iterator i = parameters->begin(); i != parameters->end(); ++i)
			{
				(*i)->print();
			}
		}
		std::shared_ptr<BaseLib::RPC::RPCVariable> ret = _rpcMethods->at(methodName)->invoke(parameters);
		if(GD::bl->debugLevel >= 5)
		{
			GD::out.printDebug("Response: ");
			ret->print();
		}
		sendRPCResponseToClient(client, ret, responseType, keepAlive);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::string RPCServer::getHttpResponseHeader(uint32_t contentLength)
{
	std::string header;
	header.append("HTTP/1.1 200 OK\r\n");
	header.append("Connection: close\r\n");
	header.append("Content-Type: text/xml\r\n");
	header.append("Content-Length: ").append(std::to_string(contentLength + 21)).append("\r\n\r\n");
	header.append("<?xml version=\"1.0\"?>");
	return header;
}

void RPCServer::analyzeRPCResponse(std::shared_ptr<Client> client, std::shared_ptr<std::vector<char>> packet, PacketType::Enum packetType, bool keepAlive)
{
	try
	{
		if(_stopped) return;
		std::shared_ptr<BaseLib::RPC::RPCVariable> response;
		if(packetType == PacketType::Enum::binaryResponse) response = _rpcDecoder->decodeResponse(packet);
		else if(packetType == PacketType::Enum::xmlResponse) response = _xmlRpcDecoder->decodeResponse(packet);
		if(!response) return;
		if(GD::bl->debugLevel >= 3)
		{
			GD::out.printWarning("Warning: RPC server received RPC response. This shouldn't happen. Response data: ");
			response->print();
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void RPCServer::packetReceived(std::shared_ptr<Client> client, std::shared_ptr<std::vector<char>> packet, PacketType::Enum packetType, bool keepAlive)
{
	try
	{
		if(packetType == PacketType::Enum::binaryRequest || packetType == PacketType::Enum::xmlRequest) analyzeRPC(client, packet, packetType, keepAlive);
		else if(packetType == PacketType::Enum::binaryResponse || packetType == PacketType::Enum::xmlResponse) analyzeRPCResponse(client, packet, packetType, keepAlive);
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void RPCServer::removeClient(int32_t clientID)
{
	try
	{
		_stateMutex.lock();
		if(_clients.find(clientID) != _clients.end()) _clients.erase(clientID);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    _stateMutex.unlock();
}

void RPCServer::readClient(std::shared_ptr<Client> client)
{
	try
	{
		if(!client) return;
		int32_t bufferMax = 1024;
		char buffer[bufferMax + 1];
		std::shared_ptr<std::vector<char>> packet(new std::vector<char>());
		uint32_t packetLength = 0;
		int32_t bytesRead;
		uint32_t dataSize = 0;
		PacketType::Enum packetType;
		HTTP http;

		GD::out.printDebug("Listening for incoming packets from client number " + std::to_string(client->fileDescriptor->descriptor) + ".");
		while(!_stopServer)
		{
			try
			{
				bytesRead = client->socket->proofread(buffer, bufferMax);
				//Some clients send only one byte in the first packet
				if(packetLength == 0 && bytesRead == 1) bytesRead += client->socket->proofread(&buffer[1], bufferMax - 1);
			}
			catch(BaseLib::SocketTimeOutException& ex)
			{
				continue;
			}
			catch(BaseLib::SocketClosedException& ex)
			{
				GD::out.printInfo("Info: " + ex.what());
				break;
			}
			catch(BaseLib::SocketOperationException& ex)
			{
				GD::out.printError(ex.what());
				break;
			}

			if(!clientValid(client)) break;

			if(GD::bl->debugLevel >= 5)
			{
				std::vector<uint8_t> rawPacket;
				rawPacket.insert(rawPacket.begin(), buffer, buffer + bytesRead);
				GD::out.printDebug("Debug: Packet received: " + BaseLib::HelperFunctions::getHexString(rawPacket));
			}
			if(!strncmp(&buffer[0], "Bin", 3))
			{
				http.reset();
				//buffer[3] & 1 is true for buffer[3] == 0xFF, too
				packetType = (buffer[3] & 1) ? PacketType::Enum::binaryResponse : PacketType::Enum::binaryRequest;
				if(bytesRead < 8) continue;
				GD::bl->hf.memcpyBigEndian((char*)&dataSize, buffer + 4, 4);
				GD::out.printDebug("Receiving binary rpc packet with size: " + std::to_string(dataSize), 6);
				if(dataSize == 0) continue;
				if(dataSize > 104857600)
				{
					GD::out.printError("Error: Packet with data larger than 100 MiB received.");
					continue;
				}
				packet.reset(new std::vector<char>());
				packet->insert(packet->end(), buffer, buffer + bytesRead);
				std::shared_ptr<BaseLib::RPC::RPCHeader> header = _rpcDecoder->decodeHeader(packet);
				if(_settings->authType == ServerSettings::Settings::AuthType::basic)
				{
					if(!client->auth.initialized()) client->auth = Auth(client->socket, _settings->validUsers);
					bool authFailed = false;
					try
					{
						if(!client->auth.basicServer(header))
						{
							GD::out.printError("Error: Authorization failed. Closing connection.");
							break;
						}
						else GD::out.printDebug("Client successfully authorized using basic authentification.");
					}
					catch(AuthException& ex)
					{
						GD::out.printError("Error: Authorization failed. Closing connection. Error was: " + ex.what());
						break;
					}
				}
				if(dataSize > bytesRead - 8) packetLength = bytesRead - 8;
				else
				{
					packetLength = 0;
					std::thread t(&RPCServer::packetReceived, this, client, packet, packetType, true);
					BaseLib::Threads::setThreadPriority(GD::bl.get(), t.native_handle(), _threadPriority, _threadPolicy);
					t.detach();
				}
			}
			else if(!strncmp(&buffer[0], "POST", 4) || !strncmp(&buffer[0], "HTTP/1.", 7))
			{
				packetType = (!strncmp(&buffer[0], "POST", 4)) ? PacketType::Enum::xmlRequest : PacketType::Enum::xmlResponse;
				//We are using string functions to process the buffer. So just to make sure,
				//they don't do something in the memory after buffer, we add '\0'
				buffer[bytesRead] = '\0';

				try
				{
					http.reset();
					http.process(buffer, bytesRead);
				}
				catch(HTTPException& ex)
				{
					GD::out.printError("XML RPC Server: Could not process HTTP packet: " + ex.what() + " Buffer: " + std::string(buffer, bytesRead));
				}

				if(http.getHeader()->contentLength > 104857600)
				{
					GD::out.printError("Error: Packet with data larger than 100 MiB received.");
					continue;
				}

				if(_settings->authType == ServerSettings::Settings::AuthType::basic)
				{
					if(!client->auth.initialized()) client->auth = Auth(client->socket, _settings->validUsers);
					bool authFailed = false;
					try
					{
						if(!client->auth.basicServer(http))
						{
							GD::out.printError("Error: Authorization failed for host " + http.getHeader()->host + ". Closing connection.");
							break;
						}
						else GD::out.printDebug("Client successfully authorized using basic authentification.");
					}
					catch(AuthException& ex)
					{
						GD::out.printError("Error: Authorization failed for host " + http.getHeader()->host + ". Closing connection. Error was: " + ex.what());
						break;
					}
				}
			}
			else if(packetLength > 0 || http.dataProcessed())
			{
				if(packetType == PacketType::Enum::binaryRequest || packetType == PacketType::Enum::binaryRequest)
				{
					if(packetLength + bytesRead > dataSize)
					{
						GD::out.printError("Error: Packet length is wrong.");
						packetLength = 0;
						continue;
					}
					packet->insert(packet->end(), buffer, buffer + bytesRead);
					packetLength += bytesRead;
					if(packetLength == dataSize)
					{
						packet->push_back('\0');
						std::thread t(&RPCServer::packetReceived, this, client, packet, packetType, true);
						BaseLib::Threads::setThreadPriority(GD::bl.get(), t.native_handle(), _threadPriority, _threadPolicy);
						t.detach();
						packetLength = 0;
					}
				}
				else
				{
					try
					{
						http.process(buffer, bytesRead);
					}
					catch(HTTPException& ex)
					{
						GD::out.printError("XML RPC Server: Could not process HTTP packet: " + ex.what() + " Buffer: " + std::string(buffer, bytesRead));
					}

					if(http.getContentSize() > 104857600)
					{
						http.reset();
						GD::out.printError("Error: Packet with data larger than 100 MiB received.");
					}
				}
			}
			else
			{
				GD::out.printError("Error: Uninterpretable packet received. Closing connection. Packet was: " + std::string(buffer, buffer + bytesRead));
				break;
			}
			if(http.isFinished())
			{
				std::thread t(&RPCServer::packetReceived, this, client, http.getContent(), packetType, http.getHeader()->connection == HTTP::Connection::Enum::keepAlive);
				BaseLib::Threads::setThreadPriority(GD::bl.get(), t.native_handle(), _threadPriority, _threadPolicy);
				t.detach();
				packetLength = 0;
				http.reset();
			}
		}
		//This point is only reached, when stopServer is true or the socket is closed
		closeClientConnection(client);
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

std::shared_ptr<BaseLib::FileDescriptor> RPCServer::getClientFileDescriptor()
{
	std::shared_ptr<BaseLib::FileDescriptor> fileDescriptor;
	try
	{
		timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		fd_set readFileDescriptor;
		FD_ZERO(&readFileDescriptor);
		FD_SET(_serverFileDescriptor->descriptor, &readFileDescriptor);
		if(!select(_serverFileDescriptor->descriptor + 1, &readFileDescriptor, NULL, NULL, &timeout)) return fileDescriptor;

		struct sockaddr_storage clientInfo;
		socklen_t addressSize = sizeof(addressSize);
		fileDescriptor = GD::bl->fileDescriptorManager.add(accept(_serverFileDescriptor->descriptor, (struct sockaddr *) &clientInfo, &addressSize));
		if(!fileDescriptor) return fileDescriptor;

		getpeername(fileDescriptor->descriptor, (struct sockaddr*)&clientInfo, &addressSize);

		uint32_t port;
		char ipString[INET6_ADDRSTRLEN];
		if (clientInfo.ss_family == AF_INET) {
			struct sockaddr_in *s = (struct sockaddr_in *)&clientInfo;
			port = ntohs(s->sin_port);
			inet_ntop(AF_INET, &s->sin_addr, ipString, sizeof(ipString));
		} else { // AF_INET6
			struct sockaddr_in6 *s = (struct sockaddr_in6 *)&clientInfo;
			port = ntohs(s->sin6_port);
			inet_ntop(AF_INET6, &s->sin6_addr, ipString, sizeof(ipString));
		}
		std::string ipString2(&ipString[0]);
		GD::out.printInfo("Info: Connection from " + ipString2 + ":" + std::to_string(port) + " accepted. Client number: " + std::to_string(fileDescriptor->descriptor));
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return fileDescriptor;
}

void RPCServer::getSSLFileDescriptor(std::shared_ptr<Client> client)
{
	try
	{
		if(GD::bl->settings.devLog()) GD::out.printInfo("Position 1");
		_sslCTXMutex.lock();
		if(!_sslCTX)
		{
			GD::out.printError("Error: Could not initiate SSL connection. _sslCTX is nullptr.");
			_sslCTXMutex.unlock();
			return;
		}
		client->ssl = SSL_new(_sslCTX);
		if(GD::bl->settings.devLog()) GD::out.printInfo("Position 2");
		_sslCTXMutex.unlock();
		if(!client->ssl)
		{
			GD::out.printError("Error creating SSL structure.");
			return;
		}
		if(!client->fileDescriptor || client->fileDescriptor->descriptor == -1)
		{
			GD::out.printError("Error setting SSL file descriptor: Provided file descriptor is invalid.");
			if(client->ssl) SSL_free(client->ssl);
			client->ssl = nullptr;
			return;
		}
		if(GD::bl->settings.devLog()) GD::out.printInfo("Position 3");
		if(!SSL_set_fd(client->ssl, client->fileDescriptor->descriptor))
		{
			GD::out.printError("Error setting SSL file descriptor: " + BaseLib::HelperFunctions::getSSLError(SSL_get_error(client->ssl, 0)));
			GD::bl->fileDescriptorManager.shutdown(client->fileDescriptor);
			if(client->ssl) SSL_free(client->ssl);
			client->ssl = nullptr;
			return;
		}
		if(!client->ssl)
		{
			GD::out.printError("Error getting SSL file descriptor: client->ssl is nullptr.");
			return;
		}
		if(GD::bl->settings.devLog()) GD::out.printInfo("Position 4");
		int32_t result = SSL_accept(client->ssl);
		if(GD::bl->settings.devLog()) GD::out.printInfo("Position 5");
		if(result < 1)
		{
			if(client->ssl && result != 0) GD::out.printError("Error during TLS/SSL handshake: " + BaseLib::HelperFunctions::getSSLError(SSL_get_error(client->ssl, result)));
			else if(result == 0) GD::out.printError("The TLS/SSL handshake was unsuccessful. Client number: " + std::to_string(client->fileDescriptor->descriptor));
			else GD::out.printError("Fatal error during TLS/SSL handshake. Client number: " + std::to_string(client->fileDescriptor->descriptor));
			GD::bl->fileDescriptorManager.shutdown(client->fileDescriptor);
			if(client->ssl) SSL_free(client->ssl);
			client->ssl = nullptr;
		}
		else GD::out.printInfo("Info: New SSL connection to RPC server. Cipher: " + std::string(SSL_get_cipher(client->ssl)) + " (" + std::to_string(SSL_get_cipher_bits(client->ssl, 0)) + " bits)");
		if(GD::bl->settings.devLog()) GD::out.printInfo("Position 6");
		return;
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    GD::bl->fileDescriptorManager.shutdown(client->fileDescriptor);
    if(client->ssl) SSL_free(client->ssl);
    client->ssl = nullptr;
}

void RPCServer::getFileDescriptor()
{
	try
	{
		addrinfo hostInfo;
		addrinfo *serverInfo = nullptr;

		int32_t yes = 1;

		memset(&hostInfo, 0, sizeof(hostInfo));

		hostInfo.ai_family = AF_UNSPEC;
		hostInfo.ai_socktype = SOCK_STREAM;
		hostInfo.ai_flags = AI_PASSIVE;
		char buffer[100];
		std::string port = std::to_string(_settings->port);
		int32_t result;
		if((result = getaddrinfo(_settings->interface.c_str(), port.c_str(), &hostInfo, &serverInfo)) != 0)
		{
			GD::out.printCritical("Error: Could not get address information: " + std::string(gai_strerror(result)));
			return;
		}

		bool bound = false;
		int32_t error = 0;
		for(struct addrinfo *info = serverInfo; info != 0; info = info->ai_next)
		{
			_serverFileDescriptor = GD::bl->fileDescriptorManager.add(socket(info->ai_family, info->ai_socktype, info->ai_protocol));
			if(_serverFileDescriptor->descriptor == -1) continue;
			if(!(fcntl(_serverFileDescriptor->descriptor, F_GETFL) & O_NONBLOCK))
			{
				if(fcntl(_serverFileDescriptor->descriptor, F_SETFL, fcntl(_serverFileDescriptor->descriptor, F_GETFL) | O_NONBLOCK) < 0) throw BaseLib::Exception("Error: Could not set socket options.");
			}
			if(setsockopt(_serverFileDescriptor->descriptor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int32_t)) == -1) throw BaseLib::Exception("Error: Could not set socket options.");
			if(bind(_serverFileDescriptor->descriptor, info->ai_addr, info->ai_addrlen) == -1)
			{
				error = errno;
				continue;
			}
			std::string address;
			switch (info->ai_family)
			{
				case AF_INET:
					inet_ntop (info->ai_family, &((struct sockaddr_in *) info->ai_addr)->sin_addr, buffer, 100);
					address = std::string(buffer);
					break;
				case AF_INET6:
					inet_ntop (info->ai_family, &((struct sockaddr_in6 *) info->ai_addr)->sin6_addr, buffer, 100);
					address = std::string(buffer);
					break;
			}
			GD::out.printInfo("Info: RPC Server started listening on address " + address + " and port " + port);
			bound = true;
			break;
		}
		freeaddrinfo(serverInfo);
		if(!bound)
		{
			GD::bl->fileDescriptorManager.close(_serverFileDescriptor);
			GD::out.printCritical("Error: Server could not start listening on port " + port + ": " + std::string(strerror(error)));
			return;
		}
		if(_serverFileDescriptor->descriptor == -1 || !bound || listen(_serverFileDescriptor->descriptor, _backlog) == -1)
		{
			GD::bl->fileDescriptorManager.close(_serverFileDescriptor);
			GD::out.printCritical("Error: Server could not start listening on port " + port + ": " + std::string(strerror(errno)));
			return;
		}
    }
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}
