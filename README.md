## Bitcoin-core-prefilledtxn
: ‘PREFILLEDTXN’ 은 압축 블록 패킷(헤더)의 필드 중 하나로 코인베이스 트랜잭션을 제공. 또한 수신 피어가 없을 것으로 추정하는 트랜잭션도 포함.
압축 블록을 개발한 개발자는 이같이 소개하고 있으나, 실제로 블록의 PREFILLEDTXN에는 코인베이스 트랜잭션, 단 하나의 거래만 있음. 즉 다른 거래가 포함되지 않고 필드의 목적이 충족되지 않음. 비트코인 코어의 코드에서도 PREFILLEDTXN은 코인베이스만 채우고 있음.   

블록 전파 지연이 발생하는 주요 원인은 압축 블록 전달 릴레이에서 블록을 조립하는 과정 중 부족한 트랜잭션을 요청하는 것, 즉 추가 메시지와 릴레이가 발생하면서임.   

또한, 트랜잭션을 요청하는 원인 중 하나는 트랜잭션의 수수료와 크기가 큰 경우임. 트랜잭션의 수수료와 크기가 크면 마이너가 트랜잭션을 블록에 포함할 확률이 높아지고, 이는 곧 트랜잭션의 전파가 되기도 전에 블록에 포함될 수도 있기 때문. 이 트랜잭션들은 원래 개발자가 의도한 대로 수신 피어가 없을 것으로 추정되는 트랜잭션임. 더불어 블록에 포함된 트랜잭션 중, 송신 피어의 메모리 풀에 유입된 후 얼마 되지 않은 트랜잭션도 의도와 같은 트랜잭션임. 메모리 풀에 들어온 시각이 최신이라면, 다른 노드에게 전파되지 않았을 수도 있기 때문.   

따라서 PREFILLEDTXN 필드를 사용하여 수수료와 크기가 크고 최근 메모리 풀에 진입한 트랜잭션(m-time)을 포함하여 수신 노드에 보내, 추가 릴레이 과정이 발생하지 않게 함으로써 블록 전파 지연을 줄임.

PREFIILEDTXN에 트랜잭션을 담을 수 있도록 코드가 수정됨. 수정된 코드는 블록에 담긴 트랜잭션을 수수료, 크기, m-time에 따라 내림차순으로 정렬하고, PREFILLEDTXN에 상위 n개의 트랜잭션을 포함하는 코드.


## How to Implementation
1. **blockencoding.cpp / Line 24**   
블록에 포함된 트랜잭션을 GenTxid형태로 바꾸고, vector로 선언하여 리스트를 만들어줌. 단! 코인베이스는 포함하지 않음
<pre>
<code>
std::vector<GenTxid> gtx;
    for (size_t i = 1; i < block.vtx.size(); i++){
        const CTransaction& t = *block.vtx[i];
        GenTxid gtxid{false, t.GetHash()};
        gtx.push_back(gtxid);
        LogPrint(BCLog::NET, "KAR's Log GTX %s\n", gtx[i].GetHash().ToString());
    }

    LogPrint(BCLog::NET, "KAR's Log GTX Size %d\n", gtx.size());
    LogPrint(BCLog::NET, "KAR's Log GTX VTX Size %d\n", block.vtx.size());
</code>
</pre>

2. **txmempool.h / Line 737**   
함수 구현전 헤더파일에 함수 선언
<pre>
<code>
std::vector<TxMempoolInfo> prefilledinfo(std::vector<GenTxid> gtx);
</code>
</pre>

3. **txmempool.cpp / Line 823**   
벡터 형태로 호출하면 해당하는 값의 info를 얻고 이를 벡터로 만들어서 반환
<pre>
<code>
std::vector<TxMempoolInfo> CTxMemPool::prefilledinfo(std::vector<GenTxid> gtx){
     LOCK(cs);
     std::vector<TxMempoolInfo> ret;
     ret.reserve(gtx.size());
     for (size_t i = 0; i < gtx.size(); i++){
             GenTxid gtxid = gtx[i];
             ret.push_back(info(gtxid));
     }
     return ret;
}
</code>
</pre>

4. **blockencodings.h / Line 107**   
Mempool 객체를 받아오기 위해서 함수 형태 수정, CBlockHeaderAndShortTxIDs 함수에 멤풀 매개변수 추가
<pre>
<code>
CBlockHeaderAndShortTxIDs(const CBlock& block, bool fUseWTXID, CTxMemPool *m_mempool);
</code>
</pre>

