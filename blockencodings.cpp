// Copyright (c) 2016-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockencodings.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <chainparams.h>
#include <crypto/sha256.h>
#include <crypto/siphash.h>
#include <random.h>
#include <streams.h>
#include <txmempool.h>
#include <validation.h>
#include <util/system.h>
#include <net_processing.h>

#include <unordered_map>
#include <algorithm>


CBlockHeaderAndShortTxIDs::CBlockHeaderAndShortTxIDs(const CBlock& block, bool fUseWTXID, CTxMemPool *m_mempool) :
        nonce(GetRand(std::numeric_limits<uint64_t>::max())),
        shorttxids(block.vtx.size() - 1), prefilledtxn(1), header(block) {
    FillShortTxIDSelector();
    //TODO: Use our mempool prior to block acceptance to predictively fill more than just the coinbase
    size_t pfsize = 30;		//set up
    LogPrint(BCLog::NET, "KAR's Log Start\n");
    if (block.vtx.size() < pfsize){	// if the block.vtx.size is shorter than pfsize, resize prefileldtxn size and pfsize to block.vtx.size
	    prefilledtxn.resize(block.vtx.size());
    	    pfsize = block.vtx.size();
    }
    else
	    prefilledtxn.resize(pfsize);

    if (block.vtx.size() == 1)		 // the block include one transaction, that coinbase
    {
	prefilledtxn[0] = {0, block.vtx[0]};
    	for (size_t i = 1; i < block.vtx.size(); i++) {
        	const CTransaction& tx = *block.vtx[i];
        	shorttxids[i - 1] = GetShortID(fUseWTXID ? tx.GetWitnessHash() : tx.GetHash());
    	}
    }
    else
    {

    	std::vector<GenTxid> gtx;
    	for (size_t i = 1; i < block.vtx.size(); i++)
   	{
    		const CTransaction& t = *block.vtx[i];
   		GenTxid gtxid{false, t.GetHash()};
  		gtx.push_back(gtxid);
	}	

    	LogPrint(BCLog::NET, "KAR's Log Load TxInfo %d\n", block.vtx.size());
    
    	CTxMemPool *mp = m_mempool;
   	std::vector<TxMempoolInfo> btxinfo = mp->prefilledinfo(gtx);
   
    	LogPrint(BCLog::NET, "KAR's Log Make indexVector\n");
        
        // size = unsigned int
	// fee = long long int
	// mempoolintime = int	
    	std::vector<std::pair<int,unsigned int>> indexAndInfo;

    	for (size_t i = 0; i < btxinfo.size(); i++)
    	{
		indexAndInfo.push_back(std::pair<int,unsigned int>(i+1, btxinfo[i].vsize));
    	}

   	/*
        LogPrint(BCLog::NET, "KAR's Log GTX prefilled0 %d : %u\n", indexAndInfo[0].first,  indexAndInfo[0].second);
    	LogPrint(BCLog::NET, "KAR's Log GTX prefilled1 %d : %u\n", indexAndInfo[1].first,  indexAndInfo[1].second);
    	LogPrint(BCLog::NET, "KAR's Log GTX prefilled2 %d : %u\n", indexAndInfo[2].first,  indexAndInfo[2].second);
	*/


   	LogPrint(BCLog::NET, "KAR's Log Sorting Descending\n");

    	sort(indexAndInfo.begin(), indexAndInfo.end(), compareSize);

	
    	LogPrint(BCLog::NET, "KAR's Log GTX prefilled0 %d : %u\n", indexAndInfo[0].first,  indexAndInfo[0].second);
    	LogPrint(BCLog::NET, "KAR's Log GTX prefilled1 %d : %u\n", indexAndInfo[1].first,  indexAndInfo[1].second);
    	LogPrint(BCLog::NET, "KAR's Log GTX prefilled2 %d : %u\n", indexAndInfo[2].first,  indexAndInfo[2].second);
  
    	const CTransaction& t1 = *block.vtx[indexAndInfo[0].first];
    	const CTransaction& t2 = *block.vtx[indexAndInfo[1].first];
    	const CTransaction& t3 = *block.vtx[indexAndInfo[2].first];

    	LogPrint(BCLog::NET, "KAR's Log GTX prefilled0 hash %s\n", t1.GetHash().ToString());
    	LogPrint(BCLog::NET, "KAR's Log GTX prefilled1 hash %s\n", t2.GetHash().ToString());
    	LogPrint(BCLog::NET, "KAR's Log GTX prefilled2 hash %s\n", t3.GetHash().ToString());
	

    	LogPrint(BCLog::NET, "KAR's Log PrefilledTxn\n");

    	std::vector<int> prefilledindex;
    	std::vector<int>::iterator it;

    	prefilledtxn[0] = {0, block.vtx[0]};	 // input coinbase

    	for (size_t i = 1; i < pfsize; i++)
    	{
		uint16_t index = indexAndInfo[i-1].first;
            	prefilledtxn[i] = {index, block.vtx[index]};
            	prefilledindex.push_back(index);
    	}

    	LogPrint(BCLog::NET, "KAR's Log Sorting prefilledtxn\n");
	 // index list and prefilledtxn list sorting
    	sort(prefilledtxn.begin(), prefilledtxn.end(), compareIndex);
	sort(prefilledindex.begin(), prefilledindex.end());

	LogPrint(BCLog::NET, "KAR's Log Change index\n");
	
	prefilledtxn[1].index = uint16_t(prefilledindex[0] - 1);
	for (size_t i = 2; i < pfsize; i++)
	{
		//LogPrint(BCLog::NET, "KAR's Log origin pf index = %d %d\n", prefilledtxn[i].index, prefilledindex[i-1]);
		prefilledtxn[i].index = uint16_t(prefilledindex[i-1] - prefilledindex[i-2] - 1);
		//LogPrint(BCLog::NET, "KAR's Log change pf index = %d\n", prefilledtxn[i].index);
	}

    	LogPrint(BCLog::NET, "KAR's Log Make Shortids\n");
	 // except transactions that already input prefilledtxn and other transactions input in the shorttxids
    	size_t stindex = 0;
    	for (size_t i = 1; i < block.vtx.size(); i++) {
        	it = std::find(prefilledindex.begin(), prefilledindex.end(), i);
        	if (it == prefilledindex.end())
        	{
         		const CTransaction& tx = *block.vtx[i];
          		shorttxids[stindex++] = GetShortID(fUseWTXID ? tx.GetWitnessHash() : tx.GetHash());
        	}
    	}
    	shorttxids.resize(shorttxids.size() - (pfsize - 1));
    	LogPrint(BCLog::NET, "KAR's Log Shortids size %d\n", shorttxids.size());
    }
}


