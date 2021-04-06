// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRANSACTIONRECORD_H
#define BITCOIN_QT_TRANSACTIONRECORD_H

#include "amount.h"
#include "script/script.h"
#include "uint256.h"

#include <QList>
#include <QString>

class CWallet;
class CWalletTx;

/** UI model for transaction status. The transaction status is the part of a transaction that will change over time.
 */
class TransactionStatus
{
public:
    TransactionStatus() : countsForBalance(false), sortKey(""),
                          matures_in(0), status(Unconfirmed), depth(0), open_for(0), cur_num_blocks(-1)
    {
    }

    enum Status {
        Confirmed, /**< Have 6 or more confirmations (normal tx) or fully mature (mined tx) **/
        /// Normal (sent/received) transactions
        OpenUntilDate,  /**< Transaction not yet final, waiting for date */
        OpenUntilBlock, /**< Transaction not yet final, waiting for block */
        Unconfirmed,    /**< Not yet mined into a block **/
        Confirming,     /**< Confirmed, but waiting for the recommended number of confirmations **/
        Conflicted,     /**< Conflicts with other transaction or mempool **/
        /// Generated (mined) transactions
        Immature,       /**< Mined but waiting for maturity */
        NotAccepted     /**< Mined but not accepted */
    };

    /// Transaction counts towards available balance
    bool countsForBalance;
    /// Sorting key based on status
    std::string sortKey;

    /** @name Generated (mined) transactions
       @{*/
    int matures_in;
    /**@}*/

    /** @name Reported status
       @{*/
    Status status;
    qint64 depth;
    qint64 open_for; /**< Timestamp if status==OpenUntilDate, otherwise number
                      of additional blocks that need to be mined before
                      finalization */
    /**@}*/

    /** Current number of blocks (to know whether cached status is still valid) */
    int cur_num_blocks;

    //** Know when to update transaction for ix locks **/
    int cur_num_ix_locks;
};

/** UI model for a transaction. A core transaction can be represented by multiple UI transactions if it has
    multiple outputs.
 */
class TransactionRecord
{
public:
    enum Type {
        Other,
        Generated,
        StakeMint,
        StakeZAGE,
        SendToAddress,
        SendToOther,
        RecvWithAddress,
        MNReward,
        RecvFromOther,
        SendToSelf,
        ZerocoinMint,
        ZerocoinSpend,
        RecvFromZerocoinSpend,
        ZerocoinSpend_Change_zAge,
        ZerocoinSpend_FromMe,
        StakeDelegated, // Received cold stake (owner)
        StakeHot, // Staked via a delegated P2CS.
        P2CSDelegation, // Non-spendable P2CS, staker side.
        P2CSDelegationSent, // Non-spendable P2CS delegated utxo. (coin-owner transferred ownership to external wallet)
        P2CSDelegationSentOwner, // Spendable P2CS delegated utxo. (coin-owner)
        P2CSUnlockOwner, // Coin-owner spent the delegated utxo
        P2CSUnlockStaker // Staker watching the owner spent the delegated utxo
    };

    /** Number of confirmation recommended for accepting a transaction */
    static const int RecommendedNumConfirmations = 6;

    TransactionRecord(unsigned int size) : hash(), time(0), type(Other), address(""), debit(0), credit(0), size(size), idx(0)
    {
    }

    TransactionRecord(uint256 hash, qint64 time, unsigned int size) : hash(hash), time(time), type(Other), address(""), debit(0),
                                                   credit(0), size(size), idx(0)
    {
    }

    TransactionRecord(uint256 hash, qint64 time, unsigned int size, Type type, const std::string& address, const CAmount& debit, const CAmount& credit) : hash(hash), time(time), type(type), address(address), debit(debit), credit(credit),
                                                                                                                                       size(size), idx(0)
    {
    }

    /** Decompose CWallet transaction to model transaction records.
     */
    static QList<TransactionRecord> decomposeTransaction(const CWallet* wallet, const CWalletTx& wtx);

    /// Helpers
    static bool decomposeCoinStake(const CWallet* wallet, const CWalletTx& wtx,
                                   const CAmount& nCredit, const CAmount& nDebit, bool fZSpendFromMe,
                                   QList<TransactionRecord>& parts);

    static bool decomposeZcSpendTx(const CWallet* wallet, const CWalletTx& wtx,
                                    const CAmount& nCredit, const CAmount& nDebit, bool fZSpendFromMe,
                                    QList<TransactionRecord>& parts);

    static bool decomposeP2CS(const CWallet* wallet, const CWalletTx& wtx,
                                    const CAmount& nCredit, const CAmount& nDebit,
                                    QList<TransactionRecord>& parts);

    static bool decomposeCreditTransaction(const CWallet* wallet, const CWalletTx& wtx,
                                    QList<TransactionRecord>& parts);

    static bool decomposeSendToSelfTransaction(const CWalletTx& wtx, const CAmount& nCredit,
                                    const CAmount& nDebit, bool involvesWatchAddress,
                                    QList<TransactionRecord>& parts);

    static bool decomposeDebitTransaction(const CWallet* wallet, const CWalletTx& wtx,
                                                      const CAmount& nDebit, bool involvesWatchAddress,
                                                      QList<TransactionRecord>& parts);

    static std::string getValueOrReturnEmpty(const std::map<std::string, std::string>& mapValue, const std::string& key);
    static bool ExtractAddress(const CScript& scriptPubKey, bool fColdStake, std::string& addressStr);
    static void loadHotOrColdStakeOrContract(const CWallet* wallet, const CWalletTx& wtx,
                                            TransactionRecord& record, bool isContract = false);
    static void loadUnlockColdStake(const CWallet* wallet, const CWalletTx& wtx, TransactionRecord& record);

    /** @name Immutable transaction attributes
      @{*/
    uint256 hash;
    qint64 time;
    Type type;
    std::string address;
    CAmount debit;
    CAmount credit;
    unsigned int size;
    /**@}*/

    /** Subtransaction index, for sort key */
    int idx;

    /** Status: can change with block chain update */
    TransactionStatus status;

    /** Whether the transaction was sent/received with a watch-only address */
    bool involvesWatchAddress;

    /** Return the unique identifier for this transaction (part) */
    QString getTxID() const;

    /** Return the output index of the subtransaction  */
    int getOutputIndex() const;

    /** Update status from core wallet tx.
     */
    void updateStatus(const CWalletTx& wtx);

    /** Return whether a status update is needed.
     */
    bool statusUpdateNeeded();

    /** Return transaction status
     */
    std::string statusToString();

    /** Return true if the tx is a coinstake
     */
    bool isCoinStake() const;

    /** Return true if the tx is a any cold staking type tx.
     */
    bool isAnyColdStakingType() const;

    /** Return true if the tx hash is null and/or if the size is 0
     */
    bool isNull() const;

};

#endif // BITCOIN_QT_TRANSACTIONRECORD_H
