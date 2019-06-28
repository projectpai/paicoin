#include "stake/stakenode.h"
#include "stake/hash256prng.h"
#include "hash.h"

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

std::string StakeNode::FinalStateToString() const
{
    std::string str;

    for (auto c : finalState)
        str += std::to_string((int)c);

    return str;
}

uint32_t StakeNode::Height() const
{
    return height;
}

std::unique_ptr<StakeNode> StakeNode::genesisNode(const Consensus::Params& params)
{
    return std::unique_ptr<StakeNode>(new StakeNode(params));
}

std::shared_ptr<StakeNode> StakeNode::ConnectNode(const HashVector& ticketsVoted, const HashVector& revokedTickets, const HashVector& newTickets) const
{
    const auto connectedNode =  std::make_shared<StakeNode>(
        this->height + 1,
        this->liveTickets,// TODO now it is a pointer to liveTickets, it needs not mutate that
        this->missedTickets,
        this->revokedTickets,
        UndoTicketDataVector{},
        newTickets,
        HashVector{},
        this->params);

    // We only have to deal with vote-related issues and expiry after
    // StakeEnabledHeight.
    if (connectedNode->height >= connectedNode->params.nStakeEnabledHeight) {
        // Basic sanity check.
        for (const auto& it : ticketsVoted) {
            if (end(nextWinners) == std::find(begin(nextWinners),end(nextWinners),it))
                assert("unknown ticket spent in block");
        }

        // Iterate through all possible winners and construct the undo data,
        // updating the live and missed ticket treaps as necessary.  We need
        // to copy the value here so we don't modify it in the previous treap.
        for (const auto& it : ticketsVoted) {
            auto value = connectedNode->liveTickets->get(it);
            assert(value != nullptr);

            // If it's spent in this block, mark it as being spent.  Otherwise,
            // it was missed.  Spent tickets are dropped from the live ticket
            // bucket, while missed tickets are pushed to the missed ticket
            // bucket.  Because we already know from the above check that the
            // ticket should still be in the live tickets treap, we probably
            // do not have to use the safe delete functions, but do so anyway
            // just to be safe.
            if (end(ticketsVoted) != std::find(begin(ticketsVoted),end(ticketsVoted),it)) {
                value->spent = true;
                value->missed = false;
                connectedNode->liveTickets->deleteKey(it);
            }
            else{
                value->spent = false;
                value->missed = true;
                connectedNode->liveTickets->deleteKey(it);
                connectedNode->missedTickets->put(it,value);
            }

            connectedNode->databaseUndoUpdate.push_back(
                UndoTicketData{it,value->height, value->missed, value->revoked, value->spent, value->expired});
        }

        // Find the expiring tickets and drop them as well.  We already know what
        // the winners are from the cached information in the previous block, so
        // no drop the results of that here.
        auto toExpireHeight = uint32_t{0};
        if (connectedNode->height > connectedNode->params.nTicketExpiry) {
            toExpireHeight = connectedNode->height - connectedNode->params.nTicketExpiry;
        }

        connectedNode->liveTickets->forEachByHeight(toExpireHeight + 1, [&connectedNode](const uint256& treapKey, const ValuePtr& value) {
                // Make a copy of the value.
                auto v = std::make_shared<Value>(*value);
                v->missed = true;
                v->expired = true;
                connectedNode->liveTickets->deleteKey(treapKey);
                connectedNode->missedTickets->put(treapKey, v);

                connectedNode->databaseUndoUpdate.push_back(UndoTicketData{
                    treapKey,
                    v->height,
                    v->missed,
                    v->revoked,
                    v->spent,
                    v->expired
                });

                return true;
            }
        );

        // Process all the revocations, moving them from the missed to the
        // revoked treap and recording them in the undo data.
        for (const auto& it : revokedTickets) {
            auto value = connectedNode->missedTickets->get(it);

            value->revoked = true;
            connectedNode->missedTickets->deleteKey(it);
            connectedNode->revokedTickets->put(it,value);

            connectedNode->databaseUndoUpdate.push_back(UndoTicketData{
                it,
                value->height,
                value->missed,
                value->revoked,
                value->spent,
                value->expired
            });
        }
    }

    // Add all the new tickets.
    for (const auto& it : newTickets) {
        const auto& k = it;
        const auto& v = std::make_shared<Value>(
            connectedNode->height,
            false,
            false,
            false,
            false
        );
        connectedNode->liveTickets->put(k,v);

        connectedNode->databaseUndoUpdate.push_back(UndoTicketData{
            it,
            v->height,
            v->missed,
            v->revoked,
            v->spent,
            v->expired
        });
    }

    // The first block voted on is at StakeValidationHeight, so begin calculating
    // winners at the block before StakeValidationHeight.
    if (connectedNode->height >= connectedNode->params.nStakeValidationHeight - 1 ) {
        // Find the next set of winners.
        std::string strMsg = "Very deterministic message";
        uint256 lotteryIV = Hash(strMsg.begin(), strMsg.end()); //TODO pass this a a parameter
        auto prng = Hash256PRNG(lotteryIV);
        const auto& idxs = prng.FindTicketIdxs(connectedNode->liveTickets->len(), connectedNode->params.nTicketsPerBlock);

        const auto& nextWinnerKeys = connectedNode->liveTickets->fetchWinners(idxs);

        auto stateBuffer = HashVector{};
        for(const auto& it : nextWinnerKeys){
            connectedNode->nextWinners.push_back(it);
            stateBuffer.push_back(it);
        }
        stateBuffer.push_back(prng.StateHash());
        //TODO implement Hash48 or find another solution to obtain the final state
        const auto& hash = Hash160(stateBuffer.begin(),stateBuffer.end()); 
        connectedNode->finalState = uint48();
    }

    return connectedNode;
}

std::shared_ptr<StakeNode> StakeNode::DisconnectNode() const
{
    return nullptr;
}
