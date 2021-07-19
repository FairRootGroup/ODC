// Copyright 2019 GSI, Inc. All rights reserved.
//
//

#ifndef __ODC__EpncClient__
#define __ODC__EpncClient__

// STD
#include <string>
// GRPC
#include "epnc.grpc.pb.h"
#include <grpcpp/grpcpp.h>

class CEpncClient
{
  public:
    CEpncClient(std::shared_ptr<grpc::Channel> channel);

    void allocateNodes(const std::string& _partitionID,
                       const std::string& _zone,
                       size_t _nodeCount,
                       std::vector<std::string>& _outputNodes);
    void releaseNode(const std::string& _partitionID, const std::string& _node, const std::string& _message);
    void releasePartition(const std::string& _partitionID);

  private:
    template <typename Reply_t>
    std::string GetReplyString(const grpc::Status& _status, const Reply_t& _reply);

  private:
    std::unique_ptr<epnc::EPNController::Stub> m_stub;
};

#endif /* defined(__ODC__EpncClient__) */