bool compareIndex(PrefilledTransaction a, PrefilledTransaction b)
{
	return a.index < b.index;
}

bool compareFee(std::pair<int,long long int> a, std::pair<int,long long int> b)
{
	return a.second > b.second;
}

bool compareSize(std::pair<int, unsigned int> a, std::pair<int, unsigned int> b)
{
	return a.second > b.second;
}

bool compareTime(std::pair<int, int> a, std::pair<int, int> b)
{
	return a.second > b.second; 
}

void CBlockHeaderAndShortTxIDs::FillShortTxIDSelector() const {
    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    stream << header << nonce;
    CSHA256 hasher;
    hasher.Write((unsigned char*)&(*stream.begin()), stream.end() - stream.begin());
    uint256 shorttxidhash;
    hasher.Finalize(shorttxidhash.begin());
    shorttxidk0 = shorttxidhash.GetUint64(0);
    shorttxidk1 = shorttxidhash.GetUint64(1);
}

uint64_t CBlockHeaderAndShortTxIDs::GetShortID(const uint256& txhash) const {
    static_assert(SHORTTXIDS_LENGTH == 6, "shorttxids calculation assumes 6-byte shorttxids");
    return SipHashUint256(shorttxidk0, shorttxidk1, txhash) & 0xffffffffffffL;
}



