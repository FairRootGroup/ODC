// Copyright 2019 GSI, Inc. All rights reserved.
//
//

// ODC
#include "EpncClient.h"
#include "Logger.h"

using namespace odc::core;
using namespace std;

CEpncClient::CEpncClient(shared_ptr<grpc::Channel> channel)
    : m_stub(epnc::EPNController::NewStub(channel))
{
}

void CEpncClient::allocateNodes(const std::string& _partitionID,
                                const std::string& _zone,
                                size_t _nodeCount,
                                vector<string>& _outputNodes)
{
    // Protobuf message takes the ownership and deletes the object
    epnc::Partition* partition = new epnc::Partition();
    partition->set_id(_partitionID);
    partition->set_zone(_zone);
    epnc::AllocationRequest request;
    request.set_allocated_partition(partition);
    request.set_node_count(_nodeCount);
    OLOG(ESeverity::debug) << "epnc: AllocateNodes request: " << request.DebugString();

    epnc::NodeList reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->AllocateNodes(&context, request, &reply);
    OLOG(ESeverity::debug) << "epnc: AllocateNodes reply: " << GetReplyString(status, reply);

    // epnc::EPNControllerStatus epncStatus {reply.status().status()};
    if (!status.ok())
    { //} || epncStatus != epnc::EPNControllerStatus::OK) {
        throw runtime_error("epnc: AllocateNodes request failed: " + GetReplyString(status, reply));
    }

    // string partitionID {reply.partition().id()};
    // string zone {reply.partition().zone()};
    _outputNodes.clear();
    for (int i = 0; i < reply.nodes_size(); i++)
    {
        _outputNodes.push_back(reply.nodes(i));
    }
}

void CEpncClient::releaseNode(const std::string& _partitionID,
                              const std::string& _zone,
                              const std::string& _node,
                              const std::string& _message)
{
    // Protobuf message takes the ownership and deletes the object
    epnc::Partition* partition = new epnc::Partition();
    partition->set_id(_partitionID);
    partition->set_zone(_zone);
    epnc::ReleaseNodeRequest request;
    request.set_allocated_partition(partition);
    request.set_node(_node);
    request.set_message(_message);
    OLOG(ESeverity::debug) << "epnc: ReleaseNode request: " << request.DebugString();

    epnc::EPNControllerReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->ReleaseNode(&context, request, &reply);
    OLOG(ESeverity::debug) << "epnc: ReleaseNode reply: " << GetReplyString(status, reply);

    epnc::EPNControllerStatus epncStatus{ reply.status() };
    if (!status.ok() || epncStatus != epnc::EPNControllerStatus::OK)
    {
        throw runtime_error("epnc: ReleaseNode request failed: " + GetReplyString(status, reply));
    }
}

void CEpncClient::releasePartition(const std::string& _partitionID, const std::string& _zone)
{
    epnc::Partition request;
    request.set_id(_partitionID);
    request.set_zone(_zone);
    OLOG(ESeverity::debug) << "epnc: ReleasePartition request: " << request.DebugString();

    epnc::EPNControllerReply reply;
    grpc::ClientContext context;
    grpc::Status status = m_stub->ReleasePartition(&context, request, &reply);
    OLOG(ESeverity::debug) << "epnc: ReleasePartition reply: " << GetReplyString(status, reply);

    epnc::EPNControllerStatus epncStatus{ reply.status() };
    if (!status.ok() || epncStatus != epnc::EPNControllerStatus::OK)
    {
        throw runtime_error("epnc: ReleasePartition request failed: " + GetReplyString(status, reply));
    }
}

template <typename Reply_t>
std::string CEpncClient::GetReplyString(const grpc::Status& _status, const Reply_t& _reply)
{
    if (_status.ok())
    {
        return _reply.DebugString();
    }
    else
    {
        std::stringstream ss;
        ss << "RPC failed with error code " << _status.error_code() << ": " << _status.error_message() << endl;
        return ss.str();
    }
}
