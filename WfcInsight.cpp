#include "WfcInsight.h"

#if 0
#define DEFAULT_INSIGHT_URL "https://wfc.xyblock.net"
#else
#define DEFAULT_INSIGHT_URL "https://insight.wificoin.club"
#endif
WfcInsight::WfcInsight() : m_path_root(DEFAULT_INSIGHT_URL)
{
    client.AddHeader("Connection", "close");
    m_path_root.append("/insight-api");
}
WfcInsight::WfcInsight(const std::string &path_root) : m_path_root(path_root)
{
    client.AddHeader("Connection", "close");
}

bool WfcInsight::getBalance(double &balance, const std::string &address)
{
    balance = 0.0;
    std::string url = m_path_root + "/addr/" + address + "/?noTxList=1";
    Json::Value root;

    if (!client.doHttpGetRequest(url, root)) {
        return false;
    }

    if (!root.isMember("balance") || !root["balance"].isNumeric()) {
        return false;
    }

    balance = root["balance"].asDouble();
    return true;
}

bool WfcInsight::getInfo(Json::Value &info)
{
    std::string url = m_path_root + "/" + "status?q=getInfo";
    Json::Value root;
    if (!client.doHttpGetRequest(url, root)) {
        return false;
    }

    if (!root.isObject() || !root.isMember("info") || !root["info"].isObject())
        return false;

    info = root["info"];
    return true;
}

bool WfcInsight::syncStatus(Json::Value &status)
{
    std::string url = m_path_root + "/" + "sync";
    Json::Value root;
    if (!client.doHttpGetRequest(url, root)) {
        return false;
    }

    if (!root.isObject())
        return false;

    status = root;
    return true;
}

bool WfcInsight::getBlockHash(std::string &hash, const uint64_t &height)
{
    std::string url = m_path_root + "/" + "block-index/" + std::to_string(height);
    Json::Value root;
    if (!client.doHttpGetRequest(url, root)) {
        return false;
    }

    if (!root.isObject() || !root.isMember("blockHash") || !root["blockHash"].isString()) {
        return false;
    }

    hash = root["blockHash"].asString();
    return true;
}

bool WfcInsight::getBlock(Json::Value &block, const std::string &hash)
{
    std::string url = m_path_root + "/" + "block/" + hash;
    Json::Value root;
    if (!client.doHttpGetRequest(url, root)) {
        return false;
    }

    if (!root.isObject() || !root.isMember("tx") || !root["tx"].isArray()) {
        return false;
    }

    block = root;
    return true;
}

bool WfcInsight::getTx(Json::Value &tx, const std::string &txid)
{
    std::string url = m_path_root + "/" + "tx/" + txid;
    Json::Value root;
    if (!client.doHttpGetRequest(url, root)) {
        return false;
    }

    if (!root.isObject()) {
        return false;
    }

    tx = root;
    return true;
}
