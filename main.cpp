#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <fstream>
#include <iostream>
#include <jsoncpp/json/json.h>
#include <unordered_map>
#include <unordered_set>

#include "WfcHelper.h"
#include "WfcInsight.h"

#include "threadpool.h"

using namespace std;
using namespace boost;

std::unordered_set<std::string> addressList;

static unsigned long block_height = 1;
static bool stoped = false;

static void detect_tx_vin(const Json::Value &vin)
{
    // cout << vin.toStyledString() << endl;
    int vin_j, vin_size = vin.size();
    for (vin_j = 0; vin_j < vin_size; vin_j++) {
        if (!vin[vin_j].isMember("addr") || !vin[vin_j]["addr"].isString()) {
            continue;
        }
        std::string address = vin[vin_j]["addr"].asString();
        // cout << address << endl;
        addressList.insert(address);
    }
}

static void detect_tx_vout(const Json::Value &vout)
{
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
            //  cout << address << endl;
            addressList.insert(address);
        }
    }
}

static void parse_block_round(const unsigned long &blk_index, bool &error)
{
    error = false;
    WfcInsight insight;
    std::string hash;
    Json::Value block;
    cout << "blk_index: " << blk_index << endl;
    if (!insight.getBlockHash(hash, blk_index) || !insight.getBlock(block, hash)) {
        return;
    }
    if (block.isMember("tx") && block["tx"].isArray()) {
        int i, tx_size = block["tx"].size();
        for (i = 0; i < tx_size; i++) {
            std::string txid;
            txid = block["tx"][i].asString();
            Json::Value tx;
            if (insight.getTx(tx, txid)) {
                // cout << tx.toStyledString() << endl;
                if (tx.isMember("vin") && tx["vin"].isArray()) {
                    detect_tx_vin(tx["vin"]);
                }
                if (tx.isMember("vout") && tx["vout"].isArray()) {
                    detect_tx_vout(tx["vout"]);
                }
            } else {
                cerr << ("txid: <" + txid + "> get failed.");
            }
        }
    } else {
        cerr << ("block: <" + std::to_string(blk_index) + "> bad block.");
    }
    error = true;
}

void thread_detect_address()
{
    unsigned long now_block_height;
    block_height = std::max(getLastHeight(), block_height);
    cout << "block_height: " << block_height << endl;

    WfcInsight insight;
    Json::Value info;
    if (insight.getInfo(info)) {
        // cout << info.toStyledString() << endl;
        now_block_height = (unsigned long)info["blocks"].asUInt64();
    } else {
        now_block_height = 0;
    }
    now_block_height = min(now_block_height, block_height + 5000);

    cout << block_height << " -> " << now_block_height << endl;

    if (now_block_height <= block_height) {
        return;
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
                tpool.submit(&waiter, boost::bind(&parse_block_round, std::cref(blk_height[j]), std::ref(error[j])));
            }
            waiter.wait();
            for (int j = 0; j < thread_num; ++j) {
                if (!error[j]) {
                    cerr << "parse_block_round failed: block: " << blk_height[j] << endl;
                }
            }
            count++;
#if 0
            if (count % 1000 == 0) {
                wfc_write_address_db(addressList);
            }
#endif
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
#if 0
            if (count % 1000 == 0) {
                wfc_write_address_db(addressList);
            }
#endif
        }
    }
    wfc_write_address_db(addressList);

    block_height = now_block_height;

    cout << __FUNCTION__ << endl;
}

void thread_detect_balance()
{
    std::vector<wallet_info> wallets;
    if (listAddress(wallets)) {
        WfcInsight insight;
        for (auto &wallet : wallets) {
            if (wallet.refresh == 0) {
                cout << wallet.address << "\t" << wallet.balance << " no need update." << endl;
                continue;
            }
            double balance;
            if (insight.getBalance(balance, wallet.address)) {
                wallet.balance = balance;
                cout << wallet.address << "\t" << wallet.balance << endl;
            } else {
                wallet.balance = 0.0;
                cout << wallet.address << "\t"
                     << "getBalance failed." << endl;
            }
        }
        wfc_write_balance_db(wallets);
    }

    wfc_write_system_info();
    cout << __FUNCTION__ << ":" << wallets.size() << endl;
}

void test()
{

    WfcInsight insight;
    Json::Value info;
    if (!insight.getInfo(info)) {
        exit(-1);
    }
    cout << info.toStyledString() << endl;
    uint64_t height = info["blocks"].asUInt64();

    std::string hash;

    if (!insight.getBlockHash(hash, height)) {
        exit(-2);
    }

    Json::Value block;

    if (!insight.getBlock(block, hash)) {
        exit(-3);
    }

    cout << block.toStyledString() << endl;
    if (block.isMember("tx") && block["tx"].isArray()) {
        int i, tx_size = block["tx"].size();
        for (i = 0; i < tx_size; i++) {
            std::string txid;
            txid = block["tx"][i].asString();
            Json::Value tx;
            if (insight.getTx(tx, txid)) {
                cout << tx.toStyledString() << endl;
                if (tx.isMember("vout") && tx["vout"].isArray()) {
                    detect_tx_vout(tx["vout"]);
                }
            }
        }
    }

    exit(0);
}

int main(int argc, char *argv[])
{
    // test();
    wfc_init_db();
    thread_detect_address();
    thread_detect_balance();

    wfc_write_system_db(block_height);
    return 0;
}
