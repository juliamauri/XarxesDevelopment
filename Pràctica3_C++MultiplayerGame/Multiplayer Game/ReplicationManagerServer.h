#pragma once

// TODO(you): World state replication lab session
#include <unordered_map>

class ReplicationManagerServer
{
public:

	void create(uint32 networkId);
	void update(uint32 networkId);
	void destroy(uint32 networkId);

	bool write(OutputMemoryStream& packet);

private:
	std::unordered_map<uint32, ReplicationCommand> replicationCommands;

};