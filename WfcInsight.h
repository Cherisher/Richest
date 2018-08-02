#pragma once
#include "httpclient.h"
#include <cstdint>
#include <jsoncpp/json/json.h>

class WfcInsight
{
public:
    WfcInsight();
    WfcInsight(const std::string &path_root);

    bool getBalance(double &balance, const std::string &adddress);
    bool getInfo(Json::Value &info);
    bool syncStatus(Json::Value &status);

    bool getBlockHash(std::string &hash, const uint64_t &height);
    bool getBlock(Json::Value &block, const std::string &hash);
    bool getTx(Json::Value &tx, const std::string &txid);

private:
    std::string m_path_root;
    HD::HttpClient client;
};
