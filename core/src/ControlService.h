// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__ControlService__
#define __ODC__ControlService__

// STD
#include <string>

// DDS
#include <DDS/Tools.h>
#include <DDS/Topology.h>

// FairMQ
#include <fairmq/SDK.h>

namespace odc
{
    namespace core
    {
        /// \brief Return status code of request
        enum EStatusCode
        {
            unknown = 0,
            ok,
            error
        };

        /// \brief General error
        struct SError
        {
            SError()
            {
            }

            SError(int _code, const std::string& _msg)
                : m_code(_code)
                , m_msg(_msg)
            {
            }

            int m_code{ 0 };   ///< Error code
            std::string m_msg; ///< Error message
        };

        /// \brief Structure holds return value of the request
        struct SReturnValue
        {
            SReturnValue()
            {
            }

            SReturnValue(EStatusCode _statusCode, const std::string& _msg, size_t _execTime, const SError& _error)
                : m_statusCode(_statusCode)
                , m_msg(_msg)
                , m_execTime(_execTime)
                , m_error(_error)
            {
            }

            EStatusCode m_statusCode{ EStatusCode::unknown }; ///< Operation status code
            std::string m_msg;                                ///< General message about the status
            size_t m_execTime{ 0 };                           ///< Execution time in milliseconds
            SError m_error; ///< In case of error containes information about the error
        };

        /// \brief Structure holds parameters of the Initiaalize request
        struct SInitializeParams
        {
            SInitializeParams()
            {
            }

            SInitializeParams(const std::string& _topologyFile,
                              const std::string& _rmsPlugin,
                              const std::string& _configFile)
                : m_topologyFile(_topologyFile)
                , m_rmsPlugin(_rmsPlugin)
                , m_configFile(_configFile)
            {
            }
            std::string m_topologyFile;
            std::string m_rmsPlugin;
            std::string m_configFile;
        };

        class CControlService
        {
          public:
            CControlService();

          public:
            SReturnValue Initialize(const SInitializeParams& _params);
            SReturnValue ConfigureRun();
            SReturnValue Start();
            SReturnValue Stop();
            SReturnValue Terminate();
            SReturnValue Shutdown();

          private:
            SReturnValue createReturnValue(bool _success,
                                           const std::string& _msg,
                                           const std::string& _errMsg,
                                           size_t _execTime);
            bool createDDSSession();
            bool submitDDSAgents(const std::string& _rmsPlugin,
                                 const std::string& _configFile,
                                 size_t _numAgents,
                                 size_t _numSlots);
            bool activateDDSTopology(const std::string& _topologyFile);
            bool waitForNumActiveAgents(size_t _numAgents);
            bool shutdownDDSSession();
            bool changeState(fair::mq::sdk::TopologyTransition _transition);

          private:
            std::shared_ptr<dds::topology_api::CTopology> m_topo;  ///< DDS topology
            std::shared_ptr<dds::tools_api::CSession> m_session;   ///< DDS session
            std::shared_ptr<fair::mq::sdk::Topology> m_fairmqTopo; ///< FairMQ topology
            const size_t m_timeout;                                ///< Request timeout in sec
        };
    } // namespace core
} // namespace odc

#endif /* defined(__ODC__ControlService__) */
