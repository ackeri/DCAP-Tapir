// -*- mode: c++; c-file-style: "k&r"; c-basic-offset: 4 -*-
/***********************************************************************
 *
 * timeserver/timeserver.h:
 *   Timeserver API
 *
 **********************************************************************/

#ifndef _TIME_SERVER_H_
#define _TIME_SERVER_H_

#include "tapir/lib/configuration.h"
#include "tapir/replication/common/replica.h"
#include "tapir/lib/udptransport.h"
#include "tapir/replication/vr/replica.h"

#include <string>

using namespace std;

class TimeStampServer : public replication::AppReplica
{
public:
    TimeStampServer();
    ~TimeStampServer();

    void ReplicaUpcall(opnum_t opnum, const string &str1, string &str2);

private:
    long ts;
    string newTimeStamp();
};

#endif /* _TIME_SERVER_H_ */