ReadStatus PartiallyDownloadedBlock::InitData(const CBlockHeaderAndShortTxIDs& cmpctblock, const std::vector<std::pair<uint256, CTransactionRef>>& extra_txn) {
    if (cmpctblock.header.IsNull() || (cmpctblock.shorttxids.empty() && cmpctblock.prefilledtxn.empty()))
        return READ_STATUS_INVALID;
    if (cmpctblock.shorttxids.size() + cmpctblock.prefilledtxn.size() > MAX_BLOCK_WEIGHT / MIN_SERIALIZABLE_TRANSACTION_WEIGHT)
        return READ_STATUS_INVALID;

    assert(header.IsNull() && txn_available.empty());
    header = cmpctblock.header;
    txn_available.resize(cmpctblock.BlockTxCount());

    int32_t lastprefilledindex = -1;
    for (size_t i = 0; i < cmpctblock.prefilledtxn.size(); i++) {
        if (cmpctblock.prefilledtxn[i].tx->IsNull())
            return READ_STATUS_INVALID;

        lastprefilledindex += cmpctblock.prefilledtxn[i].index + 1; //index is a uint16_t, so can't overflow here
        if (lastprefilledindex > std::numeric_limits<uint16_t>::max())
            return READ_STATUS_INVALID;
        if ((uint32_t)lastprefilledindex > cmpctblock.shorttxids.size() + i) {
            // If we are inserting a tx at an index greater than our full list of shorttxids
            // plus the number of prefilled txn we've inserted, then we have txn for which we
            // have neither a prefilled txn or a shorttxid!
            return READ_STATUS_INVALID;
        }
        txn_available[lastprefilledindex] = cmpctblock.prefilledtxn[i].tx;
    }
    prefilled_count = cmpctblock.prefilledtxn.size();

    // Calculate map of txids -> positions and check mempool to see what we have (or don't)
    // Because well-formed cmpctblock messages will have a (relatively) uniform distribution
    // of short IDs, any highly-uneven distribution of elements can be safely treated as a
    // READ_STATUS_FAILED.
    std::unordered_map<uint64_t, uint16_t> shorttxids(cmpctblock.shorttxids.size());
    uint16_t index_offset = 0;
    for (size_t i = 0; i < cmpctblock.shorttxids.size(); i++) {
        while (txn_available[i + index_offset])
            index_offset++;
        shorttxids[cmpctblock.shorttxids[i]] = i + index_offset;
        // To determine the chance that the number of entries in a bucket exceeds N,
        // we use the fact that the number of elements in a single bucket is
        // binomially distributed (with n = the number of shorttxids S, and p =
        // 1 / the number of buckets), that in the worst case the number of buckets is
        // equal to S (due to std::unordered_map having a default load factor of 1.0),
        // and that the chance for any bucket to exceed N elements is at most
        // buckets * (the chance that any given bucket is above N elements).
        // Thus: P(max_elements_per_bucket > N) <= S * (1 - cdf(binomial(n=S,p=1/S), N)).
        // If we assume blocks of up to 16000, allowing 12 elements per bucket should
        // only fail once per ~1 million block transfers (per peer and connection).
        if (shorttxids.bucket_size(shorttxids.bucket(cmpctblock.shorttxids[i])) > 12)
            return READ_STATUS_FAILED;
    }
    // TODO: in the shortid-collision case, we should instead request both transactions
    // which collided. Falling back to full-block-request here is overkill.
    if (shorttxids.size() != cmpctblock.shorttxids.size())
        return READ_STATUS_FAILED; // Short ID collision

    std::vector<bool> have_txn(txn_available.size());
    {
    LOCK(pool->cs);
    for (size_t i = 0; i < pool->vTxHashes.size(); i++) {
        uint64_t shortid = cmpctblock.GetShortID(pool->vTxHashes[i].first);
        std::unordered_map<uint64_t, uint16_t>::iterator idit = shorttxids.find(shortid);
        if (idit != shorttxids.end()) {
            if (!have_txn[idit->second]) {
                txn_available[idit->second] = pool->vTxHashes[i].second->GetSharedTx();
                have_txn[idit->second]  = true;
                mempool_count++;
            } else {
                // If we find two mempool txn that match the short id, just request it.
                // This should be rare enough that the extra bandwidth doesn't matter,
                // but eating a round-trip due to FillBlock failure would be annoying
                if (txn_available[idit->second]) {
                    txn_available[idit->second].reset();
                    mempool_count--;
                }
            }
        }
        // Though ideally we'd continue scanning for the two-txn-match-shortid case,
        // the performance win of an early exit here is too good to pass up and worth
        // the extra risk.
        if (mempool_count == shorttxids.size())
            break;
    }
    }

    for (size_t i = 0; i < extra_txn.size(); i++) {
        uint64_t shortid = cmpctblock.GetShortID(extra_txn[i].first);
        std::unordered_map<uint64_t, uint16_t>::iterator idit = shorttxids.find(shortid);
        if (idit != shorttxids.end()) {
            if (!have_txn[idit->second]) {
                txn_available[idit->second] = extra_txn[i].second;
                have_txn[idit->second]  = true;
                mempool_count++;
                extra_count++;
            } else {
                // If we find two mempool/extra txn that match the short id, just
                // request it.
                // This should be rare enough that the extra bandwidth doesn't matter,
                // but eating a round-trip due to FillBlock failure would be annoying
                // Note that we don't want duplication between extra_txn and mempool to
                // trigger this case, so we compare witness hashes first
                if (txn_available[idit->second] &&
                        txn_available[idit->second]->GetWitnessHash() != extra_txn[i].second->GetWitnessHash()) {
                    txn_available[idit->second].reset();
                    mempool_count--;
                    extra_count--;
                }
            }
        }
        // Though ideally we'd continue scanning for the two-txn-match-shortid case,
        // the performance win of an early exit here is too good to pass up and worth
        // the extra risk.
        if (mempool_count == shorttxids.size())
            break;
    }

    LogPrint(BCLog::CMPCTBLOCK, "Initialized PartiallyDownloadedBlock for block %s using a cmpctblock of size %lu\n", cmpctblock.header.GetHash().ToString(), GetSerializeSize(cmpctblock, PROTOCOL_VERSION));

    return READ_STATUS_OK;
}

