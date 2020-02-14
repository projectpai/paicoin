#include "stake/stakenode.h"
#include "stake/hash256prng.h"
#include "hash.h"

std::string StakeStateToString(const StakeState& stakeState)
{
    std::string str;
    for (auto c : stakeState)
        str += std::to_string((int)c) + " ";
    return str;
}

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
    HashVector tickets{};

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
    HashVector tickets{};

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
    HashVector tickets{};

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

std::unique_ptr<StakeNode> StakeNode::genesisNode(const Consensus::Params& params)
{
    return std::unique_ptr<StakeNode>(new StakeNode(params));
}

std::shared_ptr<StakeNode> StakeNode::ConnectNode(const uint256& lotteryIV, const HashVector& ticketsVoted, const HashVector& revokedTickets, const HashVector& newTickets) const
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
        for (const auto& it : nextWinners) {
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
                connectedNode->liveTickets = std::make_shared<TicketTreap>(connectedNode->liveTickets->deleteKey(it));
            }
            else{
                value->spent = false;
                value->missed = true;
                connectedNode->liveTickets = std::make_shared<TicketTreap>(connectedNode->liveTickets->deleteKey(it));
                connectedNode->missedTickets = std::make_shared<TicketTreap>(connectedNode->missedTickets->put(it,value));
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
                connectedNode->liveTickets = std::make_shared<TicketTreap>(connectedNode->liveTickets->deleteKey(treapKey));
                connectedNode->missedTickets = std::make_shared<TicketTreap>(connectedNode->missedTickets->put(treapKey, v));

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
            connectedNode->missedTickets = std::make_shared<TicketTreap>(connectedNode->missedTickets->deleteKey(it));
            connectedNode->revokedTickets = std::make_shared<TicketTreap>(connectedNode->revokedTickets->put(it,value));

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
        connectedNode->liveTickets = std::make_shared<TicketTreap>(connectedNode->liveTickets->put(k,v));

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
        auto prng = Hash256PRNG(lotteryIV);
        const auto& idxs = prng.FindTicketIdxs(connectedNode->liveTickets->len(), connectedNode->params.nTicketsPerBlock);

        const auto& nextWinnerKeys = connectedNode->liveTickets->fetchWinners(idxs);

        auto stateBuffer = HashVector{};
        for(const auto& it : nextWinnerKeys){
            connectedNode->nextWinners.push_back(it);
            stateBuffer.push_back(it);
        }
        stateBuffer.push_back(prng.StateHash());
        const auto& hex = Hash(stateBuffer.begin(),stateBuffer.end()).GetHex();
        connectedNode->finalState.SetHex(hex);
    }

    return connectedNode;
}

std::shared_ptr<StakeNode> StakeNode::DisconnectNode(const uint256& parentLotteryIV, const UndoTicketDataVector& parentUtds, const HashVector& parentTickets) const
{
    // Edge case for the parent being the genesis block.
    if (this->height == 1) {
        return genesisNode(this->params);
    }

	// The undo ticket slice is normally stored in memory for the most
	// recent blocks and the sidechain, but it may be the case that it
	// is missing because it's in the mainchain and very old (thus
	// outside the node cache).  In this case, restore this data from
	// disk.
	// if parentUtds == nil || parentTickets == nil {
	// 	if dbTx == nil {
	// 		return nil, stakeRuleError(ErrMissingDatabaseTx, "needed to "+
	// 			"look up undo data in the database, but no dbtx passed")
	// 	}

	// 	var err error
	// 	parentUtds, err = ticketdb.DbFetchBlockUndoData(dbTx, node.height-1)
	// 	if err != nil {
	// 		return nil, err
	// 	}

	// 	parentTickets, err = ticketdb.DbFetchNewTickets(dbTx, node.height-1)
	// 	if err != nil {
	// 		return nil, err
	// 	}
	// }

    const auto restoredNode =  std::make_shared<StakeNode>(
        this->height - 1,
        this->liveTickets,// TODO now it is a pointer to liveTickets, it needs not mutate that
        this->missedTickets,
        this->revokedTickets,
        parentUtds,
        parentTickets,
        HashVector{},
        this->params);

    // Iterate through the block undo data and write all database
    // changes to the respective treap, reversing all the changes
    // added when the child block was added to the chain.
    auto stateBuffer = HashVector{};
    for (const auto& it : this->databaseUndoUpdate) {
        const auto& k = it.ticketHash;
        auto v = std::make_shared<Value>(
            it.ticketHeight,
            it.missed,
            it.revoked,
            it.spent,
            it.expired
        );


        // All flags are unset; this is a newly added ticket.
        // Remove it from the list of live tickets.
        if (!it.missed && !it.revoked && !it.spent) {
            // TODO check every deleteKey/put usage
            restoredNode->liveTickets = std::make_shared<TicketTreap>(restoredNode->liveTickets->deleteKey(k));
        }

        // The ticket was missed and revoked. It needs to
        // be moved from the revoked ticket treap to the
        // missed ticket treap.
        else if ( it.missed && it.revoked) {
            v->revoked = false;
            restoredNode->revokedTickets = std::make_shared<TicketTreap>(restoredNode->revokedTickets->deleteKey(k));
            restoredNode->missedTickets = std::make_shared<TicketTreap>(restoredNode->missedTickets->put(k,v));
        }

        // The ticket was missed and was previously live.
        // Remove it from the missed tickets bucket and
        // move it to the live tickets bucket.
        else if ( it.missed && !it.revoked) {
            // Expired tickets could never have been
            // winners.
            if (!it.expired) {
                restoredNode->nextWinners.push_back(it.ticketHash);
                stateBuffer.push_back(it.ticketHash);
            } else {
                v->expired = false;
            }

            v->missed = false;
            restoredNode->missedTickets = std::make_shared<TicketTreap>(restoredNode->missedTickets->deleteKey(k));
            restoredNode->liveTickets = std::make_shared<TicketTreap>(restoredNode->liveTickets->put(k,v));
        }

        // The ticket was spent. Reinsert it into the live
        // tickets treap and add it to the list of next
        // winners.
        else if (it.spent) {
            v->spent = false;
            restoredNode->nextWinners.push_back(it.ticketHash);
            stateBuffer.push_back(it.ticketHash);
            restoredNode->liveTickets = std::make_shared<TicketTreap>(restoredNode->liveTickets->put(k,v));
        }

        else {
            assert(!"unknown ticket state in undo data");
        }
    }

    if (this->height >= this->params.nStakeValidationHeight) {
        auto prng = Hash256PRNG(parentLotteryIV);
        
        const auto& idxs = prng.FindTicketIdxs(restoredNode->liveTickets->len(), restoredNode->params.nTicketsPerBlock);
        stateBuffer.push_back(prng.StateHash());
        const auto& hex = Hash(stateBuffer.begin(),stateBuffer.end()).GetHex();
        restoredNode->finalState.SetHex(hex);
    }

    return restoredNode;
}