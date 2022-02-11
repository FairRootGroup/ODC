/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// ODC
#include <odc/Restore.h>
#include <odc/Logger.h>
// Boost
#include <boost/property_tree/json_parser.hpp>

using namespace std;
using namespace odc::core;
namespace bpt = boost::property_tree;

//
// SRestorePartition
//

SRestorePartition::SRestorePartition()
{
}

SRestorePartition::SRestorePartition(const partitionID_t& _partitionId, const std::string& _sessionId)
    : m_partitionId(_partitionId)
    , m_sessionId(_sessionId)
{
}

SRestorePartition::SRestorePartition(const boost::property_tree::ptree& _pt)
{
    fromPT(_pt);
}

boost::property_tree::ptree SRestorePartition::toPT() const
{
    bpt::ptree pt;
    pt.put<partitionID_t>("partition", m_partitionId);
    pt.put<string>("session", m_sessionId);
    return pt;
}

void SRestorePartition::fromPT(const bpt::ptree& _pt)
{
    m_partitionId = _pt.get<partitionID_t>("partition", "");
    m_sessionId = _pt.get<string>("session", "");
}

//
// SRestoreData
//

SRestoreData::SRestoreData()
{
}

SRestoreData::SRestoreData(const boost::property_tree::ptree& _pt)
{
    fromPT(_pt);
}

boost::property_tree::ptree SRestoreData::toPT() const
{
    bpt::ptree children;
    for (const auto& v : m_partitions)
    {
        children.push_back(make_pair("", v.toPT()));
    }
    bpt::ptree pt;
    pt.add_child("sessions", children);
    return pt;
}

void SRestoreData::fromPT(const bpt::ptree& _pt)
{
    auto rpt{ _pt.get_child_optional("sessions") };
    if (rpt)
    {
        for (const auto& v : rpt.get())
        {
            m_partitions.push_back(SRestorePartition(v.second));
        }
    }
}

//
// CRestoreFile
//

CRestoreFile::CRestoreFile(const std::string& _id)
    : m_id(_id)
{
}

CRestoreFile::CRestoreFile(const std::string& _id, const SRestoreData& _data)
    : m_id(_id)
    , m_data(_data)
{
}

void CRestoreFile::write()
{
    try
    {
        auto dir{ boost::filesystem::path(getDir()) };
        if (!boost::filesystem::exists(dir) && !boost::filesystem::create_directories(dir))
        {
            throw std::runtime_error(toString("Restore failed to create directory ", std::quoted(dir.string())));
        }

        bpt::write_json(getFilepath(), m_data.toPT());
    }
    catch (const exception& _e)
    {
        OLOG(error) << "Failed to write restore data " << quoted(m_id) << " to file "
                               << quoted(getFilepath()) << ": " << _e.what();
    }
}

const SRestoreData& CRestoreFile::read()
{
    try
    {
        bpt::ptree pt;
        bpt::read_json(getFilepath(), pt);
        m_data = SRestoreData(pt);
    }
    catch (const exception& _e)
    {
        OLOG(error) << "Failed to read restore data " << quoted(m_id) << " from file "
                               << quoted(getFilepath()) << ": " << _e.what();
    }
    return m_data;
}

std::string CRestoreFile::getDir() const
{
    return smart_path(toString("$HOME/.ODC/restore/"));
}

std::string CRestoreFile::getFilepath() const
{
    return toString(getDir(), "odc_", m_id, ".json");
}
