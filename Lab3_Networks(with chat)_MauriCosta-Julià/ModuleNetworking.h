#pragma once


class ModuleNetworking : public Module
{
private:

	//////////////////////////////////////////////////////////////////////
	// Module virtual methods
	//////////////////////////////////////////////////////////////////////

	bool init() override;

	bool preUpdate() override;

	bool cleanUp() override;



	//////////////////////////////////////////////////////////////////////
	// Socket event callbacks
	//////////////////////////////////////////////////////////////////////

	virtual bool isListenSocket(SOCKET socket) const { return false; }

	virtual void onSocketConnected(SOCKET socket, const sockaddr_in &socketAddress) { }

	virtual void onSocketReceivedData(SOCKET s, byte * data, uint32 size) = 0;

	virtual void onSocketDisconnected(SOCKET s) = 0;


protected:

	std::vector<SOCKET> sockets;

	void addSocket(SOCKET socket);

	void disconnect();

	static void reportError(const char *message, DWORD errorNum = 0);


	//////////////////////////////////////////////////////////////////////
	// Chat events enum
	//////////////////////////////////////////////////////////////////////

	enum ChatEvents {
		C_USERCONNECTED = 1,
		C_USERDISCONNECTED,
		C_MENSAGE
	};
};

