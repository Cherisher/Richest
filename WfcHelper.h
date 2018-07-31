#pragma once
#include <cstdint>
#include <sstream>
#include <unordered_set>
#include <vector>
class wfcClient;
struct wallet_info {
    std::string address;
    double balance;
};

#define WFC_RPC_SERVER "http://test:admin@127.0.0.1:9665"
#define WFC_WALLET_DBPATH "/home/wwwroot/wfc.dpifw.cn/test.db"

const std::string getCurrentSystemTime();
std::string print_money(std::string amount, unsigned int decimal_point = 8);
std::string print_money(uint64_t amount, unsigned int decimal_point = 8);

void init_db();
unsigned long getLastHeight();
double getBalance(wfcClient &client, const std::string &address);
bool listAddress(std::vector<wallet_info> &wallets);
bool wfc_write_balance_db(std::vector<wallet_info> &wallets);
bool wfc_write_address_db(const std::unordered_set<std::string> &addressSet);
bool wfc_write_system_db(unsigned long height);
void wfc_write_system_info();
