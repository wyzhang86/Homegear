#ifndef BIDCOSQUEUE_H
#define BIDCOSQUEUE_H

class Peer;
class BidCoSMessage;
class BidCoSPacket;
class HomeMaticDevice;

#include <iostream>
#include <string>
#include <deque>
#include <queue>
#include <thread>
#include <mutex>

#include "Exception.h"

enum class QueueEntryType { UNDEFINED, MESSAGE, PACKET };

class BidCoSQueueEntry {
protected:
	QueueEntryType _type = QueueEntryType::UNDEFINED;
	std::shared_ptr<BidCoSMessage> _message;
	std::shared_ptr<BidCoSPacket> _packet;
public:
	BidCoSQueueEntry() {}
	virtual ~BidCoSQueueEntry() {}
	QueueEntryType getType() { return _type; }
	void setType(QueueEntryType type) { _type = type; }
	std::shared_ptr<BidCoSPacket> getPacket() { return _packet; }
	void setPacket(std::shared_ptr<BidCoSPacket> packet, bool setQueueEntryType) { _packet = packet; if(setQueueEntryType) _type = QueueEntryType::PACKET; }
	std::shared_ptr<BidCoSMessage> getMessage() { return _message; }
	void setMessage(std::shared_ptr<BidCoSMessage> message, bool setQueueEntryType) { _message = message; if(setQueueEntryType) _type = QueueEntryType::MESSAGE; }
};

enum class BidCoSQueueType { EMPTY, DEFAULT, PAIRING, PAIRINGCENTRAL, UNPAIRING };

class BidCoSQueue
{
    protected:
        std::deque<BidCoSQueueEntry> _queue;
        std::shared_ptr<std::queue<std::shared_ptr<BidCoSQueue>>> _pendingQueues;
        std::mutex _queueMutex;
        BidCoSQueueType _queueType;
        bool _stopResendThread = false;
        std::unique_ptr<std::thread> _resendThread;
        std::mutex _resendThreadMutex;
        int32_t resendCounter = 0;
        bool _workingOnPendingQueue = false;

        void pushPendingQueue();
        void sleepAndPushPendingQueue();
    public:
        int64_t* lastAction = nullptr;
        bool noSending = false;
        HomeMaticDevice* device = nullptr;
        std::shared_ptr<Peer> peer;
        BidCoSQueueType getQueueType() { return _queueType; }
        std::deque<BidCoSQueueEntry>* getQueue() { return &_queue; }
        void setQueueType(BidCoSQueueType queueType) {  _queueType = queueType; }

        void push(std::shared_ptr<BidCoSMessage> message);
        void push(std::shared_ptr<BidCoSMessage> message, std::shared_ptr<BidCoSPacket> packet);
        void push(std::shared_ptr<BidCoSPacket> packet);
        void push(std::shared_ptr<std::queue<std::shared_ptr<BidCoSQueue>>>& pendingBidCoSQueues);
        void push(std::shared_ptr<BidCoSQueue> pendingBidCoSQueue, bool popImmediately = true, bool clearPendingQueues = false);
        BidCoSQueueEntry* front() { return &_queue.front(); }
        void pop();
        bool isEmpty() { return _queue.empty(); }
        void clear();
        void resend();
        void startResendThread();
        void send(std::shared_ptr<BidCoSPacket> packet);
        void keepAlive();
        std::string serialize();

        BidCoSQueue();
        BidCoSQueue(std::string serializedObject, HomeMaticDevice* device);
        BidCoSQueue(BidCoSQueueType queueType);
        virtual ~BidCoSQueue();
};

#endif // BIDCOSQUEUE_H
