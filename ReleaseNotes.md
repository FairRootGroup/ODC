# ODC Release Notes

## v0.14 (NOT YET RELEASED)


## v0.12 (2020-11-17)
### ODC common
Modified: new C++ standard requirement - C++17.    
Added: new Commnad Line Interface of  `odc-cli-server` and `odc-grpc-client`. New interface was adapted for the multi-partition case: each request containes now a list of command line options.    


## v0.10 (2020-10-21)
### ODC common
Added: support of multiple partitions. In DDS terminology partition translates to a DDS session. ODC internally manages a mapping between a partition ID and corresponding DDS session.    

### ODC gRPC protocol
Modified: protocol was adapted to support multiple partitions (DDS sessions). Each request to ODC and each reply from ODC containes a `string partitionid` which uniquely identifies a partition. `runid` is removed.    


## v0.8 (2020-08-10)

### ODC common
Modified: improved error propagation. Use proper error codes and messages.    
Fixed: reset topology on shutdown. Topology can be activated and stoped multiple times.    
Added: execution of a sequence of commands in a batch mode. New "--batch" and "--cmds" command line arguments.    
Fixed: command line options of set properties request.    


## v0.6 (2020-07-16)

### ODC common
Added: new Run request which combines Initialize, Submit and Activate. Run request always creates a new DDS session.    
Added: new GetState request which returns a current aggregated state of FairMQ devices.    
Modified: bump DDS version is 3.5.1    

### ODC gRPC protocol
Modified: documentation of the proto file.    
Modified: StateChangeRequest changed to StateRequest.    
Modified: StateChangeReply changed to StateReply. New `state` field containing aggregated device state was added.    
Modified: implement SetPropertiesRequest instead of SetPropertyRequest. Multiple properties can be set with a single request.     


## v0.4 (2020-07-01)

### ODC common
Added: possibility to attach to the running DDS session.    
Added: DDS session ID in each reply.    
Added: set request timeout via command line interface.    
Added: unit tests.    
Modified: minimum required DDS version is 3.4    


## v0.2 (2020-05-13)

The first stable internal release.
