// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * common/transaction.h:
 *   A Transaction representation.
 *
 **********************************************************************/

#ifndef _TRANSACTION_H_
#define _TRANSACTION_H_

#include "tapir/lib/assert.h"
#include "tapir/lib/message.h"
#include "tapir/store/common/timestamp.h"
#include "tapir/store/common/common-proto.pb.h"
#include "tapir/store/common/increment.h"

#include <unordered_map>
#include <vector>

// Reply types
#define REPLY_OK 0
#define REPLY_FAIL 1
#define REPLY_RETRY 2
#define REPLY_ABSTAIN 3
#define REPLY_TIMEOUT 4
#define REPLY_NETWORK_FAILURE 5
#define REPLY_MAX 6

class Transaction {
private:
    // map between key and timestamp at
    // which the read happened and how
    // many times this key has been read
    std::unordered_map<std::string, Timestamp> readSet;

    // map between key and value(s)
    std::unordered_map<std::string, std::string> writeSet;

	// map between key and what to increment by
	std::unordered_map<std::string, std::vector<Increment>> incrementSet;

public:
    Transaction();
    Transaction(const TransactionMessage &msg);
    ~Transaction();

    const std::unordered_map<std::string, Timestamp>& getReadSet() const;
    const std::unordered_map<std::string, std::string>& getWriteSet() const;
	const std::unordered_map<std::string, std::vector<Increment>>& getIncrementSet() const;
    
    void addReadSet(const std::string &key, const Timestamp &readTime);
    void addWriteSet(const std::string &key, const std::string &value);
	void addIncrementSet(const std::string &key, const Increment inc);
    void serialize(TransactionMessage *msg) const;
};

#endif /* _TRANSACTION_H_ */