bool PartiallyDownloadedBlock::IsTxAvailable(size_t index) const {
    assert(!header.IsNull());
    assert(index < txn_available.size());
    return txn_available[index] != nullptr;
}

ReadStatus PartiallyDownloadedBlock::FillBlock(CBlock& block, const std::vector<CTransactionRef>& vtx_missing) {
    assert(!header.IsNull());
    uint256 hash = header.GetHash();
    block = header;
    block.vtx.resize(txn_available.size());

    size_t tx_missing_offset = 0;
    for (size_t i = 0; i < txn_available.size(); i++) {
        if (!txn_available[i]) {
            if (vtx_missing.size() <= tx_missing_offset)
                return READ_STATUS_INVALID;
            block.vtx[i] = vtx_missing[tx_missing_offset++];
        } else
            block.vtx[i] = std::move(txn_available[i]);
    }

    // Make sure we can't call FillBlock again.
    header.SetNull();
    txn_available.clear();

    if (vtx_missing.size() != tx_missing_offset)
        return READ_STATUS_INVALID;

    BlockValidationState state;
    if (!CheckBlock(block, state, Params().GetConsensus())) {
        // TODO: We really want to just check merkle tree manually here,
        // but that is expensive, and CheckBlock caches a block's
        // "checked-status" (in the CBlock?). CBlock should be able to
        // check its own merkle root and cache that check.
        if (state.GetResult() == BlockValidationResult::BLOCK_MUTATED)
            return READ_STATUS_FAILED; // Possible Short ID collision
        return READ_STATUS_CHECKBLOCK_FAILED;
    }

    LogPrint(BCLog::CMPCTBLOCK, "Successfully reconstructed block %s with %lu txn prefilled, %lu txn from mempool (incl at least %lu from extra pool) and %lu txn requested\n", hash.ToString(), prefilled_count, mempool_count, extra_count, vtx_missing.size());
    if (vtx_missing.size() < 5) {
        for (const auto& tx : vtx_missing) {
            LogPrint(BCLog::CMPCTBLOCK, "Reconstructed block %s required tx %s\n", hash.ToString(), tx->GetHash().ToString());
        }
    }

    return READ_STATUS_OK;
}
