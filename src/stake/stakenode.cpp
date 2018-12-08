#include "stake/stakenode.h"

UndoTicketDataVector StakeNode::UndoData() const
{
    return databaseUndoUpdate;
}

HashVector StakeNode::NewTickets() const
{
    return databaseBlockTickets;
}

HashVector StakeNode::SpentByBlock() const
{
    HashVector spent;
    for (const auto& it : databaseUndoUpdate){
        if (it.spent){
            spent.push_back(it.ticketHash);
        }
    }
    return spent;
}

HashVector StakeNode::MissedByBlock() const
{
    HashVector missed;
    for (const auto& it : databaseUndoUpdate){
        if (it.missed){
            missed.push_back(it.ticketHash);
        }
    }
    return missed;
}

HashVector StakeNode::ExpiredByBlock() const
{
    HashVector expired;
    for (const auto& it : databaseUndoUpdate) {
        if(it.expired && !it.revoked) {
            expired.push_back(it.ticketHash);
        }
    }
    return expired;
}

bool StakeNode::ExistsLiveTicket(const uint256& ticket) const
{
    return liveTickets->has(ticket);
}

HashVector StakeNode::LiveTickets() const
{
    HashVector tickets{static_cast<size_t>(liveTickets->len())};

    liveTickets->forEach(
        [&tickets](const uint256& key, const ValuePtr&) {
            tickets.push_back(key);
            return true;
        }
    );

    return tickets;
}

int StakeNode::PoolSize() const
{
    return liveTickets->len();
}

bool StakeNode::ExistsMissedTicket(const uint256& ticket) const
{
    return missedTickets->has(ticket);
}

HashVector StakeNode::MissedTickets() const
{
    HashVector tickets{static_cast<size_t>(missedTickets->len())};

    missedTickets->forEach(
        [&tickets](const uint256& key, const ValuePtr&) {
            tickets.push_back(key);
            return true;
        }
    );

    return tickets;
}

bool StakeNode::ExistsRevokedTicket(const uint256& ticket) const
{
    return revokedTickets->has(ticket);
}

HashVector StakeNode::RevokedTickets() const
{
    HashVector tickets{static_cast<size_t>(revokedTickets->len())};

    revokedTickets->forEach(
        [&tickets](const uint256& key, const ValuePtr&) {
            tickets.push_back(key);
            return true;
        }
    );

    return tickets;
}

bool StakeNode::ExistsExpiredTicket(const uint256& ticket) const
{
    auto v = missedTickets->get(ticket);
    if (v != nullptr && v->expired) {
        return true;
    }
    v = revokedTickets->get(ticket);
    if (v != nullptr && v->expired) {
        return true;
    }
    return false;
}

HashVector StakeNode::Winners() const
{
    return nextWinners;
}

StakeState StakeNode::FinalState() const
{
    return finalState;
}

uint32_t StakeNode::Height() const
{
    return height;
}