5. **blockencodings.h / Line 107**   
역시나 함수에 매개변수 추가 후, txmempool에 구현한 함수를 호출하고 로그를 찍어봄
<pre>
<code>
CTxMemPool *mp = m_mempool;
std::vector<TxMempoolInfo> btxinfo = mp->prefilledinfo(gtx);
LogPrint(BCLog::NET, "KAR's Log GTX prefilled0 %lld\n", btxinfo[0].fee);
LogPrint(BCLog::NET, "KAR's Log GTX prefilled1 %lld\n", btxinfo[1].fee);

prefilledtxn[0] = {0, block.vtx[0]};
for (size_t i = 1; i < block.vtx.size(); i++) {
     const CTransaction& tx = *block.vtx[i];
     shorttxids[i - 1] = GetShortID(fUseWTXID ? tx.GetWitnessHash() : tx.GetHash());
}
</code>
</pre>

6. **net_processing.cpp / Line 1308, 1652, 4255, 4265**   
CBlockHeaderAndShortTxIDs shortIDs를 호출할 때 mempool 매개변수 전달하는 부분 추가
<pre>
<code>
1308
std::shared_ptr<const CBlockHeaderAndShortTxIDs> pcmpctblock = std::make_shared<const CBlockHeaderAndShortTxIDs> (*pblock, true, &m_mempool);

1652
CBlockHeaderAndShortTxIDs cmpctblock(*pblock, fPeerWantsWitness, &mempool);

4255
CBlockHeaderAndShortTxIDs cmpctblock(*most_recent_block, state.fWantsCmpctWitness, &m_mempool);

4265
CBlockHeaderAndShortTxIDs cmpctblock(block, state.fWantsCmpctWitness, &m_mempool);
</code>
</pre>

7. **blockencodings.h / Line 18**   
정렬하는 코드를 위해 헤더 파일에 함수 선언
상단에 헤더 파일 포함 필요
<pre>
<code>
 #include < txmempool.h >
 
bool compareFee(std::pair<int, long long int> a, std::pair<int, long long int> b);
bool compareSize(std::pair<int, unsigned int> a, std::pair<int, unsigned int> b);
bool compareTime(std::pair<int, int> a, std::pair<int,int> b);
</code>
</pre>


8. **blockencodings.cpp / Line 28**   
  >* Fee를 기준으로 정렬하는 코드를 구현하고 algorithm 라이브러리를 통해 sorting
  >* 상단에 헤더 파일 포함 필요   
  >* 또한, index를 이용해서 prefilledtxn을 채워야 하기 때문에 새로운 자료형 std::vector<std::pair<int, long long int>> indexAndFee; 선언 해줌   
  >  --> pair에 대한 접근은 indexAndFee.first      indexAndFee.second 로 하면 됨   
  >* 각 구간별 걸린 시간을 측정하기 위해서 log 를 찍음
<pre>
<code>
 #include < algorithm >
 
 LogPrint(BCLog::NET, "KAR's Log Start\n");

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

    std::vector<std::pair<int,long long int>> indexAndFee;

    for (size_t i = 0; i < btxinfo.size(); i++)
    {
            indexAndFee.push_back(std::pair<int,long long int>(i+1, btxinfo[i].fee));
    }

    LogPrint(BCLog::NET, "KAR's Log GTX prefilled0 %d : %lld\n", indexAndFee[0].first,  indexAndFee[0].second);
    LogPrint(BCLog::NET, "KAR's Log GTX prefilled1 %d : %lld\n", indexAndFee[1].first,  indexAndFee[1].second);
    LogPrint(BCLog::NET, "KAR's Log GTX prefilled2 %d : %lld\n", indexAndFee[2].first,  indexAndFee[2].second);



    LogPrint(BCLog::NET, "KAR's Log Sorting\n");

    sort(indexAndFee.begin(), indexAndFee.end(), compareFee);

    LogPrint(BCLog::NET, "KAR's Log GTX prefilled0 %d : %lld\n", indexAndFee[0].first,  indexAndFee[0].second);
    LogPrint(BCLog::NET, "KAR's Log GTX prefilled1 %d : %lld\n", indexAndFee[1].first,  indexAndFee[1].second);
    LogPrint(BCLog::NET, "KAR's Log GTX prefilled2 %d : %lld\n", indexAndFee[2].first,  indexAndFee[2].second);

}

bool compareFee(std::pair<int,long long int> a, std::pair<int,long long int> b)
{
        return a.second > b.second;
}
</code>
</pre>

