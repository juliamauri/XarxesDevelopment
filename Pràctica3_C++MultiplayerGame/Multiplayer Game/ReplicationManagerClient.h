#pragma once

// TODO(you): World state replication lab session

class ReplicationManagerClient
{
public:

	void read(const InputMemoryStream& packet);


private:
	std::vector<uint32> netGODelted;
};