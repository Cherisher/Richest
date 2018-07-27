#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <fstream>
#include <unordered_set>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <unordered_map>

#include <jsonrpccpp/client/connectors/httpclient.h>

#include "wfcclient.h"
#include "WfcHelper.h"
#include "threadpool.h"
#include "sqlitedb.h"

using namespace std;
using namespace boost;
using namespace jsonrpc;

std::unordered_set<std::string> addressList;

static unsigned long block_height = 1;
static bool stoped = false;

static void parse_block_round(const unsigned long &blk_index, bool &error)
{
    error = false;
    HttpClient httpclient(WFC_RPC_SERVER);
    wfcClient client(httpclient, JSONRPC_CLIENT_V1);
    try {
        std::string hash = client.getblockhash(blk_index);
        cout << "block hash: " << hash << endl;
        Json::Value result = client.getblock(hash, 1);
        if (!result.isMember("tx") || !result["tx"].isArray()) {
            cout << ("block index: " + std::to_string(blk_index) + " hash: " + hash + " Bad block");
            return;
        }

        const Json::Value &tx = result["tx"];
        int tx_i, tx_size = tx.size();
        cout << "blk_index: " << blk_index << "\t" << tx_size << endl;
        for (tx_i = 0; tx_i < tx_size; tx_i++) {
            std::string tx_hash = tx[tx_i].asString();
            Json::Value txinfo;

            try {
                txinfo = client.getrawtransaction(tx_hash, true);
            } catch (jsonrpc::JsonRpcException &e) {
                cout << "getrawtransaction: " << tx_hash << e.GetMessage() << endl;
                continue;
#if 0
				 try{
					 txinfo = client.gettransaction(tx_hash);
					  }catch(jsonrpc::JsonRpcException &e){
						  cout << "gettransaction: " << tx_hash << e.GetMessage() << endl;
					  }
#endif
            }

            if (txinfo.isMember("vout") && txinfo["vout"].isArray()) {
                const Json::Value &vout = txinfo["vout"];
                int vout_j, vout_size = vout.size();
                for (vout_j = 0; vout_j < vout_size; vout_j++) {
                    if (!vout[vout_j].isMember("scriptPubKey") || !vout[vout_j]["scriptPubKey"].isObject() ||
                        !vout[vout_j]["scriptPubKey"].isMember("addresses") ||
                        !vout[vout_j]["scriptPubKey"]["addresses"].isArray()) {
                        // cerr << "Error transaction:" <<  vout[vout_j].toStyledString() << endl;
                        continue;
                    }
                    const Json::Value &addr = vout[vout_j]["scriptPubKey"]["addresses"];
                    for (int idx = 0; idx < addr.size(); idx++) {
                        std::string address = addr[idx].asString();
                        addressList.insert(address);
                    }
                }
            }
        }

    } catch (jsonrpc::JsonRpcException &e) {
        cerr << __FUNCTION__ << ": " << e.GetMessage() << endl;
        return;
    }
    error = true;
}



void thread_detect_address()
{
    HttpClient httpclient(WFC_RPC_SERVER);
    wfcClient client(httpclient, JSONRPC_CLIENT_V1);
    unsigned long now_block_height;
    while (!stoped) {

        try {
            now_block_height = static_cast<unsigned long>(client.getblockcount());
            cout << block_height << " -> "<< now_block_height << endl;
        } catch (JsonRpcException &e) {
            cout << "getblockcount: " << e.GetMessage() << endl;
            continue;
        }

        if(now_block_height <=block_height){
          continue;
        }
        tools::threadpool &tpool = tools::threadpool::getInstance();
        unsigned long threads = tpool.get_max_concurrency();

        unsigned long blk_index;
        int count = 1;
        if (threads > 1) {
            unsigned long rounds_size = now_block_height - block_height;

            for (blk_index = 0; blk_index < rounds_size; blk_index += threads) {
                if (stoped) {
                    return;
                }
                std::deque<bool> error(threads);
                unsigned long blk_height[threads];
                unsigned long thread_num = std::min(threads, rounds_size - blk_index);
                tools::threadpool::waiter waiter;
                for (int j = 0; j < thread_num; ++j) {
                    blk_height[j] = block_height + blk_index + j;
                    tpool.submit(&waiter,
                                 boost::bind(&parse_block_round, std::cref(blk_height[j]), std::ref(error[j])));
                }
                waiter.wait();
                for (int j = 0; j < thread_num; ++j) {
                    if (!error[j]) {
                        cerr << "parse_block_round failed: block: " << blk_height[j] << endl;
                    }
                }
                count++;
                if (count % 1000 == 0) {
                      wfc_write_address_db(addressList);
                }
            }
        } else {
            for (blk_index = block_height; blk_index < now_block_height; blk_index++) {
                if (stoped) {
                    return;
                }

                bool ret;
                parse_block_round(blk_index, ret);
                if (!ret) {
                    cerr << "parse_block_round failed: block: " << blk_index << endl;
                }
                count++;
                if(count % 1000 == 0){
                    wfc_write_address_db(addressList);
                }
            }
        }

        block_height = now_block_height;
        boost::this_thread::sleep(boost::posix_time::seconds(30));
        cout << __FUNCTION__ << endl;
    }
}

void thread_detect_balance()
{
  while (!stoped) {
    std::vector<wallet_info> wallets;
    if(listAddress(wallets))
      wfc_write_balance_db(wallets);

      wfc_write_system_info();
      cout << __FUNCTION__ <<":" << wallets.size() << endl;
	break;
      boost::this_thread::sleep(boost::posix_time::seconds(60));
  }
}

int main(int argc, char *argv[])
{
    init_db();

//    boost::thread t1(thread_detect_address);

    boost::thread t2(thread_detect_balance);

  //  t1.join();
    t2.join();

    return 0;
}
