// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__EpncClient__
#define __ODC__EpncClient__

// ODC
#include "Logger.h"
// STD
#include <string>
// GRPC
#include "epnc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

class CEpncClient
{
  public:
    CEpncClient(std::shared_ptr<grpc::Channel> channel)
        : m_stub(epnc::EPNController::NewStub(channel))
    {}

    void allocateNodes(const std::string& _partitionID, const std::string& _zone, size_t _nodeCount, std::vector<std::string>& _outputNodes)
    {
        using namespace odc::core;
        // Protobuf message takes the ownership and deletes the object
        epnc::Partition* partition = new epnc::Partition();
        partition->set_id(_partitionID);
        partition->set_zone(_zone);
        epnc::AllocationRequest request;
        request.set_allocated_partition(partition);
        request.set_node_count(_nodeCount);
        OLOG(debug, _partitionID, 0) << "epnc: AllocateNodes request: " << request.DebugString();

        epnc::NodeList reply;
        grpc::ClientContext context;
        grpc::Status status = m_stub->AllocateNodes(&context, request, &reply);
        OLOG(debug, _partitionID, 0) << "epnc: AllocateNodes reply: " << GetReplyString(status, reply);

        if (!status.ok() || reply.status() != epnc::EPNControllerStatus::OK) {
            throw std::runtime_error(toString("epnc: AllocateNodes request failed: ", GetReplyString(status, reply)));
        }

        // string partitionID {reply.partition().id()};
        // string zone {reply.partition().zone()};
        _outputNodes.clear();
        for (int i = 0; i < reply.nodes_size(); i++) {
            _outputNodes.push_back(reply.nodes(i));
        }
    }

    void releaseNode(const std::string& _partitionID, const std::string& _node, const std::string& _message)
    {
        using namespace odc::core;
        epnc::ReleaseNodeRequest request;
        request.set_partition_id(_partitionID);
        request.set_node(_node);
        request.set_message(_message);
        OLOG(debug, _partitionID, 0) << "epnc: ReleaseNode request: " << request.DebugString();

        epnc::EPNControllerReply reply;
        grpc::ClientContext context;
        grpc::Status status = m_stub->ReleaseNode(&context, request, &reply);
        OLOG(debug, _partitionID, 0) << "epnc: ReleaseNode reply: " << GetReplyString(status, reply);

        epnc::EPNControllerStatus epncStatus{ reply.status() };
        if (!status.ok() || epncStatus != epnc::EPNControllerStatus::OK) {
            throw std::runtime_error(toString("epnc: ReleaseNode request failed: ", GetReplyString(status, reply)));
        }
    }

    void releasePartition(const std::string& _partitionID)
    {
        using namespace odc::core;
        epnc::ReleasePartitionRequest request;
        request.set_partition_id(_partitionID);
        OLOG(debug, _partitionID, 0) << "epnc: ReleasePartition request: " << request.DebugString();

        epnc::EPNControllerReply reply;
        grpc::ClientContext context;
        grpc::Status status = m_stub->ReleasePartition(&context, request, &reply);
        OLOG(debug, _partitionID, 0) << "epnc: ReleasePartition reply: " << GetReplyString(status, reply);

        epnc::EPNControllerStatus epncStatus{ reply.status() };
        if (!status.ok() || epncStatus != epnc::EPNControllerStatus::OK) {
            throw std::runtime_error(toString("epnc: ReleasePartition request failed: ", GetReplyString(status, reply)));
        }
    }

  private:
    template<typename Reply_t>
    std::string GetReplyString(const grpc::Status& _status, const Reply_t& _reply)
    {
        if (_status.ok()) {
            return _reply.DebugString();
        } else {
            std::stringstream ss;
            ss << "RPC failed with error code " << _status.error_code() << ": " << _status.error_message() << std::endl;
            return ss.str();
        }
    }

  private:
    std::unique_ptr<epnc::EPNController::Stub> m_stub;
};

#endif /* defined(__ODC__EpncClient__) */
