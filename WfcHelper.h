#pragma once
#include <cstdint>
#include <jsoncpp/json/json.h>
#include <sstream>
#include <unordered_set>
#include <vector>

struct wallet_info {
    std::string address;
    double balance;
    int refresh;
};
#if 1
#define WFC_WALLET_DBPATH "/home/wwwroot/wfc.dpifw.cn/test.db"
#define WFC_SYSTEM_INFOPATH "/home/wwwroot/wfc.dpifw.cn/system-info.js"
#else
#define WFC_WALLET_DBPATH "test.db"
#define WFC_SYSTEM_INFOPATH "system-info.js"

#endif
const std::string getCurrentSystemTime();

unsigned long getLastHeight();
bool listAddress(std::vector<wallet_info> &wallets);

void wfc_init_db();
bool wfc_write_balance_db(std::vector<wallet_info> &wallets);
bool wfc_write_address_db(const std::unordered_set<std::string> &addressSet);
bool wfc_write_system_db(unsigned long height);
void wfc_write_system_info();
