// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * store/tapirstore/store.cc:
 *   Key-value store with support for transactions using TAPIR.
 *
 * Copyright 2015 Irene Zhang <iyzhang@cs.washington.edu>
 *                Naveen Kr. Sharma <naveenks@cs.washington.edu>
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

#include "tapir/store/tapirstore/store.h"

namespace tapirstore {

using namespace std;

Store::Store(bool linearizable) : linearizable(linearizable), store() { }

Store::~Store() { }

int
Store::Get(uint64_t id, const string &key, pair<Timestamp,string> &value)
{
    Debug("[%lu] GET %s", id, key.c_str());

	VersionedValue val;
    bool ret = store.get(key, val);
    if (ret) {
        Debug("Value: %s at <%lu, %lu>", value.second.c_str(), value.first.getTimestamp(), value.first.getID());
		value.first = val.time;
		value.second = val.value;
        return REPLY_OK;
    } else {
        return REPLY_FAIL;
    }
}

int
Store::Get(uint64_t id, const string &key, const Timestamp &timestamp, pair<Timestamp,string> &value)
{
    Debug("[%lu] GET %s at <%lu, %lu>", id, key.c_str(), timestamp.getTimestamp(), timestamp.getID());

	VersionedValue val;
    bool ret = store.get(key, timestamp, val);
    if (ret) {
		value.first = val.time;
		value.second = val.value;
        return REPLY_OK;
    } else {
        return REPLY_FAIL;
    }
}

int
Store::Prepare(uint64_t id, const Transaction &txn, const Timestamp &timestamp, Timestamp &proposedTimestamp)
{   
    Debug("[%lu] START PREPARE", id);

    if (prepared.find(id) != prepared.end()) {
        if (prepared[id].first == timestamp) {
            Warning("[%lu] Already Prepared!", id);
            return REPLY_OK;
        } else {
            // run the checks again for a new timestamp
            prepared.erase(id);
        }
    }

    // do OCC checks
    unordered_map<string, set<Timestamp>> pWrites;
    GetPreparedWrites(pWrites);
    unordered_map<string, set<Timestamp>> pReads;
    GetPreparedReads(pReads);
	unordered_map<string, set<Timestamp>> pIncs;
	GetPreparedIncrements(pIncs);

    // check for conflicts with the read set
    for (auto &read : txn.getReadSet()) {
        pair<Timestamp, Timestamp> range;
        bool ret = store.getRange(read.first, read.second, range);

        // if we don't have this key then no conflicts for read
        if (!ret) continue;

        // if we don't have this version then no conflicts for read
        if (range.first != read.second) continue;

        // if the value is still valid
        if (!range.second.isValid()) {
            // check pending writes.
            if ( pWrites.find(read.first) != pWrites.end() && 
                 (linearizable || 
                  pWrites[read.first].upper_bound(timestamp) != pWrites[read.first].begin()) ) {
                Debug("[%lu] ABSTAIN rw conflict w/ prepared key:%s",
                      id, read.first.c_str());
                return REPLY_ABSTAIN;
            }

			// check pending increments.
			if ( pIncs.find(read.first) != pIncs.end() &&
			     (linearizable ||
				  pIncs[read.first].upper_bound(timestamp) != pIncs[read.first].begin() )) {
			    Debug("[%lu] ABSTAIN ri conflict w/ prepared key:%s",
				     id, read.first.c_str());
				return REPLY_ABSTAIN;
			}


        } else if (linearizable || timestamp > range.second) {
            /* if value is not still valid, if we are running linearizable, then abort.
             *  Else check validity range. if
             * proposed timestamp not within validity range, then
             * conflict and abort
             */
            ASSERT(timestamp > range.first);
            Debug("[%lu] ABORT rw conflict key:%s",
                  id, read.first.c_str());
            return REPLY_FAIL;
        } else {
            /* there may be a pending write in the past.  check
             * pending writes again.  If proposed transaction is
             * earlier, abstain
             */
            if (pWrites.find(read.first) != pWrites.end()) {
                for (auto &writeTime : pWrites[read.first]) {
                    if (writeTime > range.first && 
                        writeTime < timestamp) {
                        Debug("[%lu] ABSTAIN rw conflict w/ prepared key:%s",
                              id, read.first.c_str());
                        return REPLY_ABSTAIN;
                    }
                }
            }
			if (pIncs.find(read.first) != pIncs.end()) {
                for (auto &incTime : pIncs[read.first]) {
                    if (incTime > range.first && 
                        incTime < timestamp) {
                        Debug("[%lu] ABSTAIN ri conflict w/ prepared key:%s",
                              id, read.first.c_str());
                        return REPLY_ABSTAIN;
                    }
                }
            }

        }
    }

    // check for conflicts with the write set
    for (auto &write : txn.getWriteSet()) {
        VersionedValue val;
        // if this key is in the store
        if ( store.get(write.first, val) ) {
            Timestamp lastRead;
            bool ret;

            // if the last committed write/inc is bigger than the timestamp,
            // then can't accept in linearizable
            if ( linearizable && val.time > timestamp ) {
                Debug("[%lu] RETRY ww conflict w/ prepared key:%s", 
                      id, write.first.c_str());
                proposedTimestamp = val.time;
                return REPLY_RETRY;	                    
            }

            // if last committed read is bigger than the timestamp, can't
            // accept this transaction, but can propose a retry timestamp

            // if linearizable mode, then we get the timestamp of the last
            // read ever on this object
            if (linearizable) {
                ret = store.getLastRead(write.first, lastRead);
            } else {
                // otherwise, we get the last read for the version that is being written
                ret = store.getLastRead(write.first, timestamp, lastRead);
            }

            // if this key is in the store and has been read before
            if (ret && lastRead > timestamp) {
                Debug("[%lu] RETRY wr conflict w/ prepared key:%s", 
                      id, write.first.c_str());
                proposedTimestamp = lastRead;
                return REPLY_RETRY; 
            }
        }


        // if there is a pending write for this key, greater than the
        // proposed timestamp, retry
        if ( linearizable &&
             pWrites.find(write.first) != pWrites.end()) {
            set<Timestamp>::iterator it = pWrites[write.first].upper_bound(timestamp);
            if ( it != pWrites[write.first].end() ) {
                Debug("[%lu] RETRY ww conflict w/ prepared key:%s",
                      id, write.first.c_str());
                proposedTimestamp = *it;
                return REPLY_RETRY;
            }
        }
		if ( linearizable &&
             pIncs.find(write.first) != pIncs.end()) {
            set<Timestamp>::iterator it = pIncs[write.first].upper_bound(timestamp);
            if ( it != pIncs[write.first].end() ) {
                Debug("[%lu] RETRY wi conflict w/ prepared key:%s",
                      id, write.first.c_str());
                proposedTimestamp = *it;
                return REPLY_RETRY;
            }
        }


        //if there is a pending read for this key, greater than the
        //propsed timestamp, abstain
        if ( pReads.find(write.first) != pReads.end() &&
             pReads[write.first].upper_bound(timestamp) != pReads[write.first].end() ) {
            Debug("[%lu] ABSTAIN wr conflict w/ prepared key:%s", 
                  id, write.first.c_str());
            return REPLY_ABSTAIN;
        }
    }

    // check for conflicts with the increment set
    for (auto &inc : txn.getIncrementSet()) {
        
		set<VersionedValue>::iterator it;
		store.getValue(inc.first, timestamp, it);
		// if there exists a committed write of distince increment op
		// of bigger timestamp, then can't accept in linearizable
		if (linearizable) {
			Timestamp suggest;
			for( ; it != store.store[inc.first].end(); it++) {
				for(auto i : inc.second) {
					if((*it).op != i.op) {
						suggest = (*it).time;
					}
				}
			}
			if(suggest.isValid()) {
				Debug("[%lu] RETRY iw conflict w/ prepared key:%s", 
						id, inc.first.c_str());
				proposedTimestamp = suggest;
				return REPLY_RETRY;
			}
		}

		// if last committed read is bigger than the timestamp, can't
		// accept this transaction, but can propose a retry timestamp

		// if linearizable mode, then we get the timestamp of the last
		// read ever on this object
		Timestamp lastRead;
		bool ret;

		if (linearizable) {
			ret = store.getLastRead(inc.first, lastRead);
		} else {
			// otherwise, we get the last read for the version that is being written
			ret = store.getLastRead(inc.first, timestamp, lastRead);
		}

		// if this key is in the store and has been read before
		if (ret && lastRead > timestamp) {
			Debug("[%lu] RETRY ir conflict w/ prepared key:%s", 
					id, inc.first.c_str());
			proposedTimestamp = lastRead;
			return REPLY_RETRY; 
		}

		// if there is a pending write for this key, greater than the
        // proposed timestamp, retry
        if ( linearizable &&
             pWrites.find(inc.first) != pWrites.end()) {
            set<Timestamp>::iterator it = pWrites[inc.first].upper_bound(timestamp);
            if ( it != pWrites[inc.first].end() ) {
                Debug("[%lu] RETRY iw conflict w/ prepared key:%s",
                      id, inc.first.c_str());
                proposedTimestamp = *it;
                return REPLY_RETRY;
            }
        }


        // if there is a pending increment of distinct increment op 
		// for this key, greater than the proposed timestamp, retry
        if ( linearizable &&
             pIncs.find(inc.first) != pIncs.end()) {
            set<Timestamp>::iterator it = pIncs[inc.first].upper_bound(timestamp);
			Timestamp suggest;
			for( ; it != pIncs[inc.first].end() ; it++) {
				//find increment op of operation at iterator
				vector<Increment> incList;
				for(auto p : prepared) {
					if(p.second.first == *it) {
						auto temp = p.second.second.getIncrementSet();
						incList = temp[inc.first];
						break;
					}
				}
				for(auto pinc : incList) {
					for(auto i : inc.second) {
						if(pinc.op != i.op) {
							suggest = *it;
						}
					}
				}
			}
			if(suggest.isValid()) {
				Debug("[%lu] RETRY ww conflict w/ prepared key:%s",
                   	  id, inc.first.c_str());
				proposedTimestamp = suggest;
	            return REPLY_RETRY;
			}

        }


        //if there is a pending read for this key, greater than the
        //propsed timestamp, abstain
        if ( pReads.find(inc.first) != pReads.end() &&
             pReads[inc.first].upper_bound(timestamp) != pReads[inc.first].end() ) {
            Debug("[%lu] ABSTAIN wr conflict w/ prepared key:%s", 
                  id, inc.first.c_str());
            return REPLY_ABSTAIN;
        }
    }

    // Otherwise, prepare this transaction for commit
    prepared[id] = make_pair(timestamp, txn);
    Debug("[%lu] PREPARED TO COMMIT", id);

    return REPLY_OK;
}
    
void
Store::Commit(uint64_t id, uint64_t timestamp)
{

    Debug("[%lu] COMMIT", id);
    
    // Nope. might not find it
    //ASSERT(prepared.find(id) != prepared.end());

    pair<Timestamp, Transaction> p = prepared[id];

    Commit(p.first, p.second);

    prepared.erase(id);
}

void
Store::Commit(const Timestamp &timestamp, const Transaction &txn)
{
    // updated timestamp of last committed read for the read set
    for (auto &read : txn.getReadSet()) {
        store.commitGet(read.first, // key
                        read.second, // timestamp of read version
                        timestamp); // commit timestamp
    }

    // insert writes into versioned key-value store
    for (auto &write : txn.getWriteSet()) {
        store.put(write.first, // key
                  write.second, // value
                  timestamp); // timestamp
    }

	// perform all increments on the key-value store
	for (auto &incList : txn.getIncrementSet()) {
		for (auto inc : incList.second) {
			store.increment(incList.first,
							inc,
							timestamp);
		}
	}
}

void
Store::Abort(uint64_t id, const Transaction &txn)
{
    Debug("[%lu] ABORT", id);
    
    if (prepared.find(id) != prepared.end()) {
        prepared.erase(id);
    }
}

void
Store::Load(const string &key, const string &value, const Timestamp &timestamp)
{
    store.put(key, value, timestamp);
}

void
Store::GetPreparedWrites(unordered_map<string, set<Timestamp>> &writes)
{
    // gather up the set of all writes that are currently prepared
    for (auto &t : prepared) {
        for (auto &write : t.second.second.getWriteSet()) {
            writes[write.first].insert(t.second.first);
        }
    }
}

void
Store::GetPreparedReads(unordered_map<string, set<Timestamp>> &reads)
{
    // gather up the set of all writes that are currently prepared
    for (auto &t : prepared) {
        for (auto &read : t.second.second.getReadSet()) {
            reads[read.first].insert(t.second.first);
        }
    }
}

void
Store::GetPreparedIncrements(unordered_map<string, set<Timestamp>> &incs)
{
	// gather up the set of all increments that are currently prepared
	for (auto &t : prepared) {
		for (auto &incList : t.second.second.getIncrementSet()) {
			incs[incList.first].insert(t.second.first);
		}
	}
}

} // namespace tapirstore
