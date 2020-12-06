#pragma once

// TODO(you): Reliability on top of UDP lab session

class DeliveryManager;

class DeliveryDelegate
{
public:

	virtual void onDeliverySuccess(DeliveryManager* deliveryManager) = 0;
	virtual void onDeliveryFailure(DeliveryManager* deliveryManager) = 0;

	virtual void Get(OutputMemoryStream& packet) = 0;
};

class DeliveryReplication : public DeliveryDelegate
{
public:
	DeliveryReplication(OutputMemoryStream& packet) : deliveryPacket(packet) {}
	~DeliveryReplication() {}

	void onDeliverySuccess(DeliveryManager* deliveryManager)override;
	void onDeliveryFailure(DeliveryManager* deliveryManager)override;

	void Get(OutputMemoryStream& packet) override{
		packet.Write(deliveryPacket.GetBufferPtr(), deliveryPacket.GetSize());
	}

private:
	OutputMemoryStream deliveryPacket;
};

struct Delivery
{
	uint32 sequenceNumber = 0;
	double dispatchTime = 0.0;
	DeliveryDelegate* delegate = nullptr;
};

class DeliveryManager
{
public:

	void clear();

	//client
	bool hasSequenceNumberPendingAck()const;
	bool processSequencerNumber(const InputMemoryStream& packet);

	void writeSequenceNumbersPendingAck(OutputMemoryStream& packet);

	//server
	bool hasSequenceNumberPending()const;
	Delivery* createDelivery(OutputMemoryStream& packet);
	void writeAllSequenceNumber(OutputMemoryStream& packet);
	void processAckdSequenceNumbers(const InputMemoryStream& packet);
	void processTiemdOutPackets();

private:
	//sender side
	uint32 next_sequence_number = 0;
	std::vector<Delivery> pendingDeliveries;

	//recieiver side
	uint32 next_expected_number = 0;
	std::vector<uint32> sequence_numbers_pending_ack;
};