/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__Restore__
#define __ODC__Restore__

#include <odc/Logger.h>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <string>

namespace odc::core {

struct SRestorePartition
{
    using Vector_t = std::vector<SRestorePartition>;

    SRestorePartition() {}
    SRestorePartition(const std::string& _partitionId, const std::string& sessionId)
        : mPartitionID(_partitionId)
        , mDDSSessionId(sessionId)
    {}
    SRestorePartition(const boost::property_tree::ptree& _pt) { fromPT(_pt); }

    boost::property_tree::ptree toPT() const
    {
        boost::property_tree::ptree pt;
        pt.put<std::string>("partition", mPartitionID);
        pt.put<std::string>("session", mDDSSessionId);
        return pt;
    }
    void fromPT(const boost::property_tree::ptree& _pt)
    {
        mPartitionID = _pt.get<std::string>("partition", "");
        mDDSSessionId = _pt.get<std::string>("session", "");
    }

    std::string mPartitionID;
    std::string mDDSSessionId;
};

struct SRestoreData
{
    SRestoreData() {}
    SRestoreData(const boost::property_tree::ptree& _pt) { fromPT(_pt); }

    boost::property_tree::ptree toPT() const
    {
        boost::property_tree::ptree children;
        for (const auto& v : m_partitions) {
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
                m_partitions.push_back(SRestorePartition(v.second));
            }
        }
    }

    SRestorePartition::Vector_t m_partitions;
};

class CRestoreFile
{
  public:
    CRestoreFile(const std::string& _id)
        : m_id(_id)
    {}
    CRestoreFile(const std::string& _id, const SRestoreData& _data)
        : m_id(_id)
        , m_data(_data)
    {}

    void write()
    {
        try {
            auto dir{ boost::filesystem::path(getDir()) };
            if (!boost::filesystem::exists(dir) && !boost::filesystem::create_directories(dir)) {
                throw std::runtime_error(toString("Restore failed to create directory ", std::quoted(dir.string())));
            }

            boost::property_tree::write_json(getFilepath(), m_data.toPT());
        } catch (const std::exception& _e) {
            OLOG(error) << "Failed to write restore data " << quoted(m_id) << " to file " << quoted(getFilepath()) << ": " << _e.what();
        }
    }
    const SRestoreData& read()
    {
        try {
            boost::property_tree::ptree pt;
            boost::property_tree::read_json(getFilepath(), pt);
            m_data = SRestoreData(pt);
        } catch (const std::exception& _e) {
            OLOG(error) << "Failed to read restore data " << quoted(m_id) << " from file " << quoted(getFilepath()) << ": " << _e.what();
        }
        return m_data;
    }

  private:
    std::string getDir() const { return smart_path(toString("$HOME/.ODC/restore/")); }
    std::string getFilepath() const { return toString(getDir(), "odc_", m_id, ".json"); }

    std::string m_id;
    SRestoreData m_data;
};

} // namespace odc::core

#endif /* defined(__ODC__Restore__) */
