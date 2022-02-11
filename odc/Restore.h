/********************************************************************************
 * Copyright (C) 2019-2022 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef __ODC__Restore__
#define __ODC__Restore__

// STD
#include <string>
// ODC
#include <odc/Def.h>
// BOOST
#include <boost/property_tree/ptree.hpp>

namespace odc::core
{
    struct SRestorePartition
    {
        using Vector_t = std::vector<SRestorePartition>;

        SRestorePartition();
        SRestorePartition(const partitionID_t& _partitionId, const std::string& _sessionId);
        SRestorePartition(const boost::property_tree::ptree& _pt);

        boost::property_tree::ptree toPT() const;
        void fromPT(const boost::property_tree::ptree& _pt);

        partitionID_t m_partitionId;
        std::string m_sessionId;
    };

    struct SRestoreData
    {
        SRestoreData();
        SRestoreData(const boost::property_tree::ptree& _pt);

        boost::property_tree::ptree toPT() const;
        void fromPT(const boost::property_tree::ptree& _pt);

        SRestorePartition::Vector_t m_partitions;
    };

    class CRestoreFile
    {
      public:
        CRestoreFile(const std::string& _id);
        CRestoreFile(const std::string& _id, const SRestoreData& _data);

        void write();
        const SRestoreData& read();

      private:
        std::string getDir() const;
        std::string getFilepath() const;

        std::string m_id;
        SRestoreData m_data;
    };
} // namespace odc::core

#endif /* defined(__ODC__Restore__) */
