#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <limits.h>
#include <set>
#include <string>

#include "tapir/store/common/timestamp.h"
#include "tapir/lib/tcptransport.h"

#define NO_NOTIFICATION ULLONG_MAX

class ReactiveTransaction {
 public:
    ReactiveTransaction(const uint64_t frontend_index,
                        const uint64_t reactive_id,
                        const uint64_t client_id,
                        const std::set<std::string> &keys,
                        const TransportAddress *client) :
        frontend_index(frontend_index),
        reactive_id(reactive_id),
        client_id(client_id),
        keys(keys)
        { this->client = client->clone(); };
    ~ReactiveTransaction() { delete client; };
   
    const uint64_t frontend_index; // ((client_id << 32) | reactive_id)
    const uint64_t reactive_id;
    const uint64_t client_id;
    Timestamp next_timestamp;
    Timestamp last_timestamp;
    std::set<std::string> keys;
    TransportAddress *client;
};

#endif //NOTIFICATION_H
