// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__ControlService__
#define __ODC__ControlService__

// STD
#include <memory>
#include <string>
#include <system_error>
// ODC
#include "DDSSubmit.h"
#include "Def.h"
#include "Topology.h"

namespace odc::core
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

        SError(std::error_code _code, const std::string& _details)
            : m_code(_code)
            , m_details(_details)
        {
        }

        std::error_code m_code; ///< Error code
        std::string m_details;  ///< Details of the error

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& _os, const SError& _error);
    };

    /// \brief Holds device status of detailed output
    struct SDeviceStatus
    {
        using container_t = std::vector<SDeviceStatus>;

        SDeviceStatus()
        {
        }

        SDeviceStatus(const DeviceStatus& _status, const std::string& _path)
            : m_status(_status)
            , m_path(_path)
        {
        }

        DeviceStatus m_status;
        std::string m_path;
    };

    /// \brief Aggregated topology state
    using TopologyState = SDeviceStatus::container_t;

    enum class ESessionStatus
    {
        unknown = 0,
        running = 1,
        stopped = 2
    };

    struct SPartitionStatus
    {
        using container_t = std::vector<SPartitionStatus>;

        SPartitionStatus()
        {
        }

        SPartitionStatus(const partitionID_t& _partitionID,
                         const std::string& _sessionID,
                         ESessionStatus _sessionStatus,
                         AggregatedTopologyState _aggregatedState)
            : m_partitionID(_partitionID)
            , m_sessionID(_sessionID)
            , m_sessionStatus(_sessionStatus)
            , m_aggregatedState(_aggregatedState)
        {
        }

        partitionID_t m_partitionID;                               ///< Partition ID
        std::string m_sessionID;                                   ///< Session ID of DDS
        ESessionStatus m_sessionStatus{ ESessionStatus::unknown }; ///< DDS session status
        AggregatedTopologyState m_aggregatedState{
            AggregatedTopologyState::Undefined
        }; ///< Aggregated state of the affected divices
    };

    struct SBaseReturnValue
    {
        SBaseReturnValue()
        {
        }

        SBaseReturnValue(EStatusCode _statusCode, const std::string& _msg, size_t _execTime, const SError& _error)
            : m_statusCode(_statusCode)
            , m_msg(_msg)
            , m_execTime(_execTime)
            , m_error(_error)
        {
        }
        EStatusCode m_statusCode{ EStatusCode::unknown }; ///< Operation status code
        std::string m_msg;                                ///< General message about the status
        size_t m_execTime{ 0 };                           ///< Execution time in milliseconds
        SError m_error;                                   ///< In case of error containes information about the error
    };

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
    struct SReturnValue : public SBaseReturnValue
    {
        SReturnValue()
        {
        }

        SReturnValue(EStatusCode _statusCode,
                     const std::string& _msg,
                     size_t _execTime,
                     const SError& _error,
                     const partitionID_t& _partitionID,
                     const std::string& _sessionID,
                     AggregatedTopologyState _aggregatedState,
                     SReturnDetails::ptr_t _details = nullptr)
            : SBaseReturnValue(_statusCode, _msg, _execTime, _error)
            , m_partitionID(_partitionID)
            , m_sessionID(_sessionID)
            , m_aggregatedState(_aggregatedState)
            , m_details(_details)
        {
        }

        partitionID_t m_partitionID; ///< Partition ID
        std::string m_sessionID;     ///< Session ID of DDS
        AggregatedTopologyState m_aggregatedState{
            AggregatedTopologyState::Undefined
        }; ///< Aggregated state of the affected divices

        // Optional parameters
        SReturnDetails::ptr_t m_details; ///< Details of the return value. Stored only if requested.
    };

    /// \brief Structure holds information about return status
    struct SStatusReturnValue : public SBaseReturnValue
    {
        SStatusReturnValue()
        {
        }

        SStatusReturnValue(EStatusCode _statusCode, const std::string& _msg, size_t _execTime, const SError& _error)
            : SBaseReturnValue(_statusCode, _msg, _execTime, _error)
        {
        }

        SPartitionStatus::container_t m_partitions; ///< Statuses of partitions
    };

    /// \brief Structure holds configuration parameters of the Initiaalize request
    struct SInitializeParams
    {
        SInitializeParams()
        {
        }

        SInitializeParams(const std::string& _sessionID)
            : m_sessionID(_sessionID)
        {
        }

        std::string m_sessionID; ///< DDS session ID

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& _os, const SInitializeParams& _params);
    };

    /// \brief Structure holds configuration parameters of the submit request
    struct SSubmitParams
    {
        SSubmitParams()
        {
        }

        SSubmitParams(const std::string& _plugin, const std::string& _resources)
            : m_plugin(_plugin)
            , m_resources(_resources)
        {
        }
        std::string m_plugin;    ///< ODC resource plugin name. Plugin has to be registered in ODC server.
        std::string m_resources; ///< Parcable description of the requested resources.

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& _os, const SSubmitParams& _params);
    };

    /// \brief Structure holds configuration parameters of the activate topology request
    struct SActivateParams
    {
        SActivateParams()
        {
        }

        SActivateParams(const std::string& _topologyFile,
                        const std::string& _topologyContent,
                        const std::string& _topologyScript)
            : m_topologyFile(_topologyFile)
            , m_topologyContent(_topologyContent)
            , m_topologyScript(_topologyScript)
        {
        }
        std::string m_topologyFile;    ///< Path to the topoloy file
        std::string m_topologyContent; ///< Content of the XML topology
        std::string m_topologyScript;  ///< Script that generates topology content

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& _os, const SActivateParams& _params);
    };

    /// \brief Structure holds configuration parameters of the updatetopology request
    struct SUpdateParams
    {
        SUpdateParams()
        {
        }

        SUpdateParams(const std::string& _topologyFile,
                      const std::string& _topologyContent,
                      const std::string& _topologyScript)
            : m_topologyFile(_topologyFile)
            , m_topologyContent(_topologyContent)
            , m_topologyScript(_topologyScript)
        {
        }
        std::string m_topologyFile;    ///< Path to the topoloy file
        std::string m_topologyContent; ///< Content of the XML topology
        std::string m_topologyScript;  ///< Script that generates topology content

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& _os, const SUpdateParams& _params);
    };

    /// \brief Structure holds configuaration parameters of the SetProperties request
    struct SSetPropertiesParams
    {
        using Property_t = std::pair<std::string, std::string>;
        using Properties_t = std::vector<Property_t>;

        SSetPropertiesParams()
        {
        }

        SSetPropertiesParams(const Properties_t& _properties, const std::string& _path)
            : m_path(_path)
            , m_properties(_properties)
        {
        }
        std::string m_path;        ///< Path in the topology
        Properties_t m_properties; ///< List of device configuration properties

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& _os, const SSetPropertiesParams& _params);
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

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& _os, const SDeviceParams& _params);
    };

    /// \brief Parameters of status request
    struct SStatusParams
    {
        SStatusParams()
        {
        }

        // \brief ostream operator.
        friend std::ostream& operator<<(std::ostream& _os, const SStatusParams& _params);
    };

    class CControlService
    {
      public:
        /// \brief Default constructor
        CControlService();

        /// \brief Set timeout of requests
        /// \param [in] _timeout Timeout in seconds
        void setTimeout(const std::chrono::seconds& _timeout);

        /// \brief Register resource plugins
        /// \param [in] _pluginMap Map of plugin name to path
        void registerResourcePlugins(const CPluginManager::PluginMap_t& _pluginMap);

        /// \brief Register request triggers
        /// \param [in] _triggerMap Map of plugin name to path
        void registerRequestTriggers(const CPluginManager::PluginMap_t& _triggerMap);

        //
        // DDS topology and session requests
        //

        /// \brief Initialize DDS session
        SReturnValue execInitialize(const partitionID_t& _partitionID, const SInitializeParams& _params);
        /// \brief Submit DDS agents. Can be called multiple times in order to submit more agents.
        SReturnValue execSubmit(const partitionID_t& _partitionID, const SSubmitParams& _params);
        /// \brief Activate topology
        SReturnValue execActivate(const partitionID_t& _partitionID, const SActivateParams& _params);
        /// \brief Run request combines Initialize, Submit and Activate
        SReturnValue execRun(const partitionID_t& _partitionID,
                             const SInitializeParams& _initializeParams,
                             const SSubmitParams& _submitParams,
                             const SActivateParams& _activateParams);
        /// \brief Update topology. Can be called multiple times in order to update topology.
        SReturnValue execUpdate(const partitionID_t& _partitionID, const SUpdateParams& _params);
        /// \brief Shutdown DDS session
        SReturnValue execShutdown(const partitionID_t& _partitionID);

        /// \brief Set properties
        SReturnValue execSetProperties(const partitionID_t& _partitionID, const SSetPropertiesParams& _params);
        /// \brief Get state
        SReturnValue execGetState(const partitionID_t& _partitionID, const SDeviceParams& _params);

        //
        // FairMQ device change state requests
        //

        /// \brief Configure devices: InitDevice->CompleteInit->Bind->Connect->InitTask
        SReturnValue execConfigure(const partitionID_t& _partitionID, const SDeviceParams& _params);
        /// \brief Start devices: Run
        SReturnValue execStart(const partitionID_t& _partitionID, const SDeviceParams& _params);
        /// \brief Stop devices: Stop
        SReturnValue execStop(const partitionID_t& _partitionID, const SDeviceParams& _params);
        /// \brief Reset devices: ResetTask->ResetDevice
        SReturnValue execReset(const partitionID_t& _partitionID, const SDeviceParams& _params);
        /// \brief Terminate devices: End
        SReturnValue execTerminate(const partitionID_t& _partitionID, const SDeviceParams& _params);

        //
        // Generic requests
        //

        /// \brief Status request
        SStatusReturnValue execStatus(const SStatusParams& _params);

      private:
        struct SImpl;
        std::shared_ptr<SImpl> m_impl;
    };
} // namespace odc::core

#endif /* defined(__ODC__ControlService__) */
