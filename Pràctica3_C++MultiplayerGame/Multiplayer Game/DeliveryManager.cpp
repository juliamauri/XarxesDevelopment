#include "Networks.h"
#include "DeliveryManager.h"

#include <iterator>
#include <stack>

// TODO(you): Reliability on top of UDP lab session

void DeliveryReplication::onDeliverySuccess(DeliveryManager* deliveryManager) {
	LOG("Delivery Success");
}

void DeliveryReplication::onDeliveryFailure(DeliveryManager* deliveryManager)
{
	LOG("Delivery failed, deleting it", NULL, LOG_TYPE_ERROR);
}

void DeliveryManager::processAckdSequenceNumbers(const InputMemoryStream& packet, ReplicationManagerServer* replicationServer)
{
	while (packet.RemainingByteCount() > 0)
	{
		uint32 sequenceN;
		packet >> sequenceN;

		if (!pendingDeliveries.empty()) {
			uint32 count = (*pendingDeliveries.begin()).sequenceNumber;
			for (auto iter = pendingDeliveries.begin(); iter != pendingDeliveries.end(); iter++, count++) {
				if (count == sequenceN)
				{
					replicationServer->popFrontAction();
					iter->delegate->onDeliverySuccess(this);
					pendingDeliveries.erase(iter);
					break;
				}
			}
		}
	}
}

void DeliveryManager::processTiemdOutPackets(ReplicationManagerServer* replicationServer)
{
	std::stack<std::vector<Delivery>::iterator> toDelete;

	for (auto iter = pendingDeliveries.begin(); iter != pendingDeliveries.end(); iter++)
	{
		iter->dispatchTime += Time.deltaTime;
		if(iter->dispatchTime > PACKET_DELIVERY_TIMEOUT_SECONDS)
		{
			replicationServer->popFrontAction();
			iter->delegate->onDeliveryFailure(this);
			toDelete.push(iter);
		}
	}

	while (!toDelete.empty())
	{
		pendingDeliveries.erase(toDelete.top());
		toDelete.pop();
	}
}

bool DeliveryManager::processSequencerNumber(const InputMemoryStream& packet, float& timeSequence)
{
	bool ret = false;
	size_t totalSequenceN;
	packet >> totalSequenceN;
	std::vector<uint32> savedNumbers;

	for (size_t i = 0; i < totalSequenceN; i++) {
		uint32 sequenceNumber;
		packet >> sequenceNumber;
		savedNumbers.push_back(sequenceNumber);
		if (next_expected_number == sequenceNumber)
		{
			sequence_numbers_pending_ack.push_back(sequenceNumber);
			next_expected_number++;
			ret = true;
		}
	}

	if (!ret && timeSequence > RESET_NEXT_EXPECTED_SECONDS) {
		timeSequence = 0.0f;
		if (!savedNumbers.empty()) {
			uint32 teoricalNextNumber = *savedNumbers.begin();
			if (teoricalNextNumber > next_expected_number) {
				next_expected_number = teoricalNextNumber;
				for (uint32 i = 0; i < totalSequenceN; i++) {
					uint32 sequenceNumber = savedNumbers[i];
					if (next_expected_number == sequenceNumber)
					{
						sequence_numbers_pending_ack.push_back(sequenceNumber);
						next_expected_number++;
						ret = true;
					}
				}
			}
		}
	}

	if (!ret) sequence_numbers_pending_ack.clear();
	return ret;
}

void DeliveryManager::clear()
{
	pendingDeliveries.clear();
	sequence_numbers_pending_ack.clear();
}

bool DeliveryManager::hasSequenceNumberPendingAck() const
{
	return !sequence_numbers_pending_ack.empty();
}

void DeliveryManager::writeSequenceNumbersPendingAck(OutputMemoryStream& packet)
{
	for (auto iter = sequence_numbers_pending_ack.begin(); iter != sequence_numbers_pending_ack.end(); iter++)
		packet << *iter;
	sequence_numbers_pending_ack.clear();
}

bool DeliveryManager::hasSequenceNumberPending() const
{
	return !pendingDeliveries.empty();
}

Delivery* DeliveryManager::createDelivery()
{
	Delivery newDelivery;
	newDelivery.sequenceNumber = next_sequence_number++;
	newDelivery.delegate = static_cast<DeliveryDelegate*>(new DeliveryReplication());
	pendingDeliveries.push_back(newDelivery);
	return &pendingDeliveries[pendingDeliveries.size() - 1];
}

void DeliveryManager::writeAllSequenceNumber(OutputMemoryStream& packet)
{
	size_t totalDeliveries = pendingDeliveries.size();
	packet << totalDeliveries;
	for (auto iter = pendingDeliveries.begin(); iter != pendingDeliveries.end(); iter++)
		packet << iter->sequenceNumber;
}
