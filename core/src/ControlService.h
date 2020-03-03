// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__ControlService__
#define __ODC__ControlService__

// STD
#include <memory>
#include <string>

namespace odc
{
    namespace core
    {
        using runID_t = uint64_t;

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

            SReturnValue(EStatusCode _statusCode,
                         const std::string& _msg,
                         size_t _execTime,
                         const SError& _error,
                         runID_t _runID)
                : m_statusCode(_statusCode)
                , m_msg(_msg)
                , m_execTime(_execTime)
                , m_error(_error)
                , m_runID(_runID)
            {
            }

            EStatusCode m_statusCode{ EStatusCode::unknown }; ///< Operation status code
            std::string m_msg;                                ///< General message about the status
            size_t m_execTime{ 0 };                           ///< Execution time in milliseconds
            SError m_error;       ///< In case of error containes information about the error
            runID_t m_runID{ 0 }; ///< Run ID
        };

        /// \brief Structure holds configuration parameters of the Initiaalize request
        struct SInitializeParams
        {
            SInitializeParams()
            {
            }

            SInitializeParams(runID_t _runID)
                : m_runID(_runID)
            {
            }

            runID_t m_runID{ 0 }; ///< Run ID
        };

        /// \brief Structure holds configuration parameters of the submit request
        struct SSubmitParams
        {
            SSubmitParams()
            {
            }

            SSubmitParams(const std::string& _rmsPlugin,
                          const std::string& _configFile,
                          size_t _numAgents,
                          size_t _numSlots)
                : m_rmsPlugin(_rmsPlugin)
                , m_configFile(_configFile)
                , m_numAgents(_numAgents)
                , m_numSlots(_numSlots)
            {
            }
            std::string m_rmsPlugin;  ///< RMS plugin of DDS
            std::string m_configFile; ///< Path to the configuration file of the RMS plugin
            size_t m_numAgents{ 0 };  ///< Number of DDS agents
            size_t m_numSlots{ 0 };   ///< Number of slots per DDS agent
        };

        /// \brief Structure holds configuration parameters of the activate topology request
        struct SActivateParams
        {
            SActivateParams()
            {
            }

            SActivateParams(const std::string& _topologyFile)
                : m_topologyFile(_topologyFile)
            {
            }
            std::string m_topologyFile; ///< Path to the topoloy file
        };

        /// \brief Structure holds configuration parameters of the updatetopology request
        struct SUpdateParams
        {
            SUpdateParams()
            {
            }

            SUpdateParams(const std::string& _topologyFile)
                : m_topologyFile(_topologyFile)
            {
            }
            std::string m_topologyFile; ///< Path to the topoloy file
        };

        /// \brief Structure holds configuaration parameters of the SetProperty request
        struct SSetPropertyParams
        {
            SSetPropertyParams()
            {
            }

            SSetPropertyParams(const std::string& _key, const std::string& _value, const std::string& _path)
                : m_key(_key)
                , m_value(_value)
                , m_path(_path)
            {
            }
            std::string m_key;   ///< Property key
            std::string m_value; ///< Property value
            std::string m_path;  ///< Path in the topology
        };

        class CControlService
        {
          public:
            /// \brief Default constructor
            CControlService();

            /// \brief Initialize DDS session
            SReturnValue execInitialize(const SInitializeParams& _params);
            /// \brief Submit DDS agents. Can be called multiple times in order to submit more agents.
            SReturnValue execSubmit(const SSubmitParams& _params);
            /// \brief Activate topology
            SReturnValue execActivate(const SActivateParams& _params);
            /// \brief Update topology. Can be called multiple times in order to update topology.
            SReturnValue execUpdate(const SUpdateParams& _params);
            /// \brief Configure devices: InitDevice->CompleteInit->Bind->Connect->InitTask
            SReturnValue execConfigure();
            /// \brief Set property
            SReturnValue execSetProperty(const SSetPropertyParams& _params);
            /// \brief Start devices: Run
            SReturnValue execStart();
            /// \brief Stop devices: Stop
            SReturnValue execStop();
            /// \brief Reset devices: ResetTask->ResetDevice
            SReturnValue execReset();
            /// \brief Terminate devices: End
            SReturnValue execTerminate();
            /// \brief Shutdown DDS session
            SReturnValue execShutdown();

          private:
            struct SImpl;
            std::shared_ptr<SImpl> m_impl;
        };
    } // namespace core
} // namespace odc

#endif /* defined(__ODC__ControlService__) */
