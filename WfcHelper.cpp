#include "WfcHelper.h"
#include "httpclient.h"
#include "sqlitedb.h"
#include "wfcclient.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <vector>

#include <boost/thread/thread.hpp>

using namespace std;
using namespace jsonrpc;
using namespace boost;

#define INSERT_FMT "replace into wallet_info (id, address,amount) values (?, ?, ?)"
#define ADDRESS_INSERT_FMT "replace into wallet_info (address) values (?)"

shared_mutex dbio_s_mu; //全局shared_mutex对象

const std::string getCurrentSystemTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

unsigned long getLastHeight()
{
    unsigned long height = 0;
    try {
        shared_lock<shared_mutex> m(dbio_s_mu); //读锁定，shared_loc
        SQLiteDB db(WFC_WALLET_DBPATH);
        std::stringstream sql;
        sql << "select * from system_info limit 1";
        SQLiteDB::Cursor cursor(db, sql.str().c_str());
        while (cursor.read()) {
            if (!cursor["height"]) {
                continue;
            }
            height = strtoul(cursor["height"], nullptr, 0);
        }
    } catch (...) {
    }

    return height;
}
bool listAddress(std::vector<wallet_info> &wallets)
{
    try {
        shared_lock<shared_mutex> m(dbio_s_mu); //读锁定，shared_loc
        SQLiteDB db(WFC_WALLET_DBPATH);
        std::stringstream sql;
        sql << "select address from wallet_info";
        SQLiteDB::Cursor cursor(db, sql.str().c_str());
        while (cursor.read()) {
            if (!cursor["address"]) {
                continue;
            }

            wallet_info wallet;
            wallet.address = cursor["address"];
            wallet.balance = 0.0;
            wallets.emplace_back(wallet);
        }
    } catch (...) {
        return false;
    }
    return true;
}

void wfc_init_db()
{
    try {
        SQLiteDB db(WFC_WALLET_DBPATH, SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE);
        db.exec("create table if not exists wallet_info(id INTEGER PRIMARY KEY AUTOINCREMENT, address TEXT NOT NULL, "
                "amount REAL DEFAULT 0.0)");
        db.exec("create table if not exists system_info(id INT PRIMARY KEY, height INTEGER, last_update TEXT)");
        db.exec("create unique  index if not exists address_index on wallet_info(address)");
        db.exec("PRAGMA synchronous = OFF; ");
    } catch (...) {
        cerr << "init_db failed!!!" << endl;
    }
}

bool wfc_write_balance_db(std::vector<wallet_info> &wallets)
{
    sort(wallets.begin(), wallets.end(), [](wallet_info &a, wallet_info &b) { return a.balance > b.balance; });

    try {
        unique_lock<shared_mutex> m(dbio_s_mu);
        SQLiteDB db(WFC_WALLET_DBPATH, SQLITE_OPEN_READWRITE);
        sqlite3_stmt *stmt = nullptr;
        try {
            db.exec("BEGIN");

            db.exec("delete from wallet_info");
            sqlite3_prepare_v2(db.getConn(), INSERT_FMT, strlen(INSERT_FMT), &stmt, 0);
            int i = 1;
            for (const auto &wallet : wallets) {
                sqlite3_reset(stmt);
                // 此参数有两个常数，SQLITE_STATIC告诉sqlite3_bind_text函数字符串为常量，可以放心使用；
                // 而SQLITE_TRANSIENT会使得sqlite3_bind_text函数对字符串做一份拷贝。一般使用这两个常量参数来调用sqlite3_bind_text。
                sqlite3_bind_int(stmt, 1, i++);
                sqlite3_bind_text(stmt, 2, wallet.address.c_str(), wallet.address.length(), SQLITE_TRANSIENT);
                sqlite3_bind_double(stmt, 3, wallet.balance);

                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    cerr << "Error insert "
                         << ": " << wallet.address << "\t" << wallet.balance << endl;
                }
            }
            db.exec("COMMIT;");
        } catch (...) {
            db.exec("ROLLBACK;");
        }

        if (stmt) {
            sqlite3_finalize(stmt);
        }

    } catch (...) {
    }

    return true;
}

bool wfc_write_address_db(const std::unordered_set<std::string> &addressSet)
{
    try {
        unique_lock<shared_mutex> m(dbio_s_mu);
        SQLiteDB db(WFC_WALLET_DBPATH, SQLITE_OPEN_READWRITE);
        sqlite3_stmt *stmt = nullptr;
        try {
            sqlite3_prepare_v2(db.getConn(), ADDRESS_INSERT_FMT, strlen(ADDRESS_INSERT_FMT), &stmt, 0);
            db.exec("BEGIN;");
            for (const auto &address : addressSet) {
                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, address.c_str(), address.length(), SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    cerr << "Error insert " << address << " failed." << endl;
                }
            }
            db.exec("COMMIT;");
        } catch (...) {
            db.exec("ROLLBACK;");
        }

        if (stmt) {
            sqlite3_finalize(stmt);
        }

    } catch (...) {
        return false;
    }
    return true;
}

bool wfc_write_system_db(unsigned long height)
{
    try {
        unique_lock<shared_mutex> m(dbio_s_mu);
        SQLiteDB db(WFC_WALLET_DBPATH, SQLITE_OPEN_READWRITE);
        std::stringstream sql;
        sql << "replace into system_info(id,height,last_update) values (1," << height << ","
            << /*getCurrentSystemTime()*/ time(NULL) << ");";
        cout << sql.str() << endl;
        db.exec(sql.str().c_str());
    } catch (...) {
        cout << "Insert system failed." << endl;
        return false;
    }
    return true;
}
void wfc_write_system_info()
{
    Json::Value system_info;
    Json::Value item1, item2, item3, item4;
    item1["modname"] = "WFC价格";
    item1["value"] = "￥0.03";

    item2["modname"] = "涨幅(24h)";
    item2["value"] = "-100%";

    item3["modname"] = "流通量";
    item3["value"] = "1.68亿";

    item4["modname"] = "更新时间";
    item4["value"] = getCurrentSystemTime();

    system_info["list"].append(item1);
    system_info["list"].append(item2);
    system_info["list"].append(item3);
    system_info["list"].append(item4);

    ofstream out("/home/wwwroot/wfc.dpifw.cn/system-info.js");

    out << "var data1 = " << system_info.toStyledString() << endl;
}
