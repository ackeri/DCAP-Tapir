// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/common/backend/versionstore.cc:
 *   Timestamped version store
 *
 * Copyright 2015 Irene Zhang <iyzhang@cs.washington.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************************/

#ifndef _VERSIONED_KV_STORE_H_
#define _VERSIONED_KV_STORE_H_

#include "tapir/lib/assert.h"
#include "tapir/lib/message.h"
#include "tapir/store/common/timestamp.h"
#include "tapir/store/common/increment.h"

#include <set>
#include <map>
#include <unordered_map>

#define WRITE 0
#define INCREMENT 1
#define APPEND 2

struct VersionedValue {
	Timestamp time;
	std::string value;
	uint64_t op;

	VersionedValue() : time(Timestamp()), value("tmp"), op(WRITE) { };
	VersionedValue(Timestamp commit) : time(commit), value("tmp"), op(WRITE) { };
	VersionedValue(Timestamp commit, std::string val) : time(commit), value(val), op(WRITE) { };
	VersionedValue(Timestamp commit, std::string val, int operation) : time(commit), value(val), op(operation) { };


	friend bool operator> (const VersionedValue &v1, const VersionedValue &v2) {
		return v1.time > v2.time;
	};
	friend bool operator< (const VersionedValue &v1, const VersionedValue &v2) {
		return v1.time < v2.time;
	};
};

class VersionedKVStore
{
public:
    VersionedKVStore();
    ~VersionedKVStore();

    bool get(const std::string &key, VersionedValue &value);
    bool get(const std::string &key, const Timestamp &t, VersionedValue &value);
    bool getRange(const std::string &key, const Timestamp &t, std::pair<Timestamp, Timestamp> &range);
	void getValue(const std::string &key, const Timestamp &t, std::set<VersionedValue>::iterator &it);
    bool getLastRead(const std::string &key, Timestamp &readTime);
    bool getLastRead(const std::string &key, const Timestamp &t, Timestamp &readTime);
    void put(const std::string &key, const std::string &value, const Timestamp &t);
	void increment(const std::string &key, const Increment inc, const Timestamp &t);
    void commitGet(const std::string &key, const Timestamp &readTime, const Timestamp &commit);

    /* Global store which keeps key -> (timestamp, value) list. */
    std::unordered_map< std::string, std::set<VersionedValue> > store;
    std::unordered_map< std::string, std::map< Timestamp, Timestamp > > lastReads;
    bool inStore(const std::string &key);
};

#endif  /* _VERSIONED_KV_STORE_H_ */