9. **blockencodings.h / Line 83**
compareIndex 함수를 선언 해줌
prefilledtxn을 선언하고 난 이후에 선언해주어야 함
<pre>
<code>
bool compareIndex(PrefilledTransaction a, PrefilledTransaction b);
</code>
</pre>

10. **blockencodings.cpp / Line 97**   
  >* prefilledtxn에 트랜잭션 넣어주기 (코인 베이스 트랜잭션은 기존대로 넣고, 이후 for 문을 통해서 채움 // 코인 베이스르 제외 했기 때문에 index에 유의해서 넣어주어야 함)
  >* 또한, 한번 더 sorting을 해주어서 prefilledtxn을 오름차순으로 정렬해줌 // 받은 노드가 처리할 때 인덱스가 낮은 순서대로 처리될 필요가 있음
<pre>
<code>
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

bool compareIndex(PrefilledTransaction a, PrefilledTransaction b)
{
        return a.index < b.index;
}
</code>
</pre>


11. **blockencodings.cpp / Line 126**   
shorttxids 인덱스에 차례대로 넣을 수 있도록 변수 하나 선언해주고, find 함수를 사용해 prefilledtxn으로 채운 트랜잭션에 대해서는 제외하고 shorttxid를 만듦
<pre>
<code>
LogPrint(BCLog::NET, "KAR's Log Make Shortids\n");

size_t stindex = 0;
for (size_t i = 1; i < block.vtx.size(); i++) {
    it = std::find(prefilledindex.begin(), prefilledindex.end(), i);
    if (it == prefilledindex.end())
    {
      const CTransaction& tx = *block.vtx[i];
      shorttxids[stindex++] = GetShortID(fUseWTXID ? tx.GetWitnessHash() : tx.GetHash());
    }
}
shorttxids.resize(shorttxids.size() - 10);
LogPrint(BCLog::NET, "KAR's Log Shortids size %d\n", shorttxids.size());
</code>
</pre>

12. **blockencodings.cpp / Line 116**
> prefilledtxn 인덱스 값 넘겨줄 때에 원래 코드대로면   
> 각 txn 인덱스 사이의 값을 보내줘야 함   
> 예를 들어서 1 5 24 면   
> 코인베이스 0   
> 1 == 1 (1 - 0 )   
> 5 == 4 ( 5 - 1 )   
> 24 == 19  (24 - 5)    
> --> prefilledindex는 필요함, 있는지 없는지 확인하기 위해서
<pre>
<code>
LogPrint(BCLog::NET, "KAR's Log Change index\n");

prefilledtxn[1].index = uint16_t(prefilledindex[0] - 1);
for (size_t i = 2; i < pfsize; i++)
{
        prefilledtxn[i].index = uint16_t(prefilledindex[i-1] - prefilledindex[i-2] - 1);
}
</code>
</pre>

## How to Use

1. 해당 코드를 다운 받고, /bitcoin/src/ 폴더에 붙여넣기 해줌 (기존 파일에 덮어씌우기)

2. **blockencodings.cpp / Line 27**   
Prefilled-txn에 몇 개의 트랜잭션을 넣을 건지 지정
<pre>
<code>
size_t pfsize = 30;
</code>
</pre>

3. make 명령어를 통해 코드 수정 내용을 반영 후 비트코인 가동
<pre>
<code>
make
make install
</code>
</pre>

* **Bitcoin Core Version**   
v0.20.99.0-30568d3f1

* **Bitcoin Core Github**   
https://github.com/bitcoin/bitcoin


* * *

> **관련 논문**   
* 비트코인 네트워크의 압축 블록 전달 지연 개선 / 김애리[저] - 계명대학교 석사 학위 논문
* IJNM   

> **참고 논문**
* 김애리, 주홍택, "비트코인 노드의 압축 블록 전달 지연 원인 분석", 통신망운용관리 학술대회(KNOM Conf. 2021), pp. 69-79, Apr. 2021.
* Aeri Kim, Jungyeon Kim, Meryam Essaid, Sejin Park, Hongtaek Ju, "Analysis of Compact Block Propagation Delay in Bitcoin Network", Asia-Pacific Network Operations and Management Symposium (APNOMS 2021), Sep. 2021.
* 김애리, 주홍택,"비트코인 네트워크에서 압축 블록 전달 방식의 전송 지연 분석", 한국통신학회논문지, Vol. 47, No. 6, pp. 826-835, Jun. 2022 (KCI)
