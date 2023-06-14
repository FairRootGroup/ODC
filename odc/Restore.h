/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ODC_CORE_RESTORE
#define ODC_CORE_RESTORE

#include <odc/Logger.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <filesystem>
#include <string>

namespace odc::core {

struct RestorePartition
{
    RestorePartition() {}
    RestorePartition(const std::string& partitionId, const std::string& sessionId)
        : mPartitionID(partitionId)
        , mDDSSessionId(sessionId)
    {}
    RestorePartition(const boost::property_tree::ptree& pt) { fromPT(pt); }

    boost::property_tree::ptree toPT() const
    {
        boost::property_tree::ptree pt;
        pt.put<std::string>("partition", mPartitionID);
        pt.put<std::string>("session", mDDSSessionId);
        return pt;
    }
    void fromPT(const boost::property_tree::ptree& pt)
    {
        mPartitionID = pt.get<std::string>("partition", "");
        mDDSSessionId = pt.get<std::string>("session", "");
    }

    std::string mPartitionID;
    std::string mDDSSessionId;
};

struct RestoreData
{
    RestoreData() {}
    RestoreData(const boost::property_tree::ptree& pt) { fromPT(pt); }

    boost::property_tree::ptree toPT() const
    {
        boost::property_tree::ptree children;
        for (const auto& v : mPartitions) {
            children.push_back(make_pair("", v.toPT()));
        }
        boost::property_tree::ptree pt;
        pt.add_child("sessions", children);
        return pt;
    }
    void fromPT(const boost::property_tree::ptree& _pt)
    {
        auto rpt{ _pt.get_child_optional("sessions") };
        if (rpt) {
            for (const auto& v : rpt.get()) {
                mPartitions.push_back(RestorePartition(v.second));
            }
        }
    }

    std::vector<RestorePartition> mPartitions;
};

class RestoreFile
{
  public:
    RestoreFile(const std::string& id, const std::string& dir)
        : mId(id)
        , mDir(dir.empty() ? smart_path(toString("$HOME/.ODC/restore/")) : dir)
    {}
    RestoreFile(const std::string& id, const std::string& dir, const RestoreData& data)
        : mId(id)
        , mDir(dir.empty() ? smart_path(toString("$HOME/.ODC/restore/")) : dir)
        , mData(data)
    {}

    void write()
    {
        try {
            if (!std::filesystem::exists(mDir) && !std::filesystem::create_directories(mDir)) {
                throw std::runtime_error(toString("Restore failed to create directory ", std::quoted(mDir.string())));
            }

            OLOG(info) << "Writing restore file " << std::quoted(getFilepath());
            boost::property_tree::write_json(getFilepath(), mData.toPT());
        } catch (const std::exception& e) {
            OLOG(error) << "Failed to write restore data " << quoted(mId) << " to file " << quoted(getFilepath()) << ": " << e.what();
        }
    }
    const RestoreData& read()
    {
        try {
            boost::property_tree::ptree pt;
            OLOG(info) << "Reading restore file " << std::quoted(getFilepath());
            boost::property_tree::read_json(getFilepath(), pt);
            mData = RestoreData(pt);
        } catch (const std::exception& e) {
            OLOG(error) << "Failed to read restore data " << quoted(mId) << " from file " << quoted(getFilepath()) << ": " << e.what();
        }
        return mData;
    }

  private:
    std::string getFilepath() const
    {
        return std::filesystem::path(mDir / toString("odc_", mId, ".json")).string();
    }

    std::string mId;
    std::filesystem::path mDir;
    RestoreData mData;
};

} // namespace odc::core

#endif /* defined(ODC_CORE_RESTORE) */
