// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__DDSSubmit__
#define __ODC__DDSSubmit__

// STD
#include <ostream>
#include <string>
// BOOST
#include <boost/property_tree/ptree.hpp>
// ODC
#include "PluginManager.h"

namespace odc::core
{
    class CDDSSubmit : public CPluginManager
    {
      public:
        /// \brief DDS submit parameters
        struct SParams
        {
            /// \brief Default constructor
            SParams();
            /// \brief Constructor with arguments
            SParams(const std::string& _rmsPlugin,
                    const std::string& _configFile,
                    size_t _numAgents,
                    size_t _numSlots,
                    size_t _requiredNumSlots);

            /// \brief Initialization of structure from an XML file
            //            void initFromXML(const std::string& _filepath);
            /// \brief Initialization of structure froma an XML stream
            void initFromXML(std::istream& _stream);
            /// \brief Initialization of structure from property tree
            void initFromPT(const boost::property_tree::ptree& _pt);

            std::string m_rmsPlugin;        ///< RMS plugin of DDS
            std::string m_configFile;       ///< Path to the configuration file of the RMS plugin
            size_t m_numAgents{ 0 };        ///< Number of DDS agents
            size_t m_numSlots{ 0 };         ///< Number of slots per DDS agent
            size_t m_requiredNumSlots{ 0 }; ///< Wait for the required number of slots become active

            // \brief ostream operator.
            friend std::ostream& operator<<(std::ostream& _os, const SParams& _params);
        };

        CDDSSubmit();

        SParams makeParams(const std::string& _plugin,
                           const std::string& _resources,
                           const partitionID_t& _partitionID,
                           runNr_t _runNr);

      private:
        void registerDefaultPlugin(const std::string& _name);

      public:
        // Shared pointer
        using Ptr_t = std::shared_ptr<CDDSSubmit>;
    };
} // namespace odc::core

#endif /* defined(__ODC__DDSSubmit__) */
