#include "Networks.h"
#include "DeliveryManager.h"

// TODO(you): Reliability on top of UDP lab session

void DeliveryReplication::onDeliverySuccess(DeliveryManager* deliveryManager) {
	LOG("Delivery Success");
}

void DeliveryReplication::onDeliveryFailure(DeliveryManager* deliveryManager)
{
	LOG("Delivery failed, sending the packet again", NULL, LOG_TYPE_ERROR);
}

void DeliveryManager::processAckdSequenceNumbers(const InputMemoryStream& packet)
{
	while (packet.RemainingByteCount() > 0)
	{
		uint32 sequenceN;
		packet >> sequenceN;

		uint32 count = 0;
		for (auto iter = pendingDeliveries.begin(); iter != pendingDeliveries.end(); iter++, count++) {
			if (count == sequenceN)
			{
				iter->delegate->onDeliverySuccess(this);
				pendingDeliveries.erase(iter);
				break;
			}
		}
	}
}

void DeliveryManager::processTiemdOutPackets()
{
	for (auto iter = pendingDeliveries.begin(); iter != pendingDeliveries.end(); iter++)
	{
		iter->dispatchTime += Time.deltaTime;
		if(iter->dispatchTime > PACKET_DELIVERY_TIMEOUT_SECONDS)
		{
			iter->dispatchTime = 0.0;
			iter->delegate->onDeliveryFailure(this);
		}
	}
}

bool DeliveryManager::processSequencerNumber(const InputMemoryStream& packet)
{
	bool ret = false;
	uint32 totalSequenceN;
	packet >> totalSequenceN;

	for (uint32 i = 0; i < totalSequenceN; i++) {
		uint32 sequenceNumber;
		packet >> sequenceNumber;
		if (next_expected_number == sequenceNumber)
		{
			sequence_numbers_pending_ack.push_back(sequenceNumber);
			ret = true;
		}
	}
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

Delivery* DeliveryManager::createDelivery(OutputMemoryStream& packet)
{
	Delivery newDelivery;
	newDelivery.sequenceNumber = next_sequence_number++;
	newDelivery.delegate = static_cast<DeliveryDelegate*>(new DeliveryReplication(packet));
	pendingDeliveries.push_back(newDelivery);
	return &pendingDeliveries[newDelivery.sequenceNumber];
}

void DeliveryManager::writeAllSequenceNumber(OutputMemoryStream& packet)
{
	packet << pendingDeliveries.size();
	for (auto iter = pendingDeliveries.begin(); iter != pendingDeliveries.end(); iter++)
		packet << iter->sequenceNumber;

	for (auto iter = pendingDeliveries.begin(); iter != pendingDeliveries.end(); iter++)
		dynamic_cast<DeliveryReplication*>(iter->delegate)->Get(packet);
}
