#ifndef PAICOIN_STAKE_STAKENODE_H
#define PAICOIN_STAKE_STAKENODE_H

#include "stake/treap/tickettreap.h"

// UndoTicketData is the data for any ticket that has been spent, missed, or
// revoked at some new height.  It is used to roll back the database in the
// event of reorganizations or determining if a side chain block is valid.
// The last 3 are encoded as a single byte of flags.
// The flags describe a particular state for the ticket:
//  1. Missed is set, but revoked and spent are not (0000 0001). The ticket
//      was selected in the lottery at this block height but missed, or the
//      ticket became too old and was missed. The ticket is being moved to the
//      missed ticket bucket from the live ticket bucket.
//  2. Missed and revoked are set (0000 0011). The ticket was missed
//      previously at a block before this one and was revoked, and
//      as such is being moved to the revoked ticket bucket from the
//      missed ticket bucket.
//  3. Spent is set (0000 0100). The ticket has been spent and is removed
//      from the live ticket bucket.
//  4. No flags are set. The ticket was newly added to the live ticket
//      bucket this block as a maturing ticket.
struct UndoTicketData {
    uint256  ticketHash;
    uint32_t ticketHeight;
    bool missed;
    bool revoked;
    bool spent;
    bool expired;
};

typedef std::vector<UndoTicketData> UndoTicketDataVector;

// HashVector is a list of ticket hashes that will mature in TicketMaturity
// many blocks from the block in which they were included.
typedef std::vector<uint256> HashVector;

typedef std::array<char,6> StakeState;

// StakeNode is in-memory stake data for a node.  It contains a list of database
// updates to be written in the case that the block is inserted in the main
// chain database.  Because of its use of immutable treap data structures, it
// allows for a fast, efficient in-memory representation of the ticket database
// for each node.  It handles connection of and disconnection of new blocks
// simply.
//
// Like the immutable treap structures, stake nodes themselves are considered
// to be immutable.  The connection or disconnection of past or future nodes
// returns a pointer to a new stake node, which must be saved and used
// appropriately.
class StakeNode final
{
private:
    uint32_t                    height;
    TicketTreapPtr              liveTickets; 
    TicketTreapPtr              missedTickets; 
    TicketTreapPtr              revokedTickets; 
    UndoTicketDataVector        databaseUndoUpdate;
    HashVector                  databaseBlockTickets;
    HashVector                  nextWinners;
    StakeState                  finalState;
    // params *chaincfg.Params

public:
    // UndoData returns the stored UndoTicketDataSlice used to remove this node
    // and restore it to the parent state.
    UndoTicketDataVector UndoData() const;

    // NewTickets returns the stored UndoTicketDataSlice used to remove this node
    // and restore it to the parent state.
    HashVector NewTickets() const;

    // SpentByBlock returns the tickets that were spent in this block.
    HashVector SpentByBlock() const;

    // MissedByBlock returns the tickets that were missed in this block. This
    // includes expired tickets and winning tickets that were not spent by a vote.
    // Also note that when a miss is later revoked, that ticket hash will also
    // appear in the output of this function for the block with the revocation.
    HashVector MissedByBlock() const;
    
    // ExpiredByBlock returns the tickets that expired in this block. This is a
    // subset of the missed tickets returned by MissedByBlock. The output only
    // includes the initial expiration of the ticket, not when an expired ticket is
    // revoked. This is unlike MissedByBlock that includes the revocation as well.
    HashVector ExpiredByBlock() const;
    
    // ExistsLiveTicket returns whether or not a ticket exists in the live ticket
    // treap for this stake node.
    bool ExistsLiveTicket(const uint256& ticket) const;

    // LiveTickets returns the list of live tickets for this stake node.
    HashVector LiveTickets() const;

    // PoolSize returns the size of the live ticket pool.
    int PoolSize() const;

    // ExistsMissedTicket returns whether or not a ticket exists in the missed
    // ticket treap for this stake node.
    bool ExistsMissedTicket(const uint256& ticket) const;

    // MissedTickets returns the list of missed tickets for this stake node.
    HashVector MissedTickets() const;

    // ExistsRevokedTicket returns whether or not a ticket exists in the revoked
    // ticket treap for this stake node.
    bool ExistsRevokedTicket(const uint256& ticket) const;

    // RevokedTickets returns the list of revoked tickets for this stake node.
    HashVector RevokedTickets() const;

    // ExistsExpiredTicket returns whether or not a ticket was ever expired from
    // the perspective of this stake node.
    bool ExistsExpiredTicket(const uint256& ticket) const;

    // Winners returns the current list of winners for this stake node, which
    // can vote on this node.
    HashVector Winners() const;

    // FinalState returns the final state lottery checksum of the node.
    StakeState FinalState() const;

    // FinalStateToString returns the final state lottery checksum as a printable string.
    std::string FinalStateToString() const;

    // Height returns the height of the node.
    uint32_t Height() const;

    // ConnectNode connects a stake node to the node and returns a pointer
    // to the stake node of the child.
    // func (sn *Node) ConnectNode(lotteryIV chainhash.Hash, ticketsVoted, revokedTickets, newTickets []chainhash.Hash) (*Node, error) {

    // DisconnectNode disconnects a stake node from the node and returns a pointer
    // to the stake node of the parent.
    // func (sn *Node) DisconnectNode(parentLotteryIV chainhash.Hash, parentUtds UndoTicketDataSlice, parentTickets []chainhash.Hash, dbTx database.Tx) (*Node, error) {
};

#endif // PAICOIN_STAKE_STAKENODE_H
