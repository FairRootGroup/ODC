// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__ControlService__
#define __ODC__ControlService__

// STD
#include <memory>
#include <string>
// FairMQ
#include <fairmq/sdk/Topology.h>

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

        /// \brief Holds device status of detailed output
        struct SDeviceStatus
        {
            using container_t = std::vector<SDeviceStatus>;

            SDeviceStatus()
            {
            }

            SDeviceStatus(const fair::mq::sdk::DeviceStatus& _status, const std::string& _path)
                : m_status(_status)
                , m_path(_path)
            {
            }

            fair::mq::sdk::DeviceStatus m_status;
            std::string m_path;
        };

        /// \brief Aggregated topology state
        using TopologyState = SDeviceStatus::container_t;

        struct SReturnDetails
        {
            using ptr_t = std::shared_ptr<SReturnDetails>;

            SReturnDetails()
            {
            }

            SReturnDetails(const TopologyState& _topologyState)
                : m_topologyState(_topologyState)
            {
            }

            TopologyState m_topologyState; ///< FairMQ aggregated topology state
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
                         runID_t _runID,
                         const std::string& _sessionID,
                         fair::mq::sdk::DeviceState _aggregatedState,
                         SReturnDetails::ptr_t _details = nullptr)
                : m_statusCode(_statusCode)
                , m_msg(_msg)
                , m_execTime(_execTime)
                , m_error(_error)
                , m_runID(_runID)
                , m_sessionID(_sessionID)
                , m_aggregatedState(_aggregatedState)
                , m_details(_details)
            {
            }

            EStatusCode m_statusCode{ EStatusCode::unknown }; ///< Operation status code
            std::string m_msg;                                ///< General message about the status
            size_t m_execTime{ 0 };                           ///< Execution time in milliseconds
            SError m_error;          ///< In case of error containes information about the error
            runID_t m_runID{ 0 };    ///< Run ID
            std::string m_sessionID; ///< Session ID of DDS
            // TODO: FIXME: find a better initial value, for the moment there is no undefined state
            fair::mq::sdk::DeviceState m_aggregatedState{
                fair::mq::sdk::DeviceState::Ok
            }; ///< Aggregated state of the affected divices

            // Optional parameters
            SReturnDetails::ptr_t m_details; ///< Details of the return value. Stored only if requested.
        };

        /// \brief Structure holds configuration parameters of the Initiaalize request
        struct SInitializeParams
        {
            SInitializeParams()
            {
            }

            SInitializeParams(runID_t _runID, const std::string& _sessionID)
                : m_runID(_runID)
                , m_sessionID(_sessionID)
            {
            }

            runID_t m_runID{ 0 };    ///< Run ID
            std::string m_sessionID; ///< DDS session ID
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

        /// \brief Structure holds device state params used in FairMQ device state chenge requests.
        struct SDeviceParams
        {
            SDeviceParams()
            {
            }

            SDeviceParams(const std::string& _path, bool _detailed)
                : m_path(_path)
                , m_detailed(_detailed)
            {
            }
            std::string m_path;       ///< Path to the topoloy file
            bool m_detailed{ false }; ///< If True than return also detailed information
        };

        class CControlService
        {
          public:
            /// \brief Default constructor
            CControlService();

            /// \brief Set timeout of requests
            /// \param [in] _timeout Timeout in seconds
            void setTimeout(const std::chrono::seconds& _timeout);

            //
            // DDS topology and session requests
            //

            /// \brief Initialize DDS session
            SReturnValue execInitialize(const SInitializeParams& _params);
            /// \brief Submit DDS agents. Can be called multiple times in order to submit more agents.
            SReturnValue execSubmit(const SSubmitParams& _params);
            /// \brief Activate topology
            SReturnValue execActivate(const SActivateParams& _params);
            /// \brief Run request combines Initialize, Submit and Activate
            SReturnValue execRun(const SInitializeParams& _initializeParams,
                                 const SSubmitParams& _submitParams,
                                 const SActivateParams& _activateParams);
            /// \brief Update topology. Can be called multiple times in order to update topology.
            SReturnValue execUpdate(const SUpdateParams& _params);
            /// \brief Shutdown DDS session
            SReturnValue execShutdown();

            /// \brief Set property
            SReturnValue execSetProperty(const SSetPropertyParams& _params);
            /// \brief Get state
            SReturnValue execGetState(const SDeviceParams& _params);

            //
            // FairMQ device change state requests
            //

            /// \brief Configure devices: InitDevice->CompleteInit->Bind->Connect->InitTask
            SReturnValue execConfigure(const SDeviceParams& _params);
            /// \brief Start devices: Run
            SReturnValue execStart(const SDeviceParams& _params);
            /// \brief Stop devices: Stop
            SReturnValue execStop(const SDeviceParams& _params);
            /// \brief Reset devices: ResetTask->ResetDevice
            SReturnValue execReset(const SDeviceParams& _params);
            /// \brief Terminate devices: End
            SReturnValue execTerminate(const SDeviceParams& _params);

          private:
            struct SImpl;
            std::shared_ptr<SImpl> m_impl;
        };
    } // namespace core
} // namespace odc

#endif /* defined(__ODC__ControlService__) */
