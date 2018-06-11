// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * common/transaction.cc
 *   A transaction implementation.
 *
 **********************************************************************/

#include "tapir/store/common/transaction.h"

using namespace std;

Transaction::Transaction() :
    readSet(), writeSet() { }

Transaction::Transaction(const TransactionMessage &msg) 
{
    for (int i = 0; i < msg.readset_size(); i++) {
        ReadMessage readMsg = msg.readset(i);
		addReadSet(readMsg.key(), readMsg.readtime());
    }

    for (int i = 0; i < msg.writeset_size(); i++) {
        WriteMessage writeMsg = msg.writeset(i);
		addWriteSet(writeMsg.key(), writeMsg.value());
    }

    for (int i = 0; i < msg.incrementset_size(); i++) {
        IncrementMessage incMsg = msg.incrementset(i);
		addIncrementSet(incMsg.key(), Increment(incMsg.value(), incMsg.op()));
    }
}

Transaction::~Transaction() { 
}

const unordered_map<string, Timestamp>&
Transaction::getReadSet() const
{
    return readSet;
}

const unordered_map<string, string>&
Transaction::getWriteSet() const
{
    return writeSet;
}

const unordered_map<string, std::vector<Increment>>&
Transaction::getIncrementSet() const
{
    return incrementSet;
}

void
Transaction::addReadSet(const string &key,
                        const Timestamp &readTime)
{
    readSet[key] = readTime;
}

void
Transaction::addWriteSet(const string &key,
                         const string &value)
{
    writeSet[key] = value;
	//we are overwriting the increments we previously did
	incrementSet[key].clear();
}

void
Transaction::addIncrementSet(const string &key,
                             const Increment inc)
{
	auto list = incrementSet[key];
	list.push_back(inc);	
}

void
Transaction::serialize(TransactionMessage *msg) const
{
    for (auto read : readSet) {
        ReadMessage *readMsg = msg->add_readset();
        readMsg->set_key(read.first);
        read.second.serialize(readMsg->mutable_readtime());
    }

    for (auto write : writeSet) {
        WriteMessage *writeMsg = msg->add_writeset();
        writeMsg->set_key(write.first);
        writeMsg->set_value(write.second);
    }

	for (auto incList : incrementSet) {
		for (auto inc : incList.second) {
			IncrementMessage *incMsg = msg->add_incrementset();
			incMsg->set_key(incList.first);
			incMsg->set_value(inc.value);
			incMsg->set_op(inc.op);
		}
	}
}
